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


struct task1 {
  int src;
  int dest;
  int type;
  int count;
  void *fun_arg;
  void (*msg_handler)(void *fun_arg);
  double schTime; // in sec
};

std::map<std::pair<int, std::pair<int, int>>, int> sender_src_port_map;
map<std::pair<int, std::pair<int, int>>, struct task1> expeRecvHash;
map<std::pair<int, std::pair<int, int>>, int> recvHash;
map<std::pair<int, std::pair<int, int>>, struct task1> sentHash;
map<std::pair<int, int>, int> nodeHash;

void SendFlow(int src, int dst, int maxPacketCount,
              void (*msg_handler)(void *fun_arg), void *fun_arg, int tag) {
  uint32_t port = portNumber[src][dst]++; // get a new port number
  sender_src_port_map[make_pair(port, make_pair(src, dst))] = tag;
  int pg = 3, dport = 100;
  flow_input.idx++;
  //std::cout << "flow input is " << flow_input.idx << " src " << src << " dst "
  //          << dst << "\n";
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

  if (expeRecvHash.find(make_pair(
          tag, make_pair(sender_node, receiver_node))) != expeRecvHash.end()) {
    task1 t2 =
        expeRecvHash[make_pair(tag, make_pair(sender_node, receiver_node))];
    // std::cout<<"count and t2.count is"<<count<<" "<<t2.count<<"\n";
    if (message_size == t2.count) {
      expeRecvHash.erase(make_pair(tag, make_pair(sender_node, receiver_node)));
      // std::cout<<"already in expected recv hash src dest count "<<src<<"
      // "<<dest<<" "<<count<<"\n";
      // totalRecvs++;
      // cout<<"total recvs: "<<totalRecvs<<endl;
      t2.msg_handler(t2.fun_arg);
    } else if (message_size > t2.count) {
      recvHash[make_pair(tag, make_pair(sender_node, receiver_node))] =
          message_size - t2.count;
      expeRecvHash.erase(make_pair(tag, make_pair(sender_node, receiver_node)));
      // std::cout<<"already in recv hash with more data\n";
      // std::cout<<"already in expected recv hash src dest count "<<src<<"
      // "<<dest<<" "<<count<<"\n";
      // totalRecvs++;
      // cout<<"total recvs: "<<totalRecvs<<endl;
      t2.msg_handler(t2.fun_arg);
    } else {
      t2.count -= message_size;
      expeRecvHash[make_pair(tag, make_pair(sender_node, receiver_node))] = t2;
      // std::cout<<"t2.count is"<<t2.count<<"\n";
      // std::cout<<"partially in recv hash \n";
    }
    // t2.msg_handler(t2.fun_arg);
  } else {
    if (recvHash.find(make_pair(tag, make_pair(sender_node, receiver_node))) ==
        recvHash.end()) {
      recvHash[make_pair(tag, make_pair(sender_node, receiver_node))] =
          message_size;
      // std::cout<<"not in expected recv hash\n";
    } else {
      // TODO: is this really required?
      recvHash[make_pair(tag, make_pair(sender_node, receiver_node))] +=
          message_size;
      // std::cout<<"in recv hash already maybe from previous flows\n";
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
  if (sentHash.find(make_pair(tag, make_pair(sender_node, receiver_node))) !=
      sentHash.end()) {
    task1 t2 = sentHash[make_pair(tag, make_pair(sender_node, receiver_node))];
    // sentHash.erase(make_pair(tag,make_pair(sender_node, receiver_node)));
    if (t2.count == message_size) {
      sentHash.erase(make_pair(tag, make_pair(sender_node, receiver_node)));
      if (nodeHash.find(make_pair(sender_node, 0)) == nodeHash.end()) {
        nodeHash[make_pair(sender_node, 0)] = message_size;
      } else {
        nodeHash[make_pair(sender_node, 0)] += message_size;
      }
      t2.msg_handler(t2.fun_arg);
    }
  }
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

  if (false){//sid == 1 && did == 2) {
  std::cout << "qp_finish " << sid << " " << did << " " << q->sport << " " << q->dport
            << " " << q->m_size << std::endl;
  }
  // remove rxQp from the receiver
  Ptr<Node> dstNode = n.Get(did);
  Ptr<RdmaDriver> rdma = dstNode->GetObject<RdmaDriver>();
  rdma->m_rdma->DeleteRxQp(q->sip.Get(), q->m_pg, q->sport);

  if (sender_src_port_map.find(make_pair(q->sport, make_pair(sid, did))) ==
      sender_src_port_map.end()) {
    std::cout << "could not find the tag, there must be something wrong"
              << std::endl;
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
