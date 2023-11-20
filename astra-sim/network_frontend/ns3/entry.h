#undef PGO_TRAINING
#define PATH_TO_PGO_CONFIG "path_to_pgo_config"

#include <ns3/rdma-client-helper.h>
#include <ns3/rdma-client.h>
#include <ns3/rdma-driver.h>
#include <ns3/rdma.h>
#include <ns3/sim-setting.h>
#include <ns3/switch-node.h>
#include <time.h>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include "astra-sim/common/Logging.hh"
#include "common.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/error-model.h"
#include "ns3/global-route-manager.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/packet.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/qbb-helper.h"

using namespace ns3;
using namespace std;

/*
 * This file defines the interaction between the System layer and the NS3
 * simulator (Network layer). The system layer issues send/receive events, and
 * waits until the ns3 simulates the conclusion of these events to issue the
 * next collective communication. When ns3 simulates the conclusion of an event,
 * it will call qp_finish to lookup the maps in this file and call the callback
 * handlers. Refer to below comments for further detail.
 */

// MsgEvent represents a single send or receive event, issued by the system
// layer. The system layer will wait for the ns3 backend to simulate the event
// finishing (i.e. node 0 finishes sending message, or node 1 finishes receiving
// the message) The callback handler 'msg_handler' signals the System layer that
// the event has finished in ns3.
class MsgEvent {
 public:
  int src_id;
  int dst_id;
  int type;
  // Indicates the number of bytes remaining to be sent or received.
  // Initialized with the original size of the message, and
  // incremented/decremented depending on how many bytes were sent/received.
  // Eventually, this value will reach 0 when the event has completed.
  int remaining_msg_bytes;
  void* fun_arg;
  void (*msg_handler)(void* fun_arg);

  MsgEvent(
      int _src_id,
      int _dst_id,
      int _type,
      int _remaining_msg_bytes,
      void* _fun_arg,
      void (*_msg_handler)(void* fun_arg))
      : src_id(_src_id),
        dst_id(_dst_id),
        type(_type),
        remaining_msg_bytes(_remaining_msg_bytes),
        fun_arg(_fun_arg),
        msg_handler(_msg_handler) {}

  // Default constructor to prevent compile errors. When looking up MsgEvents
  // from maps such as sim_send_waiting_hash, we should always check that a
  // MsgEvent exists for the given key. (i.e. this default constructor should
  // not be called in runtime.)
  MsgEvent()
      : src_id(0),
        dst_id(0),
        type(0),
        remaining_msg_bytes(0),
        fun_arg(nullptr),
        msg_handler(nullptr) {}

  // CallHandler will call the callback handler associated with this MsgEvent.
  void callHandler() {
    msg_handler(fun_arg);
    return;
  }
};

// MsgEventKey is a key to uniquely identify each MsgEvent.
//  - Pair <Tag, Pair <src_id, dst_id>>
typedef pair<int, pair<int, int>> MsgEventKey;

// The ns3 RdmaClient structure cannot hold the 'tag' information, which is a
// Astra-sim specific implementation. We use a mapping with the source port
// number (another unique value) to hold tag information.
//   - key: Pair <port_id, Pair <src_id, dst_id>>
//   - value: tag
// TODO: It seems we *can* obtain the tag through q->GetTag() at qp_finish.
// Verify & Simplify.
map<pair<int, pair<int, int>>, int> sender_src_port_map;

// NodeHash is used to count how many bytes were sent/received by this node.
// Refer to sim_finish().
//   - key: Pair <node_id, send/receive>. Where 'send/receive' indicates if the
//   value is for send or receive
//   - value: Number of bytes this node has sent (if send/receive is 0) and
//   received (if send/receive is 1)
map<pair<int, int>, int> node_to_bytes_sent_map;

// SentHash stores a MsgEvent for sim_send events and its callback handler.
//   - key: A pair of <MsgEventKey, port_id>.
//          A single collective phase can be split into multiple sim_send
//          messages, which all have the same MsgEventKey.
//          TODO: Adding port_id as key is a hacky solution. The real solution
//          would be to split this map, similar to sim_recv_waiting_hash and
//          received_msg_standby_hash.
//   - value: A MsgEvent instance that indicates that Sys layer is waiting for a
//   send event to finish
map<pair<MsgEventKey, int>, MsgEvent> sim_send_waiting_hash;

// While ns3 cannot send packets before System layer calls sim_send, it
// is possible for ns3 to simulate Incoming messages before System layer calls
// sim_recv to 'reap' the messages. Therefore, we maintain two maps:
//   - sim_recv_waiting_hash holds messages where sim_recv has been called but
//   ns3 has not yet simulated the message arriving,
//   - received_msg_standby_hash holds messages which ns3 has simulated the
//   arrival, but sim_recv has not yet been called.

//   - key: A MsgEventKey isntance.
//   - value: A MsgEvent instance that indicates that Sys layer is waiting for a
//   receive event to finish
map<MsgEventKey, MsgEvent> sim_recv_waiting_hash;

//   - key: A MsgEventKey isntance.
//   - value: The number of bytes that ns3 has simulated completed, but the
//   System layer has not yet called sim_recv
map<MsgEventKey, int> received_msg_standby_hash;

// send_flow commands the ns3 simulator to schedule a RDMA message to be sent
// between two pair of nodes. send_flow is triggered by sim_send.
void send_flow(
    int src_id,
    int dst,
    int maxPacketCount,
    void (*msg_handler)(void* fun_arg),
    void* fun_arg,
    int tag) {
  // Get a new port number.
  uint32_t port = portNumber[src_id][dst]++;
  sender_src_port_map[make_pair(port, make_pair(src_id, dst))] = tag;
  int pg = 3, dport = 100;
  flow_input.idx++;

  // Create a MsgEvent instance and register callback function.
  MsgEvent send_event =
      MsgEvent(src_id, dst, 0, maxPacketCount, fun_arg, msg_handler);
  pair<MsgEventKey, int> send_event_key = make_pair(
      make_pair(tag, make_pair(send_event.src_id, send_event.dst_id)), port);
  sim_send_waiting_hash[send_event_key] = send_event;

  // Create a queue pair and schedule within the ns3 simulator.
  RdmaClientHelper clientHelper(
      pg,
      serverAddress[src_id],
      serverAddress[dst],
      port,
      dport,
      maxPacketCount,
      has_win ? (global_t == 1 ? maxBdp : pairBdp[n.Get(src_id)][n.Get(dst)])
              : 0,
      global_t == 1 ? maxRtt : pairRtt[src_id][dst],
      msg_handler,
      fun_arg,
      tag,
      src_id,
      dst);
  ApplicationContainer appCon = clientHelper.Install(n.Get(src_id));
  appCon.Start(Time(0));
}

// notify_receiver_receive_data looks at whether the System layer has issued
// sim_recv for this message. If the system layer is waiting for this message,
// call the callback handler for the MsgEvent. If the system layer is not *yet*
// waiting for this message, register that this message has arrived,
// so that the system layer can later call the callback handler when sim_recv
// is called.
void notify_receiver_receive_data(
    int src_id,
    int dst_id,
    int message_size,
    int tag) {
  MsgEventKey recv_expect_event_key = make_pair(tag, make_pair(src_id, dst_id));

  if (sim_recv_waiting_hash.find(recv_expect_event_key) !=
      sim_recv_waiting_hash.end()) {
    // The Sys object is waiting for packets to arrive.
    MsgEvent recv_expect_event = sim_recv_waiting_hash[recv_expect_event_key];
    if (message_size == recv_expect_event.remaining_msg_bytes) {
      // We received exactly the amount of data what Sys object was expecting.
      sim_recv_waiting_hash.erase(recv_expect_event_key);
      recv_expect_event.callHandler();
    } else if (message_size > recv_expect_event.remaining_msg_bytes) {
      // We received more packets than the Sys object is expecting.
      // Place task in received_msg_standby_hash and wait for Sys object to
      // issue more sim_recv calls. Call callback handler for the amount Sys
      // object was waiting for.
      received_msg_standby_hash[recv_expect_event_key] =
          message_size - recv_expect_event.remaining_msg_bytes;
      sim_recv_waiting_hash.erase(recv_expect_event_key);
      recv_expect_event.callHandler();
    } else {
      // There are still packets to arrive.
      // Reduce the number of packets we are waiting for. Do not call callback
      // handler.
      recv_expect_event.remaining_msg_bytes -= message_size;
      sim_recv_waiting_hash[recv_expect_event_key] = recv_expect_event;
    }
  } else {
    // The Sys object is not yet waiting for packets to arrive.
    if (received_msg_standby_hash.find(recv_expect_event_key) ==
        received_msg_standby_hash.end()) {
      // Place task in received_msg_standby_hash and wait for Sys object to
      // issue more sim_recv calls.
      received_msg_standby_hash[recv_expect_event_key] = message_size;
    } else {
      // Sys object is still waiting. Add number of bytes we are waiting for.
      received_msg_standby_hash[recv_expect_event_key] += message_size;
    }
  }

  // Add to the number of total bytes received.
  if (node_to_bytes_sent_map.find(make_pair(dst_id, 1)) ==
      node_to_bytes_sent_map.end()) {
    node_to_bytes_sent_map[make_pair(dst_id, 1)] = message_size;
  } else {
    node_to_bytes_sent_map[make_pair(dst_id, 1)] += message_size;
  }
}

void notify_sender_sending_finished(
    int src_id,
    int dst_id,
    int message_size,
    int tag,
    int src_port) {
  // Lookup the send_event registered at send_flow().
  pair<MsgEventKey, int> send_event_key =
      make_pair(make_pair(tag, make_pair(src_id, dst_id)), src_port);
  auto logger = AstraSim::Logger::getLogger("network_frontend::ns3::entry");
  if (sim_send_waiting_hash.find(send_event_key) ==
      sim_send_waiting_hash.end()) {
    logger->critical(
        "Cannot find send_event in sent_hash. Something is wrong.");
    logger->critical("tag={}, src_id={}, dst_id={}", tag, src_id, dst_id);

    exit(1);
  }

  // Verify that the (ns3 identified) sent message size matches what was
  // expected by the system layer.
  MsgEvent send_event = sim_send_waiting_hash[send_event_key];
  if (send_event.remaining_msg_bytes != message_size) {
    logger->critical(
        "The message size does not match what is expected. Something is wrong.");
    logger->critical(
        "tag={}, src_id={}, dst_id={}, expected_msg_bytes={}, actual_msg_bytes={}",
        tag,
        src_id,
        dst_id,
        send_event.remaining_msg_bytes,
        message_size);
    exit(1);
  }
  sim_send_waiting_hash.erase(send_event_key);

  // Add to the number of total bytes sent.
  if (node_to_bytes_sent_map.find(make_pair(src_id, 0)) ==
      node_to_bytes_sent_map.end()) {
    node_to_bytes_sent_map[make_pair(src_id, 0)] = message_size;
  } else {
    node_to_bytes_sent_map[make_pair(src_id, 0)] += message_size;
  }
  send_event.callHandler();
}

void qp_finish_print_log(FILE* fout, Ptr<RdmaQueuePair> q) {
  uint32_t sid = ip_to_node_id(q->sip), did = ip_to_node_id(q->dip);
  uint64_t base_rtt = pairRtt[sid][did], b = pairBw[sid][did];
  uint32_t total_bytes = q->m_size +
      ((q->m_size - 1) / packet_payload_size + 1) *
          (CustomHeader::GetStaticWholeHeaderSize() -
           IntHeader::GetStaticSize()); // translate to the minimum bytes
                                        // required (with header but no INT)
  uint64_t standalone_fct = base_rtt + total_bytes * 8000000000lu / b;
  // sip, dip, sport, dport, size (B), start_time, fct (ns), standalone_fct (ns)
  fprintf(
      fout,
      "%08x %08x %u %u %lu %lu %lu %lu\n",
      q->sip.Get(),
      q->dip.Get(),
      q->sport,
      q->dport,
      q->m_size,
      q->startTime.GetTimeStep(),
      (Simulator::Now() - q->startTime).GetTimeStep(),
      standalone_fct);
  fflush(fout);
}

// qp_finish is triggered by NS3 to indicate that an RDMA queue pair has
// finished. qp_finish is registered as the callback handlerto the RdmaClient
// instance created at send_flow. This registration is done at
// common.h::SetupNetwork().
void qp_finish(FILE* fout, Ptr<RdmaQueuePair> q) {
  uint32_t sid = ip_to_node_id(q->sip), did = ip_to_node_id(q->dip);
  qp_finish_print_log(fout, q);

  // remove rxQp from the receiver.
  Ptr<Node> dstNode = n.Get(did);
  Ptr<RdmaDriver> rdma = dstNode->GetObject<RdmaDriver>();
  rdma->m_rdma->DeleteRxQp(q->sip.Get(), q->m_pg, q->sport);

  // Identify the tag of this message.
  if (sender_src_port_map.find(make_pair(q->sport, make_pair(sid, did))) ==
      sender_src_port_map.end()) {
    Logger::getLogger("network_frontend::ns3::entry")
        ->critical("could not find the tag, there must be something wrong");
    exit(-1);
  }
  int tag = sender_src_port_map[make_pair(q->sport, make_pair(sid, did))];
  sender_src_port_map.erase(make_pair(q->sport, make_pair(sid, did)));

  // Let sender knows that the flow has finished.
  notify_sender_sending_finished(sid, did, q->m_size, tag, q->sport);

  // Let receiver knows that it has received packets.
  notify_receiver_receive_data(sid, did, q->m_size, tag);
}

int setup_ns3_simulation(string network_configuration) {
  if (!ReadConf(network_configuration))
    return -1;
  SetConfig();
  SetupNetwork(qp_finish);
}
