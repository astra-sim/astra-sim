#include "astra-sim/system/AstraNetworkAPI.hh"
#include "astra-sim/system/Sys.hh"
#include "astra-sim/json.hpp"
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
using json = nlohmann::json;

queue<struct task1> workerQueue;

class ASTRASimNetwork : public AstraSim::AstraNetworkAPI {

public:
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

  AstraSim::timespec_t sim_get_time() {
    AstraSim::timespec_t timeSpec;
    timeSpec.time_val = Simulator::Now().GetNanoSeconds();
    return timeSpec;
  }

  virtual void schedule(AstraSim::timespec_t schTime,
                        void (*fun_ptr)(void *fun_arg), void *fun_arg) {
    task1 t;
    t.type = 2;
    t.fun_arg = fun_arg;
    t.msg_handler = fun_ptr;
    t.schTime = schTime.time_val;
    Simulator::Schedule(NanoSeconds(t.schTime)- Simulator::Now(), t.msg_handler, t.fun_arg);
    return;
  }

  virtual int sim_send(void *buffer,   // not yet used
                       uint64_t count, // number of bytes to be send
                       int type,       // not yet used
                       int dst,
                       int tag,                        // not yet used
                       AstraSim::sim_request *request, // not yet used
                       void (*msg_handler)(void *fun_arg), void *fun_arg) {
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

  void handleEvent(int dst, int cnt) {}
};

// TODO: Migrate to yaml
void read_logical_topo_config(string network_configuration, vector<int>&physical_dims) {
  
  ifstream inFile;
  inFile.open(network_configuration);
  if (!inFile) {
    cerr << "Unable to open file: " << network_configuration << endl;
    exit(1);
  }

  json j;
  inFile >> j;
  if (j.contains("physical-dims")) {
    vector<string> physical_dims_str_vec = j["physical-dims"];
    for (auto physical_dims_str : physical_dims_str_vec) {
      physical_dims.push_back(stoi(physical_dims_str));
    }
  }
}

int main(int argc, char *argv[]) {
  LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
  LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

  cout << "ASTRA-sim + NS3" << endl;

  // Read command line arguments
  CommandLine cmd;
  string workload_configuration;
  cmd.AddValue("workload-configuration", "Workload configuration file",
               workload_configuration);

  string system_configuration;
  cmd.AddValue("system-configuration", "System configuration file",
               system_configuration);
               
  string memory_configuration;
  cmd.AddValue("remote-memory-configuration", "Memory configuration file", 
                memory_configuration);

  string comm_group_configuration;
  cmd.AddValue("comm-group-configuration",
               "Communicator group configuration file",
               comm_group_configuration);

  string logical_topology_configuration;
  cmd.AddValue("logical-topology-configuration",
               "Logical topology configuration file",
               logical_topology_configuration);

  int num_queues_per_dim = 1;
  cmd.AddValue("num-queues-per-dim", "Number of queues per each dimension",
               num_queues_per_dim);

  double comm_scale = 1;
  cmd.AddValue("comm-scale", "Communication scale", comm_scale);

  double injection_scale = 1;
  cmd.AddValue("injection-scale", "Injection scale", injection_scale);

  bool rendezvous_protocol = false;
  cmd.AddValue("rendezvous-protocol", "Whether to enable rendezvous protocol",
               rendezvous_protocol);
  cmd.Parse(argc, argv);

  // Read network config and find physical dims
  auto physical_dims = vector<int>();
  read_logical_topo_config(logical_topology_configuration, physical_dims);
  int num_npus = 1;
  
  stringstream dimstr;
  for (auto num_npus_per_dim : physical_dims){
    num_npus *= num_npus_per_dim;
    dimstr << num_npus_per_dim << ",";
  }
  cout << "There are " << num_npus << " npus: " << dimstr.str() << "\n";

  vector<int> queues_per_dim = vector<int>(physical_dims.size(), num_queues_per_dim);

  // Setup network & System layer
  vector<ASTRASimNetwork *> networks(num_npus, nullptr);
  vector<AstraSim::Sys *> systems(num_npus, nullptr);
  Analytical::AnalyticalRemoteMemory* mem =
      new Analytical::AnalyticalRemoteMemory(memory_configuration);

  for (int npu_id = 0; npu_id < num_npus; npu_id++) {
    networks[npu_id] = new ASTRASimNetwork(npu_id);
    systems[npu_id] = new AstraSim::Sys(
        npu_id, 
        workload_configuration, 
        comm_group_configuration,
        system_configuration, 
        mem, 
        networks[npu_id], 
        physical_dims, // conf
        queues_per_dim, 
        injection_scale,  //cm d
        comm_scale,  //cmd
        rendezvous_protocol); //cmd
  }

  // Initialize ns3 simulation
  setup_ns3_simulation(argc, argv);

  // Actually run simulation
  for (int i = 0; i < num_npus; i++) {
    systems[i]->workload->fire();
  }

  Simulator::Run();
  Simulator::Stop(Seconds(2000000000));
  Simulator::Destroy();
  return 0;
}
