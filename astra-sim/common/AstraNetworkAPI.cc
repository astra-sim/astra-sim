// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <astra-sim/common/AstraNetworkAPI.hh>
#include <astra-sim/system/gpu/TimingGPUModel.hh>

#include <iostream>

using namespace AstraSim;

void AstraNetworkAPI::register_callback(uint64_t req_id,
                                        const CallbackFunc& callback) {
    /*
     *   This function is called when a send calls
     *       Create an entry in m_outstanding_callbacks
     */

    assert(m_outstanding_callbacks.count(req_id) == 0);
    m_outstanding_callbacks[req_id] = callback;
}

AstraNetworkAPI::CallbackFunc AstraNetworkAPI::retrieve_callback(
    uint64_t req_id) {
    /*
     *   This function is called when a send completes (order doesn't matter)
     *       Erase the (send) callback entry of m_outstanding_callbacks
     *       Return the SEND callback
     */

    assert(m_outstanding_callbacks.count(req_id) == 1);

    // clear and erase
    auto func = m_outstanding_callbacks[req_id].first;
    auto args = m_outstanding_callbacks[req_id].second;
    m_outstanding_callbacks.erase(req_id);

    return std::make_pair(func, args);
}

bool AstraNetworkAPI::release_semaphore(uint64_t vaddr) {
    auto target_gpu_id = AstraSim::TimingGPUModel::getGpuIdFromAddr(vaddr);
    auto target_gpu_model_p =
        AstraSim::TimingGPUModel::getGpuModelPtr(target_gpu_id);
    return target_gpu_model_p->AstraSim::TimingGPUModel::releaseSemaphore(
        vaddr);
}

std::string AstraNetworkAPI::getCmdTypeName(
    AstraNetworkAPI::CommandType cmd_type) {
    switch (cmd_type) {
    case AstraNetworkAPI::CommandType::Send:
        return "send";
    case AstraNetworkAPI::CommandType::Recv:
        return "recv";
    case AstraNetworkAPI::CommandType::Local_Load:
        return "local_ld";
    case AstraNetworkAPI::CommandType::Local_Store:
        return "local_st";
    case AstraNetworkAPI::CommandType::Remote_Load:
        return "remote_ld";
    case AstraNetworkAPI::CommandType::Remote_Store:
        return "remote_st";
    case AstraNetworkAPI::CommandType::GLdRed:
        return "glred";
    case AstraNetworkAPI::CommandType::GSt:
        return "gst";
    case AstraNetworkAPI::CommandType::FLdRedSt:
        return "flres";
    default:
        std::cerr << "Invalid command type: " << static_cast<int>(cmd_type)
                  << "\n";
        exit(1);
        return "";
    };
}
