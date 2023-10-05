#undef PGO_TRAINING
#define PATH_TO_PGO_CONFIG "path_to_pgo_config"

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
#include <fstream>
#include <iostream>
#include <ns3/rdma-client-helper.h>
#include <ns3/rdma-client.h>
#include <ns3/rdma-driver.h>
#include <ns3/rdma.h>
#include <ns3/sim-setting.h>
#include <ns3/switch-node.h>
#include <time.h>
#include <unordered_map>

using namespace ns3;
using namespace std;

/*
 * This file defines the interaction between the System layer and the NS3
 * simulator (Network layer). System layer consists of num_npus different Sys
 * objects, while there is only one ns3 simulator queue. Whenever send_flow. Will
 * create an RDMAClient application, simplifying the ns3 application to send.
 * Whenever this application terminates, it will call qp_finish. qp_finish will
 * then trigger handler functions, That will reach out to system layer. This
 * will forward the collectives, remove them from system layer queue, etc. For
 * more detail, refer to the comments of each function. Sending is using maps.
 * When system layer schedules send, it will place send/receive at maps.
 * Whenever ns3 determines complete, it will trigger the handler in these maps.
 */

/*
 * Task is used as a unit, to determine the map. Acts as a placeholder.
 */
struct task1 {
  int src;
  int dest;
  int type;
  int remaining_msg_bytes;
  void *fun_arg;
  void (*msg_handler)(void *fun_arg);
  double schTime; // in sec
};

// TaskKey is used as the key to all.
typedef std::pair<int, std::pair<int, int>> TaskKey;

map<pair<int, pair<int, int>>, int> sender_src_port_map;
map<TaskKey, struct task1> sentHash;

/*
 * Receive is more complex. Due to timing, ns3 may simulate receive earlier than when system layer calls sim_recv. 
 * Therefore, we main two hashmaps. 
 */

map<TaskKey, struct task1> expeRecvHash;
map<TaskKey, int> recvHash;
// Used to count how many bytes were sent/received by this node. Refer to
// sim_finish().
map<pair<int, int>, int> nodeHash;


// send_flow commands the ns3 simulator to schedule a RDMA message to be sent between two pair of nodes.
// send_flow is triggered by sim_send.
void send_flow(int src, int dst, int maxPacketCount,
              void (*msg_handler)(void *fun_arg), void *fun_arg, int tag) {
  // get a new port number
  uint32_t port = portNumber[src][dst]++;
  sender_src_port_map[make_pair(port, make_pair(src, dst))] = tag;
  int pg = 3, dport = 100;
  flow_input.idx++;

  // Create a queue pair from ns3. NS3 will send data. 
  RdmaClientHelper clientHelper(
      pg, serverAddress[src], serverAddress[dst], port, dport, maxPacketCount,
      has_win ? (global_t == 1 ? maxBdp : pairBdp[n.Get(src)][n.Get(dst)]) : 0,
      global_t == 1 ? maxRtt : pairRtt[src][dst], msg_handler, fun_arg, tag,
      src, dst);
  ApplicationContainer appCon = clientHelper.Install(n.Get(src));
  appCon.Start(Time(0));
}

void notify_receiver_receive_data(int sender_node, int receiver_node,
                                  int message_size, int tag) {

  TaskKey tagKey = make_pair(tag, make_pair(sender_node, receiver_node));

  // The Sys object is waiting for packets to arrive.
  if (expeRecvHash.find(tagKey) != expeRecvHash.end()) {
    task1 t2 = expeRecvHash[tagKey];
    if (message_size == t2.remaining_msg_bytes) {
      // We received exactly what Sys object was expecting.
      expeRecvHash.erase(tagKey);
      t2.msg_handler(t2.fun_arg);
    } else if (message_size > t2.remaining_msg_bytes) {
      // We received more packets than the Sys object is expecting. 
      // Place task in recvHash and wait for Sys object to issue more sim_recv calls.
      // Call handler for the amount Sys object was waiting for.
      recvHash[tagKey] = message_size - t2.remaining_msg_bytes;
      expeRecvHash.erase(tagKey);
      t2.msg_handler(t2.fun_arg);
    } else {
      // There are still packets to arrive. 
      // Reduce the number of packets we are waiting for. Do not call handler. 
      t2.remaining_msg_bytes -= message_size;
      expeRecvHash[tagKey] = t2;
    }
  // The Sys object is not yet waiting for packets to arrive
  } else {
    if (recvHash.find(tagKey) == recvHash.end()) {
      // Place task in recvHash and wait for Sys object to issue more sim_recv calls.
      // Call handler for the amount Sys object was waiting for.
      recvHash[tagKey] = message_size;
    } else {
      // Sys object is still waiting. Add number of bytes we are waiting for. 
      recvHash[tagKey] += message_size;
    }
  }


  if (nodeHash.find(make_pair(receiver_node, 1)) == nodeHash.end()) {
    nodeHash[make_pair(receiver_node, 1)] = message_size;
  } else {
    nodeHash[make_pair(receiver_node, 1)] += message_size;
  }
}

void notify_sender_sending_finished(int sender_node, int receiver_node,
                                    int message_size, int tag) {

  TaskKey tagKey = make_pair(tag, make_pair(sender_node, receiver_node));
  if (sentHash.find(tagKey) == sentHash.end()) {
    cout << "Something is wrong"
         << "\n";
  }

  task1 t2 = sentHash[tagKey];
  if (t2.remaining_msg_bytes != message_size) {
    cout << "Something is wrong"
         << "\n";
  }

  sentHash.erase(tagKey);
  if (nodeHash.find(make_pair(sender_node, 0)) == nodeHash.end()) {
    nodeHash[make_pair(sender_node, 0)] = message_size;
  } else {
    nodeHash[make_pair(sender_node, 0)] += message_size;
  }
  t2.msg_handler(t2.fun_arg);
}

void qp_finish(FILE *fout, Ptr<RdmaQueuePair> q) {
  uint32_t sid = ip_to_node_id(q->sip), did = ip_to_node_id(q->dip);
  uint64_t base_rtt = pairRtt[sid][did], b = pairBw[sid][did];
  uint32_t total_bytes =
      q->m_size +
      ((q->m_size - 1) / packet_payload_size + 1) *
          (CustomHeader::GetStaticWholeHeaderSize() -
           IntHeader::GetStaticSize()); // translate to the minimum bytes
                                        // required (with header but no INT)
  uint64_t standalone_fct = base_rtt + total_bytes * 8000000000lu / b;
  // sip, dip, sport, dport, size (B), start_time, fct (ns), standalone_fct (ns)
  fprintf(fout, "%08x %08x %u %u %lu %lu %lu %lu\n", q->sip.Get(), q->dip.Get(),
          q->sport, q->dport, q->m_size, q->startTime.GetTimeStep(),
          (Simulator::Now() - q->startTime).GetTimeStep(), standalone_fct);
  fflush(fout);

  if (false) {
    cout << "qp_finish " << sid << " " << did << " " << q->sport << " "
         << q->dport << " " << q->m_size << endl;
  }
  // remove rxQp from the receiver
  Ptr<Node> dstNode = n.Get(did);
  Ptr<RdmaDriver> rdma = dstNode->GetObject<RdmaDriver>();
  rdma->m_rdma->DeleteRxQp(q->sip.Get(), q->m_pg, q->sport);

  if (sender_src_port_map.find(make_pair(q->sport, make_pair(sid, did))) ==
      sender_src_port_map.end()) {
    cout << "could not find the tag, there must be something wrong" << endl;
    exit(-1);
  }
  int tag = sender_src_port_map[make_pair(q->sport, make_pair(sid, did))];
  sender_src_port_map.erase(make_pair(q->sport, make_pair(sid, did)));

  // let sender knows that the flow finishes;
  notify_sender_sending_finished(sid, did, q->m_size, tag);

  // let receiver knows that it receives packets;
  notify_receiver_receive_data(sid, did, q->m_size, tag);
}

int setup_ns3_simulation(int argc, char *argv[]) {
  if (!ReadConf(argc, argv))
    return -1;
  SetConfig();
  SetupNetwork(qp_finish);
}
