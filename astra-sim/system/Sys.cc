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

#include "Sys.hh"
#include "BaseStream.hh"
#include "StreamBaseline.hh"
#include "DataSet.hh"
#include "SimSendCaller.hh"
#include "SimRecvCaller.hh"
#include "QueueLevels.hh"
#include "MemBus.hh"
#include "astra-sim/system/collective/DoubleBinaryTreeAllReduce.hh"
#include "astra-sim/system/collective/Ring.hh"
#include "astra-sim/system/collective/AllToAll.hh"

#include "astra-sim/system/topology/DoubleBinaryTreeTopology.hh"
#include "astra-sim/system/topology/Torus3D.hh"
#include "astra-sim/system/topology/RingTopology.hh"
#include "astra-sim/system/topology/LocalRingGlobalBinaryTree.hh"
#include "astra-sim/system/topology/LocalRingNodeA2AGlobalDBT.hh"

#include "RendezvousSendData.hh"
#include "RendezvousRecvData.hh"


namespace AstraSim{
Tick Sys::offset=0;
int Sys::total_nodes=0;
uint8_t *Sys::dummy_data=new uint8_t[2];
std::vector<Sys*> Sys::all_generators;

Sys::~Sys() {
    end_sim_time=std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::minutes>(end_sim_time - start_sim_time);
    if(id==0){
        auto timenow =std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::cout<<"*****"<<std::endl
        <<"Time to exit: "<<ctime(&timenow)
        <<"Collective implementation: "<<inp_collective_implementation<<std::endl
        <<"Collective optimization: "<<inp_collective_optimization<<std::endl
        <<"Total sim duration: "<<duration.count()/60<<":"<<duration.count()%60<<" hours"<<std::endl
        <<"Total streams injected: "<<streams_injected<<std::endl
        <<"Total streams finished: "<<streams_finished<<std::endl
        <<"Percentage of finished streams: "<<(((double)streams_finished)/streams_injected)*100<<" %"<<std::endl
        <<"*****"<<std::endl;
    }
    all_generators[id]=NULL;
    for(auto lt:logical_topologies){
        delete lt.second;
    }
    logical_topologies.clear();
    if(scheduler_unit!=NULL)
        delete scheduler_unit;
    if(vLevels!=NULL)
        delete vLevels;
    if(memBus!=NULL)
        delete memBus;
    if(workload!=NULL)
        delete workload;
    bool shouldExit=true;
    for(auto &a:all_generators){
        if(a!=NULL){
            shouldExit=false;
            break;
        }
    }
    if(shouldExit){
        exitSimLoop("Exiting");
    }
}

//Sys::Sys(AstraNetworkAPI *NI,int id,int num_passes,std::string my_sys,std::string my_workload,int total_stat_rows,int stat_row,std::string path,std::string run_name)
Sys::Sys(AstraNetworkAPI *NI,AstraMemoryAPI *MEM,int id,int num_passes,int local_dim, int vertical_dim,int horizontal_dim,
    int perpendicular_dim,int fourth_dim,int local_queus,int vertical_queues,int horizontal_queues,
    int perpendicular_queues,int fourth_queues,std::string my_sys,
    std::string my_workload,float comm_scale, float compute_scale,float injection_scale,int total_stat_rows,int stat_row,
    std::string path,std::string run_name,bool seprate_log,bool rendezvous_enabled)
{
    scheduler_unit=NULL;
    vLevels=NULL;
    memBus=NULL;
    workload=NULL;
    this->initialized=false;

    start_sim_time=std::chrono::high_resolution_clock::now();
    this->NI=NI;
    this->MEM=MEM;
    this->id=id;
    this->method="baseline";
    this->finished_workloads=0;
    this->streams_finished=0;
    this->streams_injected=0;
    this->first_phase_streams=0;
    this->total_running_streams=0;
    this->priority_counter=0;

    this->local_dim=local_dim;
    this->vertical_dim=vertical_dim;
    this->horizontal_dim=horizontal_dim;
    this->perpendicular_dim=perpendicular_dim;
    this->fourth_dim=fourth_dim;

    this->local_queus=local_queus;
    this->vertical_queues=vertical_queues;
    this->horizontal_queues=horizontal_queues;
    this->perpendicular_queues=perpendicular_queues;
    this->fourth_queues=fourth_queues;
    this->comm_scale=comm_scale;
    this->compute_scale=compute_scale;
    this->injection_scale=injection_scale;
    this->inp_model_shared_bus=0;
    this->inp_boost_mode=0;
    this->processing_latency=10;
    this->communication_delay=10;
    this->local_reduction_delay=1;
    this->seprate_log=seprate_log;
    this->rendezvous_enabled=rendezvous_enabled;
    if((id+1)>all_generators.size()){
        all_generators.resize(id+1);
    }
    all_generators[id]=this;

    bool result=initialize_sys(my_sys+".txt");

    if(result==false){
        Tick cycle=1;
        try_register_event(this,EventType::NotInitialized,NULL,cycle);
        return;
    }

    if(id==0){
        std::cout<<"loc dim: "<<this->local_dim<<" ,vert dim: "<<this->vertical_dim<<" ,horiz dim: "<<horizontal_dim
        <<" ,perpend dim: "<<perpendicular_dim<<" ,fourth dim: "<<this->fourth_dim<<" ,queues: "<<local_queus<<", "<<vertical_queues
        <<", "<<horizontal_queues<<std::endl;
    }



    this->pending_events=0;

    int total_disabled=0;
    int ver=vertical_dim<1?1:vertical_dim;
    int perp=perpendicular_dim<1?1:perpendicular_dim;
    int loc=local_dim<1?1:local_dim;
    int hor=horizontal_dim<1?1:horizontal_dim;
    int four=fourth_dim<1?1:fourth_dim;
    total_nodes=hor*loc*perp*ver*four;


    int element;
    for (element=0;element<local_queus;element++) {
        bool enabled=!boost_mode;
        if(id<loc){
            enabled=true;
        }
        std::list<BaseStream*> temp;
        active_Streams[element]=temp;
        std::list<int> pri;
        stream_priorities[element]=pri;
        if(!enabled){
            total_disabled++;
        }
    }
    int pastElement=element;
    for (;element<vertical_queues+pastElement;element++) {
        bool enabled=!boost_mode;
        if(id%(loc*hor)==0 && id<(loc*hor*ver)){
            enabled=true;
        }
        std::list<BaseStream*> temp;
        active_Streams[element]=temp;
        std::list<int> pri;
        stream_priorities[element]=pri;
        if(!enabled){
            total_disabled++;
        }
    }
    pastElement=element;
    for (;element<horizontal_queues+pastElement;element++) {
        bool enabled=!boost_mode;
        if(id%loc==0 && id<(loc*hor)){
            enabled=true;
        }
        std::list<BaseStream*> temp;
        active_Streams[element]=temp;
        std::list<int> pri;
        stream_priorities[element]=pri;
        if(!enabled){
            total_disabled++;
        }
    }
    pastElement=element;
    for (;element<perpendicular_queues+pastElement;element++) {
        bool enabled=!boost_mode;
        if(id%(loc*hor*ver)==0 && id<(loc*hor*ver*perp)){
        enabled=true;
        }
        std::list<BaseStream*> temp;
        active_Streams[element]=temp;
        std::list<int> pri;
        stream_priorities[element]=pri;
        if(!enabled){
            total_disabled++;
        }
    }
    pastElement=element;
    for (;element<fourth_queues+pastElement;element++) {
        bool enabled=!boost_mode;
        if(id%(loc*hor*ver*perp)==0){
        enabled=true;
        }
        std::list<BaseStream*> temp;
        active_Streams[element]=temp;
        std::list<int> pri;
        stream_priorities[element]=pri;
        if(!enabled){
            total_disabled++;
        }

    }
    std::vector<int> levels{local_queus,vertical_queues,horizontal_queues,perpendicular_queues,fourth_queues};
    int total_levels=0;
    for(auto l:levels){
        total_levels+=l;
    }
    if(total_levels==total_disabled){
        NI->enabled= false;
        std::cout<<"Node "<<id<<" has been totally disabled"<<std::endl;
    }
    int concurrent_streams=1;
    int active_first_phase=64;
    if(injection_policy==InjectionPolicy::SemiAggressive){
        concurrent_streams=16;
    }
    else if(injection_policy==InjectionPolicy::Aggressive){
        concurrent_streams=32;
    }
    else if(injection_policy==InjectionPolicy::ExtraAggressive){
        concurrent_streams=64;
        active_first_phase=128;
    }
    else if(injection_policy==InjectionPolicy::Infinite){
        active_first_phase=1000000;
        concurrent_streams=1000000;
    }
    int max_running;
    if(local_dim==1){
        max_running=horizontal_queues*concurrent_streams;
    }
    else{
        max_running=(local_queus+horizontal_queues)*concurrent_streams;
    }
    max_running=100000000;
    scheduler_unit=new SchedulerUnit(this,levels,max_running,active_first_phase,concurrent_streams);
    vLevels=new QueueLevels(levels,0);
    logical_topologies["DBT"]=new DoubleBinaryTreeTopology(id,hor,id%loc,loc,loc);          //horizontal_dim,id%local_dim,local_dim);
    logical_topologies["Torus3D"]=new Torus3D(id,total_nodes,loc,ver);
    logical_topologies["LocalRingNodeA2AGlobalDBT"]=new LocalRingNodeA2AGlobalDBT(id,ver,id%(hor*loc),hor*loc,1);
    stream_counter=0;
    all_queues=local_queus+vertical_queues+horizontal_queues+perpendicular_queues+fourth_queues;
    enabled=true;

    if(id==0){
        std::atexit(exiting);
        std::cout<<"total nodes: "<<total_nodes<<std::endl;
        std::cout<<"local dim: "<<local_dim<<" , vertical dim: "<<vertical_dim<<" , horizontal dim: "<<horizontal_dim<<
        " , perpendicular dim: "<<perpendicular_dim<<" , fourth dim: "<<fourth_dim<<std::endl;
    }
    //NI->sim_init(); CHANGED BY PALLAVI**
    NI->sim_init(MEM);
    memBus=new MemBus("NPU","MA",this,inp_L,inp_o,inp_g,inp_G,model_shared_bus,communication_delay,true);
    workload=new Workload(run_name,this,my_workload+".txt",num_passes,total_stat_rows,stat_row,path,this->seprate_log);
    if(workload->initialized==false){
        Tick cycle=1;
        try_register_event(this,EventType::NotInitialized,NULL,cycle);
        return;
    }
    this->initialized=true;
}
int Sys::get_layer_numbers(std::string workload_input) {
    return Workload::get_layer_numbers(workload_input);
}
int Sys::get_priority(SchedulingPolicy pref_scheduling) {
    if(pref_scheduling==SchedulingPolicy::None){
        if(scheduling_policy==SchedulingPolicy::LIFO){
            return priority_counter++;
        }
        else{
            return priority_counter--;
        }
    }
    else if(pref_scheduling==SchedulingPolicy::HIGHEST){
        return 100000000;
    }
    else{
        if(scheduling_policy==SchedulingPolicy::LIFO){
            return priority_counter++;
        }
        else{
            return priority_counter--;
        }
    }
}
int Sys::rendezvous_sim_send(Tick delay, void *buffer, int count, int type, int dst,
                             int tag, sim_request *request,
                             void (*msg_handler)(void *fun_arg), void *fun_arg) {
    RendezvousSendData *rsd=new RendezvousSendData(id,this,buffer,count,type,dst,
                       tag,*request,msg_handler,fun_arg);
    sim_request newReq=*request;
    uint64_t rendevouz_size=8192;
    newReq.dstRank=request->srcRank;
    newReq.srcRank=request->dstRank;
    newReq.reqCount=rendevouz_size;
    int newTag=tag+500000000;
    newReq.tag=newTag;
    sim_recv(delay,buffer,rendevouz_size,type,dst,newTag,&newReq,&Sys::handleEvent,rsd);
    return 1;
}
int Sys::sim_send(Tick delay, void *buffer, int count, int type, int dst, int tag, sim_request *request,
                  void (*msg_handler)(void *fun_arg), void *fun_arg) {
    if(delay==0){
        NI->sim_send(buffer,count,type,dst,tag,request,msg_handler,fun_arg);
    }
    else{
        try_register_event(new SimSendCaller(this,buffer,count,type,dst,tag,*request,msg_handler,fun_arg),
                           EventType::General,NULL,delay);
    }
    return 1;
}
int Sys::front_end_sim_send(Tick delay, void *buffer, int count, int type, int dst, int tag, sim_request *request,
                            void (*msg_handler)(void *fun_arg), void *fun_arg) {
    if(rendezvous_enabled){
        return rendezvous_sim_send(delay,buffer,count,type,dst,tag,request,msg_handler,fun_arg);
    }
    else{
        return sim_send(delay,buffer,count,type,dst,tag,request,msg_handler,fun_arg);
    }
}
int Sys::rendezvous_sim_recv(Tick delay, void *buffer, int count, int type, int src,
                             int tag, sim_request *request,
                             void (*msg_handler)(void *fun_arg), void *fun_arg) {
    RendezvousRecvData *rrd=new RendezvousRecvData(id,this,buffer,count,type,src,
                                                   tag,*request,msg_handler,fun_arg);
    sim_request newReq=*request;
    uint64_t rendevouz_size=8192;
    newReq.dstRank=request->srcRank;
    newReq.srcRank=request->dstRank;
    newReq.reqCount=rendevouz_size;
    int newTag=tag+500000000;
    newReq.tag=newTag;
    sim_send(delay,buffer,rendevouz_size,type,src,newTag,&newReq,&Sys::handleEvent,rrd);
    return 1;
}
int Sys::sim_recv(Tick delay, void *buffer, int count, int type, int src, int tag, sim_request *request,
                  void (*msg_handler)(void *fun_arg), void *fun_arg) {
    if(delay==0) {
        NI->sim_recv(buffer,count,type,src,tag,request,msg_handler,fun_arg);
    }
    else{
        try_register_event(new SimRecvCaller(this, buffer, count, type, src, tag, *request, msg_handler, fun_arg),
                           EventType::General, NULL, delay);
    }
    return 1;
}
int Sys::front_end_sim_recv(Tick delay, void *buffer, int count, int type, int src, int tag,
                            sim_request *request, void (*msg_handler)(void *fun_arg), void *fun_arg) {
    if(rendezvous_enabled){
        return rendezvous_sim_recv(delay,buffer,count,type,src,tag,request,msg_handler,fun_arg);
    }
    else{
        return sim_recv(delay,buffer,count,type,src,tag,request,msg_handler,fun_arg);
    }
}
Tick Sys::mem_read(uint64_t bytes) {
    if(MEM==NULL){
        return 10;
    }
    uint64_t delay_ns=MEM->npu_mem_read(bytes);
    Tick delay_cycles=delay_ns/CLOCK_PERIOD;
    return delay_cycles;
}
Tick Sys::mem_write(uint64_t bytes) {
    if(MEM==NULL){
        return 10;
    }
    uint64_t delay_ns=MEM->npu_mem_write(bytes);
    Tick delay_cycles=delay_ns/CLOCK_PERIOD;
    return delay_cycles;
}
std::string Sys::trim(const std::string& str,
                 const std::string& whitespace = " \t")
{
    const auto strBegin = str.find_first_not_of(whitespace);
    if (strBegin == std::string::npos)
        return ""; // no content

    const auto strEnd = str.find_last_not_of(whitespace);
    const auto strRange = strEnd - strBegin + 1;

    return str.substr(strBegin, strRange);
}
bool Sys::parse_var(std::string var, std::string value) {
    var=trim(var);
    value=trim(value);
    if(id==0){
        std::cout<<"Var is: "<<var<<" ,val is: "<<value<<std::endl;
    }
    if(var=="scheduling-policy:"){
        inp_scheduling_policy=value;
    }
    else if(var=="collective-implementation:"){
        std::stringstream mval(value);
        mval >>inp_collective_implementation;
    }
    else if(var=="collective-optimization:"){
        std::stringstream mval(value);
        mval >>inp_collective_optimization;
    }
    else if(var=="endpoint-delay:"){
        std::stringstream mval(value);
        mval >>communication_delay;
        communication_delay=communication_delay*injection_scale;
    }
    else if(var=="local-reduction-delay:"){
        std::stringstream mval(value);
        mval >>local_reduction_delay;
    }
    else if(var=="packet-routing:"){
        inp_packet_routing=value;
    }
    else if(var=="injection-policy:"){
        inp_injection_policy=value;
    }
    else if(var=="L:"){
        std::stringstream mval(value);
        mval >>inp_L;
    }
    else if(var=="o:"){
        std::stringstream mval(value);
        mval >>inp_o;
    }
    else if(var=="g:"){
        std::stringstream mval(value);
        mval >>inp_g;
    }
    else if(var=="G:"){
        std::stringstream mval(value);
        mval >>inp_G;
    }
    else if(var=="model-shared-bus:"){
        std::stringstream mval(value);
        mval >>inp_model_shared_bus;
    }
    else if(var=="preferred-dataset-splits:"){
        std::stringstream mval(value);
        mval >>preferred_dataset_splits;
    }
    else if(var=="boost-mode:"){
        std::stringstream mval(value);
        mval >>inp_boost_mode;
    }
    else if(var=="seprate-log:"){
        std::stringstream mval(value);
        int int_to_bool;
        mval>>int_to_bool;
        if(int_to_bool==0){
            this->seprate_log=false;
        }
        else{
            this->seprate_log=true;
        }
    }
    else if(var!=""){
        if(id==0){
            std::cout<<"######### Exiting because "<<var<<" is undefined inside system input file #########"<<std::endl;
        }
        return false;
    }
    return true;
}
bool Sys::post_process_inputs() {
    if(inp_packet_routing=="hardware"){
        alltoall_routing=PacketRouting::Hardware;
    } else{
        alltoall_routing=PacketRouting ::Software;
    }
    if(inp_injection_policy =="semiAggressive"){
        injection_policy=InjectionPolicy ::SemiAggressive;
    }
    else if(inp_injection_policy =="aggressive"){
        injection_policy=InjectionPolicy ::Aggressive;
    }
    else if(inp_injection_policy =="extraAggressive"){
        injection_policy=InjectionPolicy::ExtraAggressive;
    }
    else if(inp_injection_policy =="infinite"){
        injection_policy=InjectionPolicy ::Infinite;
    }
    else{
        injection_policy=InjectionPolicy ::Normal;
    }
    if(inp_collective_implementation=="hierarchicalRing"){
        collectiveImplementation=CollectiveImplementation::HierarchicalRing;
    }
    else if(inp_collective_implementation=="allToAll"){
        collectiveImplementation=CollectiveImplementation::AllToAll;
    }
    else if(inp_collective_implementation=="doubleBinaryTree"){
        collectiveImplementation=CollectiveImplementation::DoubleBinaryTree;
    }
    else if(inp_collective_implementation=="doubleBinaryTreeLocalAllToAll"){
        collectiveImplementation=CollectiveImplementation::DoubleBinaryTreeLocalAllToAll;
    }
    else if(inp_collective_implementation=="localRingNodeA2AGlobalDBT"){
        collectiveImplementation=CollectiveImplementation::LocalRingNodeA2AGlobalDBT;
    }
    if(inp_collective_optimization=="baseline"){
        collectiveOptimization=CollectiveOptimization::Baseline;
    }
    else if(inp_collective_optimization=="localBWAware"){
        collectiveOptimization=CollectiveOptimization::LocalBWAware;
    }

    if(inp_boost_mode==1){
        boost_mode=true;
    }
    else{
        boost_mode= false;
    }
    if(inp_scheduling_policy=="LIFO"){
        this->scheduling_policy=SchedulingPolicy::LIFO;
    }
    else{
        this->scheduling_policy=SchedulingPolicy::FIFO;
    }
    if(inp_model_shared_bus==1){
        model_shared_bus=true;
    }
    else{
        model_shared_bus=false;
    }
    return true;
}
bool Sys::initialize_sys(std::string name) {
    std::ifstream inFile;
    inFile.open(name);
    if (!inFile) {
        if(id==0){
            std::cout << "Unable to open file: " << name << std::endl;
            std::cout <<"############ Exiting because unable to open the system input file ############"<<std::endl;
        }
        return false;
    } else {
        if(id==0){
            std::cout << "success in openning system file" << std::endl;
        }
    }
    std::string var;
    std::string value;
    while(inFile.peek()!=EOF){
        var="";
        inFile >> var;
        if(inFile.peek()!=EOF){
            inFile >> value;
        }
        bool result=parse_var(var,value);
        if(result==false){
            inFile.close();
            return result;
        }
    }
    inFile.close();
    post_process_inputs();
    return true;
}
Sys::SchedulerUnit::SchedulerUnit(Sys *sys, std::vector<int> queues,int max_running_streams,int ready_list_threshold,int queue_threshold) {
    this->sys=sys;
    int base=0;
    this->ready_list_threshold=ready_list_threshold;
    this->queue_threshold=queue_threshold;
    this->max_running_streams=max_running_streams;
    for(auto q:queues){
        for(int i=0;i<q;i++){
            this->running_streams[base]=0;
            std::list<BaseStream*>::iterator it;
            this->stream_pointer[base]=it;
            base++;
        }
    }
}
void Sys::SchedulerUnit::notify_stream_added_into_ready_list() {
    if(this->sys->first_phase_streams<ready_list_threshold && this->sys->total_running_streams<max_running_streams){
        int max=ready_list_threshold-sys->first_phase_streams;
        if(max>max_running_streams-this->sys->total_running_streams){
            max=max_running_streams-this->sys->total_running_streams;
        }
        //sys->ask_for_schedule(max);
        sys->schedule(max);
    }
    return;
}
void Sys::SchedulerUnit::notify_stream_added(int vnet) {
    /*if(running_streams[vnet]==0 || (sys->active_Streams[vnet]).size()==1){
        this->stream_pointer[vnet]=sys->active_Streams[vnet].begin();
    }
    else if(this->stream_pointer[vnet]==sys->active_Streams[vnet].end()){
        this->stream_pointer[vnet]=sys->active_Streams[vnet].end();
        --(this->stream_pointer[vnet]);
    }*/
    //temp
    stream_pointer[vnet]=sys->active_Streams[vnet].begin();
    std::advance(stream_pointer[vnet],running_streams[vnet]);
    while(stream_pointer[vnet]!=sys->active_Streams[vnet].end() && running_streams[vnet]<queue_threshold){
        (*stream_pointer[vnet])->init();
        running_streams[vnet]++;
        std::advance(stream_pointer[vnet],1);
    }
}
void Sys::SchedulerUnit::notify_stream_removed(int vnet) {
    //std::cout<<"hello1, vnet: "<<vnet<<std::endl;
    /*if((sys->active_Streams[vnet]).size()==0){
        this->stream_pointer[vnet]=sys->active_Streams[vnet].end();
    }*/
    running_streams[vnet]--;
    if(this->sys->first_phase_streams<ready_list_threshold && this->sys->total_running_streams<max_running_streams){
        int max=ready_list_threshold-sys->first_phase_streams;
        if(max>max_running_streams-this->sys->total_running_streams){
            max=max_running_streams-this->sys->total_running_streams;
        }
        //sys->ask_for_schedule(max);
        sys->schedule(max);
    }
    //tmp
    stream_pointer[vnet]=sys->active_Streams[vnet].begin();
    std::advance(stream_pointer[vnet],running_streams[vnet]);
    while(stream_pointer[vnet]!=sys->active_Streams[vnet].end() && running_streams[vnet]<queue_threshold){
        (*stream_pointer[vnet])->init();
        running_streams[vnet]++;
        std::advance(stream_pointer[vnet],1);
    }
}
int Sys::nextPowerOf2(int n) {
    int count = 0;
    // First n in the below condition
    // is for the case where n is 0
    if (n && !(n & (n - 1)))
        return n;
    while( n != 0)
    {
        n >>= 1;
        count += 1;
    }
    return 1 << count;

}
void Sys::sys_panic(std::string msg) {
    std::cout<<msg<<std::endl;
}
void Sys::iterate(){
    call_events();
}
int Sys::determine_chunk_size(int size,ComType type) {
    int chunk_size=size/preferred_dataset_splits;
    return chunk_size;

}
DataSet * Sys::generate_all_reduce(int size,bool local, bool vertical, bool horizontal,
                              SchedulingPolicy pref_scheduling,int layer){

    if(collectiveImplementation==CollectiveImplementation::AllToAll){
        return generate_alltoall_all_reduce(size,local,horizontal,pref_scheduling,layer);
    }
    else if(collectiveImplementation==CollectiveImplementation::DoubleBinaryTree ||
            collectiveImplementation==CollectiveImplementation::DoubleBinaryTreeLocalAllToAll){
        return generate_tree_all_reduce(size,local,horizontal,pref_scheduling,layer);
    } else if(collectiveImplementation==CollectiveImplementation::LocalRingNodeA2AGlobalDBT){
        return generate_LocalRingNodeA2AGlobalDBT_all_reduce(size,local,vertical,horizontal,pref_scheduling,layer);
    }
    else{
        return generate_hierarchical_all_reduce(size,local,vertical,horizontal,pref_scheduling,layer);
    }
}
DataSet * Sys::generate_all_gather(int size,bool local, bool vertical, bool horizontal,
                                   SchedulingPolicy pref_scheduling,int layer){
    return generate_hierarchical_all_gather(size,local,vertical,horizontal,pref_scheduling,layer);

}
DataSet * Sys::generate_reduce_scatter(int size,bool local, bool vertical, bool horizontal,
                                   SchedulingPolicy pref_scheduling,int layer){
    return generate_hierarchical_reduce_scatter(size,local,vertical,horizontal,pref_scheduling,layer);
}
DataSet * Sys::generate_all_to_all(int size,bool local, bool vertical, bool horizontal,
                              SchedulingPolicy pref_scheduling,int layer){
    if(collectiveImplementation==CollectiveImplementation::AllToAll ||
    collectiveImplementation==CollectiveImplementation::DoubleBinaryTree ||
    collectiveImplementation==CollectiveImplementation::DoubleBinaryTreeLocalAllToAll){
        return generate_alltoall_all_to_all(size,local,horizontal,pref_scheduling,layer);
    }
    else{
        return generate_hierarchical_all_to_all(size,local,vertical,horizontal,pref_scheduling,layer);
    }
}
DataSet* Sys::generate_alltoall_all_to_all(int size,bool local_run, bool horizontal_run,
        SchedulingPolicy pref_scheduling,int layer){
    int chunk_size=determine_chunk_size(size,ComType::All_to_All);
    int streams=ceil(((double)size)/chunk_size);
    int tmp;
    DataSet *dataset=new DataSet(streams);
    streams_injected+=streams;
    int pri=get_priority(pref_scheduling);
    for(int i=0;i<streams;i++){
        tmp=chunk_size;
        std::list<CollectivePhase> vect;
        std::pair<int,RingTopology::Direction> local_last;
        if(collectiveOptimization==CollectiveOptimization::LocalBWAware){
            local_last=vLevels->get_next_queue_at_level(0);
        }
        else{
            local_last=vLevels->get_next_queue_at_level(0);
        }
        std::pair<int,RingTopology::Direction> horizontal=vLevels->get_next_queue_at_level(2);
        std::map<int,CollectivePhase*>::iterator  it;
        if(id==0){
            //std::cout<<"initial chunk: "<<tmp<<std::endl;
        }
        if(local_dim>1 && local_run){
            LogicalTopology* A2A=logical_topologies["Torus3D"]->get_topology();
            PacketRouting pRouting=alltoall_routing;
            if(collectiveImplementation==CollectiveImplementation::DoubleBinaryTreeLocalAllToAll){
                pRouting=PacketRouting::Hardware;
            }
            CollectivePhase vn(this,local_last.first,new AllToAll(ComType::All_to_All,id,layer,
                                                                  ((Torus3D*)A2A)->local_dimension,tmp,
                                                                  local_last.second,
                                                                  pRouting,InjectionPolicy::Normal,boost_mode));
            vect.push_back(vn);
            tmp=vn.final_data_size;
            if(id==0){
                //std::cout<<"tmp after phase 1: "<<tmp<<std::endl;
            }
        }
        if(method=="baseline" && horizontal_dim>1 && horizontal_run){
            LogicalTopology* A2A=logical_topologies["Torus3D"]->get_topology();
            CollectivePhase vn(this,horizontal.first,new AllToAll(ComType::All_to_All,id,layer,
                                                                  ((Torus3D*)A2A)->horizontal_dimension,tmp,
                                                                  horizontal.second,PacketRouting::Hardware,
                                                                  InjectionPolicy::Normal,boost_mode));
            vect.push_back(vn);
            tmp=vn.final_data_size;
            if(id==0){
                //std::cout<<"tmp after phase 2: "<<tmp<<std::endl;
            }
        }
        if(method=="baseline"){
            StreamBaseline *newStream=new StreamBaseline(this,dataset,stream_counter++,vect,pri);
            newStream->current_queue_id=-1;
            insert_into_ready_list(newStream);
        }
    }
    return dataset;
}
DataSet* Sys::generate_alltoall_all_reduce(int size,bool local_run, bool horizontal_run,
        SchedulingPolicy pref_scheduling,int layer){
    int chunk_size=determine_chunk_size(size,ComType::All_Reduce);
    int streams=ceil(((double)size)/chunk_size);
    int tmp;
    DataSet *dataset=new DataSet(streams);
    streams_injected+=streams;
    int pri=get_priority(pref_scheduling);
    for(int i=0;i<streams;i++){
        tmp=chunk_size;
        std::list<CollectivePhase> vect;
        std::pair<int,RingTopology::Direction> local_first;
        if(collectiveOptimization==CollectiveOptimization::Baseline){
            local_first=vLevels->get_next_queue_at_level(0);
        }
        else{
            local_first=vLevels->get_next_queue_at_level(0);
        }
        std::pair<int,RingTopology::Direction> local_last=vLevels->get_next_queue_at_level(0);
        std::pair<int,RingTopology::Direction> horizontal=vLevels->get_next_queue_at_level(2);
        if(id==0){
            //std::cout<<"initial chunk: "<<tmp<<std::endl;
        }
        if(method=="baseline"){
            if(local_dim>1 && local_run){
                if(collectiveOptimization==CollectiveOptimization::Baseline){
                    LogicalTopology* A2A=logical_topologies["Torus3D"]->get_topology();
                    CollectivePhase vn(this,local_first.first,new AllToAll(ComType::All_Reduce,id,
                                                                           layer,((Torus3D*)A2A)->local_dimension,tmp,
                                                                           local_first.second,alltoall_routing,
                                                                           InjectionPolicy::Normal,boost_mode));
                    vect.push_back(vn);
                    tmp=vn.final_data_size;
                }
                else{
                    LogicalTopology* A2A=logical_topologies["Torus3D"]->get_topology();
                    CollectivePhase vn(this,local_first.first,new AllToAll(ComType::Reduce_Scatter,id,
                                                                           layer,((Torus3D*)A2A)->local_dimension,tmp,
                                                                           local_first.second,alltoall_routing,
                                                                           InjectionPolicy::Normal,boost_mode));
                    vect.push_back(vn);
                    tmp=vn.final_data_size;
                }
                if(id==0){
                    //std::cout<<"tmp after phase 1: "<<tmp<<std::endl;
                }
            }
            if(horizontal_dim>1 && horizontal_run){
                LogicalTopology* A2A=logical_topologies["Torus3D"]->get_topology();
                CollectivePhase vn(this,horizontal.first,new AllToAll(ComType::All_Reduce,
                                                                      id,layer,((Torus3D*)A2A)->horizontal_dimension,
                                                                      tmp,horizontal.second,PacketRouting::Hardware,
                                                                      InjectionPolicy::Normal,boost_mode));
                vect.push_back(vn);
                tmp=vn.final_data_size;
            }
            if(local_dim>1 && local_run && collectiveOptimization==CollectiveOptimization::LocalBWAware){
                LogicalTopology* A2A=logical_topologies["Torus3D"]->get_topology();
                CollectivePhase vn(this,local_last.first,new AllToAll(ComType::All_Gatehr,id,layer,
                                                                      ((Torus3D*)A2A)->local_dimension,tmp,local_last.second,
                                                                      alltoall_routing,InjectionPolicy::Normal,boost_mode));
                vect.push_back(vn);
                tmp=vn.final_data_size;
                if(id==0){
                    //std::cout<<"tmp after phase 4: "<<tmp<<std::endl;
                }
            }
        }
        if(method=="baseline"){
            //std::cout<<"vect size is: "<<vect.size()<<" horizontal_dim: "<<horizontal_dim<<std::endl;
            StreamBaseline *newStream=new StreamBaseline(this,dataset,stream_counter++,vect,pri);
            newStream->current_queue_id=-1;
            insert_into_ready_list(newStream);
        }
    }
    return dataset;
}
DataSet* Sys::generate_tree_all_reduce(int size,bool local_run,bool horizontal_run,
        SchedulingPolicy pref_scheduling,int layer){
    int chunk_size=determine_chunk_size(size,ComType::All_Reduce);
    int streams=ceil(((double)size)/chunk_size);
    int tmp;
    DataSet *dataset=new DataSet(streams);
    streams_injected+=streams;
    int pri=get_priority(pref_scheduling);
    for(int i=0;i<streams;i++){
        tmp=chunk_size;
        std::list<CollectivePhase> vect;
        LogicalTopology* bt=logical_topologies["DBT"]->get_topology();
        if(id==0){
            //std::cout<<"initial chunk: "<<tmp<<std::endl;
        }
        if(local_dim>1 && local_run){
            std::pair<int,RingTopology::Direction> local_first;
            if(collectiveOptimization==CollectiveOptimization::Baseline){
                local_first=vLevels->get_next_queue_at_level(0);
                if(collectiveImplementation==CollectiveImplementation::DoubleBinaryTree){
                    CollectivePhase vn(this,local_first.first,new Ring(ComType::All_Reduce,id,layer,((LocalRingGlobalBinaryTree*)bt)->local_dimension,tmp,local_first.second,PacketRouting::Software,InjectionPolicy::Normal,boost_mode));
                    vect.push_back(vn);
                    tmp=vn.final_data_size;
                }
                else if(collectiveImplementation==CollectiveImplementation::DoubleBinaryTreeLocalAllToAll){
                    CollectivePhase vn(this,local_first.first,new AllToAll(ComType::All_Reduce,id,layer,((LocalRingGlobalBinaryTree*)bt)->local_dimension,tmp,local_first.second,PacketRouting::Hardware,InjectionPolicy::Normal,boost_mode));
                    vect.push_back(vn);
                    tmp=vn.final_data_size;
                }
            }
            else if(collectiveOptimization==CollectiveOptimization::LocalBWAware){
                local_first=vLevels->get_next_queue_at_level(0);
                if(collectiveImplementation==CollectiveImplementation::DoubleBinaryTree){
                    CollectivePhase vn(this,local_first.first,new Ring(ComType::Reduce_Scatter,id,layer,((LocalRingGlobalBinaryTree*)bt)->local_dimension,tmp,local_first.second,PacketRouting::Software,InjectionPolicy::Normal,boost_mode));
                    vect.push_back(vn);
                    tmp=vn.final_data_size;
                }
                else if(collectiveImplementation==CollectiveImplementation::DoubleBinaryTreeLocalAllToAll){
                    CollectivePhase vn(this,local_first.first,new AllToAll(ComType::Reduce_Scatter,id,layer,((LocalRingGlobalBinaryTree*)bt)->local_dimension,tmp,local_first.second,PacketRouting::Hardware,InjectionPolicy::Normal,boost_mode));
                    vect.push_back(vn);
                    tmp=vn.final_data_size;
                }
            }
        }
        if(id==0){
            //std::cout<<"tmp after phase 1: "<<tmp<<std::endl;
        }
        if(horizontal_dim>1 && horizontal_run){
            std::pair<int,RingTopology::Direction> tree_queue_id=vLevels->get_next_queue_at_level(2);
            CollectivePhase vn(this, tree_queue_id.first,new DoubleBinaryTreeAllReduce(id, layer, ((LocalRingGlobalBinaryTree*)bt)->global_dimension,
                                                                                       tmp, boost_mode));
            vect.push_back(vn);
            tmp=vn.final_data_size;
        }
        if(local_dim>1 && local_run){
            std::pair<int,RingTopology::Direction> local_last;
            if(collectiveOptimization==CollectiveOptimization::LocalBWAware){
                local_last=vLevels->get_next_queue_at_level(0);
                if(collectiveImplementation==CollectiveImplementation::DoubleBinaryTree){
                    CollectivePhase vn(this,local_last.first,new Ring(ComType::All_Gatehr,id,layer,((LocalRingGlobalBinaryTree*)bt)->local_dimension,tmp,local_last.second,
                                                                      PacketRouting::Software,InjectionPolicy::Normal,boost_mode));
                    vect.push_back(vn);
                    tmp=vn.final_data_size;
                }
                else if(collectiveImplementation==CollectiveImplementation::DoubleBinaryTreeLocalAllToAll){
                    CollectivePhase vn(this,local_last.first,new AllToAll(ComType::All_Gatehr,id,layer,((LocalRingGlobalBinaryTree*)bt)->local_dimension,tmp,
                                                                          local_last.second,PacketRouting::Hardware,InjectionPolicy::Normal,boost_mode));
                    vect.push_back(vn);
                    tmp=vn.final_data_size;
                }

            }
        }
        if(id==0){
            //std::cout<<"tmp after phase 2: "<<tmp<<std::endl;
        }
        if(method=="baseline"){
            StreamBaseline *newStream=new StreamBaseline(this,dataset,stream_counter++,vect,pri);
            newStream->current_queue_id=-1;
            insert_into_ready_list(newStream);
        }
    }
    return dataset;
}

DataSet* Sys::generate_hierarchical_all_to_all(int size,bool local_run,bool vertical_run, bool horizontal_run,
                                           SchedulingPolicy pref_scheduling,int layer){

    int chunk_size=determine_chunk_size(size,ComType::All_to_All);
    int streams=ceil(((double)size)/chunk_size);
    int tmp;
    DataSet *dataset=new DataSet(streams);
    streams_injected+=streams;
    int pri=get_priority(pref_scheduling);
    for(int i=0;i<streams;i++){
        tmp=chunk_size;

        std::list<CollectivePhase> vect;
        std::list<ComType> types;
        std::pair<int,RingTopology::Direction> local_last;
        if(collectiveOptimization==CollectiveOptimization::LocalBWAware){
            local_last=vLevels->get_next_queue_at_level(0);
        }
        else{
            local_last=vLevels->get_next_queue_at_level(0);
        }
        std::pair<int,RingTopology::Direction> vertical=vLevels->get_next_queue_at_level(1);
        std::pair<int,RingTopology::Direction> horizontal=vLevels->get_next_queue_at_level(2);
        if(id==0){
            //std::cout<<"initial chunk: "<<tmp<<std::endl;
        }
        if(local_dim>1 && local_run){
            LogicalTopology* torus=logical_topologies["Torus3D"]->get_topology();
            CollectivePhase vn(this,local_last.first,new Ring(ComType::All_to_All,id,layer,((Torus3D*)torus)->local_dimension,tmp,local_last.second,alltoall_routing,InjectionPolicy::Normal,boost_mode));
            vect.push_back(vn);
            tmp=vn.final_data_size;
        }
        if(id==0){
            //std::cout<<"tmp after phase 1: "<<tmp<<std::endl;
        }
        if(vertical_dim>1 && vertical_run){
            LogicalTopology* torus=logical_topologies["Torus3D"]->get_topology();
            CollectivePhase vn(this,vertical.first,new Ring(ComType::All_to_All,id,layer,((Torus3D*)torus)->vertical_dimension,tmp,vertical.second,alltoall_routing,InjectionPolicy::Normal,boost_mode));
            vect.push_back(vn);
            tmp=vn.final_data_size;
        }
        if(id==0){
            //std::cout<<"tmp after phase 2: "<<tmp<<std::endl;
        }
        if(horizontal_dim>1 && horizontal_run){
            LogicalTopology* torus=logical_topologies["Torus3D"]->get_topology();
            CollectivePhase vn(this,horizontal.first,new Ring(ComType::All_to_All,id,layer,((Torus3D*)torus)->horizontal_dimension,tmp,horizontal.second,alltoall_routing,InjectionPolicy::Normal,boost_mode));
            vect.push_back(vn);
            tmp=vn.final_data_size;
        }
        if(id==0){
            //std::cout<<"tmp after phase 3: "<<tmp<<std::endl;
        }
        if(method=="baseline"){
            StreamBaseline *newStream=new StreamBaseline(this,dataset,stream_counter++,vect,pri);
            newStream->current_queue_id=-1;
            insert_into_ready_list(newStream);
            //proceed_to_next_vnet_baseline(newStream);
        }
    }
    return dataset;
}
DataSet* Sys::generate_hierarchical_all_reduce(int size,bool local_run, bool vertical_run, bool horizontal_run,
                                                   SchedulingPolicy pref_scheduling,int layer){
        int chunk_size=determine_chunk_size(size,ComType::All_Reduce);
        int streams=ceil(((double)size)/chunk_size);
        int tmp;
        DataSet *dataset=new DataSet(streams);
        streams_injected+=streams;
        int pri=get_priority(pref_scheduling);
        for(int i=0;i<streams;i++){
            tmp=chunk_size;

            std::list<CollectivePhase> vect;
            std::list<ComType> types;
            std::pair<int,RingTopology::Direction> local_first;
            if(collectiveOptimization==CollectiveOptimization::Baseline){
                local_first=vLevels->get_next_queue_at_level(0);
            }
            else{
                local_first=vLevels->get_next_queue_at_level(0);
            }
            std::pair<int,RingTopology::Direction> local_last=vLevels->get_next_queue_at_level(0);
            std::pair<int,RingTopology::Direction> vertical=vLevels->get_next_queue_at_level(1);
            std::pair<int,RingTopology::Direction> horizontal=vLevels->get_next_queue_at_level(2);

            if(id==0){
                //std::cout<<"initial chunk: "<<tmp<<std::endl;
            }

            if(method=="baseline"){
                if(local_dim>1 && local_run){
                    if(collectiveOptimization==CollectiveOptimization::Baseline){
                        LogicalTopology* torus=logical_topologies["Torus3D"]->get_topology();
                        CollectivePhase vn(this,local_first.first,new Ring(ComType::All_Reduce,id,layer,((Torus3D*)torus)->local_dimension,tmp,local_first.second,PacketRouting::Software,InjectionPolicy::Normal,boost_mode));
                        vect.push_back(vn);
                        tmp=vn.final_data_size;
                    }
                    else{
                        LogicalTopology* torus=logical_topologies["Torus3D"]->get_topology();
                        CollectivePhase vn(this,local_first.first,new Ring(ComType::Reduce_Scatter,id,layer,((Torus3D*)torus)->local_dimension,tmp,local_first.second,PacketRouting::Software,InjectionPolicy::Normal,boost_mode));
                        vect.push_back(vn);
                        tmp=vn.final_data_size;
                    }
                }
                if(id==0){
                    //std::cout<<"tmp after phase 1: "<<tmp<<std::endl;
                }
                //comment should be removed
                if(vertical_dim>1 && vertical_run){
                    LogicalTopology* torus=logical_topologies["Torus3D"]->get_topology();
                    CollectivePhase vn(this,vertical.first,new Ring(ComType::All_Reduce,id,layer,((Torus3D*)torus)->vertical_dimension,tmp,vertical.second,PacketRouting::Software,InjectionPolicy::Normal,boost_mode));
                    vect.push_back(vn);
                    tmp=vn.final_data_size;
                }
                if(id==0){
                    //std::cout<<"tmp after phase 2: "<<tmp<<std::endl;
                }
                if(horizontal_dim>1 && horizontal_run){
                    LogicalTopology* torus=logical_topologies["Torus3D"]->get_topology();
                    CollectivePhase vn(this,horizontal.first,new Ring(ComType::All_Reduce,id,layer,((Torus3D*)torus)->horizontal_dimension,tmp,horizontal.second,PacketRouting::Software,InjectionPolicy::Normal,boost_mode));
                    vect.push_back(vn);
                    tmp=vn.final_data_size;
                }
                if(id==0){
                    //std::cout<<"tmp after phase 3: "<<tmp<<std::endl;
                }
                if(local_dim>1 && local_run && collectiveOptimization==CollectiveOptimization::LocalBWAware){
                    LogicalTopology* torus=logical_topologies["Torus3D"]->get_topology();
                    CollectivePhase vn(this,local_last.first,new Ring(ComType::All_Gatehr,id,layer,((Torus3D*)torus)->local_dimension,tmp,local_last.second,PacketRouting::Software,InjectionPolicy::Normal,boost_mode));
                    vect.push_back(vn);
                    tmp=vn.final_data_size;
                }
                if(id==0){
                    //std::cout<<"tmp after phase 4: "<<tmp<<std::endl;
                }
            }
            if(method=="baseline"){
                StreamBaseline *newStream=new StreamBaseline(this,dataset,stream_counter++,vect,pri);
                newStream->current_queue_id=-1;
                insert_into_ready_list(newStream);
            }

        }
        return dataset;
}
DataSet* Sys::generate_hierarchical_all_gather(int size,bool local_run, bool vertical_run, bool horizontal_run,
                                                SchedulingPolicy pref_scheduling,int layer){
    int chunk_size=determine_chunk_size(size,ComType::All_Gatehr);
    int streams=ceil(((double)size)/chunk_size);
    int tmp;
    DataSet *dataset=new DataSet(streams);
    streams_injected+=streams;
    int pri=get_priority(pref_scheduling);
    for(int i=0;i<streams;i++){
        tmp=chunk_size;

        std::list<CollectivePhase> vect;
        std::list<ComType> types;

        std::pair<int,RingTopology::Direction> local_first;
        local_first=vLevels->get_next_queue_at_level(0);
        std::pair<int,RingTopology::Direction> vertical=vLevels->get_next_queue_at_level(1);
        std::pair<int,RingTopology::Direction> horizontal=vLevels->get_next_queue_at_level(2);

        if(id==0){
            //std::cout<<"initial chunk: "<<tmp<<std::endl;
        }
        if(method=="baseline"){
            if(local_dim>1 && local_run){
                LogicalTopology* torus=logical_topologies["Torus3D"]->get_topology();
                CollectivePhase vn(this,local_first.first,new Ring(ComType::All_Gatehr,id,layer,((Torus3D*)torus)->local_dimension,tmp,local_first.second,PacketRouting::Software,InjectionPolicy::Normal,boost_mode));
                vect.push_back(vn);
                tmp=vn.final_data_size;
            }
            if(id==0){
                //std::cout<<"tmp after phase 1: "<<tmp<<std::endl;
            }
            //comment should be removed
            if(vertical_dim>1 && vertical_run){
                LogicalTopology* torus=logical_topologies["Torus3D"]->get_topology();
                CollectivePhase vn(this,vertical.first,new Ring(ComType::All_Gatehr,id,layer,((Torus3D*)torus)->vertical_dimension,tmp,vertical.second,PacketRouting::Software,InjectionPolicy::Normal,boost_mode));
                vect.push_back(vn);
                tmp=vn.final_data_size;
            }
            if(id==0){
                //std::cout<<"tmp after phase 2: "<<tmp<<std::endl;
            }
            if(horizontal_dim>1 && horizontal_run){
                LogicalTopology* torus=logical_topologies["Torus3D"]->get_topology();
                CollectivePhase vn(this,horizontal.first,new Ring(ComType::All_Gatehr,id,layer,((Torus3D*)torus)->horizontal_dimension,tmp,horizontal.second,PacketRouting::Software,InjectionPolicy::Normal,boost_mode));
                vect.push_back(vn);
                tmp=vn.final_data_size;
            }
            if(id==0){
                //std::cout<<"tmp after phase 3: "<<tmp<<std::endl;
            }
        }
        if(method=="baseline"){
            StreamBaseline *newStream=new StreamBaseline(this,dataset,stream_counter++,vect,pri);
            newStream->current_queue_id=-1;
            insert_into_ready_list(newStream);
        }
    }
    return dataset;
}
DataSet * Sys::generate_hierarchical_reduce_scatter(int size, bool local_run, bool vertical_run, bool horizontal_run, SchedulingPolicy pref_scheduling, int layer) {
    int chunk_size=determine_chunk_size(size,ComType::All_Gatehr);
    int streams=ceil(((double)size)/chunk_size);
    int tmp;
    DataSet *dataset=new DataSet(streams);
    streams_injected+=streams;
    int pri=get_priority(pref_scheduling);
    for(int i=0;i<streams;i++){
        tmp=chunk_size;

        std::list<CollectivePhase> vect;
        std::list<ComType> types;

        std::pair<int,RingTopology::Direction> local_first;
        local_first=vLevels->get_next_queue_at_level(0);
        std::pair<int,RingTopology::Direction> vertical=vLevels->get_next_queue_at_level(1);
        std::pair<int,RingTopology::Direction> horizontal=vLevels->get_next_queue_at_level(2);

        if(id==0){
            //std::cout<<"initial chunk: "<<tmp<<std::endl;
        }
        if(method=="baseline"){
            if(local_dim>1 && local_run){
                LogicalTopology* torus=logical_topologies["Torus3D"]->get_topology();
                CollectivePhase vn(this,local_first.first,new Ring(ComType::Reduce_Scatter,id,layer,((Torus3D*)torus)->local_dimension,tmp,local_first.second,PacketRouting::Software,InjectionPolicy::Normal,boost_mode));
                vect.push_back(vn);
                tmp=vn.final_data_size;
            }
            if(id==0){
                //std::cout<<"tmp after phase 1: "<<tmp<<std::endl;
            }
            //comment should be removed
            if(vertical_dim>1 && vertical_run){
                LogicalTopology* torus=logical_topologies["Torus3D"]->get_topology();
                CollectivePhase vn(this,vertical.first,new Ring(ComType::Reduce_Scatter,id,layer,((Torus3D*)torus)->vertical_dimension,tmp,vertical.second,PacketRouting::Software,InjectionPolicy::Normal,boost_mode));
                vect.push_back(vn);
                tmp=vn.final_data_size;
            }
            if(id==0){
                //std::cout<<"tmp after phase 2: "<<tmp<<std::endl;
            }
            if(horizontal_dim>1 && horizontal_run){
                LogicalTopology* torus=logical_topologies["Torus3D"]->get_topology();
                CollectivePhase vn(this,horizontal.first,new Ring(ComType::Reduce_Scatter,id,layer,((Torus3D*)torus)->horizontal_dimension,tmp,horizontal.second,PacketRouting::Software,InjectionPolicy::Normal,boost_mode));
                vect.push_back(vn);
                tmp=vn.final_data_size;
            }
            if(id==0){
                //std::cout<<"tmp after phase 3: "<<tmp<<std::endl;
            }
        }
        if(method=="baseline"){
            StreamBaseline *newStream=new StreamBaseline(this,dataset,stream_counter++,vect,pri);
            newStream->current_queue_id=-1;
            insert_into_ready_list(newStream);
        }
    }
    return dataset;
}
void Sys::call_events(){
    for(auto &callable:event_queue[Sys::boostedTick()]){
        try  {
            pending_events--;
            (std::get<0>(callable))->call(std::get<1>(callable),std::get<2>(callable));
        }
        catch (...){
            std::cout<<"warning! a callable is removed before call"<<std::endl;
        }
    }
    if(event_queue[Sys::boostedTick()].size()>0){
        event_queue[Sys::boostedTick()].clear();
    }
    event_queue.erase(Sys::boostedTick());
    if((finished_workloads==1 && event_queue.size()==0) || initialized==false){
        delete this;
    }
}
DataSet* Sys::generate_LocalRingNodeA2AGlobalDBT_all_reduce(int size,bool local_run, bool vertical_run, bool horizontal_run,
                                               SchedulingPolicy pref_scheduling,int layer){
    //std::cout<<"#################This all reduce is called ################## for node: "<<id<<" ,at time: "<<boostedTick()<<std::endl;
    int chunk_size=determine_chunk_size(size,ComType::All_Reduce);
    int streams=ceil(((double)size)/chunk_size);
    int tmp;
    DataSet *dataset=new DataSet(streams);
    streams_injected+=streams;
    int pri=get_priority(pref_scheduling);
    for(int i=0;i<streams;i++){
        tmp=chunk_size;
        std::list<CollectivePhase> vect;
        std::list<ComType> types;
        std::pair<int,RingTopology::Direction> local=vLevels->get_next_queue_at_level(0);
        std::pair<int,RingTopology::Direction> vertical=vLevels->get_next_queue_at_level(1);
        std::pair<int,RingTopology::Direction> horizontal=vLevels->get_next_queue_at_level(2);

        if(id==0){
            //std::cout<<"initial chunk: "<<tmp<<" local vnet: "<<local.first<<" ,vertical: "<<vertical.first
            //<<" ,horizontal: "<<horizontal.first<<std::endl;
        }
        if(local_dim>1 && local_run){
            LogicalTopology* torus=logical_topologies["Torus3D"]->get_topology();
            CollectivePhase vn(this,local.first,new Ring(ComType::Reduce_Scatter,id,layer,
                                                               ((Torus3D*)torus)->local_dimension,tmp,local.second,
                                                               PacketRouting::Software,InjectionPolicy::Normal,boost_mode));
            vect.push_back(vn);
            tmp=vn.final_data_size;

        }
        if(id==0){
            //std::cout<<"tmp after phase 1: "<<tmp<<std::endl;
        }
        if(horizontal_dim>1 && horizontal_run){
            LogicalTopology* torus=logical_topologies["Torus3D"]->get_topology();
            CollectivePhase vn(this,horizontal.first,new AllToAll(ComType::Reduce_Scatter,id,layer,
                                                                  ((Torus3D*)torus)->horizontal_dimension,tmp,horizontal.second,
                                                                  PacketRouting::Hardware,InjectionPolicy::Normal,boost_mode));
            vect.push_back(vn);
            tmp=vn.final_data_size;
            //std::cout<<"horizontal dim: "<<horizontal_dim<<" ,final size: "<<vn.final_data_size<<std::endl;
        }
        if(id==0){
            //std::cout<<"tmp after phase 2: "<<tmp<<std::endl;
        }
        if(vertical_dim>1 && vertical_run){
            LogicalTopology* bt=logical_topologies["LocalRingNodeA2AGlobalDBT"]->get_topology();
            CollectivePhase vn(this,vertical.first,new DoubleBinaryTreeAllReduce(id, layer,
                                                                                 ((LocalRingGlobalBinaryTree *)(((LocalRingNodeA2AGlobalDBT*)bt)->global_all_reduce_dimension->get_topology()))->global_dimension,tmp, boost_mode));
            vect.push_back(vn);
            tmp=vn.final_data_size;
        }
        if(id==0){
            //std::cout<<"tmp after phase 3: "<<tmp<<std::endl;
        }
        if(horizontal_dim>1 && horizontal_run){
            LogicalTopology* torus=logical_topologies["Torus3D"]->get_topology();
            CollectivePhase vn(this,horizontal.first,new AllToAll(ComType::All_Gatehr,id,layer,
                                                                  ((Torus3D*)torus)->horizontal_dimension,tmp,horizontal.second,
                                                                  PacketRouting::Hardware,InjectionPolicy::Normal,boost_mode));
            vect.push_back(vn);
            tmp=vn.final_data_size;
        }
        if(id==0){
            //std::cout<<"tmp after phase 4: "<<tmp<<std::endl;
        }
        if(local_dim>1 && local_run){
            LogicalTopology* torus=logical_topologies["Torus3D"]->get_topology();
            CollectivePhase vn(this,local.first,new Ring(ComType::All_Gatehr,id,layer,((Torus3D*)torus)->local_dimension,tmp,local.second,PacketRouting::Software,InjectionPolicy::Normal,boost_mode));
            vect.push_back(vn);
            tmp=vn.final_data_size;
        }
        if(id==0){
            //std::cout<<"tmp after phase 5: "<<tmp<<std::endl;
        }

        StreamBaseline *newStream=new StreamBaseline(this,dataset,stream_counter++,vect,pri);
        newStream->current_queue_id=-1;
        insert_into_ready_list(newStream);
    }
    return dataset;
}
void Sys::exitSimLoop(std::string msg) {
    std::cout<<msg<<std::endl;
    NI->sim_finish();
    return;
}
Tick Sys::boostedTick(){
    Sys *ts=all_generators[0];
    if(ts==NULL){
        for(int i=1;i<all_generators.size();i++){
            if(all_generators[i]!=NULL){
                ts=all_generators[i];
                break;
            }
        }
    }
    timespec_t tmp=ts->NI->sim_get_time();
    Tick tick=tmp.time_val/CLOCK_PERIOD;
    return tick+offset;
}
void Sys::proceed_to_next_vnet_baseline(StreamBaseline *stream){
//int added_delay=0;
    if(id==0){
        std::cout<<"stream: "<<stream->stream_num<<"  scheduled after finishd steps: "<<stream->steps_finished
                 <<" in  node:"<<id<< " ,remaining: "<<stream->phases_to_go.size()<<" ,at time: "<<boostedTick()
                 <<" ,initial data size: "<<stream->initial_data_size<<" ,available synchronizer: "<<stream->synchronizer[stream->stream_num]<<std::endl;
    }
    if(!stream->is_ready()){
        stream->suspend_ready();
        return;
    }
    stream->consume_ready();
    int previous_vnet=stream->current_queue_id;
    if(stream->steps_finished==1){
        first_phase_streams--;
    }
    if(stream->steps_finished!=0){
        stream->net_message_latency.back()/=stream->net_message_counter;
    }
    if(stream->my_current_phase.algorithm!=NULL){
        delete stream->my_current_phase.algorithm;
    }
    //std::cout<<"here we are 2.5"<<std::endl;
    if(stream->phases_to_go.size()==0){
        stream->take_bus_stats_average();
        stream->dataset->notify_stream_finished((StreamStat*)stream);
        if(id==0){
            std::cout<<"stream number: "<<stream->stream_num<<"  finished its execution in time: "<<Sys::boostedTick()
            <<" total injected: "<<streams_injected<<" ,total finished: "<<streams_finished
            <<" ,total running streams: "<<total_running_streams
            <<" ,pri: "<<stream->priority<<std::endl;
        }
    }
    if(stream->current_queue_id>=0){
        std::list<BaseStream*> &target=active_Streams.at(stream->my_current_phase.queue_id);
        for(std::list<BaseStream*>::iterator it = target.begin(); it != target.end(); ++it) {
            if(((StreamBaseline *)(*it))->stream_num==stream->stream_num){
                //std::cout<<"deleted from scheduler"<<std::endl;
                target.erase(it);
                break;
            }
        }
    }
    if(stream->phases_to_go.size()==0){
        total_running_streams--;
        if(previous_vnet>=0) {
            scheduler_unit->notify_stream_removed(previous_vnet);
            //std::cout<<"notified stream removed for first phase streams: "<<first_phase_streams<<std::endl;
        }
        delete stream;
        return;
    }
    stream->steps_finished++;
    stream->current_queue_id=stream->phases_to_go.front().queue_id;
    stream->current_com_type=stream->phases_to_go.front().comm_type;

    CollectivePhase vi=stream->phases_to_go.front();
    stream->my_current_phase=vi;
    stream->phases_to_go.pop_front();
    stream->test=0;
    stream->test2=0;
    stream->initialized= false;
    stream->last_phase_change=Sys::boostedTick();
    stream->total_packets_sent=0;

    stream->net_message_latency.push_back(0);
    stream->net_message_counter=0;

    if(id==0){
        std::cout<<"info ,for node: "<<id<<" stream num "<<stream->stream_num<<" has been changed to vnet: "<<stream->current_queue_id
        <<" at time: "<<boostedTick()<<" ,remaining: "<<stream->phases_to_go.size()<<" ,intial size: "<<stream->initial_data_size<<" total injected: "
        <<streams_injected<<" ,total finished: "<<streams_finished<<" ,total running streams: "<<total_running_streams<<
        " ,pri: "<<stream->priority<<std::endl;
    }
    insert_stream(&active_Streams[stream->current_queue_id],stream);
    //active_Streams[stream->current_queue_id].push_back(stream);
    stream->state=StreamState::Ready;

    if(!stream->my_current_phase.enabled){
        stream->declare_ready();
        stream->suspend_ready();
    }
    scheduler_unit->notify_stream_added(stream->current_queue_id);
    if(previous_vnet>=0){
        scheduler_unit->notify_stream_removed(previous_vnet);
    }
}
void Sys::exiting(){
}
void Sys::insert_stream(std::list<BaseStream*> *queue, BaseStream *baseStream) {
    std::list<BaseStream*>::iterator it=queue->begin();
    while (it!=queue->end()){
        if((*it)->initialized==true){
            std::advance(it,1);
            continue;
        }
        else if((*it)->priority>baseStream->priority){
            std::advance(it,1);
            continue;
        }
        else{
            break;
        }
    }
    queue->insert(it,baseStream);
}
void Sys::register_for_finished_stream(Callable *callable) {
    registered_for_finished_stream_event.push_back(callable);
}
void Sys::increase_finished_streams(int amount) {
    streams_finished+=amount;
    for(auto c:registered_for_finished_stream_event){
        c->call(EventType::StreamsFinishedIncrease,NULL);
    }
}

void Sys::register_phases(BaseStream *stream,std::list<CollectivePhase> phases_to_go){
    for(auto &vnet:phases_to_go){
        stream_priorities[vnet.queue_id].push_back(stream->stream_num);
    }
}

void Sys::register_event(Callable *callable,EventType event,CallData *callData,int cycles){
    Tick mycycles=cycles;
    try_register_event(callable,event,callData,mycycles);
    return;
}
void Sys::call(EventType type,CallData *data){
    if(id==0 && type==EventType::General){
        increase_finished_streams(1);
    }

}
void Sys::try_register_event(Callable *callable,EventType event,CallData *callData,Tick &cycles){
    bool should_schedule=false;
    if(event_queue.find(Sys::boostedTick()+cycles)==event_queue.end()){
        std::list<std::tuple<Callable*,EventType,CallData *>> tmp;
        event_queue[Sys::boostedTick()+cycles]=tmp;
        should_schedule=true;
    }
    event_queue[Sys::boostedTick()+cycles].push_back(std::make_tuple(callable,event,callData));
    if(should_schedule){
        timespec_t tmp=generate_time(cycles);
        BasicEventHandlerData *data=new BasicEventHandlerData(id,EventType::CallEvents);
        NI->sim_schedule(tmp,&Sys::handleEvent,data);
    }
    cycles=0;
    pending_events++;
    return;
}
void Sys::insert_into_ready_list(BaseStream *stream){
    insert_stream(&ready_list,stream);
    scheduler_unit->notify_stream_added_into_ready_list();
}
void Sys::ask_for_schedule(int max){
    if(ready_list.size()==0 || ready_list.front()->synchronizer[ready_list.front()->stream_num]<all_generators.size()){return;}
    int top=ready_list.front()->stream_num;
    int min=ready_list.size();
    if(min>max){
        min=max;
    }
    //std::cout<<"ask for schedule checkpoint 1, all gen size: "<<all_generators.size()<<std::endl;
    for(auto &gen:all_generators){
        if(gen->ready_list.size()==0 || gen->ready_list.front()->stream_num!=top){
            //std::cout<<"ask for schedule checkpoint 2.1"<<std::endl;
            return;
        }
        if(gen->ready_list.size()<min){
            //std::cout<<"ask for schedule checkpoint 2.2"<<std::endl;
            min=gen->ready_list.size();
        }
    }
    //std::cout<<"ask for schedule checkpoint 3"<<std::endl;
    for(auto &gen:all_generators){
        gen->schedule(min);
    }
    return;
}
void Sys::schedule(int num){
    int ready_list_size=ready_list.size();
    int counter=std::min(num,ready_list_size);
    //std::cout<<"schedule is called with num: "<<num<<" and counter"<<std::endl;
    while(counter>0){
        //register_phases(ready_list.front(),ready_list.front()->phases_to_go);
        int top_vn=ready_list.front()->phases_to_go.front().queue_id;
        int total_waiting_streams=ready_list.size();
        int total_phases=ready_list.front()->phases_to_go.size();
        if(method=="proposed"){}//proceed_to_next_vnet((Stream *)ready_list.front());}
        else{proceed_to_next_vnet_baseline((StreamBaseline *)ready_list.front());}
        if(ready_list.front()->current_queue_id==-1){
            Sys::sys_panic("should not happen! "+std::to_string(BaseStream::synchronizer[ready_list.front()->stream_num])+" , "+
            std::to_string(BaseStream::ready_counter[ready_list.front()->stream_num])+" , top queue id: "+std::to_string(top_vn)+
            " , total phases: "+std::to_string(total_phases)+" , waiting streams: "+std::to_string(total_waiting_streams));
        }
        ready_list.pop_front();
        counter--;
        first_phase_streams++;
        total_running_streams++;
        scheduler_unit->notify_stream_added(top_vn);
    }
}
void Sys::handleEvent(void *arg) {
    if(arg==NULL){
        return;
    }
    BasicEventHandlerData *ehd=(BasicEventHandlerData *)arg;
    int id=ehd->nodeId;
    EventType event=ehd->event;

    if(event==EventType::CallEvents){
        //std::cout<<"handle event triggered at node: "<<id<<" for call events! at time: "<<Sys::boostedTick()<<std::endl;
        all_generators[id]->iterate();
        delete ehd;
    }
    else if(event==EventType::RendezvousSend){
        //std::cout<<"rendevouz send handle event triggered at node: "<<id<<" for call events! at time: "<<Sys::boostedTick()<<std::endl;
        RendezvousSendData *rsd=(RendezvousSendData *)ehd;
        rsd->send->call(EventType::General,NULL);
        delete rsd;
    }
    else if(event==EventType::RendezvousRecv){
        //std::cout<<"rendevouz recv triggered at node: "<<id<<" for call events! at time: "<<Sys::boostedTick()<<std::endl;
        RendezvousRecvData *rrd=(RendezvousRecvData *)ehd;
        rrd->recv->call(EventType::General,NULL);
        delete rrd;
    }
    else if(event==EventType::PacketReceived){
        RecvPacketEventHadndlerData *rcehd=(RecvPacketEventHadndlerData *)ehd;
        //std::cout<<"****************************handle event triggered for received packets! at node: "
        //<<rcehd->owner->owner->id<<" at time: "<<Sys::boostedTick()<<" ,Tag: "<<rcehd->owner->stream_num<<std::endl;
        rcehd->owner->consume(rcehd);
        delete rcehd;
    }
}
timespec_t Sys::generate_time(int cycles) {
    timespec_t tmp=NI->sim_get_time();
    double addition=cycles*((double)CLOCK_PERIOD);
    tmp.time_val=addition;
    return tmp;

}
}
