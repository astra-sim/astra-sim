#include "astra-sim/system/AstraNetworkAPI.hh"
#include "astra-sim/system/Sys.hh"
#include "extern/remote_memory_backend/analytical/AnalyticalRemoteMemory.hh"

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "entry.h"
#include <execinfo.h>
#include <fstream>
#include <iostream>
#include <queue>
#include <stdio.h>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace std;
using namespace ns3;

/*
std::vector<string> workloads{"Resnet50_fused.txt"};
//std::vector<string> workloads{"Resnet50_fused.txt"};
std::vector<std::vector<int>> physical_dims{{8, 4}};

unsigned long long tempcnt = 999;
unsigned long long cnt = 0;
struct sim_event {
  void *buffer;
  uint64_t count;
  int type;
  int dst;
  int tag;
  string fnType;
};
*/
queue<struct task1> workerQueue;

class ASTRASimNetwork : public AstraSim::AstraNetworkAPI {
private:
  int npu_offset;

public:
  //queue<sim_event> sim_event_queue;
  ASTRASimNetwork(int rank) : AstraNetworkAPI(rank) {}

  ~ASTRASimNetwork() {}

  int sim_finish() {
    for (auto it = nodeHash.begin(); it != nodeHash.end(); it++) {
      pair<int, int> p = it->first;
      if (p.second == 0) {
        cout << "All data sent from node " << p.first << " is " << it->second
             << "\n";
      } else {
        cout << "All data received by node " << p.first << " is " << it->second
             << "\n";
      }
    }
    exit(0);
    return 0;
  }

  double sim_time_resolution() { return 0; }
  //int sim_init(AstraSim::AstraMemoryAPI *MEM) { return 0; }

  AstraSim::timespec_t sim_get_time() {
    AstraSim::timespec_t timeSpec;
    timeSpec.time_val = Simulator::Now().GetNanoSeconds();
    return timeSpec;
  }

  virtual void schedule(AstraSim::timespec_t delta,
                        void (*fun_ptr)(void *fun_arg), void *fun_arg) {
    task1 t;
    t.type = 2;
    t.fun_arg = fun_arg;
    t.msg_handler = fun_ptr;
    t.schTime = delta.time_val;
    Simulator::Schedule(NanoSeconds(t.schTime), t.msg_handler, t.fun_arg);
    return;
  }

  virtual int sim_send(void *buffer,   // not yet used
                       uint64_t count, // number of bytes to be send
                       int type,       // not yet used
                       int dst,
                       int tag,                        // not yet used
                       AstraSim::sim_request *request, // not yet used
                       void (*msg_handler)(void *fun_arg), void *fun_arg) {
    dst += npu_offset;
    // if(rank==0 && dst == 1 && cnt == 0){
    //	cout<<“rank 0 and destination 1 sim_send test\n”;
    // }
    task1 t;
    t.src = rank;
    t.dest = dst;
    t.count = count;
    t.type = 0;
    t.fun_arg = fun_arg;
    t.msg_handler = msg_handler;
    sentHash[make_pair(tag, make_pair(t.src, t.dest))] = t;
    SendFlow(rank, dst, count, msg_handler, fun_arg, tag);
    return 0;
  }

  virtual int sim_recv(void *buffer, uint64_t count, int type, int src, int tag,
                       AstraSim::sim_request *request,
                       void (*msg_handler)(void *fun_arg), void *fun_arg) {
    task1 t;
    t.src = src;
    t.dest = rank;
    t.count = count;
    t.type = 1;
    t.fun_arg = fun_arg;
    t.msg_handler = msg_handler;
    if (recvHash.find(make_pair(tag, make_pair(t.src, t.dest))) !=
        recvHash.end()) {
      int count = recvHash[make_pair(tag, make_pair(t.src, t.dest))];
      if (count == t.count) {
        recvHash.erase(make_pair(tag, make_pair(t.src, t.dest)));
        t.msg_handler(t.fun_arg);
      } else if (count > t.count) {
        recvHash[make_pair(tag, make_pair(t.src, t.dest))] = count - t.count;
        t.msg_handler(t.fun_arg);
      } else {
        recvHash.erase(make_pair(tag, make_pair(t.src, t.dest)));
        t.count -= count;
        expeRecvHash[make_pair(tag, make_pair(t.src, t.dest))] = t;
      }
    } else {
      if (expeRecvHash.find(make_pair(tag, make_pair(t.src, t.dest))) ==
          expeRecvHash.end()) {
        expeRecvHash[make_pair(tag, make_pair(t.src, t.dest))] = t;
      } else {
        int expecount =
            expeRecvHash[make_pair(tag, make_pair(t.src, t.dest))].count;
        t.count += expecount;
        expeRecvHash[make_pair(tag, make_pair(t.src, t.dest))] = t;
      }
    }
    return 0;
  }
  void handleEvent(int dst, int cnt) {
    // cout<<"event handled\n";
  }
};

int main(int argc, char *argv[]) {
  LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
  LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

  cout << "ASTRA-sim + NS3" << endl;

  string workload_configuration;
  string comm_group_configuration;
  string system_configuration;
  string network_configuration;
  string memory_configuration;
  const int num_npus = 64; // TODO: parametrize
  int num_queues_per_dim = 1;
  vector<int> physical_dims{num_npus}; // TODO: parametrize
  vector<int> queues_per_dim{1};
  double comm_scale = 1;
  double injection_scale = 1;
  bool rendezvous_protocol = false;

  CommandLine cmd;
  cmd.AddValue("workload-configuration", "Workload configuration file",
               workload_configuration);
  cmd.AddValue("comm-group-configuration",
               "Communicator group configuration file",
               comm_group_configuration);
  cmd.AddValue("system-configuration", "System configuration file",
               system_configuration);
  cmd.AddValue("network-configuration", "Network configuration file",
               network_configuration);
  cmd.AddValue("num-queues-per-dim", "Number of queues per each dimension",
               num_queues_per_dim);
  cmd.AddValue("comm-scale", "Communication scale", comm_scale);
  cmd.AddValue("injection-scale", "Injection scale", injection_scale);
  cmd.AddValue("rendezvous-protocol", "Whether to enable rendezvous protocol",
               rendezvous_protocol);
  cmd.AddValue("remote-memory-configuration", "Memory configuration file", 
                memory_configuration);
  cmd.Parse(argc, argv);

  

  vector<ASTRASimNetwork *> networks(num_npus, nullptr);
  vector<AstraSim::Sys *> systems(num_npus, nullptr);
  // make_unique is a C++14 feature.
  Analytical::AnalyticalRemoteMemory* mem =
      new Analytical::AnalyticalRemoteMemory(memory_configuration);

/*
  assert(workloads.size() == physical_dims.size());
  int num_gpus = 0;
  for (auto &a : physical_dims) {
    int job_npus = 1;
    for (auto &dim : a) {
      job_npus *= dim;
    }
    num_gpus += job_npus;
  }
  */

  LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
  LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

  int npu_offset = 0;
  /*
  for (int i = 0; i < physical_dims.size(); i++) {
    std::vector<int> queues_per_dim(physical_dims[i].size(), 1);
    // determining the appropriate system input file
    std::string system_input;
    if (physical_dims[i].size() == 1) {
      system_input = "sample_1D_switch_sys.txt";
    } else if (physical_dims[i].size() == 2) {
      system_input = "sample_2D_switch_sys.txt";
    } else if (physical_dims[i].size() == 3) {
      system_input = "sample_3D_switch_sys.txt";
    }
    // initializing the net and sys layers
    int job_npus = 1;
    for (auto dim : physical_dims[i]) {
      job_npus *= dim;
    }
  */
  for (int npu_id = 0; npu_id < num_npus; npu_id++) {
    networks[npu_id] = new ASTRASimNetwork(npu_id);
    systems[npu_id] = new AstraSim::Sys(
        npu_id, workload_configuration, comm_group_configuration,
        system_configuration, mem, networks[npu_id], physical_dims,
        queues_per_dim, injection_scale, comm_scale, rendezvous_protocol);
        /*
    systems[npu_id] = new AstraSim::Sys(
          networks[j + npu_offset], // AstraNetworkAPI
          nullptr,                  // AstraMemoryAPI
          j,                        // id
          npu_offset,               // npu ofsset in a multi-job scenario
          1,                        // num_passes
          physical_dims[i],         // dimensions
          queues_per_dim,           // queues per corresponding dimension
          "../../../../../astra-sim/inputs/system/" +
              system_input, // system configuration
          "../../../../../astra-sim/inputs/workload/" +
              workloads[i], // DLRM_HybridParallel.txt, //
                            // Resnet50_DataParallel.txt, // workload
                            // configuration
          comm_scale,       // communication scale
          1,                // computation scale
          1,                // injection scale
          1,
          0,                  // total_stat_rows and stat_row
          "scratch/results/", // stat file path
          "test1",            // run name
          true,               // separate_log
          false               // randezvous protocol
      );
      */
    //}
    //npu_offset += job_npus;
  }
  main1(argc, argv);
  // pass number of nodes
  for (int i = 0; i < num_npus; i++) {
    systems[i]->workload->fire();
  }

  Simulator::Run();
  // Simulator::Stop(TimeStep (0x7fffffffffffffffLL));
  Simulator::Stop(Seconds(2000000000));
  Simulator::Destroy();
  return 0;
}
