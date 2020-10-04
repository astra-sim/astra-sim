/******************************************************************************
Copyright (c) 2020 Georgia Institute of Technology
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Author : Saeed Rashidi (saeed.rashidi@gatech.edu)
*******************************************************************************/

#ifndef __SYSTEM_HH__
#define __SYSTEM_HH__

#include <map>
#include <math.h>
#include <fstream>
#include <chrono>
#include <ctime>
#include <tuple>
#include <cstdint>
#include <list>
#include <vector>
#include <algorithm>
#include <chrono>
#include <sstream>
#include <assert.h>
#include "Common.hh"
#include "AstraNetworkAPI.hh"
#include "AstraMemoryAPI.hh"
#include "CollectivePhase.hh"
#include "Callable.hh"
#include "src/astra-sim/workload/Workload.hh"

namespace AstraSim{
    class MemBus;
    class BaseStream;
    class StreamBaseline;
    class DataSet;
    class SimSendCaller;
    class SimRecvCaller;
    class QueueLevels;
    class Workload;
    class LogicalTopology;
    class Sys:public Callable
    {
    public:
        class SchedulerUnit{
            public:
                Sys *sys;
                int ready_list_threshold;
                int queue_threshold;
                int max_running_streams;
                std::map<int,int> running_streams;
                std::map<int,std::list<BaseStream*>::iterator> stream_pointer;
                SchedulerUnit(Sys *sys,std::vector<int> queues,int max_running_streams,int ready_list_threshold,int queue_threshold);
                void notify_stream_removed(int vnet);
                void notify_stream_added(int vnet);
                void notify_stream_added_into_ready_list();
        };
        SchedulerUnit *scheduler_unit;
        enum class CollectiveOptimization{Baseline,LocalBWAware};
        enum class CollectiveImplementation{AllToAll,DoubleBinaryTreeLocalAllToAll,LocalRingNodeA2AGlobalDBT,HierarchicalRing,DoubleBinaryTree};
        ~Sys();
        AstraNetworkAPI *NI;
        AstraMemoryAPI *MEM;
        int finished_workloads;
        int id;

        CollectiveImplementation collectiveImplementation;
        CollectiveOptimization collectiveOptimization;

        std::chrono::high_resolution_clock::time_point start_sim_time;
        std::chrono::high_resolution_clock::time_point end_sim_time;

        std::list<Callable *> registered_for_finished_stream_event;

        int local_dim;
        int vertical_dim;
        int horizontal_dim;
        int perpendicular_dim;
        int fourth_dim;
        int local_queus;
        int vertical_queues;
        int horizontal_queues;int perpendicular_queues;
        int fourth_queues;
        int priority_counter;
        bool boost_mode;
        bool rendezvous_enabled;
        bool initialized;

        int processing_latency;
        int communication_delay;

        int preferred_dataset_splits;
        PacketRouting alltoall_routing;
        InjectionPolicy injection_policy;
        float compute_scale;
        float comm_scale;
        float injection_scale;
        int local_reduction_delay;
        uint64_t pending_events;
        std::string method;

        //for test
        Workload *workload;
        MemBus *memBus;
        int all_queues;
        //for supporting LIFO
        std::list<BaseStream*> ready_list;
        SchedulingPolicy scheduling_policy;
        int first_phase_streams;
        int total_running_streams;
        std::map<int,std::list<BaseStream*>> active_Streams;
        std::map<int,std::list<int>> stream_priorities;

        QueueLevels *vLevels;
        std::map<std::string,LogicalTopology*> logical_topologies;
        std::map<Tick,std::list<std::tuple<Callable*,EventType,CallData *>>> event_queue;
        static int total_nodes;
        static Tick offset;
        static std::vector<Sys*> all_generators;
        static uint8_t *dummy_data;
        //for reports
        uint64_t streams_injected;
        uint64_t streams_finished;
        int stream_counter;
        bool enabled;


        std::string inp_scheduling_policy;
        std::string inp_packet_routing;
        std::string inp_injection_policy;
        std::string inp_collective_implementation;
        std::string inp_collective_optimization;
        float inp_L;
        float inp_o;
        float inp_g;
        float inp_G;
        int inp_model_shared_bus;
        bool model_shared_bus;
        int inp_boost_mode;

        void register_for_finished_stream(Callable *callable);
        void increase_finished_streams(int amount);
        void register_event(Callable *callable,EventType event,CallData *callData,int cycles);
        void insert_into_ready_list(BaseStream *stream);
        void ask_for_schedule(int max);
        void schedule(int num);


        void register_phases(BaseStream *stream,std::list<CollectivePhase> phases_to_go);
        void call(EventType type,CallData *data);
        void try_register_event(Callable *callable,EventType event,CallData *callData,Tick &cycles);
        void call_events();
        void workload_finished(){finished_workloads++;};
        static Tick boostedTick();
        static void exiting();
        int nextPowerOf2(int n);
        static void sys_panic(std::string msg);
        void exitSimLoop(std::string msg);
        bool seprate_log;

        Sys(AstraNetworkAPI *NI,AstraMemoryAPI *MEM,int id,int num_passes,int local_dim, int vertical_dim,int horizontal_dim,
                 int perpendicular_dim,int fourth_dim,int local_queus,int vertical_queues,int horizontal_queues,
                 int perpendicular_queues,int fourth_queues,std::string my_sys,
                 std::string my_workload,float comm_scale,float compute_scale,float injection_scale,int total_stat_rows,int stat_row,
                 std::string path,std::string run_name,bool seprate_log,bool rendezvous_enabled);

        void iterate();
        bool initialize_sys(std::string name);
        std::string trim(const std::string& str,const std::string& whitespace);
        bool parse_var(std::string var,std::string value);
        bool post_process_inputs();
        int front_end_sim_send(Tick delay,void *buffer, int count, int type, int dst, int tag, sim_request *request, void (*msg_handler)(void *fun_arg), void* fun_arg);
        int front_end_sim_recv(Tick delay,void *buffer, int count, int type, int src, int tag, sim_request *request, void (*msg_handler)(void *fun_arg), void* fun_arg);
        int sim_send(Tick delay,void *buffer, int count, int type, int dst, int tag, sim_request *request, void (*msg_handler)(void *fun_arg), void* fun_arg);
        int sim_recv(Tick delay,void *buffer, int count, int type, int src, int tag, sim_request *request, void (*msg_handler)(void *fun_arg), void* fun_arg);
        int rendezvous_sim_send(Tick delay,void *buffer, int count, int type, int dst, int tag, sim_request *request,
                                void (*msg_handler)(void *fun_arg), void* fun_arg);
        int rendezvous_sim_recv(Tick delay,void *buffer, int count, int type, int src, int tag, sim_request *request,
                     void (*msg_handler)(void *fun_arg), void* fun_arg);
        Tick mem_read(uint64_t bytes);
        Tick mem_write(uint64_t bytes);
        static int get_layer_numbers(std::string workload_input);
        DataSet * generate_all_reduce(int size,bool local, bool vertical, bool horizontal,SchedulingPolicy pref_scheduling,int layer);
        DataSet * generate_all_to_all(int size,bool local, bool vertical, bool horizontal,SchedulingPolicy pref_scheduling,int layer);
        DataSet * generate_all_gather(int size,bool local, bool vertical, bool horizontal,SchedulingPolicy pref_scheduling,int layer);
        DataSet * generate_reduce_scatter(int size,bool local, bool vertical, bool horizontal,SchedulingPolicy pref_scheduling,int layer);
        DataSet *generate_alltoall_all_to_all(int size,bool local_run, bool horizontal_run,SchedulingPolicy pref_scheduling,int layer);
        DataSet *generate_alltoall_all_reduce(int size,bool local_run, bool horizontal_run,SchedulingPolicy pref_scheduling,int layer);
        DataSet *generate_tree_all_reduce(int size,bool local_run, bool horizontal_run,SchedulingPolicy pref_scheduling,int layer);
        DataSet *generate_hierarchical_all_to_all(int size,bool local_run,bool vertical_run, bool horizontal_run, SchedulingPolicy pref_scheduling,int layer);
        DataSet *generate_hierarchical_all_reduce(int size,bool local_run, bool vertical_run, bool horizontal_run, SchedulingPolicy pref_scheduling,int layer);
        DataSet *generate_hierarchical_all_gather(int size,bool local_run, bool vertical_run, bool horizontal_run, SchedulingPolicy pref_scheduling,int layer);
        DataSet *generate_hierarchical_reduce_scatter(int size,bool local_run, bool vertical_run, bool horizontal_run, SchedulingPolicy pref_scheduling,int layer);
        DataSet *generate_LocalRingNodeA2AGlobalDBT_all_reduce(int size,bool local_run, bool vertical_run, bool horizontal_run, SchedulingPolicy pref_scheduling,int layer);
        void insert_stream(std::list<BaseStream*> *queue,BaseStream *baseStream);
        void proceed_to_next_vnet_baseline(StreamBaseline *stream);
        int determine_chunk_size(int size,ComType type);
        int get_priority(SchedulingPolicy pref_scheduling);
        static void handleEvent(void *arg);
        timespec_t generate_time(int cycles);
    };
}
#endif
