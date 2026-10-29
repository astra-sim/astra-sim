// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <astra-sim/system/gpu/ComputeUnit.hh>

#include <astra-sim/common/Random.hh>
#include <astra-sim/common/logging/EventTracer.hh>
#include <astra-sim/common/logging/Logger.hh>
#include <astra-sim/system/gpu/ComputeEngine.hh>
#include <astra-sim/system/gpu/CuPacketEventHandlerData.hh>
#include <astra-sim/system/gpu/GPUModel.hh>
#include <astra-sim/system/gpu/GpuSys.hh>
#include <astra-sim/system/gpu/TimingGPUModel.hh>
#include <astra-sim/system/gpu/Workgroup.hh>

using namespace AstraSim;

ComputeUnit::ComputeUnit(TimingGPUModel* gpu_model_p,
                         GpuSys* sys_p,
                         uint32_t id)
    : m_gpu_model_p(gpu_model_p),
      m_id(id),
      m_max_wgs(sys_p->max_wgs_per_cu),
      m_wfreq_size_nbytes(sys_p->wfreq_size_nbytes),
      m_num_wf_groups(sys_p->num_wf_groups_per_cu),
      m_nthreads_per_wf(sys_p->num_threads_per_wf),
      m_max_outstanding_wfreqs(sys_p->max_outstanding_wfreqs_per_cu),
      m_max_wfreqs_dispatched_per_cycle(sys_p->max_wfreqs_dispatched_per_cycle),
      m_max_retry_threshold(100000000),  // for forward-progress checking
      m_cu_issue_cycles(sys_p->cu_issue_cycles),
      m_use_random_wf_select(sys_p->use_random_wf_select),
      m_use_mshr(sys_p->use_mshr),
      m_name(m_gpu_model_p->name() + ".cu" + std::to_string(m_id)),
      m_next_wf_group_id(0),
      m_num_outstanding_wfreqs(0),
      m_wfreq_dispatch_scheduled(false) {
    m_mem_engine = std::make_unique<MemoryEngine>(
        sys_p->lsq_depth, sys_p->lsq_req_size_nbytes, sys_p->use_mshr);
    assert(m_max_wgs > 0);
    assert(m_max_outstanding_wfreqs > 0);
    assert(m_max_wfreqs_dispatched_per_cycle > 0);
    m_compute_engine_p =
        new ComputeEngine(this, sys_p->simd_latency, sys_p->simd_bandwidth);
    m_wf_groups.resize(m_num_wf_groups);

    // Init stats
    m_stat_num_wgs.init(this, "num_workgroups",
                        "Total number of workgroups dispatched to this CU");
    m_stat_num_blocked_net_ops.init(
        this, "num_blocked_net_ops",
        "Total number of operations blocked by network");
    m_stat_num_blocked_simd_ops.init(
        this, "num_blocked_simd_ops",
        "Total number of operations blocked by SIMD engines");
    m_stat_sampled_wfreq_cnt.init(
        this, "sampled_wfreq_cnt",
        "Sampled number of outstanding WF requests per CU");
    m_stat_coalescing_factor.init(
        this, "coalescing_factor",
        "Coalescing factor sampled from all WFs in this CU");
}

ComputeUnit::~ComputeUnit() {
    delete m_compute_engine_p;
}

const std::string& ComputeUnit::name() const {
    return m_name;
}

uint32_t ComputeUnit::getId() const {
    return m_id;
}

Tick ComputeUnit::curTick() const {
    return m_gpu_model_p->curTick();
}

uint32_t ComputeUnit::getMaxWGs() const {
    return m_max_wgs;
}

bool ComputeUnit::isAvailForNewWG() const {
    return m_active_wg_set.size() < m_max_wgs;
}

void ComputeUnit::dispatchWorkgroup(TimingWorkgroup* wg_p) {
    assert(wg_p != nullptr);
    assert(isAvailForNewWG());
    assert(m_active_wg_set.count(wg_p) == 0);

    // Insert this WG into the active WG set
    m_active_wg_set.insert(wg_p);

    // Create WG's wavefronts.
    wg_p->execute();

    // Map WG's wavefronts to different WF groups in RR order
    auto num_wfs = wg_p->getNumWavefronts();
    for (auto wf_id = 0; wf_id < num_wfs; ++wf_id) {
        auto wf_gid = wf_id % m_num_wf_groups;
        auto wf_p = wg_p->getWavefrontPtr(wf_id);
        assert(wf_p);
        m_wf_groups[wf_gid].push_back(wf_p);
        wf_p->setWfGroupId(wf_gid);
        DPRINTF("Mapped WF%d of %s to WF group %d of CU_%d\n", wf_id,
                wg_p->to_string(), wf_gid, m_id);
    }

    // Schedule an event to start dispatching requests from active wavefronts
    schedWFReqDispatchEvent();
    DPRINTF("Issued %s\n", wg_p->to_string());
}

void ComputeUnit::dispatchNextWFReqs() {
    // We iterate across WF groups in RR order, starting from the
    // next_wf_group_id. In each WF group, we try to issue WF requests for the
    // oldest WF first. We keep dispatching new WF requests until we hit the max
    // outstanding wfreqs or there is no more wfreq to dispatch.
    std::queue<uint32_t> ordered_wf_group_ids;
    for (uint32_t i = 0; i < m_num_wf_groups; ++i) {
        ordered_wf_group_ids.push((m_next_wf_group_id + i) % m_num_wf_groups);
    }

    struct SelWFInfo {
        Wavefront* wf_p;
        WFReqPtr wfreq_p;
        uint32_t num_lsq_entries;
    };

    uint32_t dispatched_req_cnt = 0;
    bool sched_next_wfreq_dispatch = false;
    while ((dispatched_req_cnt < m_max_wfreqs_dispatched_per_cycle) &&
           (m_num_outstanding_wfreqs < m_max_outstanding_wfreqs) &&
           !ordered_wf_group_ids.empty()) {
        auto wf_group_id = ordered_wf_group_ids.front();
        auto& wf_list = m_wf_groups[wf_group_id];

        // Count the number of advancable and compute wfreqs in the queue
        std::vector<SelWFInfo> sel_wf_list;
        uint32_t num_compute_wfreqs = 0;
        uint32_t num_advance_ready_wfreqs = 0;
        for (auto wf_p : wf_list) {
            WFReqPtr wfreq_p = wf_p->peekNextReadyWFReq();
            uint32_t num_lsq_entries =
                (wfreq_p != nullptr) ? m_mem_engine->getNumLSQEntries(wfreq_p)
                                     : 0;
            sel_wf_list.push_back({wf_p, wfreq_p, num_lsq_entries});
            // Count the number of non-LSQ wfreqs in the queue
            if (num_lsq_entries == 0) {
                if (wfreq_p != nullptr) {
                    // Compute wfreqs that are ready to be dispatched
                    num_compute_wfreqs++;
                } else if (wf_p->isCompleted() ||
                           ((wf_p->isNop() || wf_p->isBarrier()) &&
                            !wf_p->isStalled() &&
                            (wf_p->getNumOutstandingWFReqs() == 0))) {
                    // Find actionable wfreqs that are ready to be dispatched
                    num_advance_ready_wfreqs++;
                }
            }
        }

        // Shuffle the wfreqs if random wf selection is enabled
        if (m_use_random_wf_select) {
            std::shuffle(sel_wf_list.begin(), sel_wf_list.end(),
                         g_rand_generator);
        }

        // If the LSQ is full (no space for new LSQ wfreqs) and there are no
        // non-LSQ wfreqs in the queue, stop dispatching from this WF group
        if (m_mem_engine->lsq()->isFull() &&
            (num_advance_ready_wfreqs == 0 &&
             num_compute_wfreqs ==
                 0)) {  // No advanceable or compute wfreqs in the queue, so no
                        // need to dispatch from this WF group
            DPRINTF(
                "LSQ is full and there are no actionable or compute wfreqs in "
                "the queue, stopping dispatching from this WF group %s\n",
                wf_group_id);
        } else {
            // We either can dispatch LSQ wfreqs, or there are actionable wfreqs
            // in the queue, so iterate over the wfreqs queue to find the next
            // wfreq to dispatch
            for (auto& sel_wf_info : sel_wf_list) {
                auto wf_p = sel_wf_info.wf_p;
                auto wfreq_p = sel_wf_info.wfreq_p;
                auto num_lsq_entries = sel_wf_info.num_lsq_entries;

                if (wfreq_p != nullptr) {
                    // If it is an LSQ wfreq and the LSQ is full or the number
                    // of outstanding wfreqs would exceed the max, schedule the
                    // next dispatching event, and skip dispatching this wfreq.
                    if (num_lsq_entries > 0 &&
                        (m_num_outstanding_wfreqs + num_lsq_entries >
                             m_max_outstanding_wfreqs ||
                         m_mem_engine->lsq()->isFull())) {
                        continue;
                    }

                    wfreq_p = wf_p->execute();
                    assert(wfreq_p != nullptr);

                    if (num_lsq_entries > 0) {
                        m_mem_engine->insertWFReqToLSQ(wfreq_p);
                        auto* lsq_entry =
                            m_mem_engine->lsq()->getEntry(wfreq_p->wfreq_id);
                        assert(lsq_entry != nullptr);
                        DPRINTF("LSQ dispatch %s: %d addrs generated"
                                " (vaddr=0x%lx, size=%d):\n",
                                wfreq_p->to_string(),
                                (int)lsq_entry->mem_reqs.size(), wfreq_p->vaddr,
                                (int)wfreq_p->reqSizeNBytes);
                        for (int addr_idx = 0;
                             addr_idx < (int)lsq_entry->mem_reqs.size();
                             ++addr_idx) {
                            DPRINTF("  addr[%d]=0x%lx\n", addr_idx,
                                    lsq_entry->mem_reqs[addr_idx].vaddr);
                        }
                    }

                    wfreq_p->status = WFReq::Status::Dispatched;
                    DPRINTF("Dispatched a wfreq: %s from WF %s\n",
                            wfreq_p->to_string(), wf_p->to_string());
                    dispatched_req_cnt++;

                    scheduleEventInCycles(EventType::CU_IssueWFReq,
                                          new CU_Event_CallData(wfreq_p),
                                          m_cu_issue_cycles);
                    m_num_outstanding_wfreqs += num_lsq_entries;
                    m_stat_sampled_wfreq_cnt.sample(m_num_outstanding_wfreqs);

                    sched_next_wfreq_dispatch = true;
                    break;
                }

                // peekNextReadyWFReq() returns nullptr for Nop/Barrier,
                // WaitCnt, and end-of-iteration with no ready wfreqs. Call
                // execute() to advance WF state without peeking past LSQ
                // backpressure above.
                (void)wf_p->execute();

                if (wf_p->isCompleted()) {
                    // Completing this WF
                    DPRINTF("No more requests from this completing WF %s\n",
                            wf_p->to_string());
                    completeWavefront(wf_p);
                } else if (wf_p->isNop() || wf_p->isBarrier()) {
                    // For Nop and Barrier ops, we can dispatch new requests
                    // only when the Nop/Barrier is executed (i.e., all prior
                    // requests are completed)
                    if (!wf_p->isStalled() &&
                        (wf_p->getNumOutstandingWFReqs() == 0)) {
                        sched_next_wfreq_dispatch = true;
                        DPRINTF("Scheduled next dispatching from WF %s at "
                                "Nop/Barrier\n",
                                wf_p->to_string());
                    } else {
                        DPRINTF(
                            "Paused dispatching from WF %s at Nop/Barrier\n",
                            wf_p->to_string());
                    }
                } else if (wf_p->hasMoreOps() && !wf_p->isStalled() &&
                           !wf_p->isOnWaitCnt()) {
                    // As long as the WF is not completed, stalled, or waiting
                    // on a waitcnt, there should be more request to dispatch.
                    sched_next_wfreq_dispatch = true;
                    // DPRINTF("Scheduling next dispatching from WF %s: "
                    //         "has_more_ops=%d, is_stalled=%d, is_nop=%d, "
                    //         "is_barrier=%d, is_on_waitcnt=%d\n",
                    //         wf_p->to_string(), wf_p->hasMoreOps(),
                    //         wf_p->isStalled(), wf_p->isNop(),
                    //         wf_p->isBarrier(), wf_p->isOnWaitCnt());
                } else {
                    DPRINTF("Paused dispatching from WF %s: has_more_ops=%d, "
                            "is_stalled=%d, is_nop=%d, is_barrier=%d, "
                            "is_on_waitcnt=%d\n",
                            wf_p->to_string(), wf_p->hasMoreOps(),
                            wf_p->isStalled(), wf_p->isNop(), wf_p->isBarrier(),
                            wf_p->isOnWaitCnt());
                }
            }
        }
        // Move on to the next WF group in this cycle
        ordered_wf_group_ids.pop();
        m_next_wf_group_id = (wf_group_id + 1) % m_num_wf_groups;
    }

    // Schedule the next WF request dispatching event if needed
    if (sched_next_wfreq_dispatch) {
        schedWFReqDispatchEvent();
    }

    DPRINTF("Dispatched %d wfreqs, outstanding_wfreqs=%d, "
            "sched_next_dispatch=%d\n",
            dispatched_req_cnt, m_num_outstanding_wfreqs,
            sched_next_wfreq_dispatch);
}

void ComputeUnit::issueWFReq(WFReqPtr wfreq_p) {
    assert(wfreq_p != nullptr);
    // A retry re-issue event (EventType::CU_IssueWFReq) may have been scheduled
    // before another event finalized this WFReq -- e.g. a multi-address
    // SemAcquire in polling mode whose remaining addresses were completed by a
    // different retry/wakeup. Once finalized, the WFReq is Completed and its
    // LSQ entry is gone, so this leftover event is stale -> no-op.
    // (First issue: status == Dispatched; subsequent (re)issues: Issued.)
    if (wfreq_p->status == WFReq::Status::Completed) {
        return;
    }

    // Non-LSQ instructions are handled by the SIMD engine
    if (wfreq_p->inst == GpuInst::Reduce) {
        // For compute ops, send them to the compute engine for timing
        // Currently, all compute ops are handled by the compute engine (no
        // backpressure from SIMD engines), and this function always returns 1
        int ret_val = m_compute_engine_p->send(wfreq_p);
        if (ret_val != 1) {
            m_stat_num_blocked_simd_ops++;
        } else {
            wfreq_p->status = WFReq::Status::Issued;
            wfreq_p->issue_tick = curTick();
            DPRINTF("Issued %s\n", wfreq_p->to_string());
        }
        return;
    }

    // For LSQ instructions, issue one sim_request per Pending aligned address.
    auto* lsq_entry = m_mem_engine->lsq()->getEntry(wfreq_p->wfreq_id);
    // Only (re)issue when this WFReq still has real pending work. If another
    // event already finalized it (entry removed) or moved every address to
    // InFlight/Completed, this retry is a stale no-op.
    if (lsq_entry == nullptr || !lsq_entry->hasPending()) {
        return;
    }

    int ret_val = 0;
    // Issue one sim_request per Pending aligned address.
    for (size_t i = 0; i < lsq_entry->mem_reqs.size(); ++i) {
        auto& mem_req = lsq_entry->mem_reqs[i];
        if (mem_req.status != LSQEntry::MemReqStatus::Pending) {
            continue;
        }

        uint64_t addr = mem_req.vaddr;

        // MSHR coalescing for loads
        if (wfreq_p->inst == GpuInst::Load && m_use_mshr) {
            bool is_primary = m_mem_engine->insertMSHR(addr, wfreq_p->wfreq_id);
            if (!is_primary) {
                // MSHR tracks this wfreq_id; completeMSHRAddr will mark the
                // LSQ line done when the primary's line completes.
                lsq_entry->setMemReqStatus(i, LSQEntry::MemReqStatus::InFlight);
                DPRINTF("MSHR coalesced addr=0x%lx for %s (InFlight via "
                        "MSHR)\n",
                        addr, wfreq_p->to_string());
                continue;
            }
        }

        auto cu_handler_data = new ComputeUnitPacketEventHandlerData(
            m_gpu_model_p->getSysPtr()->id, EventType::PacketCmdAcked, this,
            new CU_Event_CallData(wfreq_p, addr), EventType::CU_WakeupWFReq);

        // Generate one sim_request for each pending aligned address that did
        // not coalesce in MSHR
        auto sim_req =
            wfreq_p->genSimRequest(addr, m_mem_engine->lsq()->getReqSize());
        mem_req.astra_req_id = sim_req.astraReqId;
        auto cmd_type = wfreq_p->getAstraNetCmdType(m_gpu_model_p->getId());

        ret_val = m_gpu_model_p->getSysPtr()->sim_cmd(
            cmd_type, sim_req, &Sys::handleEvent, cu_handler_data);

        if (ret_val == 1) {
            lsq_entry->setMemReqStatus(i, LSQEntry::MemReqStatus::InFlight);
            if (((wfreq_p->inst == GpuInst::SemAcquire) ||
                 (wfreq_p->inst == GpuInst::Load)) &&
                (m_gpu_model_p->isSemAddr(addr)) && (wfreq_p->retry_cnt == 0)) {
                if (m_gpu_model_p->getSysPtr()->use_mwait) {
                    GPUModel::CallbackFunc callback{
                        &Sys::handleEvent,
                        new ComputeUnitPacketEventHandlerData(
                            m_gpu_model_p->getSysPtr()->id,
                            EventType::PacketCmdAcked, this,
                            new CU_Event_CallData(wfreq_p, addr),
                            EventType::CU_WakeupWFReq)};
                    m_gpu_model_p->registerAcquireSemaphore(addr, callback);
                } else {
                    GPUModel::CallbackFunc callback{nullptr, nullptr};
                    m_gpu_model_p->registerAcquireSemaphore(addr, callback);
                }
            }
        } else if (ret_val == 0) {
            // Stay Pending; re-issue this line on a later CU_IssueWFReq.
            m_stat_num_blocked_net_ops++;
        }
    }

    lsq_entry->assertStatusCounts();
    const bool has_in_flight = lsq_entry->hasInFlight();
    const bool has_pending = lsq_entry->hasPending();

    wfreq_p->status =
        has_in_flight ? WFReq::Status::Issued : WFReq::Status::Dispatched;
    if (has_pending) {
        scheduleEventInCycles(EventType::CU_IssueWFReq,
                              new ComputeUnit::CU_Event_CallData(wfreq_p),
                              m_gpu_model_p->getSysPtr()->wait_retry_cycles);
    }
    wfreq_p->issue_tick = curTick();
    DPRINTF("Issued LSQ %s (%d addrs, pending_retry=%d):\n",
            wfreq_p->to_string(), (int)lsq_entry->mem_reqs.size(), has_pending);
    for (int di = 0; di < (int)lsq_entry->mem_reqs.size(); ++di) {
        const auto& mem_req = lsq_entry->mem_reqs[di];
        DPRINTF("  addr[%d]=0x%lx status=%s netReqId=%d\n", di, mem_req.vaddr,
                LSQEntry::getMemReqStatusName(mem_req.status),
                mem_req.astra_req_id);
    }
}

void ComputeUnit::wakeupWFReq(WFReqPtr wfreq_p,
                              uint64_t vaddr) {  // use vaddr ID
    assert(wfreq_p != nullptr);

    if (wfreq_p->inst == GpuInst::Reduce) {
        assert(wfreq_p->status == WFReq::Status::Issued);
        auto inst_type = wfreq_p->inst;
        auto inst_type_name = getGpuInstName(inst_type);
        if (m_stat_wfreq_lat.count(inst_type) == 0) {
            m_stat_wfreq_lat[inst_type].init(
                this, "wfreq_latency_" + inst_type_name,
                "Execution latency for inst " + inst_type_name);
        }
        auto exec_ticks = curTick() - wfreq_p->issue_tick;
        m_stat_wfreq_lat[inst_type].sample(exec_ticks);
        DPRINTF("Completed %s, exec_lat=%d\n", wfreq_p->to_string(),
                exec_ticks);
        completeWFReq(wfreq_p);
        return;
    }

    bool retry = false;
    auto* lsq_entry = m_mem_engine->lsq()->getEntry(wfreq_p->wfreq_id);
    // Under use_mwait a semaphore load has two completion drivers (network
    // read-ack and the registerAcquireSemaphore callback). The first one to
    // finalize this WFReq removes its LSQ entry; the second wakeup is then
    // stale -> no-op (and must NOT call acquireSemaphore() a second time).
    if (lsq_entry == nullptr) {
        assert(wfreq_p->status == WFReq::Status::Completed);
        return;
    }

    int addr_idx = lsq_entry->findAddrIndex(vaddr);
    if (addr_idx < 0) {
        THROW_ERR("LSQ addr 0x%lx not found for %s\n", vaddr,
                  wfreq_p->to_string());
    }

    auto& mem_req = lsq_entry->mem_reqs[addr_idx];
    auto addr_st = mem_req.status;

    if (addr_st == LSQEntry::MemReqStatus::InFlight) {
        if ((wfreq_p->inst == GpuInst::Load ||
             wfreq_p->inst == GpuInst::SemAcquire) &&
            m_gpu_model_p->isSemAddr(vaddr)) {
            bool completed = m_gpu_model_p->acquireSemaphore(vaddr);
            if (!completed) {
                if (m_gpu_model_p->getSysPtr()->use_mwait) {
                    // Stay InFlight; GPUModel will deliver another wakeup
                    // when the semaphore can be acquired.
                } else {
                    retry = true;
                    lsq_entry->setMemReqStatus(addr_idx,
                                               LSQEntry::MemReqStatus::Pending);
                    DPRINTF("LSQ wakeup %s addr[%d]=0x%lx netReqId=%d:"
                            " InFlight->Pending (retry %d)\n",
                            wfreq_p->to_string(), addr_idx, vaddr,
                            mem_req.astra_req_id, wfreq_p->retry_cnt + 1);
                }
            } else {
                lsq_entry->markComplete(addr_idx);
                DPRINTF("LSQ wakeup %s addr[%d]=0x%lx netReqId=%d:"
                        " InFlight->Completed (%d/%d completed)\n",
                        wfreq_p->to_string(), addr_idx, vaddr,
                        mem_req.astra_req_id, lsq_entry->num_completed,
                        (int)lsq_entry->mem_reqs.size());
                if (lsq_entry->isAllComplete()) {
                    finalizeLSQEntry(wfreq_p);
                }
            }
        } else if (wfreq_p->inst == GpuInst::Load && m_use_mshr) {
            DPRINTF("LSQ wakeup %s addr[%d]=0x%lx netReqId=%d:"
                    " InFlight->MSHR complete\n",
                    wfreq_p->to_string(), addr_idx, vaddr,
                    mem_req.astra_req_id);
            m_mem_engine->completeMSHRAddr(
                vaddr, [this](WFReqPtr p) { finalizeLSQEntry(p); });
        } else {
            lsq_entry->markComplete(addr_idx);
            DPRINTF("LSQ wakeup %s addr[%d]=0x%lx netReqId=%d:"
                    " InFlight->Completed (%d/%d completed)\n",
                    wfreq_p->to_string(), addr_idx, vaddr, mem_req.astra_req_id,
                    lsq_entry->num_completed, (int)lsq_entry->mem_reqs.size());
            if (lsq_entry->isAllComplete()) {
                finalizeLSQEntry(wfreq_p);
            }
        }
    }

    if (retry) {
        if (++wfreq_p->retry_cnt > m_max_retry_threshold) {
            THROW_ERR("Detected a WF req not making forward progress after "
                      "%d retries: %s\n",
                      wfreq_p->retry_cnt, wfreq_p->to_string());
        }
        wfreq_p->status = WFReq::Status::Dispatched;
        scheduleEventInCycles(EventType::CU_IssueWFReq,
                              new ComputeUnit::CU_Event_CallData(wfreq_p),
                              m_gpu_model_p->getSysPtr()->wait_retry_cycles);
    }
}

void ComputeUnit::completeWFReq(WFReqPtr wfreq_p) {
    const uint32_t num_lsq_entries = m_mem_engine->getNumLSQEntries(wfreq_p);

    if (num_lsq_entries > 0) {
        auto* lsq_entry = m_mem_engine->lsq()->getEntry(wfreq_p->wfreq_id);
        assert(lsq_entry != nullptr);
        assert(lsq_entry->isAllComplete());
        m_mem_engine->removeWFReqFromLSQ(wfreq_p);
    }

    assert(m_num_outstanding_wfreqs >= num_lsq_entries);
    m_num_outstanding_wfreqs -= num_lsq_entries;

    // Call back the issuing wavefront to notify the completion of this request
    auto wf_p = wfreq_p->wf_p;
    wf_p->completeWFReq(wfreq_p);

    if (wf_p->isCompleted()) {
        auto wf_group_id = wf_p->getWfGroupId();
        auto& wf_list = m_wf_groups.at(wf_group_id);
        auto wf_it = std::find(wf_list.begin(), wf_list.end(), wf_p);
        assert(wf_it != wf_list.end());
        completeWavefront(wf_p);
    }

    uint32_t num_outstanding_wfs = 0;
    for (auto& wf_list : m_wf_groups) {
        num_outstanding_wfs += wf_list.size();
    }
    if (num_outstanding_wfs > 0) {
        schedWFReqDispatchEvent();
    }
}

void ComputeUnit::completeWavefront(Wavefront* wf_p) {
    assert(wf_p);
    // This WF is completed, so we need to remove it from its WF group
    auto wf_group_id = wf_p->getWfGroupId();
    assert(wf_group_id < m_wf_groups.size());
    DPRINTF("Completing a WF: wf_id=%d from wf_group_id=%d\n", wf_p->getWfId(),
            wf_group_id);
    auto& wf_list = m_wf_groups.at(wf_group_id);
    auto it = std::find(wf_list.begin(), wf_list.end(), wf_p);
    assert(it != wf_list.end());
    wf_list.erase(it);

    // Inform the owning workgroup that this WF has been completed
    TimingWorkgroup* wg_p = wf_p->getWorkgroupPtr();
    wg_p->completeWavefront(wf_p->getWfId());

    // Check if the owning WG has completed all its WFs. If so, retire this WG.
    assert(m_active_wg_set.count(wg_p) == 1);
    DPRINTF("Number of remaining WFs in WG_%d: %d\n", wg_p->getWorkgroupId(),
            wg_p->getNumOutstandingWavefronts());
    if (wg_p->isCompleted()) {
        DPRINTF("Retiring %s\n", wg_p->to_string());

        // Notify GPU model that this WG has been completed
        m_gpu_model_p->completeWorkgroup(wg_p);

        // Erase this WG from the active WG set
        m_active_wg_set.erase(wg_p);
    }
}

void ComputeUnit::call(EventType type, CallData* data) {
    auto call_data_p = dynamic_cast<CU_Event_CallData*>(data);
    assert(call_data_p);
    if (type == EventType::CU_DispatchWFReqs) {
        assert(m_wfreq_dispatch_scheduled);
        m_wfreq_dispatch_scheduled = false;
        dispatchNextWFReqs();
    } else if (type == EventType::CU_IssueWFReq) {
        assert(call_data_p);
        issueWFReq(call_data_p->wfreq_p);
    } else if (type == EventType::CU_WakeupWFReq) {
        assert(call_data_p);
        wakeupWFReq(call_data_p->wfreq_p, call_data_p->vaddr);
    } else if (type == EventType::CU_UnstallWorkgroup) {
        assert(call_data_p);
        auto wg_p = call_data_p->wg_p;
        assert(wg_p);
        assert(m_active_wg_set.count(wg_p) == 1);
        wg_p->unstall();
        DPRINTF("Unstalled %s\n", wg_p->to_string());
        // Schedule an event in the next cycle to dispatch new WF requests
        schedWFReqDispatchEvent();
    } else {
        THROW_ERR("Invalid ComputeUnit event type: %d\n",
                  static_cast<int>(type));
    }
    // Clean up
    if (data != nullptr) {
        delete data;
    }
}

GpuSys* ComputeUnit::getSysPtr() const {
    return m_gpu_model_p->getSysPtr();
}

TimingGPUModel* ComputeUnit::getGpuModelPtr() const {
    return m_gpu_model_p;
}

void ComputeUnit::scheduleEventInCycles(EventType event_type,
                                        CallData* call_data_p,
                                        uint64_t cycles) {
    getSysPtr()->registerEventInCycles(this, event_type, call_data_p, cycles);
}

void ComputeUnit::schedWFReqDispatchEvent() {
    if (m_wfreq_dispatch_scheduled) {
        return;
    }

    // Schedule an event in the next cycle to dispatch new WF requests
    scheduleEventInCycles(EventType::CU_DispatchWFReqs,
                          new ComputeUnit::CU_Event_CallData(), 1);
    m_wfreq_dispatch_scheduled = true;
}

void ComputeUnit::schedWorkgroupUnstallEvent(TimingWorkgroup* wg_p) {
    if (m_active_wg_set.count(wg_p) == 0) {
        THROW_ERR("Cannot unstall a non-active workgroup: %s\n",
                  wg_p->to_string());
    }

    // Schedule an event in the next cycle to unstall this workgroup
    scheduleEventInCycles(EventType::CU_UnstallWorkgroup,
                          new CU_Event_CallData(wg_p), 1);
}

void ComputeUnit::traceWavefrontOpUpdate(Wavefront* wf_p) const {
    assert(wf_p);
    g_wavefront_tracer.trace(
        static_cast<const SimObject*>(this),
        [](std::ofstream& os, Wavefront* wf_p) {
            os << "wg=" << wf_p->getWorkgroupPtr()->getWorkgroupId() << ",";
            os << "wf=" << wf_p->getWfId() << ",";
            os << "op="
               << (wf_p->getCurOpPtr() ? wf_p->getCurOpPtr()->getName()
                                       : "null")
               << ",";
            os << "iter=" << wf_p->getCurrentIterationIdx();
        },
        wf_p);
}

void ComputeUnit::finalizeLSQEntry(WFReqPtr wfreq_p) {
    auto* lsq_entry = m_mem_engine->lsq()->getEntry(wfreq_p->wfreq_id);
    assert(lsq_entry != nullptr);

    auto inst_type = wfreq_p->inst;
    auto inst_type_name = getGpuInstName(inst_type);
    if (m_stat_wfreq_lat.count(inst_type) == 0) {
        m_stat_wfreq_lat[inst_type].init(
            this, "wfreq_latency_" + inst_type_name,
            "Execution latency for inst " + inst_type_name);
    }
    auto exec_ticks = curTick() - wfreq_p->issue_tick;
    m_stat_wfreq_lat[inst_type].sample(exec_ticks);

    DPRINTF("LSQ finalize %s: exec_lat=%d, %d addrs all completed:\n",
            wfreq_p->to_string(), exec_ticks, (int)lsq_entry->mem_reqs.size());
    for (int i = 0; i < (int)lsq_entry->mem_reqs.size(); ++i) {
        const auto& mem_req = lsq_entry->mem_reqs[i];
        DPRINTF("  addr[%d]=0x%lx status=%s netReqId=%d\n", i, mem_req.vaddr,
                LSQEntry::getMemReqStatusName(mem_req.status),
                mem_req.astra_req_id);
    }

    wfreq_p->status = WFReq::Status::Issued;
    completeWFReq(wfreq_p);
}
