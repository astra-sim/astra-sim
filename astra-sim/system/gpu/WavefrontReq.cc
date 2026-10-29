// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <sstream>

#include <astra-sim/common/logging/Logger.hh>
#include <astra-sim/system/common/ReqId.hh>
#include <astra-sim/system/gpu/GpuOp.hh>
#include <astra-sim/system/gpu/TimingGPUModel.hh>
#include <astra-sim/system/gpu/Wavefront.hh>
#include <astra-sim/system/gpu/WavefrontReq.hh>
#include <astra-sim/system/gpu/Workgroup.hh>

using namespace AstraSim;

WFReq::WFReq(Wavefront* _wf_p,
             GpuInst _inst,
             uint64_t _vaddr,
             uint32_t _reqSizeNBytes)
    : wfreq_id(next_request_id()),
      inst(_inst),
      vaddr(_vaddr),
      wf_p(_wf_p),
      status(WFReq::Status::Invalid),
      retry_cnt(0),
      issue_tick(0),
      waitcnt(0),
      waitcnt_wfreq_p(nullptr),
      reqSizeNBytes(_reqSizeNBytes) {}

sim_request WFReq::genSimRequest(uint64_t addr,
                                 uint64_t req_size_nbytes) const {
    bool is_subreq = (addr != 0);
    uint64_t a = is_subreq ? addr : vaddr;
    uint64_t sz = is_subreq ? req_size_nbytes : reqSizeNBytes;

    sim_request sim_req;
    sim_req.astraReqId = is_subreq ? next_request_id() : wfreq_id;
    sim_req.reqCuId = wf_p->getWorkgroupPtr()->getCuId();
    sim_req.reqSizeNBytes = sz;
    sim_req.node = TimingGPUModel::getGpuIdFromAddr(a);
    sim_req.vaddr = TimingGPUModel::getCacheLineAddr(a);
    sim_req.buf = TimingGPUModel::getBufTypeFromAddr(a);
    sim_req.bufByteOff = TimingGPUModel::getBufByteOffFromAddr(a);
    return sim_req;
}

AstraNetworkAPI::CommandType WFReq::getAstraNetCmdType(
    uint32_t loc_gpu_id) const {
    uint32_t target_gpu_id = TimingGPUModel::getGpuIdFromAddr(vaddr);
    switch (inst) {
    case GpuInst::Load:
        if (loc_gpu_id == target_gpu_id) {
            return AstraNetworkAPI::CommandType::Local_Load;
        } else {
            return AstraNetworkAPI::CommandType::Remote_Load;
        }
    case GpuInst::Store:
        if (loc_gpu_id == target_gpu_id) {
            return AstraNetworkAPI::CommandType::Local_Store;
        } else {
            return AstraNetworkAPI::CommandType::Remote_Store;
        }
    case GpuInst::SemAcquire:
        return AstraNetworkAPI::CommandType::Local_Load;
    case GpuInst::SemRelease:
        if (loc_gpu_id == target_gpu_id) {
            return AstraNetworkAPI::CommandType::Local_Store;
        } else {
            return AstraNetworkAPI::CommandType::Remote_Store;
        }
    default:
        THROW_ERR_NR("[WFReq::getAstraNetCmdType] Cannot get Astra net command "
                     "for WFReq %s\n",
                     static_cast<int>(inst), to_string().c_str());
    }
}

std::string WFReq::to_string() const {
    std::stringstream ss;
    ss << "WFReq[id=" << wfreq_id << ",inst=" << getGpuInstName(inst)
       << ",vaddr=0x" << std::hex << vaddr << ",reqNBytes=" << std::dec
       << reqSizeNBytes << ",wf=" << wf_p->getWfId() << ",status=";
    switch (status) {
    case Status::Dispatched:
        ss << "Dispatched";
        break;
    case Status::Issued:
        ss << "Issued";
        break;
    case Status::Blocked:
        ss << "Blocked";
        break;
    case Status::Completed:
        ss << "Completed";
        break;
    default:
        ss << "Invalid";
        break;
    }
    ss << ",retry_cnt=" << retry_cnt << "]";
    return ss.str();
}
