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
#include "AstraNetworkAPI.hh"
#include "AstraMemoryAPI.hh"

namespace AstraSim{
#define CLOCK_PERIOD 1
typedef unsigned long long Tick;
class BaseStream;
class Stream;
class Sys;
class FIFOMovement;
class FIFO;
class BackwardLink;
class DataSet;

enum class ComType {None,Reduce_Scatter, All_Gatehr, All_Reduce,All_to_All,All_Reduce_All_to_All};
enum class CollectiveBarrier{Blocking,Non_Blocking};
enum class SchedulingPolicy {LIFO,FIFO,HIGHEST,None};
enum class InjectionPolicy {Infinite,Aggressive,SemiAggressive,ExtraAggressive,Normal};
enum class PacketRouting {Hardware,Software};
enum class BusType {Both,Shared,Mem};
enum class StreamState {Created,Transferring,Ready,Executing,Zombie,Dead};
enum class EventType {CallEvents,PacketReceived,WaitForVnetTurn,General,TX_DMA,RX_DMA,Wight_Grad_Comm_Finished,Input_Grad_Comm_Finished,Fwd_Comm_Finished,Wight_Grad_Comm_Finished_After_Delay,Input_Grad_Comm_Finished_After_Delay,Fwd_Comm_Finished_After_Delay,Workload_Wait,Reduction_Ready,Rec_Finished,Send_Finished,
    Processing_Finished,Delivered,NPU_to_MA,MA_to_NPU,Read_Port_Free,Write_Port_Free,Apply_Boost,Stream_Transfer_Started,Stream_Ready,Consider_Process,Consider_Retire,Consider_Send_Back,StreamInit,StreamsFinishedIncrease,CommProcessingFinished,NotInitialized};


class CallData{
    public:
        ~CallData()= default;
};
class BasicEventHandlerData:public CallData{
public:
    int nodeId;
    EventType event;
    BasicEventHandlerData(int nodeId, EventType event);
};
class RecvPacketEventHadndlerData:public BasicEventHandlerData,public MetaData{
public:
    BaseStream *owner;
    int vnet;
    int stream_num;
    bool message_end;
    Tick ready_time;
    RecvPacketEventHadndlerData(BaseStream *owner,int nodeId, EventType event,int vnet,int stream_num);
};
class NetworkStat{
public:
    std::list<double> net_message_latency;
    int net_message_counter;
    NetworkStat(){
        net_message_counter=0;
    }
    void update_network_stat(NetworkStat *networkStat){
        if(net_message_latency.size()<networkStat->net_message_latency.size()){
            int dif=networkStat->net_message_latency.size()-net_message_latency.size();
            for(int i=0;i<dif;i++){
                net_message_latency.push_back(0);
            }
        }
        std::list<double>::iterator it=net_message_latency.begin();
        for(auto &ml:networkStat->net_message_latency){
            (*it)+=ml;
            std::advance(it,1);
        }
        net_message_counter++;
    }
    void take_network_stat_average(){
        for(auto &ml:net_message_latency){
            ml/=net_message_counter;
        }
    }
};
class SharedBusStat:public CallData{
public:
    double total_shared_bus_transfer_queue_delay;
    double total_shared_bus_transfer_delay;
    double total_shared_bus_processing_queue_delay;
    double total_shared_bus_processing_delay;

    double total_mem_bus_transfer_queue_delay;
    double total_mem_bus_transfer_delay;
    double total_mem_bus_processing_queue_delay;
    double total_mem_bus_processing_delay;
    int mem_request_counter;
    int shared_request_counter;
    SharedBusStat(BusType busType,double total_bus_transfer_queue_delay,double total_bus_transfer_delay,
                  double total_bus_processing_queue_delay,double total_bus_processing_delay){
        if(busType==BusType::Shared){
            this->total_shared_bus_transfer_queue_delay=total_bus_transfer_queue_delay;
            this->total_shared_bus_transfer_delay=total_bus_transfer_delay;
            this->total_shared_bus_processing_queue_delay=total_bus_processing_queue_delay;
            this->total_shared_bus_processing_delay=total_bus_processing_delay;

            this->total_mem_bus_transfer_queue_delay=0;
            this->total_mem_bus_transfer_delay=0;
            this->total_mem_bus_processing_queue_delay=0;
            this->total_mem_bus_processing_delay=0;
        }
        else{
            this->total_shared_bus_transfer_queue_delay=0;
            this->total_shared_bus_transfer_delay=0;
            this->total_shared_bus_processing_queue_delay=0;
            this->total_shared_bus_processing_delay=0;

            this->total_mem_bus_transfer_queue_delay=total_bus_transfer_queue_delay;
            this->total_mem_bus_transfer_delay=total_bus_transfer_delay;
            this->total_mem_bus_processing_queue_delay=total_bus_processing_queue_delay;
            this->total_mem_bus_processing_delay=total_bus_processing_delay;
        }
        shared_request_counter=0;
        mem_request_counter=0;
    }
    void update_bus_stats(BusType busType,SharedBusStat *sharedBusStat){
        if(busType==BusType::Shared){
            total_shared_bus_transfer_queue_delay+=sharedBusStat->total_shared_bus_transfer_queue_delay;
            total_shared_bus_transfer_delay+=sharedBusStat->total_shared_bus_transfer_delay;
            total_shared_bus_processing_queue_delay+=sharedBusStat->total_shared_bus_processing_queue_delay;
            total_shared_bus_processing_delay+=sharedBusStat->total_shared_bus_processing_delay;
            shared_request_counter++;
        }
        else if(busType==BusType::Mem){
            total_mem_bus_transfer_queue_delay+=sharedBusStat->total_mem_bus_transfer_queue_delay;
            total_mem_bus_transfer_delay+=sharedBusStat->total_mem_bus_transfer_delay;
            total_mem_bus_processing_queue_delay+=sharedBusStat->total_mem_bus_processing_queue_delay;
            total_mem_bus_processing_delay+=sharedBusStat->total_mem_bus_processing_delay;
            mem_request_counter++;
        }
        else{
            total_shared_bus_transfer_queue_delay+=sharedBusStat->total_shared_bus_transfer_queue_delay;
            total_shared_bus_transfer_delay+=sharedBusStat->total_shared_bus_transfer_delay;
            total_shared_bus_processing_queue_delay+=sharedBusStat->total_shared_bus_processing_queue_delay;
            total_shared_bus_processing_delay+=sharedBusStat->total_shared_bus_processing_delay;
            total_mem_bus_transfer_queue_delay+=sharedBusStat->total_mem_bus_transfer_queue_delay;
            total_mem_bus_transfer_delay+=sharedBusStat->total_mem_bus_transfer_delay;
            total_mem_bus_processing_queue_delay+=sharedBusStat->total_mem_bus_processing_queue_delay;
            total_mem_bus_processing_delay+=sharedBusStat->total_mem_bus_processing_delay;
            shared_request_counter++;
            mem_request_counter++;
        }
    }
    void update_bus_stats(BusType busType,SharedBusStat sharedBusStat){
        if(busType==BusType::Shared){
            total_shared_bus_transfer_queue_delay+=sharedBusStat.total_shared_bus_transfer_queue_delay;
            total_shared_bus_transfer_delay+=sharedBusStat.total_shared_bus_transfer_delay;
            total_shared_bus_processing_queue_delay+=sharedBusStat.total_shared_bus_processing_queue_delay;
            total_shared_bus_processing_delay+=sharedBusStat.total_shared_bus_processing_delay;
            shared_request_counter++;
        }
        else if(busType==BusType::Mem){
            total_mem_bus_transfer_queue_delay+=sharedBusStat.total_mem_bus_transfer_queue_delay;
            total_mem_bus_transfer_delay+=sharedBusStat.total_mem_bus_transfer_delay;
            total_mem_bus_processing_queue_delay+=sharedBusStat.total_mem_bus_processing_queue_delay;
            total_mem_bus_processing_delay+=sharedBusStat.total_mem_bus_processing_delay;
            mem_request_counter++;
        }
        else{
            total_shared_bus_transfer_queue_delay+=sharedBusStat.total_shared_bus_transfer_queue_delay;
            total_shared_bus_transfer_delay+=sharedBusStat.total_shared_bus_transfer_delay;
            total_shared_bus_processing_queue_delay+=sharedBusStat.total_shared_bus_processing_queue_delay;
            total_shared_bus_processing_delay+=sharedBusStat.total_shared_bus_processing_delay;
            total_mem_bus_transfer_queue_delay+=sharedBusStat.total_mem_bus_transfer_queue_delay;
            total_mem_bus_transfer_delay+=sharedBusStat.total_mem_bus_transfer_delay;
            total_mem_bus_processing_queue_delay+=sharedBusStat.total_mem_bus_processing_queue_delay;
            total_mem_bus_processing_delay+=sharedBusStat.total_mem_bus_processing_delay;
            shared_request_counter++;
            mem_request_counter++;
        }
    }
    void take_bus_stats_average(){
        total_shared_bus_transfer_queue_delay/=shared_request_counter;
        total_shared_bus_transfer_delay/=shared_request_counter;
        total_shared_bus_processing_queue_delay/=shared_request_counter;
        total_shared_bus_processing_delay/=shared_request_counter;

        total_mem_bus_transfer_queue_delay/=mem_request_counter;
        total_mem_bus_transfer_delay/=mem_request_counter;
        total_mem_bus_processing_queue_delay/=mem_request_counter;
        total_mem_bus_processing_delay/=mem_request_counter;
    }
};
class StreamStat:public SharedBusStat,public NetworkStat{
public:
    std::list<double > queuing_delay;
    int stream_stat_counter;
    //~StreamStat()= default;
    StreamStat():SharedBusStat(BusType::Shared,0,0,0,0){
        stream_stat_counter=0;
    }
    void update_stream_stats(StreamStat *streamStat){
        update_bus_stats(BusType::Both,streamStat);
        update_network_stat(streamStat);
        if(queuing_delay.size()<streamStat->queuing_delay.size()){
            int dif=streamStat->queuing_delay.size()-queuing_delay.size();
            for(int i=0;i<dif;i++){
                queuing_delay.push_back(0);
            }
        }
        std::list<double>::iterator it=queuing_delay.begin();
        for(auto &tick:streamStat->queuing_delay){
            (*it)+=tick;
            std::advance(it,1);
        }
        stream_stat_counter++;
    }
    void take_stream_stats_average(){
        take_bus_stats_average();
        take_network_stat_average();
        for(auto &tick:queuing_delay){
            tick/=stream_stat_counter;
        }
    }
};
class StatData:public CallData{
public:
    Tick start;
    Tick waiting;
    Tick end;
    //~StatData()= default;
    StatData(){
        start=0;
        waiting=0;
        end=0;
    }
};
class IntData:public CallData{
public:
    int data;
    //~IntData()= default;
    IntData(int d){
        data=d;
    }
};
class Callable{
public:
    virtual ~Callable() = default;
    virtual void call(EventType type,CallData *data)=0;
};
class SimSendCaller:public Callable{
public:
    void *buffer;
    int count;
    int type;
    int dst;
    int tag;
    sim_request *request;
    void (*msg_handler)(void *fun_arg);
    void* fun_arg;
    void call(EventType type,CallData *data);
    Sys* generator;
    SimSendCaller(Sys* generator,void *buffer, int count, int type, int dst, int tag, sim_request *request, void (*msg_handler)(void *fun_arg), void* fun_arg);
};
class SimRecvCaller:public Callable{
public:
    void *buffer;
    int count;
    int type;
    int src;
    int tag;
    sim_request *request;
    void (*msg_handler)(void *fun_arg);
    void* fun_arg;
    void call(EventType type,CallData *data);
    Sys* generator;
    SimRecvCaller(Sys* generator,void *buffer, int count, int type, int src, int tag, sim_request *request, void (*msg_handler)(void *fun_arg), void* fun_arg);
};
class MyPacket:public Callable
{
public:
    int cycles_needed;
    //FIFOMovement *fMovement;
    FIFO* dest;
    int fm_id;
    int stream_num;
    Callable *notifier;
    Callable *sender;
    int preferred_vnet;
    int preferred_dest;
    int preferred_src;
    Tick ready_time;
    //MyPacket(int cycles_needed, FIFOMovement *fMovement, FIFO *dest);
    MyPacket(int preferred_vnet,int preferred_src ,int preferred_dest);
    void set_notifier(Callable* c);
    void call(EventType event,CallData *data);
    //~MyPacket()= default;
};
class LogGP;
class MemMovRequest:public Callable,public SharedBusStat{
public:
    static int id;
    int my_id;
    int size;
    int latency;
    Callable *callable;
    bool processed;
    bool send_back;
    bool mem_bus_finished;
    Sys *generator;
    EventType callEvent=EventType::General;
    LogGP *loggp;
    std::list<MemMovRequest>::iterator pointer;
    void call(EventType event,CallData *data);

    Tick total_transfer_queue_time;
    Tick total_transfer_time;
    Tick total_processing_queue_time;
    Tick total_processing_time;
    Tick start_time;
    int request_num;
    MemMovRequest(int request_num,Sys *generator,LogGP *loggp,int size,int latency,Callable *callable,bool processed,bool send_back);
    void wait_wait_for_mem_bus(std::list<MemMovRequest>::iterator pointer);
    void set_iterator(std::list<MemMovRequest>::iterator pointer){
        this->pointer=pointer;
    }
    //~MemMovRequest()= default;
};
class DataSet:public Callable,public StreamStat{
public:
    static int id_auto_increment;
    int my_id;
    int total_streams;
    int finished_streams;
    bool finished;
    Tick finish_tick;
    Tick creation_tick;
    std::pair<Callable *,EventType> *notifier;

    DataSet(int total_streams);
    void set_notifier(Callable *layer,EventType event);
    void notify_stream_finished(StreamStat *data);
    void call(EventType event,CallData *data);
    bool is_finished();
    //~DataSet()= default;
};
class MemBus;
class LogGP:public Callable{
public:
    enum class State{Free,waiting,Sending,Receiving};
    enum class ProcState{Free,Processing};
    int request_num;
    std::string name;
    Tick L;
    Tick o;
    Tick g;
    double G;
    Tick last_trans;
    State curState;
    State prevState;
    ProcState processing_state;
    std::list<MemMovRequest> sends;
    std::list<MemMovRequest> receives;
    std::list<MemMovRequest> processing;

    std::list<MemMovRequest> retirements;
    std::list<MemMovRequest> pre_send;
    std::list<MemMovRequest> pre_process;
    std::list<MemMovRequest>::iterator talking_it;

    LogGP *partner;
    Sys *generator;
    EventType trigger_event;
    int subsequent_reads;
    int THRESHOLD;
    int local_reduction_delay;
    LogGP(std::string name,Sys *generator,Tick L,Tick o,Tick g,double G,EventType trigger_event);
    void process_next_read();
    void request_read(int bytes,bool processed,bool send_back,Callable *callable);
    void switch_to_receiver(MemMovRequest mr,Tick offset);
    void call(EventType event,CallData *data);
    MemBus *NPU_MEM;
    void attach_mem_bus(Sys *generator,Tick L,Tick o,Tick g,double G,bool model_shared_bus,int communication_delay);
    ~LogGP();
};
class MemBus{
    public:
        enum class Transmition{Fast,Usual};
        LogGP *NPU_side;
        LogGP *MA_side;
        Sys *generator;
        int communication_delay;
        bool model_shared_bus;
        ~MemBus();
        MemBus(std::string side1,std::string side2,Sys *generator,Tick L,Tick o,Tick g,double G,bool model_shared_bus,int communication_delay,bool attach);
        void send_from_NPU_to_MA(Transmition transmition,int bytes,bool processed,bool send_back,Callable *callable);
        void send_from_MA_to_NPU(Transmition transmition,int bytes,bool processed,bool send_back,Callable *callable);
};
class Algorithm;
class LogicalTopology;
class CollectivePhase{
    public:
        Sys *generator;
        int queue_id;
        int initial_data_size;
        int final_data_size;
        bool enabled;
        ComType comm_type;
        Algorithm *algorithm;
        CollectivePhase(Sys *generator,int queue_id,Algorithm *algorithm);
        CollectivePhase();
        void init(BaseStream *stream);
};
class DMA_Request{
public:
    int id;
    int slots;
    int latency;
    bool executed;
    int bytes;
    Callable *stream_owner;
    DMA_Request(int id,int slots,int latency,int bytes);
    DMA_Request(int id,int slots,int latency,int bytes,Callable *stream_owner);
};
class BaseStream:public Callable,public  StreamStat{
public:
    static std::map<int,int> synchronizer;
    static std::map<int,int> ready_counter;
    static std::map<int,std::list<BaseStream *>> suspended_streams;
    virtual ~BaseStream()=default;
    int stream_num;
    int total_packets_sent;
    SchedulingPolicy preferred_scheduling;
    std::list<CollectivePhase> phases_to_go;
    int current_queue_id;
    CollectivePhase my_current_phase;
    ComType current_com_type;
    Tick creation_time;
    Sys *owner;
    DataSet *dataset;
    int steps_finished;
    int initial_data_size;
    int priority;
    StreamState state;
    bool initialized;

    Tick last_phase_change;

    int test;
    int test2;
    uint64_t phase_latencies[10];

    void changeState(StreamState state);
    virtual void consume(RecvPacketEventHadndlerData *message)=0;
    virtual void init()=0;

    BaseStream(int stream_num,Sys *owner,std::list<CollectivePhase> phases_to_go);
    void declare_ready();
    bool is_ready();
    void consume_ready();
    void suspend_ready();
    void resume_ready(int st_num);
    void destruct_ready();

};
class PacketBundle:public Callable{
public:
    std::list<MyPacket*> locked_packets;
    bool processed;
    bool send_back;
    int size;
    Sys *generator;
    BaseStream *stream;
    Tick creation_time;
    MemBus::Transmition transmition;
    PacketBundle(Sys *generator,BaseStream *stream,std::list<MyPacket*> locked_packets, bool processed, bool send_back, int size,MemBus::Transmition transmition);
    PacketBundle(Sys *generator,BaseStream *stream, bool processed, bool send_back, int size,MemBus::Transmition transmition);
    void send_to_MA();
    void send_to_NPU();
    Tick delay;
    void call(EventType event,CallData *data);
    //~PacketBundle()= default;
};
class StreamBaseline: public BaseStream
{
public:
    StreamBaseline(Sys *owner,DataSet *dataset,int stream_num,std::list<CollectivePhase> phases_to_go,int priority);
    void init();
    void call(EventType event,CallData *data);
    void consume(RecvPacketEventHadndlerData *message);
    //~StreamBaseline()= default;
};
}
#include "astra-sim/workload/Workload.hh"
namespace AstraSim{
class QueueLevels;
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
    enum class CollectiveImplementation{AllToAll,DoubleBinaryTreeLocalAllToAll,HierarchicalRing,DoubleBinaryTree};
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
             std::string path,std::string run_name,bool seprate_log);

    void iterate();
    bool initialize_sys(std::string name);
    std::string trim(const std::string& str,const std::string& whitespace);
    bool parse_var(std::string var,std::string value);
    bool post_process_inputs();
    int sim_send(Tick delay,void *buffer, int count, int type, int dst, int tag, sim_request *request, void (*msg_handler)(void *fun_arg), void* fun_arg);
    int sim_recv(Tick delay,void *buffer, int count, int type, int src, int tag, sim_request *request, void (*msg_handler)(void *fun_arg), void* fun_arg);
    Tick mem_read(uint64_t bytes);
    Tick mem_write(uint64_t bytes);
    static int get_layer_numbers(std::string workload_input);
    DataSet * generate_all_reduce(int size,bool local, bool vertical, bool horizontal,SchedulingPolicy pref_scheduling,int layer);
    DataSet * generate_all_to_all(int size,bool local, bool vertical, bool horizontal,SchedulingPolicy pref_scheduling,int layer);
    DataSet * generate_all_gather(int size,bool local, bool vertical, bool horizontal,SchedulingPolicy pref_scheduling,int layer);
    DataSet *generate_alltoall_all_to_all(int size,bool local_run, bool horizontal_run,SchedulingPolicy pref_scheduling,int layer);
    DataSet *generate_alltoall_all_reduce(int size,bool local_run, bool horizontal_run,SchedulingPolicy pref_scheduling,int layer);
    DataSet *generate_tree_all_reduce(int size,bool local_run, bool horizontal_run,SchedulingPolicy pref_scheduling,int layer);
    DataSet *generate_hierarchical_all_to_all(int size,bool local_run,bool vertical_run, bool horizontal_run, SchedulingPolicy pref_scheduling,int layer);
    DataSet *generate_hierarchical_all_reduce(int size,bool local_run, bool vertical_run, bool horizontal_run, SchedulingPolicy pref_scheduling,int layer);
    DataSet *generate_hierarchical_all_gather(int size,bool local_run, bool vertical_run, bool horizontal_run, SchedulingPolicy pref_scheduling,int layer);
    void insert_stream(std::list<BaseStream*> *queue,BaseStream *baseStream);
    void proceed_to_next_vnet_baseline(StreamBaseline *stream);
    int determine_chunk_size(int size,ComType type);
    int get_priority(SchedulingPolicy pref_scheduling);
    static void handleEvent(void *arg);
    timespec_t generate_time(int cycles);
};
class ComputeNode{

};
class Node:public ComputeNode{
    public:
        int id;
        Node *parent;
        Node *left_child;
        Node *right_child;
        Node(int id,Node *parent,Node *left_child,Node *right_child);
};
class LogicalTopology{
    public:
        virtual LogicalTopology* get_topology();
        static int get_reminder(int number,int divisible);
        virtual ~LogicalTopology()= default;
};
class Torus:public LogicalTopology{
    public:
        enum class Dimension{Local,Vertical,Horizontal,Perpendicular,Fourth};
        enum class Direction{Clockwise,Anticlockwise};
        int id;
        int local_node_id;
        int right_node_id;
        int left_node_id;
        int up_node_id;
        int down_node_id;
        int Zpositive_node_id,Znegative_node_id,Fpositive_node_id,Fnegative_node_id;
        int total_nodes,local_dim,horizontal_dim,vertical_dim,perpendicular_dim;
        Torus(int id,int total_nodes,int local_dim,int horizontal_dim,int vertical_dim,int perpendicular_dim);
        void find_neighbors(int id,int total_nodes,int local_dim,int horizontal_dim,int vertical_dim,int perpendicular_dim);
        virtual int get_receiver_node(int id,Dimension dimension,Direction direction);
        virtual int get_sender_node(int id,Dimension dimension,Direction direction);
        int get_nodes_in_ring(Dimension dimension);
        bool is_enabled(int id,Dimension dimension);
};
class BinaryTree:public Torus{
    public:
        enum class TreeType{RootMax,RootMin};
        enum class Type{Leaf,Root,Intermediate};
        int total_tree_nodes;
        int start;
        TreeType tree_type;
        int stride;
        Node *tree;
        virtual ~BinaryTree();
        std::map<int,Node*> node_list;
        BinaryTree(int id,TreeType tree_type,int total_tree_nodes,int start,int stride,int local_dim);
        Node* initialize_tree(int depth,Node* parent);
        void build_tree(Node* node);
        int get_parent_id(int id);
        int get_left_child_id(int id);
        int get_right_child_id(int id);
        Type get_node_type(int id);
        void print(Node *node);
        virtual int get_receiver_node(int id,Dimension dimension,Direction direction);
        virtual int get_sender_node(int id,Dimension dimension,Direction direction);

};
class DoubleBinaryTreeTopology:public LogicalTopology{
    public:
        int counter;
        BinaryTree *DBMAX;
        BinaryTree *DBMIN;
        LogicalTopology *get_topology();
        ~DoubleBinaryTreeTopology();
        DoubleBinaryTreeTopology(int id,int total_tree_nodes,int start,int stride,int local_dim);
};
class AllToAllTopology:public Torus{
    public:
        AllToAllTopology(int id,int total_nodes,int local_dim,int alltoall_dim);
        int get_receiver_node(int id,Dimension dimension,Direction direction);
        int get_sender_node(int id,Dimension dimension,Direction direction);
};
class Algorithm:public Callable{
    public:
        enum class Name{Ring,DoubleBinaryTree,AllToAll};
        Name name;
        int id;
        BaseStream *stream;
        LogicalTopology *logicalTopology;
        int data_size;
        int final_data_size;
        ComType comType;
        bool enabled;
        int layer_num;
        Algorithm(int layer_num);
        virtual ~Algorithm()= default;
        virtual void run(EventType event,CallData *data)=0;
        virtual void exit();
        virtual void init(BaseStream *stream);
        virtual void call(EventType event,CallData *data);
};
class Ring:public Algorithm{
    public:
        enum class Type{AllReduce,AllGather,ReduceScatter,AllToAll};
        Torus::Dimension dimension;
        Torus::Direction direction;
        MemBus::Transmition transmition;
        int zero_latency_packets;
        int non_zero_latency_packets;
        int id;
        int current_receiver;
        int current_sender;
        int nodes_in_ring;
        Type type;
        int stream_count;
        int max_count;
        int remained_packets_per_max_count;
        int remained_packets_per_message;
        int parallel_reduce;
        PacketRouting routing;
        InjectionPolicy injection_policy;
        std::list<MyPacket> packets;
        bool toggle;
        long free_packets;
        long total_packets_sent;
        long total_packets_received;
        int msg_size;
        std::list<MyPacket*> locked_packets;
        bool processed;
        bool send_back;
        bool NPU_to_MA;

        Ring(Type type,int id,int layer_num,Torus *torus,int data_size,Torus::Dimension dimension, Torus::Direction direction,PacketRouting routing,InjectionPolicy injection_policy,bool boost_mode);
        virtual void run(EventType event,CallData *data);
        void process_stream_count();
        //void call(EventType event,CallData *data);
        void release_packets();
        virtual void process_max_count();
        void reduce();
        bool iteratable();
        virtual int get_non_zero_latency_packets();
        void insert_packet(Callable *sender);
        bool ready();

        void exit();
};
class DoubleBinaryTreeAllReduce:public Algorithm{
    public:
        enum class State{Begin,WaitingForTwoChildData,WaitingForOneChildData,SendingDataToParent,WaitingDataFromParent,SendingDataToChilds,End};
        void run(EventType event,CallData *data);
        //void call(EventType event,CallData *data);
        //void exit();
        int parent;
        int left_child;
        int reductions;
        int right_child;
        //BinaryTree *tree;
        BinaryTree::Type type;
        State state;
        DoubleBinaryTreeAllReduce(int id,int layer_num,BinaryTree *tree,int data_size,bool boost_mode);
        //void init(BaseStream *stream);
};
class AllToAll:public Ring{
    public:
        AllToAll(Type type,int id,int layer_num,AllToAllTopology *allToAllTopology,int data_size,Torus::Dimension dimension, Torus::Direction direction,PacketRouting routing,InjectionPolicy injection_policy,bool boost_mode);
        void run(EventType event,CallData *data);
        void process_max_count();
        int get_non_zero_latency_packets();
};
class QueueLevelHandler{
    public:
        std::vector<int> queues;
        int allocator;
        int first_allocator;
        int last_allocator;
        int level;
        QueueLevelHandler(int level,int start,int end);
        std::pair<int,Torus::Direction> get_next_queue_id();
        std::pair<int,Torus::Direction> get_next_queue_id_first();
        std::pair<int,Torus::Direction> get_next_queue_id_last();
};
class QueueLevels{
    public:
        std::vector<QueueLevelHandler> levels;
        std::pair<int,Torus::Direction> get_next_queue_at_level(int level);
        std::pair<int,Torus::Direction> get_next_queue_at_level_first(int level);
        std::pair<int,Torus::Direction> get_next_queue_at_level_last(int level);
        QueueLevels(int levels, int queues_per_level,int offset);
        QueueLevels(std::vector<int> lv,int offset);
};
}
#endif
