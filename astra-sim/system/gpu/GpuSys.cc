// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <astra-sim/system/gpu/GpuSys.hh>

#include <cstdlib>
#include <fstream>
#include <iostream>

#include <astra-sim/common/logging/EventTracer.hh>
#include <astra-sim/system/common/BasicEventHandlerData.hh>
#include <astra-sim/system/gpu/CuPacketEventHandlerData.hh>
#include <astra-sim/system/gpu/FunctionalGPUModel.hh>
#include <astra-sim/system/gpu/GPUModel.hh>
#include <astra-sim/system/gpu/TimingGPUModel.hh>

#include <json/json.hpp>

using json = nlohmann::json;

typedef JSONNode::CollectiveCommType ChakraCollectiveCommType;

namespace AstraSim {

/** Builds a Sys that models collectives through a GPUModel. */
GpuSys::GpuSys(int id,
               std::string workload_configuration,
               std::string comm_group_configuration,
               std::string system_configuration,
               AstraRemoteMemoryAPI* remote_mem,
               AstraNetworkAPI* comm_NI,
               timespec_t clock_period,
               uint64_t comm_size,
               std::string custom_comm_plan_map_filename)
    : Sys(id,
          workload_configuration,
          comm_group_configuration,
          system_configuration,
          remote_mem,
          comm_NI,
          comm_size,
          custom_comm_plan_map_filename),
      m_clock_period(clock_period) {
    parseGpuConfig(system_configuration);

    // initialize gpu_model only after system configuration is loaded/parsed
    if (timing_simulation) {
        this->gpu_model = new TimingGPUModel(this, workload, id, memory_mode);
    } else {
        this->gpu_model =
            new FunctionalGPUModel(this, workload, id, memory_mode);
    }
}

GpuSys::~GpuSys() {
    // Confirm the verifier succeeded.
    if (!timing_simulation) {
        if (gpu_model->verify()) {
            std::cout << "Verifier passed!" << std::endl;
        } else {
            std::cout << "Verifier failed!" << std::endl;
        }
    }

    if (gpu_model != nullptr) {
        delete gpu_model;
    }
}

Tick GpuSys::getClockPeriodInTicks() const {
    assert(m_clock_period.time_res == time_type_e::PS);
    return m_clock_period.time_val;
}

void GpuSys::registerEventInCycles(Callable* callable,
                                   EventType event,
                                   CallData* callData,
                                   uint64_t delta_cycles) {
    Tick delta_ticks = delta_cycles * this->getClockPeriodInTicks();
    register_event(callable, event, callData, delta_ticks);
}

void GpuSys::parseGpuConfig(const std::string& filename) {
    std::ifstream inFile;
    inFile.open(filename);
    if (!inFile) {
        if (id == 0) {
            std::cerr << "Unable to open file: " << filename << std::endl;
        }
        exit(1);
    }

    json j;
    inFile >> j;

    // CU-level configs
    if (j.contains("num-cus-per-gpu")) {
        this->num_cus = j["num-cus-per-gpu"];
        DPRINTF("Set num_cus=%d\n", num_cus);
    } else {
        THROW_ERR("Missing num-cus-per-gpu in system config %s\n", filename);
    }
    if (j.contains("num-wf-groups-per-cu")) {
        this->num_wf_groups_per_cu = j["num-wf-groups-per-cu"];
        DPRINTF("Set num_wf_groups_per_cu=%d\n", num_wf_groups_per_cu);
    } else {
        this->num_wf_groups_per_cu = 4;
        DPRINTF("Use default num_wf_groups_per_cu=%d\n", num_wf_groups_per_cu);
    }
    if (j.contains("max-workgroups-per-cu")) {
        this->max_wgs_per_cu = j["max-workgroups-per-cu"];
        DPRINTF("Set max_wgs_per_cu=%d\n", max_wgs_per_cu);
    } else {
        THROW_ERR("Missing max-workgroups-per-cu in system config %s\n",
                  filename);
    }
    if (j.contains("max-outstanding-wfreqs-per-cu")) {
        this->max_outstanding_wfreqs_per_cu =
            j["max-outstanding-wfreqs-per-cu"];
        DPRINTF("Set max_outstanding_wfreqs_per_cu=%d\n",
                max_outstanding_wfreqs_per_cu);
    } else {
        THROW_ERR("Missing max-outstanding-wfreqs-per-cu "
                  "in system config %s\n",
                  filename);
    }
    if (j.contains("max-wfreqs-dispatched-per-cycle")) {
        this->max_wfreqs_dispatched_per_cycle =
            j["max-wfreqs-dispatched-per-cycle"];
        DPRINTF("Set max_wfreqs_dispatched_per_cycle=%d\n",
                max_wfreqs_dispatched_per_cycle);
    } else {
        this->max_wfreqs_dispatched_per_cycle = 2;
        DPRINTF("Set max_wfreqs_dispatched_per_cycle=%d (default value)\n",
                max_wfreqs_dispatched_per_cycle);
    }
    if (j.contains("num-threads-per-wf")) {
        this->num_threads_per_wf = j["num-threads-per-wf"];
        DPRINTF("Set num_threads_per_wf=%d\n", num_threads_per_wf);
    } else {
        this->num_threads_per_wf = 32;
        DPRINTF("Use default num_threads_per_wf=%d\n", num_threads_per_wf);
    }
    if (j.contains("wavefront-request-size-nbytes")) {
        this->wfreq_size_nbytes = j["wavefront-request-size-nbytes"];
        DPRINTF("Set wfreq_size_nbytes=%d\n", wfreq_size_nbytes);
    } else {
        THROW_ERR("Missing wavefront-request-size-nbytes "
                  "in system config %s\n",
                  filename);
    }
    if (j.contains("simd-latency-cycles")) {
        this->simd_latency = j["simd-latency-cycles"];
        DPRINTF("Set simd_latency=%d cycles\n", simd_latency);
    } else {
        THROW_ERR("Missing simd-latency-cycles in system config %s\n",
                  filename);
    }
    if (j.contains("simd-bandwidth-bytes-per-cycle")) {
        this->simd_bandwidth = j["simd-bandwidth-bytes-per-cycle"];
        DPRINTF("Set simd_bandwidth=%d bytes per cycles\n", simd_bandwidth);
    } else {
        THROW_ERR("Missing simd-bandwidth-bytes-per-cycle "
                  "in system config %s\n",
                  filename);
    }
    if (j.contains("msccl-loop-unrolling-factor")) {
        this->msccl_loop_unrolling_factor = j["msccl-loop-unrolling-factor"];
        DPRINTF("Set msccl_loop_unrolling_factor=%d\n",
                msccl_loop_unrolling_factor);
    } else {
        this->msccl_loop_unrolling_factor = 8;
        DPRINTF("Use default msccl_loop_unrolling_factor=%d\n",
                msccl_loop_unrolling_factor);
    }
    if (j.contains("cu-issue-cycles")) {
        this->cu_issue_cycles = j["cu-issue-cycles"];
        DPRINTF("Set cu_issue_cycles=%d cycles\n", cu_issue_cycles);
    } else {
        this->cu_issue_cycles = 1;
        DPRINTF("Use default cu_issue_cycles=%d\n", cu_issue_cycles);
    }
    if (j.contains("wait-retry-cycles")) {
        this->wait_retry_cycles = j["wait-retry-cycles"];
        DPRINTF("Set wait_retry_cycles=%d cycles\n", wait_retry_cycles);
    } else {
        this->wait_retry_cycles = 64;
        DPRINTF("Use default wait_retry_cycles=%d\n", wait_retry_cycles);
    }
    this->use_random_wf_select = false;
    if (j.contains("use-random-wf-select")) {
        if (j["use-random-wf-select"] != 0) {
            this->use_random_wf_select = true;
            DPRINTF("Use non-default random wavefront selection per WF group. "
                    "use-random-wf-select=%d\n",
                    use_random_wf_select);
        } else {
            this->use_random_wf_select = false;
            DPRINTF(
                "Use default wavefront selection (oldest ready first) per WF "
                "group. use-random-wf-select=%d\n",
                use_random_wf_select);
        }
    }
    this->use_mshr = true;
    if (j.contains("use-mshr")) {
        if (j["use-mshr"] != 0) {
            this->use_mshr = true;
            DPRINTF("Enable load MSHR coalescing use-mshr=%d\n", use_mshr);
        } else {
            this->use_mshr = false;
            DPRINTF("Disable load MSHR coalescing use-mshr=%d\n", use_mshr);
        }
    }
    this->use_mwait = false;
    if (j.contains("use-mwait")) {
        if (j["use-mwait"] != 0) {
            this->use_mwait = true;
            DPRINTF("Use mwait to wait on semaphores use-mwait=%d\n",
                    use_mwait);
        } else {
            this->use_mwait = false;
            DPRINTF("Use busy loop to wait on semaphores use-mwait=%d\n",
                    use_mwait);
        }
    }

    this->timing_simulation = true;
    if (j.contains("timing-simulation")) {
        this->timing_simulation = j["timing-simulation"] != 0;
    }
    DPRINTF("Simulation type = %s\n",
            this->timing_simulation ? "timing" : "functional");

    this->memory_mode = Memory::MemoryMode::Value;
    if (j.contains("memory-mode")) {
        if (j["memory-mode"] == "value") {
            this->memory_mode = Memory::MemoryMode::Value;
        } else if (j["memory-mode"] == "trace") {
            this->memory_mode = Memory::MemoryMode::Trace;
        } else {
            THROW_ERR("Unknown memory mode %s\n", j["memory_mode"]);
        }
    }

    this->lsq_depth = 512;
    if (j.contains("lsq-depth")) {
        this->lsq_depth = j["lsq-depth"];
        DPRINTF("Set lsq_depth=%d\n", lsq_depth);
    }
    this->lsq_req_size_nbytes = 256;
    if (j.contains("lsq-req-size-nbytes")) {
        this->lsq_req_size_nbytes = j["lsq-req-size-nbytes"];
        DPRINTF("Set lsq_req_size_nbytes=%d\n", lsq_req_size_nbytes);
    }

    if (j.contains("wavefront-trace-file")) {
        if (this->id == 0) {
            // Only sys 0 initialize wavefront tracer
            g_wavefront_tracer.init(j["wavefront-trace-file"]);
            g_wavefront_tracer.enable();
        }
    }
    if (j.contains("functional-trace-file")) {
        if (this->id == 0) {
            // Only sys 0 initialize op tracer
            g_functional_tracer.init(j["functional-trace-file"]);
            g_functional_tracer.enable();
        }
    }

    inFile.close();
}

DataSet* GpuSys::issueCollective(WrapperNode& node,
                                 const std::vector<bool>& involved_dim,
                                 CommunicatorGroup* comm_group) {
    if (node.getCommType() == ChakraCollectiveCommType::CUSTOM_MSCCL) {
        std::cout << "sys[" << id
                  << "] issuing CUSTOM_MSCCL node id=" << node.getNodeID()
                  << std::endl;
        dispatchReadyNode(node);
        return nullptr;
    }
    return Sys::issueCollective(node, involved_dim, comm_group);
}

bool GpuSys::claimEvent(EventType event, BasicEventHandlerData* ehd) {
    if (event == EventType::PacketCmdAcked) {
        auto sys_call_data =
            dynamic_cast<ComputeUnitPacketEventHandlerData*>(ehd);
        assert(sys_call_data);
        sys_call_data->cu_p->call(sys_call_data->cu_event_type,
                                  sys_call_data->cu_event_calldata);
        delete sys_call_data;
        return true;
    } else if (event == EventType::GPUModel_ReExecOps) {
        gpu_model->call(event, ehd);

        // The memory is deleted, so functionally setting `ehd->event`
        // shouldn't do anything, but this is added so that if this same event
        // object is invoked again (and the `ehd` object memory hasn't been
        // reallocated) yet, then an "Unknown event received" assertion will
        // raise. We cannot invoke this same `GPUModel_ReExecOps` callback
        // more than once. This would only ever occur if the same semaphore is
        // released multiple before it is acquired. (Note that if the double-
        // release bug is present in the simulator *but* the `ehd` memory is
        // reallocated, then there will be a segfault and the below assertion
        // will not raise. However, valgrind should identify the cause of the
        // problem to be the below `delete`.)
        ehd->event = EventType::UndefinedEvent;
        delete ehd;
        return true;
    }
    return false;
}

void GpuSys::dispatchReadyNode(WrapperNode& node) {
    gpu_model->insertWrapperNode(node);
}

}  // namespace AstraSim
