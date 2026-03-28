/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include <stdlib.h>
#include <unistd.h>

#include "astra-sim/common/Logging.hh"
#include "astra-sim/system/RecvPacketEventHandlerData.hh"
#include "astra-sim/system/SendPacketEventHandlerData.hh"
#include "astra-sim/system/astraccl/custom_collectives/CustomAlgorithm.hh"
#include "extern/graph_frontend/chakra/src/feeder_v3/et_feeder.h"

using namespace std;
using namespace AstraSim;
using namespace Chakra;

typedef ChakraProtoMsg::NodeType ChakraNodeType;

CustomAlgorithm::CustomAlgorithm(std::string et_filename, int id, uint64_t data_size, int pos_in_comm, CommunicatorGroup* comm_group, Sys* sys) : Algorithm() {
    try {
        et_filename = et_filename + "." + to_string(pos_in_comm) + ".et";
        this->et_feeder = new Chakra::FeederV3::ETFeeder(et_filename);
        this->comm_group = comm_group;
    } catch (const std::runtime_error& e) {
        // TODO(jinsun): Solve.
        auto logger = AstraSim::LoggerFactory::get_logger("system::astraccl::custom_collectives");
        logger->error("Error: Cannot access et file for collective algorithm: \n'" + et_filename + "'.\n"
                     "Please adjust the system JSON file, and keep in mind the path to the et file is relative to the working directory.\n"
                     "- If you don't know what the system JSON file is, very likely it's 'examples/system/custom_collectives/custom_collective.json'.\n"
                     "- For ns-3 backend, note the working directory is 'extern/network_backend/ns-3/build/scratch'.\n"
                     "We will find a way to avoid this issue in the future.");
        std::exit(1);
    }
    this->id = id;
    this->data_size = data_size;
    this->sys = sys;
}

// Get attributes specific to the custom collective from ETFeederNode
int get_msg_chunk_cnt(shared_ptr<Chakra::FeederV3::ETFeederNode> node) {
    const Chakra::FeederV3::ChakraAttr* attr;
    if (node->get_attr_msg("msg_chunk_cnt", &attr)) {
        return attr->int32_val();
    } else {
        throw std::runtime_error("CustomAlgorithm: Node " + to_string(node->id()) +
                                 " does not have 'msg_chunk_cnt' attribute.");
    }
}

int get_total_chunk_cnt(shared_ptr<Chakra::FeederV3::ETFeederNode> node) {
    const Chakra::FeederV3::ChakraAttr* attr;
    if (node->get_attr_msg("total_chunk_cnt", &attr)) {
        return attr->int32_val();
    } else {
        throw std::runtime_error("CustomAlgorithm: Node " + to_string(node->id()) +
                                 " does not have 'total_chunk_cnt' attribute.");
    }
}

int get_msg_chunk_idx(shared_ptr<Chakra::FeederV3::ETFeederNode> node) {
    const Chakra::FeederV3::ChakraAttr* attr;
    if (node->get_attr_msg("msg_chunk_idx", &attr)) {
        return attr->int32_val();
    } else {
        throw std::runtime_error("CustomAlgorithm: Node " + to_string(node->id()) +
                                 " does not have 'msg_chunk_idx' attribute.");
    }
}

uint64_t CustomAlgorithm::get_msg_size(shared_ptr<Chakra::FeederV3::ETFeederNode> node) {
    auto logger = AstraSim::LoggerFactory::get_logger("system::astraccl::custom_collectives");
    uint64_t msg_size = 0;
    uint64_t coll_comm_size = this->data_size;
    int msg_chunk_cnt = get_msg_chunk_cnt(node);
    int msg_chunk_idx = get_msg_chunk_idx(node);
    int total_chunk_cnt = get_total_chunk_cnt(node);
    int num_involved_npus = this->sys->all_sys.size();
    if (this->comm_group != nullptr) {
        int num_involved_npus = this->comm_group->involved_NPUs.size();
    }

    // Custom collective generated with old converter. 
    // Fallback to using the hardcoded message size.
    if (msg_chunk_cnt == -1 || msg_chunk_idx == -1 || total_chunk_cnt == -1) {
        logger->warn("Node {} of custom algorithm with filename {} has invalid inputs:"
                     "  - msg_chunk_cnt ({})\n  - msg_chunk_idx ({})\n  - total_chunk_cnt ({})"
                     "This et was generated with old et_converter that does not support chunking."
                     "Falling back to original 'comm_size' attribute for message size."
                     "The 'comm_size' in custom collectives will be deprecated soon. Please refer to PR #343 and update the custom collective generator",
                     node->id(), this->et_filename, msg_chunk_cnt, msg_chunk_idx, total_chunk_cnt);

        uint64_t comm_size = node->get_attr<uint64_t>("comm_size", 0);
        if (comm_size == 0) {
            logger->error("Node {} of custom algorithm with filename {} does not even have valid 'comm_size' attribute. Throwing",
                           node->id(), this->et_filename);
            throw std::runtime_error("Custom collective without valid attributes");
        }
        return comm_size;
    }

    // Very unlikely this is a valid case. Don't implement for now.
    if (total_chunk_cnt % num_involved_npus != 0) {
        logger->error("Node {} of custom algorithm with filename {} has total number of chunks ({}) not divisible by involved NPUs ({}).",
                        node->id(), this->et_filename, total_chunk_cnt, num_involved_npus);
        throw std::runtime_error("UNIMPLEMENTED");
    }
    
    // The collective is cleanly divisible into the total number of chunks.
    uint64_t chunk_size = 0;
    if (coll_comm_size % total_chunk_cnt == 0) {
        chunk_size = coll_comm_size / total_chunk_cnt;
        // In AllGather, coll_comm_size points to the input buffer, not the total output buffer.
        // However, total_chunk_cnt points to the total number of chunks in the output buffer.
        if (this->comType == ComType::All_Gather) {
            if ((coll_comm_size * num_involved_npus) % total_chunk_cnt != 0) {
                logger->error("Node {} of custom algorithm with filename {} has non-divisible coll_comm_size ({} bytes) and num_involved_npus ({}). "
                             "Calculating message size with remainder handling.",
                             node->id(), this->et_filename, coll_comm_size, num_involved_npus);
                // Very unlikely. Don't implement for now.
                throw std::runtime_error("UNIMPLEMENTED");
            }
            // The input buffer to AG may break down into more than 1 chunk.
            // We validated before that total_chunk_cnt % num_involved_npus == 0.
            int chunks_per_input_buffer = total_chunk_cnt / num_involved_npus;
            chunk_size = coll_comm_size / chunks_per_input_buffer;
        }
        return chunk_size * msg_chunk_cnt;
    }

    // Unlikely, but still: the collective is *not* cleanly divisible into the total number of chunks.
    logger->warn("Node {} of custom algorithm with filename {} has non-divisible coll_comm_size ({} bytes) and total_chunk_cnt ({}). "
                 "Calculating message size with remainder handling.",
                 node->id(), this->et_filename, coll_comm_size, total_chunk_cnt);
    // coll_comm_size = basic_chunk_size * total_chunk_cnt + remainder.
    uint64_t basic_chunk_size = coll_comm_size / total_chunk_cnt;
    uint64_t remainder = coll_comm_size % total_chunk_cnt;
    // Is the last chunk included in this message?
    if (msg_chunk_idx + msg_chunk_cnt > total_chunk_cnt) {
        return basic_chunk_size * msg_chunk_cnt + remainder;
    }
    return basic_chunk_size * msg_chunk_cnt;
}

int CustomAlgorithm::convert_algo_rank_to_real_rank(int algo_rank) {
    // In this custom algorithm implementation, we assume the algo ranks are
    // same as real ranks. This may change in the future.
    if (comm_group == nullptr) {
        return algo_rank;
    }
    int real_rank = comm_group->involved_NPUs[algo_rank];
    return real_rank;
}

void CustomAlgorithm::issue(shared_ptr<Chakra::FeederV3::ETFeederNode> node) {
    this->et_feeder->getDependancyResolver().take_node(node->id());
    ChakraNodeType type = node->type();
    if (type == ChakraNodeType::COMM_SEND_NODE) {
        uint64_t msg_size = get_msg_size(node);
        sim_request snd_req;
        int dst_rank = convert_algo_rank_to_real_rank(node->comm_dst());
        snd_req.srcRank = node->comm_src(this->stream->owner->id);
        snd_req.dstRank = dst_rank;
        snd_req.reqType = UINT8;
        SendPacketEventHandlerData* sehd = new SendPacketEventHandlerData;
        sehd->callable = this;
        sehd->wlhd = new WorkloadLayerHandlerData;
        sehd->wlhd->node_id = node->id();
        sehd->event = EventType::PacketSent;
        stream->owner->front_end_sim_send(
            0, Sys::dummy_data,
            msg_size, UINT8, dst_rank, node->comm_tag(),
            &snd_req, Sys::FrontEndSendRecvType::NATIVE, &Sys::handleEvent,
            sehd);
    } else if (type == ChakraNodeType::COMM_RECV_NODE) {
        uint64_t msg_size = get_msg_size(node);
        sim_request rcv_req;
        int src_rank = convert_algo_rank_to_real_rank(node->comm_src());
        RecvPacketEventHandlerData* rcehd = new RecvPacketEventHandlerData;
        rcehd->wlhd = new WorkloadLayerHandlerData;
        rcehd->wlhd->node_id = node->id();
        rcehd->custom_algorithm = this;
        rcehd->event = EventType::PacketReceived;
        stream->owner->front_end_sim_recv(
            0, Sys::dummy_data, msg_size, UINT8, src_rank,
            node->comm_tag(), &rcv_req, Sys::FrontEndSendRecvType::NATIVE,
            &Sys::handleEvent, rcehd);
    } else if (type == ChakraNodeType::COMP_NODE) {
        // This Compute corresponds to a reduce operation. The computation time
        // here is assumed to be trivial.
        WorkloadLayerHandlerData* wlhd = new WorkloadLayerHandlerData;
        wlhd->node_id = node->id();
        uint64_t runtime = 1ul;
        // TODO: Take the bandwidth from system/input.json and calculate the runtime
        // following PacketBundle::call
        // TODO: Find a way to propagate the message size resolved in COMM_RECV_NODE to here, 
        // So that we can use that message size to calculate the compute time. 
        // Ignore for now.
        if (node->runtime() != 0ul) {
            // chakra runtimes are in microseconds and we should convert it into
            // nanoseconds.
            // Following Workload::issue_replay
            runtime = node->runtime() * 1000;
        }
        stream->owner->register_event(this, EventType::General, wlhd, runtime);
    }
}

void CustomAlgorithm::issue_dep_free_nodes() {
    auto& dependancy_resolver = et_feeder->getDependancyResolver();
    const auto free_nodes = dependancy_resolver.get_dependancy_free_nodes();
    for (const auto& node_id : free_nodes) {
        shared_ptr<Chakra::FeederV3::ETFeederNode> node =
            et_feeder->lookupNode(node_id);
        if (node != nullptr) {
            issue(node);
        }
    }
}

// This is called when a SEND/RECV/COMP operator has completed.
// Release the Chakra node for the completed operator and issue downstream
// nodes.
void CustomAlgorithm::call(EventType event, CallData* data) {
    if (data == nullptr) {
        throw runtime_error(
            "CustomAlgorithm::call does not have node id encoded "
            "(CallData* data is null).");
    }

    WorkloadLayerHandlerData* wlhd = (WorkloadLayerHandlerData*)data;
    shared_ptr<Chakra::FeederV3::ETFeederNode> node =
        et_feeder->lookupNode(wlhd->node_id);
    auto& dep_resolver = et_feeder->getDependancyResolver();
    dep_resolver.finish_node(wlhd->node_id);
    issue_dep_free_nodes();
    delete wlhd;

    if (dep_resolver.get_ongoing_nodes().empty() &&
        dep_resolver.get_dependancy_free_nodes().empty()) {
        // There are no more nodes to execute, and no node is executing, so we
        // finish the collective algorithm.
        exit();
    }
}

void CustomAlgorithm::run(EventType event, CallData* data) {
    // Start executing the collective algorithm implementation by issuing the
    // root nodes.
    issue_dep_free_nodes();
}
