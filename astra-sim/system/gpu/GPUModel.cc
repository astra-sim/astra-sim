// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <cassert>
#include <fstream>

#include <astra-sim/system/common/WorkloadLayerHandlerData.hh>
#include <astra-sim/system/gpu/ComputeUnit.hh>
#include <astra-sim/system/gpu/GPUModel.hh>
#include <astra-sim/system/gpu/GpuSys.hh>
#include <astra-sim/system/gpu/Wavefront.hh>
#include <astra-sim/system/gpu/msccl/MscclOps.hh>
#include <astra-sim/system/gpu/msccl/Parser.hh>
#include <astra-sim/workload/Workload.hh>

using namespace AstraSim;

namespace {
// Calculate the per-chunk byte size for a custom-collective node from its
// per-GPU input buffer size and the plan's gpus[0].input_chunks.
// A plan with no input buffer has no meaningful chunk size and yields 0.
uint64_t calculateChunkSize(const std::string& plan_file,
                            uint64_t input_buffer_size) {
    auto input_file = std::ifstream(plan_file);
    if (!input_file.is_open()) {
        THROW_ERR_NR("[GPUModel] Custom collective JSON file (at: %s) "
                     "loading unsuccessful.\n",
                     plan_file.c_str());
    }
    auto j = nlohmann::json();
    input_file >> j;
    const auto input_chunks = j["gpus"][0]["input_chunks"].get<uint32_t>();
    if (input_chunks == 0) {
        // Plans with no data buffers (e.g. barrier, semaphore, signal/wait)
        // move no data, so there is no chunk size to calculate.
        return 0;
    }
    // Round up: a chunk must be large enough to hold this GPU's share even
    // when input_buffer_size does not divide evenly by input_chunks.
    const auto raw_nbytes =
        (input_buffer_size + (input_chunks - 1)) / input_chunks;
    return Msccl::alignUpToCacheLine(raw_nbytes);
}
}  // namespace

std::vector<GPUModel*> GPUModel::s_gpu_model_lst;  // Global GPU model pointers

GPUModel::GPUModel(GpuSys* sys,
                   Workload* wl,
                   int gpu_id,
                   enum Memory::MemoryMode memory_mode,
                   timespec_t acquire_delay)
    : m_sys_p(sys),
      m_workload_p(wl),
      m_id(gpu_id),
      m_name(sys->name() + ".gpu" + std::to_string(m_id)),
      m_memory_mode(memory_mode),
      m_memory_p(nullptr),
      // Assuming ~10ns for completing a semaphore acquire after a release
      // TODO: make this as a parameter in the future
      m_acquire_delay(acquire_delay),
      m_sem_timeout_ticks(1000000000000),  // 1s timeout for semaphore acquires
      m_wg_issue_event_scheduled(false),
      m_sem_timeout_event_scheduled(false),
      // NumProtocols is used as a sentinel to denote an uninitialized GPU. This
      // protocol must be overwritten by the time any GPU interacts with this
      // one.
      m_protocol(Msccl::ProtocolType::NumProtocols) {}

GPUModel::~GPUModel() {
    if (m_memory_p != nullptr) {
        delete m_memory_p;
        m_memory_p = nullptr;
    }
}

uint64_t GPUModel::getMemAddr(uint64_t gpu,
                              uint64_t buf,
                              uint64_t buf_byte_off) {
    // Sanity check
    if (buf != static_cast<uint64_t>(Msccl::BufferType::Input) &&
        buf != static_cast<uint64_t>(Msccl::BufferType::Output) &&
        buf != static_cast<uint64_t>(Msccl::BufferType::Scratch)) {
        THROW_ERR_NR("Invalid buffer type: %d\n", buf);
    }
    uint64_t ret_addr = 0ULL;
    ret_addr = setBits(ret_addr, GPU_ID_POS, GPU_ID_LEN, gpu);
    ret_addr = setBits(ret_addr, BUF_TYPE_POS, BUF_TYPE_LEN, buf);
    ret_addr =
        setBits(ret_addr, BUF_BYTE_OFF_POS, BUF_BYTE_OFF_LEN, buf_byte_off);
    return ret_addr;
}

uint64_t GPUModel::getSemAddr(uint64_t gpu,
                              uint64_t connected_gpu,
                              uint64_t sem_id,
                              Msccl::BufferType buf_type) {
    uint64_t buf = static_cast<uint64_t>(buf_type);
    uint64_t ret_addr = 0ULL;
    ret_addr = setBits(ret_addr, GPU_ID_POS, GPU_ID_LEN, gpu);
    ret_addr = setBits(ret_addr, BUF_TYPE_POS, BUF_TYPE_LEN, buf);
    ret_addr = setBits(ret_addr, SEM_CONNECTED_GPU_ID_POS,
                       SEM_CONNECTED_GPU_ID_LEN, connected_gpu);
    ret_addr = setBits(ret_addr, SEM_ID_POS, SEM_ID_LEN, sem_id);
    // The lower 8 bits are zeros
    return ret_addr;
}

bool GPUModel::isSemAddr(uint64_t vaddr) const {
    bool isBufTypeSemaphore = false;
    if (m_protocol == Msccl::ProtocolType::Simple) {
        isBufTypeSemaphore =
            (getBufTypeFromAddr(vaddr) ==
             static_cast<uint64_t>(Msccl::BufferType::Semaphore));
    } else if (m_protocol == Msccl::ProtocolType::LL ||
               m_protocol == Msccl::ProtocolType::LL64 ||
               m_protocol == Msccl::ProtocolType::LL128 ||
               m_protocol == Msccl::ProtocolType::LL256) {
        isBufTypeSemaphore =
            (getBufTypeFromAddr(vaddr) ==
             static_cast<uint64_t>(Msccl::BufferType::Scratch));
    } else {
        THROW_ERR("Unknown protocol type %d\n", static_cast<int>(m_protocol));
    }
    return isBufTypeSemaphore;
}

uint64_t GPUModel::getGpuIdFromAddr(uint64_t vaddr) {
    return getBits(vaddr, GPU_ID_POS, GPU_ID_LEN);
}

uint64_t GPUModel::getBufTypeFromAddr(uint64_t vaddr) {
    return getBits(vaddr, BUF_TYPE_POS, BUF_TYPE_LEN);
}

uint64_t GPUModel::getBufByteOffFromAddr(uint64_t vaddr) {
    return getBits(vaddr, BUF_BYTE_OFF_POS, BUF_BYTE_OFF_LEN);
}

uint64_t GPUModel::getCacheLineAddr(uint64_t vaddr) {
    return vaddr & (~GPUModel::CLINE_ADDR_MASK);
}

void GPUModel::insertWrapperNode(WrapperNode& node) {
    m_inp_node_lst.push(node);
    schedIssueReadyNodes();

    // Update stats
    auto comm_type = node.getCommType();
    if (m_stat_num_node_map.count(comm_type) == 0) {
        m_stat_num_node_map[comm_type].init(
            this, "num_nodes_" + node.getNodeName(),
            "Number of nodes of type " + node.getNodeName());
    }
    m_stat_num_node_map[comm_type]++;
    m_stat_num_nodes++;
}

bool GPUModel::hasReadyNodes() const {
    return m_inp_node_lst.size() > 0;
}

void GPUModel::issueNextReadyNodes() {
    // Doing a kernel launch for the next ready node
    assert(hasReadyNodes());
    assert(m_wg_issue_event_scheduled);
    m_wg_issue_event_scheduled = false;

    // TODO: for now, we assume only one kernel can be issued at a time
    // TODO: in the future, we can support multiple outstanding kernels. For
    // that support, we need somehow to separate memory address spaces of
    // different kernels.
    if (m_outstanding_kernels.size() > 0) {
        DPRINTF("Cannot issue new kernel since there is an outstanding "
                "kernel still running: %d\n",
                m_outstanding_kernels.size());
        return;
    }

    // Get the next node from
    auto& node = m_inp_node_lst.front();
    if ((node.getNodeType() != JSONNode::NodeType::COMM_COLL_NODE) &&
        (node.getCommType() != JSONNode::CollectiveCommType::CUSTOM_MSCCL)) {
        THROW_ERR("Only custom communication nodes are supported in GPUModel "
                  "currently. node_id=%d, node_type=%d, comm_type=%d\n",
                  node.getNodeID(), static_cast<int>(node.getNodeType()),
                  static_cast<int>(node.getCommType()));
    }

    // Make sure we're not issuing the same node twice
    int node_id = node.getNodeID();
    assert(m_outstanding_kernels.count(node_id) == 0);

    // Every node carries its own comm_size (the per-GPU input buffer size),
    // from which the plan's chunk size is calculated.
    const uint64_t chunk_nbytes =
        calculateChunkSize(node.getCustomCommPlanFile(), node.getCommSize());
    Msccl::GpuParser msccl_parser =
        Msccl::GpuParser(node.getCustomCommPlanFile(),
                         m_id,  // GPU ID
                         m_sys_p->msccl_loop_unrolling_factor, chunk_nbytes,
                         m_sys_p->wfreq_size_nbytes);
    uint32_t num_wgs = msccl_parser.getNumThreadblocks();
    m_protocol = msccl_parser.protocol;
    assert(num_wgs > 0);

    // Instantiate the GPU memory if it's not instantiated already.
    m_memory_p = new Memory(m_memory_mode, m_id, msccl_parser.inplace,
                            msccl_parser.num_buf_chunks);
    m_memory_p->setExpected(msccl_parser.expected_memory);

    if (!canLaunch(node.getNodeID(), num_wgs)) {
        return;
    }

    // Remove this node from the ready queue
    m_inp_node_lst.pop();

    // Create all WGs for this node/kernel
    m_outstanding_kernels.emplace(std::piecewise_construct,
                                  std::forward_as_tuple(node_id),
                                  std::forward_as_tuple(node_id, num_wgs));
    auto& kernel = m_outstanding_kernels.at(node_id);

    for (uint32_t wg_id = 0; wg_id < num_wgs; ++wg_id) {
        launch(msccl_parser, kernel, wg_id);
    }

    // Schedule more event to issue next ready nodes (if any)
    if (hasReadyNodes()) {
        schedIssueReadyNodes();
    }
}

void GPUModel::completeWorkgroup(Workgroup* wg_p) {
    assert(wg_p);
    assert(wg_p->isCompleted());
    auto kernel_id = wg_p->getKernelId();
    assert(m_outstanding_kernels.count(kernel_id) == 1);

    // Remove this WG from the outstanding list
    DPRINTF("Retiring workgroup %s\n", wg_p->to_string());
    auto& kernel = m_outstanding_kernels.at(kernel_id);
    assert(kernel.num_outstanding_wgs > 0);
    kernel.num_outstanding_wgs--;
    kernel.wg_lst.erase(wg_p->getWorkgroupId());
    delete wg_p;

    // If there is no more outstanding WG for this kernel, we can remove
    // the kernel entry
    if (kernel.num_outstanding_wgs == 0) {
        m_outstanding_kernels.erase(kernel_id);
        DPRINTF("All workgroups of node_id=%d have been completed\n",
                kernel_id);

        // Schedule an event to get next ready node from the Workload
        auto wlhd = new WorkloadLayerHandlerData(
            m_id, m_workload_p, kernel_id, EventType::Workload_NodeFinished);
        m_sys_p->registerEventInCycles(m_workload_p, wlhd->event, wlhd, 1);
    } else {
        DPRINTF("Number of remaining outstanding WGs for node_id=%d: %d\n",
                kernel_id, kernel.num_outstanding_wgs);
    }

    // Schedule more event to issue next ready nodes (if any)
    if (hasReadyNodes()) {
        schedIssueReadyNodes();
    }
}

void GPUModel::registerAcquireSemaphore(uint64_t sem_addr,
                                        const CallbackFunc& callback) {
    // Sanity check
    if (!isSemAddr(sem_addr)) {
        THROW_ERR("Trying to acquire a non-semaphore address: 0x%x\n",
                  sem_addr);
    }

    auto sem_gpu_id = GPUModel::getGpuIdFromAddr(sem_addr);
    if (sem_gpu_id != static_cast<uint64_t>(m_id)) {
        THROW_ERR("Trying to acquire a semaphore (addr=0x%x) that does not "
                  "belong to this GPU (GPU_%d)\n",
                  sem_addr, m_id);
    }

    // If there is no entry for this semaphore, create a new one with default
    // values
    if (m_outstanding_semaphores.count(sem_addr) == 0) {
        m_outstanding_semaphores[sem_addr] = Semaphore();
    }

    auto& sem = m_outstanding_semaphores.at(sem_addr);

    // We update the expect_value of this semaphore to indicate that this
    // acquire has happened and is waiting.
    sem.expect_value++;

    // Insert the acquire callback into the pending acquire queue
    sem.pending_acquires.emplace_back(
        CallbackFuncWithTime{callback.func, callback.args,
                             curTick(),  // register_tick
                             false});    // was_callback_called

    DPRINTF("Registered an acquire of a semaphore on addr=0x%x, "
            "actual_value=%d, expect_value=%d\n",
            sem_addr, sem.actual_value, sem.expect_value);

    // Schedule a timeout event to check if this acquire is making progress
    schedSemaphoreTimeoutEvent();
}

SemaphorePeekStatus GPUModel::peekSemaphore(uint64_t sem_addr) {
    // Sanity check
    if (!isSemAddr(sem_addr)) {
        THROW_ERR("Trying to peek a non-semaphore address: 0x%x\n", sem_addr);
    }

    auto sem_gpu_id = GPUModel::getGpuIdFromAddr(sem_addr);
    if (sem_gpu_id != static_cast<uint64_t>(m_id)) {
        THROW_ERR("Trying to peek a semaphore (addr=0x%x) that does not "
                  "belong to this GPU (GPU_%d)\n",
                  sem_addr, m_id);
    }

    // Assert that there is already an entry for this semaphore since we should
    // have registered this acquire before calling this acquire function.
    if (m_outstanding_semaphores.count(sem_addr) == 0) {
        // Has not been registered before.
        return SemaphorePeekStatus::NoRegister;
    }

    Semaphore& sem = m_outstanding_semaphores.at(sem_addr);
    if (sem.expect_value == 0 || sem.pending_acquires.size() == 0) {
        // A release caused this semaphore to be created, but it was not yet
        // registered.
        return SemaphorePeekStatus::NoRegister;
    }

    return (sem.actual_value >= sem.expect_value)
               ? SemaphorePeekStatus::RegisterYesAcquire
               : SemaphorePeekStatus::RegisterNoAcquire;
}

bool GPUModel::acquireSemaphore(uint64_t sem_addr) {
    SemaphorePeekStatus status = peekSemaphore(sem_addr);
    if (status == SemaphorePeekStatus::NoRegister) {
        THROW_ERR("Trying to acquire a semaphore (addr=0x%x) that has not "
                  "been registered before\n",
                  sem_addr);
    }

    auto& sem = m_outstanding_semaphores.at(sem_addr);
    assert(sem.expect_value > 0);
    assert(!sem.pending_acquires.empty());

    if (status == SemaphorePeekStatus::RegisterNoAcquire) {
        // This acquire cannot be completed now, so we just return false and
        // wait for the corresponding release to complete this acquire later.
        DPRINTF("Semaphore acquire failed on addr=0x%x, actual_value=%d, "
                "expect_value=%d\n",
                sem_addr, sem.actual_value, sem.expect_value);
        return false;
    }

    // This acquire can now be completed, so we pop one more callback from the
    // pending list.
    assert(sem.pending_acquires.size() > 0);
    CallbackFuncWithTime cb = *sem.pending_acquires.begin();
    sem.pending_acquires.pop_front();

    DPRINTF("Acquired semaphore on addr=0x%x, actual_value=%d, "
            "expect_value=%d\n",
            sem_addr, sem.actual_value, sem.expect_value);
    if (sem.pending_acquires.empty() &&
        (sem.actual_value == sem.expect_value)) {
        // For memory space optimization, once there is no pending acquire
        // and release for this semaphore, we can remove this semaphore
        // entry since we don't need to track this semaphore anymore until
        // the next acquire happens.
        m_outstanding_semaphores.erase(sem_addr);
    }

    return true;
}

bool GPUModel::releaseSemaphore(uint64_t sem_addr) {
    if (!isSemAddr(sem_addr)) {
        // Simply return false since this is not a semaphore address, so we
        // don't care about it in release
        return false;
    }

    // Sanity check
    auto sem_gpu_id = GPUModel::getGpuIdFromAddr(sem_addr);
    if (sem_gpu_id != static_cast<uint64_t>(m_id)) {
        THROW_ERR("Trying to release a semaphore (addr=0x%x) that does not "
                  "belong to this GPU (GPU_%d)\n",
                  sem_addr, m_id);
    }

    // If there is no entry for this semaphore, create a new one with default
    // values
    if (m_outstanding_semaphores.count(sem_addr) == 0) {
        m_outstanding_semaphores[sem_addr] = Semaphore();
    }

    auto& sem = m_outstanding_semaphores.at(sem_addr);

    // Increment the actual value of this semaphore to indicate that a release
    // has happened.
    sem.actual_value++;
    DPRINTF("Released a semaphore on addr=0x%x, actual_value=%d, "
            "expect_value=%d\n",
            sem_addr, sem.actual_value, sem.expect_value);
    if (sem.actual_value >= sem.expect_value) {
        // We should wake up all waiting acquire(s)
        for (CallbackFuncWithTime& cb : sem.pending_acquires) {
            if (cb.func != nullptr) {
                if (cb.was_callback_called) {
                    // This is fine, the functional verifier runs into this.
                    DPRINTF("Semaphore addr=0x%x was released twice before an "
                            "acquire could occur\n",
                            sem_addr);
                    continue;
                }
                cb.was_callback_called = true;  // We're about to invoke it.

                if (m_acquire_delay.time_val == 0) {
                    cb.func(cb.args);
                } else {
                    m_sys_p->comm_NI->sim_schedule(m_acquire_delay, cb.func,
                                                   cb.args);
                }
                DPRINTF("Waking up a semaphore acquire (by a release): "
                        "addr=0x%x after %d ps\n",
                        sem_addr, m_acquire_delay.time_val);
            }
        }
    }

    return true;
}

const std::string& GPUModel::name() const {
    return m_name;
}

Tick GPUModel::curTick() const {
    return m_sys_p->boostedTick();
}

void GPUModel::call(EventType type, CallData* data) {
    if (type == EventType::GPUModel_IssueNodes) {
        issueNextReadyNodes();
    } else if (type == EventType::GPUModel_SemaphoreTimeout) {
        auto curr_tick = curTick();
        // Go through all outstanding semaphores to check for timeout
        for (auto& p : m_outstanding_semaphores) {
            auto& sem_addr = p.first;
            auto& sem = p.second;
            // Check all pending acquires for timed-out ones
            for (CallbackFuncWithTime& cb : sem.pending_acquires) {
                Tick acquire_tick = cb.register_tick;
                if (curr_tick - acquire_tick >= m_sem_timeout_ticks) {
                    // Timeout happened for this acquire
                    THROW_ERR("Semaphore acquire timeout: addr=0x%x, "
                              "acquire_tick=%llu, curr_tick=%llu, "
                              "sem_expected_value=%d, sem_actual_value=%d\n",
                              sem_addr,
                              static_cast<unsigned long long>(acquire_tick),
                              static_cast<unsigned long long>(curr_tick),
                              sem.expect_value, sem.actual_value);
                }
            }
        }
        m_sem_timeout_event_scheduled = false;
        if (m_outstanding_semaphores.size() > 0) {
            // Reschedule the next semaphore timeout checking event as long as
            // we have at least one outstanding semaphore
            schedSemaphoreTimeoutEvent();
        }
    } else if (type == EventType::GPUModel_ReExecOps) {
        // One of the semaphores were acquired.
        WorkgroupExecHandlerData* handler =
            dynamic_cast<WorkgroupExecHandlerData*>(data);
        handler->wg_p->execute();
    } else {
        THROW_ERR("Invalid GPUModel event type: %d\n", static_cast<int>(type));
    }
}

bool GPUModel::verify() const {
    // To be "correct", we check two things:
    //   1. All of our semaphores are matched (actual == expected). This isn't
    //      technically required for a plan to be correct, but it probably
    //      indicates inefficiencies or something being wrong, so we require it
    //      as a smoke test.
    //   2. The memory contents are correct for the given collective type.

    // First, check the semaphores.
    bool correct_semaphores = true;
    for (auto& it : m_outstanding_semaphores) {
        if (it.second.expect_value != it.second.actual_value) {
            std::cerr << "GPU[" << m_id << "]'s semaphore to GPU["
                      << GPUModel::getGpuIdFromAddr(it.first) << "] has "
                      << it.second.expect_value << " waits and "
                      << it.second.actual_value << " signals" << std::endl;
            correct_semaphores = false;
        }
    }
    if (!correct_semaphores) {
        std::cerr
            << "Error: One or more semaphores do not have their actual "
            << "value match their expected value. Ensure that the number of "
            << "signals and waits in your program are equivalent." << std::endl;
    }

    // Next, check the memory.
    bool correct_memory = (m_memory_p != nullptr) ? m_memory_p->verify() : true;

    // Combine both.
    return correct_semaphores && correct_memory;
}

GpuSys* GPUModel::getSysPtr() const {
    return m_sys_p;
}

Workload* GPUModel::getWorkloadPtr() const {
    return m_workload_p;
}

Memory* GPUModel::getMemoryPtr() const {
    return m_memory_p;
}

Msccl::ProtocolType GPUModel::getProtocolType() const {
    return m_protocol;
}

int GPUModel::getId() const {
    return m_id;
}

void GPUModel::schedIssueReadyNodes() {
    if (!m_wg_issue_event_scheduled) {
        // Schedule an event in the next cycle to handle issuing all new nodes
        m_sys_p->registerEventInCycles(this, EventType::GPUModel_IssueNodes,
                                       nullptr, 1);
        m_wg_issue_event_scheduled = true;
    }
}

void GPUModel::schedSemaphoreTimeoutEvent() {
    if (!m_sem_timeout_event_scheduled) {
        m_sem_timeout_event_scheduled = true;
        m_sys_p->register_event(this, EventType::GPUModel_SemaphoreTimeout,
                                nullptr, m_sem_timeout_ticks);
    }
}

/**
 * GPUModel::Kernel
 */

GPUModel::Kernel::Kernel(int _node_id, uint32_t num_wgs)
    : node_id(_node_id),
      num_outstanding_wgs(num_wgs) {
    assert(num_wgs > 0);
    wg_lst.reserve(num_wgs);
}

/**
 * GPUModel::WorkgroupBarrier
 */

GPUModel::WorkgroupBarrier::WorkgroupBarrier(uint32_t _num_wgs)
    : num_wgs(_num_wgs) {
    assert(num_wgs > 0);
}

inline uint64_t GPUModel::getBits(uint64_t val, uint32_t pos, uint32_t len) {
    assert(len > 0);
    assert(pos + len <= 64);
    uint64_t mask = ((1ULL << len) - 1) << pos;
    return (val & mask) >> pos;
}

inline uint64_t GPUModel::setBits(uint64_t val,
                                  uint32_t pos,
                                  uint32_t len,
                                  uint64_t set_val) {
    assert(len > 0);
    assert(pos + len <= 64);
    uint64_t mask = ((1ULL << len) - 1) << pos;
    val &= ~mask;                    // clear bits at the target position
    val |= (set_val << pos) & mask;  // set bits at the target position
    return val;
}
