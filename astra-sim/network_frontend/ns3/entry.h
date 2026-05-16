#undef PGO_TRAINING
#define PATH_TO_PGO_CONFIG "path_to_pgo_config"

#include "common.h"
#include <ns3/rdma-client-helper.h>
#include <ns3/rdma-client.h>
#include <ns3/rdma-driver.h>

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
  int remaining_msg_bytes;
  void *fun_arg;
  void (*msg_handler)(void *fun_arg);

  MsgEvent(int _src_id, int _dst_id, int _type, int _remaining_msg_bytes,
           void *_fun_arg, void (*_msg_handler)(void *fun_arg))
      : src_id(_src_id), dst_id(_dst_id), type(_type),
        remaining_msg_bytes(_remaining_msg_bytes), fun_arg(_fun_arg),
        msg_handler(_msg_handler) {}

  MsgEvent()
      : src_id(0), dst_id(0), type(0), remaining_msg_bytes(0), fun_arg(nullptr),
        msg_handler(nullptr) {}

  void callHandler() {
    msg_handler(fun_arg);
    return;
  }
};

typedef pair<int, pair<int, int>> MsgEventKey;

map<pair<int, pair<int, int>>, int> sender_src_port_map;
map<pair<int, int>, int> node_to_bytes_sent_map;
map<pair<MsgEventKey, int>, MsgEvent> sim_send_waiting_hash;
map<MsgEventKey, MsgEvent> sim_recv_waiting_hash;
map<MsgEventKey, int> received_msg_standby_hash;

void send_flow(int src_id, int dst, int maxPacketCount,
               void (*msg_handler)(void *fun_arg), void *fun_arg, int tag) {
  uint32_t port = g_ctx.portNumber[src_id][dst]++;
  sender_src_port_map[make_pair(port, make_pair(src_id, dst))] = tag;
  int pg = 3, dport = 100;
  g_ctx.flow_input.idx++;

  MsgEvent send_event =
      MsgEvent(src_id, dst, 0, maxPacketCount, fun_arg, msg_handler);
  pair<MsgEventKey, int> send_event_key =
      make_pair(make_pair(tag, make_pair(send_event.src_id, send_event.dst_id)),port) ;
  sim_send_waiting_hash[send_event_key] = send_event;

  RdmaClientHelper clientHelper(
      pg, g_ctx.serverAddress[src_id], g_ctx.serverAddress[dst], port, dport,
      maxPacketCount,
      g_cfg.has_win ? (g_cfg.global_t == 1 ? g_ctx.topology.maxBdp
                                            : g_ctx.topology.pairBdp[g_ctx.nodes.Get(src_id)][g_ctx.nodes.Get(dst)])
                    : 0,
      g_cfg.global_t == 1 ? g_ctx.topology.maxRtt
                          : g_ctx.topology.pairRtt[src_id][dst],
      msg_handler, fun_arg, tag, src_id, dst);
  ApplicationContainer appCon = clientHelper.Install(g_ctx.nodes.Get(src_id));
  appCon.Start(Time(0));
}

void notify_receiver_receive_data(int src_id, int dst_id, int message_size,
                                  int tag) {

  MsgEventKey recv_expect_event_key = make_pair(tag, make_pair(src_id, dst_id));

  if (sim_recv_waiting_hash.find(recv_expect_event_key) != sim_recv_waiting_hash.end()) {
    MsgEvent recv_expect_event = sim_recv_waiting_hash[recv_expect_event_key];
    if (message_size == recv_expect_event.remaining_msg_bytes) {
      sim_recv_waiting_hash.erase(recv_expect_event_key);
      recv_expect_event.callHandler();
    } else if (message_size > recv_expect_event.remaining_msg_bytes) {
      received_msg_standby_hash[recv_expect_event_key] =
          message_size - recv_expect_event.remaining_msg_bytes;
      sim_recv_waiting_hash.erase(recv_expect_event_key);
      recv_expect_event.callHandler();
    } else {
      recv_expect_event.remaining_msg_bytes -= message_size;
      sim_recv_waiting_hash[recv_expect_event_key] = recv_expect_event;
    }
  } else {
    if (received_msg_standby_hash.find(recv_expect_event_key) == received_msg_standby_hash.end()) {
      received_msg_standby_hash[recv_expect_event_key] = message_size;
    } else {
      received_msg_standby_hash[recv_expect_event_key] += message_size;
    }
  }

  if (node_to_bytes_sent_map.find(make_pair(dst_id, 1)) == node_to_bytes_sent_map.end()) {
    node_to_bytes_sent_map[make_pair(dst_id, 1)] = message_size;
  } else {
    node_to_bytes_sent_map[make_pair(dst_id, 1)] += message_size;
  }
}

void notify_sender_sending_finished(int src_id, int dst_id, int message_size,
                                    int tag, int src_port) {
  pair<MsgEventKey, int> send_event_key = make_pair(make_pair(tag, make_pair(src_id, dst_id)), src_port);
  if (sim_send_waiting_hash.find(send_event_key) == sim_send_waiting_hash.end()) {
    cerr << "Cannot find send_event in sent_hash. Something is wrong."
         << "tag, src_id, dst_id: " << tag << " " << src_id << " " << dst_id
         << "\n";
    exit(1);
  }

  MsgEvent send_event = sim_send_waiting_hash[send_event_key];
  if (send_event.remaining_msg_bytes != message_size) {
    cerr << "The message size does not match what is expected. Something is "
            "wrong."
         << "tag, src_id, dst_id, expected msg_bytes, actual msg_bytes: " << tag
         << " " << src_id << " " << dst_id << " "
         << send_event.remaining_msg_bytes << " " << message_size << "\n";
    exit(1);
  }
  sim_send_waiting_hash.erase(send_event_key);

  if (node_to_bytes_sent_map.find(make_pair(src_id, 0)) == node_to_bytes_sent_map.end()) {
    node_to_bytes_sent_map[make_pair(src_id, 0)] = message_size;
  } else {
    node_to_bytes_sent_map[make_pair(src_id, 0)] += message_size;
  }
  send_event.callHandler();
}

void qp_finish_print_log(FILE *fout, Ptr<RdmaQueuePair> q) {
  uint32_t sid = ip_to_node_id(q->sip), did = ip_to_node_id(q->dip);
  uint64_t base_rtt = g_ctx.topology.pairRtt[sid][did],
           b = g_ctx.topology.pairBw[sid][did];
  uint32_t total_bytes =
      q->m_size +
      ((q->m_size - 1) / g_cfg.packet_payload_size + 1) *
          (CustomHeader::GetStaticWholeHeaderSize() -
           IntHeader::GetStaticSize());
  uint64_t standalone_fct = base_rtt + total_bytes * 8000000000lu / b;
  fprintf(fout, "%08x %08x %u %u %lu %lu %lu %lu\n", q->sip.Get(), q->dip.Get(),
          q->sport, q->dport, q->m_size, q->startTime.GetTimeStep(),
          (Simulator::Now() - q->startTime).GetTimeStep(), standalone_fct);
  fflush(fout);
}

void qp_finish(FILE *fout, Ptr<RdmaQueuePair> q) {
  uint32_t sid = ip_to_node_id(q->sip), did = ip_to_node_id(q->dip);
  qp_finish_print_log(fout, q);

  Ptr<Node> dstNode = g_ctx.nodes.Get(did);
  Ptr<RdmaDriver> rdma = dstNode->GetObject<RdmaDriver>();
  rdma->m_rdma->DeleteRxQp(q->sip.Get(), q->m_pg, q->sport);

  if (sender_src_port_map.find(make_pair(q->sport, make_pair(sid, did))) ==
      sender_src_port_map.end()) {
    cout << "could not find the tag, there must be something wrong" << endl;
    exit(-1);
  }
  int tag = sender_src_port_map[make_pair(q->sport, make_pair(sid, did))];
  sender_src_port_map.erase(make_pair(q->sport, make_pair(sid, did)));

  notify_sender_sending_finished(sid, did, q->m_size, tag, q->sport);
  notify_receiver_receive_data(sid, did, q->m_size, tag);
}

int setup_ns3_simulation(string network_configuration) {
  if (!ReadConf(network_configuration, g_cfg))
    return -1;

  SetConfig(g_cfg);

  if (!SetupNetwork(g_cfg, g_ctx, qp_finish)) {
    return -1;
  }

  return 0;
}
