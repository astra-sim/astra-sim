/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include <stdlib.h>
#include <unistd.h>

#include "astra-sim/system/RecvPacketEventHandlerData.hh"
#include "astra-sim/system/SendPacketEventHandlerData.hh"
#include "astra-sim/system/collective/ChakraImpl.hh"
#include "extern/graph_frontend/chakra/et_feeder/et_feeder.h"

using namespace std;
using namespace AstraSim;
using namespace Chakra;

typedef ChakraProtoMsg::NodeType ChakraNodeType;

ChakraImpl::ChakraImpl(std::string et_filename, int id) : Algorithm() {
    this->et_feeder = new Chakra::ETFeeder(et_filename);
    this->id = id;
}

void ChakraImpl::issue(shared_ptr<Chakra::ETFeederNode> node) {
    ChakraNodeType type = node->type();
    if (type == ChakraNodeType::COMM_SEND_NODE) {
        sim_request snd_req;
        snd_req.srcRank = node->comm_src();
        snd_req.dstRank = node->comm_dst();
        snd_req.reqType = UINT8;
        SendPacketEventHandlerData* sehd = new SendPacketEventHandlerData;
        sehd->callable = this;
        sehd->wlhd = new WorkloadLayerHandlerData;
        sehd->wlhd->node_id = node->id();
        sehd->event = EventType::PacketSent;
        stream->owner->front_end_sim_send(
            0, Sys::dummy_data,
            // Note that we're using the comm size as hardcoded in the Impl Chakra et, ed through the comm. api,
            // and ignore the comm.size fed in the workload chakra et. TODO: fix.
            node->comm_size(), UINT8, node->comm_dst(), node->comm_tag(), &snd_req, &Sys::handleEvent, sehd);
    } else if (type == ChakraNodeType::COMM_RECV_NODE) {
        sim_request rcv_req;
        RecvPacketEventHandlerData* rcehd = new RecvPacketEventHandlerData;
        rcehd->wlhd = new WorkloadLayerHandlerData;
        rcehd->wlhd->node_id = node->id();
        rcehd->chakra = this;
        rcehd->event = EventType::PacketReceived;
        stream->owner->front_end_sim_recv(0, Sys::dummy_data, node->comm_size(), UINT8, node->comm_src(),
                                          node->comm_tag(), &rcv_req, &Sys::handleEvent, rcehd);
    } else if (type == ChakraNodeType::COMP_NODE) {
        // This Compute corresponds to a reduce operation. The computation time here is assumed to be trivial.
        WorkloadLayerHandlerData* wlhd = new WorkloadLayerHandlerData;
        wlhd->node_id = node->id();
        uint64_t runtime = 1ul;
        if (node->runtime() != 0ul) {
            // chakra runtimes are in microseconds and we should convert it into nanoseconds.
            runtime = node->runtime() * 1000;
        }
        stream->owner->register_event(this, EventType::General, wlhd, runtime);
    }
}

void ChakraImpl::issue_dep_free_nodes() {
    shared_ptr<Chakra::ETFeederNode> node = et_feeder->getNextIssuableNode();
    while (node != nullptr) {
        issue(node);
        node = et_feeder->getNextIssuableNode();
    }
}

// This is called when a SEND/RECV/COMP operator has completed.
// Release the Chakra node for the completed operator and issue downstream nodes.
void ChakraImpl::call(EventType event, CallData* data) {
    if (data == nullptr) {
        throw runtime_error("ChakraImpl::Call does not have node id encoded (CallData* data is null).");
    }

    WorkloadLayerHandlerData* wlhd = (WorkloadLayerHandlerData*)data;
    shared_ptr<Chakra::ETFeederNode> node = et_feeder->lookupNode(wlhd->node_id);
    et_feeder->freeChildrenNodes(wlhd->node_id);
    issue_dep_free_nodes();
    et_feeder->removeNode(wlhd->node_id);
    delete wlhd;

    if (!et_feeder->hasNodesToIssue()) {
        // There are no more nodes to execute, so we finish the collective algorithm.
        exit();
    }
}

void ChakraImpl::run(EventType event, CallData* data) {
    // Start executing the collective algorithm implementation by issuing the root nodes.
    issue_dep_free_nodes();
}
