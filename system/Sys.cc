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
Tick Sys::offset=0;
int Sys::total_nodes=0;
uint8_t *Sys::dummy_data=new uint8_t[2];
std::vector<Sys*> Sys::all_generators;
int MemMovRequest::id=0;

std::map<int,int> BaseStream::synchronizer;
std::map<int,int> BaseStream::ready_counter;
std::map<int,std::list<BaseStream *>> BaseStream::suspended_streams;

MyPacket::MyPacket(int preferred_vnet,int preferred_src ,int preferred_dest) {
  this->preferred_vnet=preferred_vnet;
  this->preferred_src=preferred_src;
  this->preferred_dest=preferred_dest;
}
void MyPacket::set_notifier(Callable* c){
    notifier=c;
}
void MyPacket::call(EventType event,CallData *data){
    cycles_needed=0;
    if(notifier!=NULL)
        notifier->call(EventType::General,NULL);
}
MemMovRequest::MemMovRequest(int request_num,Sys *generator,LogGP *loggp,int size,int latency,Callable *callable,bool processed,bool send_back):
SharedBusStat(BusType::Mem,0,0,0,0){
    this->size=size;
    this->latency=latency;
    this->callable=callable;
    this->processed=processed;
    this->send_back=send_back;
    this->my_id=id++;
    this->generator=generator;
    this->loggp=loggp;
    this->total_transfer_queue_time=0;
    this->total_transfer_time=0;
    this->total_processing_queue_time=0;
    this->total_processing_time=0;
    this->request_num=request_num;
    this->start_time=Sys::boostedTick();
    this->mem_bus_finished= true;
}
void MemMovRequest::call(EventType event, CallData *data) {
    update_bus_stats(BusType::Mem,(SharedBusStat *)data);
    total_mem_bus_transfer_delay+=((SharedBusStat *)data)->total_shared_bus_transfer_delay;
    total_mem_bus_processing_delay+=((SharedBusStat *)data)->total_shared_bus_processing_delay;
    total_mem_bus_processing_queue_delay+=((SharedBusStat *)data)->total_shared_bus_processing_queue_delay;
    total_mem_bus_transfer_queue_delay+=((SharedBusStat *)data)->total_shared_bus_transfer_queue_delay;
    mem_request_counter=1;
    //delete (SharedBusStat *)data;
    //callEvent=EventType::General;
    mem_bus_finished=true;
    loggp->talking_it=pointer;
    loggp->call(callEvent,data);
}
void MemMovRequest::wait_wait_for_mem_bus(std::list<MemMovRequest>::iterator pointer) {
    mem_bus_finished=false;
    this->pointer=pointer;
}
DataSet::DataSet(int total_streams){
    this->my_id=id_auto_increment++;
    this->total_streams=total_streams;
    this->finished_streams=0;
    this->finished=false;
    this->finish_tick=0;
    this->creation_tick=Sys::boostedTick();
    this->notifier=NULL;
}
void DataSet::set_notifier(Callable *layer,EventType event){
    notifier=new std::pair<Callable *,EventType> (layer,event);
}
void DataSet::notify_stream_finished(StreamStat *data){
    finished_streams++;
    if(data!=NULL){
        update_stream_stats(data);
    }
    if(finished_streams==total_streams){
        finished=true;
        //std::cout<<"********************************Dataset finished"<<std::endl;
        finish_tick=Sys::boostedTick();
        if(notifier!=NULL){
            take_stream_stats_average();
            Callable *c=notifier->first;
            EventType ev=notifier->second;
            delete notifier;
            c->call(ev,new IntData(my_id));
        }
    }
}
void DataSet::call(EventType event,CallData *data){
    notify_stream_finished(((StreamStat *)data));
}
bool DataSet::is_finished(){
    return finished;
}
LogGP::~LogGP() {
    if(NPU_MEM!=NULL){
        delete NPU_MEM;
    }
}
LogGP::LogGP(std::string name,Sys *generator,Tick L,Tick o,Tick g,double G,EventType trigger_event){
    this->L=L;
    this->o=o;
    this->g=g;
    this->G=G;
    this->last_trans=0;
    this->curState=State::Free;
    this->prevState=State::Free;
    this->generator=generator;
    this->processing_state=ProcState::Free;
    this->name=name;
    this->trigger_event=trigger_event;
    this->subsequent_reads=0;
    this->THRESHOLD=8;
    this->NPU_MEM=NULL;
    request_num=0;
    this->local_reduction_delay=generator->local_reduction_delay;

    if(generator->id==0){
        std::cout<<"LogGP model, the local reduction delay is: "<<local_reduction_delay<<std::endl;
    }
}
void LogGP::attach_mem_bus(Sys *generator, Tick L, Tick o, Tick g, double G, bool model_shared_bus,
                           int communication_delay) {
    this->NPU_MEM=new MemBus("NPU2","MEM2",generator,L,o,g,G,model_shared_bus,communication_delay,false);
}
void LogGP::process_next_read() {
    Tick offset = 0;
    if (prevState == State::Sending) {
        assert(Sys::boostedTick() >= last_trans);
        if ((o + (Sys::boostedTick() - last_trans)) > g) {
            offset = o;
        } else {
            offset = g - (Sys::boostedTick() - last_trans);
        }
    } else {
        offset = o;
    }
    MemMovRequest tmp = sends.front();
    tmp.total_transfer_queue_time+=Sys::boostedTick()-tmp.start_time;
    partner->switch_to_receiver(tmp, offset);
    if (generator->id == 0) {
        //std::cout << "beginning sending movreq: " << tmp.my_id <<"  to "<<partner->name
        //<<" in time: "<<generator->boostedTick()<<" actual send in time: "<<generator->boostedTick()+offset<<" data size: "<<tmp.size<<std::endl;
    }
    sends.pop_front();
    curState=State::Sending;
    generator->register_event(this,EventType::Send_Finished,NULL,offset+(G*(tmp.size-1)));
}
void LogGP::request_read(int bytes,bool processed,bool send_back,Callable *callable){
    MemMovRequest mr(request_num++,generator,this,bytes,0,callable,processed,send_back);
    if(NPU_MEM!=NULL){
        mr.callEvent=EventType::Consider_Send_Back;
        pre_send.push_back(mr);
        pre_send.back().wait_wait_for_mem_bus(--pre_send.end());
        NPU_MEM->send_from_MA_to_NPU(MemBus::Transmition::Usual,mr.size,false,false,&pre_send.back());
    }
    else{
        sends.push_back(mr);
        /*if(generator->id==0) {
            std::cout << "sends of " << name << " is:" << sends.size() <<" ,subsequent reads: "<<subsequent_reads<< std::endl;
        }*/
        if(curState==State::Free){
            if(subsequent_reads>THRESHOLD && partner->sends.size()>0 && partner->subsequent_reads<=THRESHOLD) {
                if(partner->curState==State::Free){
                    partner->call(EventType::General,NULL);
                }
                return;
            }
            process_next_read();
        }
    }
}
void LogGP::switch_to_receiver(MemMovRequest mr,Tick offset){
    mr.start_time=Sys::boostedTick();
    receives.push_back(mr);
    prevState=curState;
    curState=State::Receiving;
    generator->register_event(this,EventType::Rec_Finished,NULL,offset+((mr.size-1)*G)+L+o);
    subsequent_reads=0;
}
void LogGP::call(EventType event,CallData *data){
    //std::cout<<"called "<<name<<std::endl;
    if(event==EventType::Send_Finished){
        //std::cout<<"1"<<std::endl;
        last_trans=Sys::boostedTick();
        prevState=curState;
        curState=State::Free;
        subsequent_reads++;
    }
    else if(event==EventType::Rec_Finished){
        assert(receives.size()>0);
        receives.front().total_transfer_time+=Sys::boostedTick()-receives.front().start_time;
        receives.front().start_time=Sys::boostedTick();
        last_trans=Sys::boostedTick();
        prevState=curState;
        if(receives.size()<2) {
            curState = State::Free;
        }
        if(receives.front().processed==true){
            //std::cout<<"2.1"<<std::endl;
            if(NPU_MEM!=NULL){
                receives.front().processed=false;
                receives.front().loggp=this;
                receives.front().callEvent=EventType::Consider_Process;
                if (generator->id == 0) {
                    //std::cout << "movreq: " << receives.front().my_id
                    //<< " is received to " <<name
                    //<<" time: "<<generator->boostedTick()<<std::endl;
                }
                pre_process.push_back(receives.front());
                receives.pop_front();
                pre_process.back().wait_wait_for_mem_bus(--pre_process.end());
                NPU_MEM->send_from_NPU_to_MA(MemBus::Transmition::Usual,pre_process.back().size,false,true,&pre_process.back());
            }
            else{
                receives.front().processed=false;
                if (generator->id == 0) {
                    //std::cout << "movreq: " << receives.front().my_id
                    //<< " is received to " <<name
                    //<<" time: "<<generator->boostedTick()<<std::endl;
                }
                processing.push_back(receives.front());
                receives.pop_front();
            }
            if(processing_state==ProcState::Free && processing.size()>0) {
                if (generator->id == 0) {
                    //std::cout << "movreq: " << processing.front().my_id
                    //<< " is scheduled for processing in "<<name
                    //<<" time: "<<generator->boostedTick()<<std::endl;
                }
                processing.front().total_processing_queue_time+=Sys::boostedTick()-processing.front().start_time;
                processing.front().start_time=Sys::boostedTick();
                generator->register_event(this,EventType::Processing_Finished,NULL,((processing.front().size/100)*local_reduction_delay)+50);
                processing_state=ProcState::Processing;
            }
        }
        else if(receives.front().send_back==true){
            //std::cout<<"2.2"<<std::endl;
            if(NPU_MEM!=NULL){
                receives.front().send_back=false;
                receives.front().callEvent=EventType::Consider_Send_Back;
                receives.front().loggp=this;
                pre_send.push_back(receives.front());
                receives.pop_front();
                pre_send.back().wait_wait_for_mem_bus(--pre_send.end());
                NPU_MEM->send_from_NPU_to_MA(MemBus::Transmition::Usual,pre_send.back().size,false,true,&pre_send.back());
            }
            else{
                receives.front().send_back=false;
                sends.push_back(receives.front());
                receives.pop_front();
            }
        }
        else{
            if(generator->id==0){
                //std::cout<<"movreq: "<<receives.front().my_id<<
                //" is received to "<<name<<" and is finished, receives size: "<<receives.size()
                //<<" time: "<<generator->boostedTick()<<std::endl;
            }
            if(NPU_MEM!=NULL) {
                receives.front().callEvent=EventType::Consider_Retire;
                receives.front().loggp=this;
                retirements.push_back(receives.front());
                retirements.back().wait_wait_for_mem_bus(--retirements.end());
                NPU_MEM->send_from_NPU_to_MA(MemBus::Transmition::Usual,retirements.back().size,false,false,&retirements.back());
                receives.pop_front();
            }
            else{
                SharedBusStat *tmp=new SharedBusStat(BusType::Shared,receives.front().total_transfer_queue_time,receives.front().total_transfer_time,
                                  receives.front().total_processing_queue_time,receives.front().total_processing_time);
                tmp->update_bus_stats(BusType::Mem,receives.front());
                receives.front().callable->call(trigger_event,tmp);
                //std::cout<<"2.3"<<std::endl;
                receives.pop_front();
            }
        }
    }
    else if(event==EventType::Processing_Finished){
        //std::cout<<"3"<<std::endl;
        assert(processing.size()>0);
        processing.front().total_processing_time+=Sys::boostedTick()-processing.front().start_time;
        processing.front().start_time=Sys::boostedTick();
        processing_state=ProcState::Free;
        if(generator->id==0){
            //std::cout<<"movreq: "<<processing.front().my_id<<" finished processing in "<<name
            //<<" time: "<<generator->boostedTick()<<std::endl;
        }
        if(processing.front().send_back==true){
            if(NPU_MEM!=NULL){
                processing.front().send_back=false;
                processing.front().loggp=this;
                processing.front().callEvent=EventType::Consider_Send_Back;
                pre_send.push_back(processing.front());
                processing.pop_front();
                pre_send.back().wait_wait_for_mem_bus(--pre_send.end());
                NPU_MEM->send_from_NPU_to_MA(MemBus::Transmition::Usual,pre_send.back().size,false,true,&pre_send.back());
            }
            else{
                processing.front().send_back=false;
                sends.push_back(processing.front());
                processing.pop_front();
            }
        }
        else{
            if(NPU_MEM!=NULL){
                processing.front().callEvent=EventType::Consider_Retire;
                processing.front().loggp=this;
                retirements.push_back(processing.front());
                retirements.back().wait_wait_for_mem_bus(--retirements.end());
                NPU_MEM->send_from_NPU_to_MA(MemBus::Transmition::Usual,retirements.back().size,false,false,&retirements.back());
                processing.pop_front();
            }
            else{
                if(generator->id==0){
                    // std::cout<<"movreq: "<<processing.front().my_id<<
                    //" finished processing in "<<name<<" and is finished, processing size: "<<processing.size()
                    //<<" time: "<<generator->boostedTick()<<std::endl;
                }
                SharedBusStat *tmp=new SharedBusStat(BusType::Shared,processing.front().total_transfer_queue_time,processing.front().total_transfer_time,
                                 processing.front().total_processing_queue_time,processing.front().total_processing_time);
                tmp->update_bus_stats(BusType::Mem,processing.front());
                processing.front().callable->call(trigger_event,tmp);
                processing.pop_front();
            }
        }
        if(processing.size()>0){
            if(generator->id==0){
                // std::cout<<"movreq: "<<processing.front().my_id<<" is scheduled for processing in "<<name
                // <<" time: "<<generator->boostedTick()<<std::endl;
            }
            processing.front().total_processing_queue_time+=Sys::boostedTick()-processing.front().start_time;
            processing.front().start_time=Sys::boostedTick();
            processing_state=ProcState::Processing;
            generator->register_event(this,EventType::Processing_Finished,NULL,((processing.front().size/100)*local_reduction_delay)+50);
        }
    }
    else if(event==EventType::Consider_Retire){
        //std::cout<<"4"<<std::endl;
        SharedBusStat *tmp=new SharedBusStat(BusType::Shared,retirements.front().total_transfer_queue_time,retirements.front().total_transfer_time,
                          retirements.front().total_processing_queue_time,retirements.front().total_processing_time);
        MemMovRequest movRequest=*talking_it;
        tmp->update_bus_stats(BusType::Mem,movRequest);
        movRequest.callable->call(trigger_event,tmp);
        if(!movRequest.mem_bus_finished){
            //std::cout<<"***********************Violation****************"<<std::endl;
        }
        retirements.erase(talking_it);
        delete data;
    }
    else if(event==EventType::Consider_Process){
        //std::cout<<"5"<<std::endl;
        MemMovRequest movRequest=*talking_it;
        processing.push_back(movRequest);
        if(!movRequest.mem_bus_finished){
            //std::cout<<"***********************Violation****************"<<std::endl;
        }
        pre_process.erase(talking_it);
        if(processing_state==ProcState::Free && processing.size()>0) {
            if (generator->id == 0) {
                //std::cout << "movreq: " << processing.front().my_id
                //<< " is scheduled for processing in "<<name
                //<<" time: "<<generator->boostedTick()<<std::endl;
            }
            processing.front().total_processing_queue_time+=Sys::boostedTick()-processing.front().start_time;
            processing.front().start_time=Sys::boostedTick();
            generator->register_event(this,EventType::Processing_Finished,NULL,((processing.front().size/100)*local_reduction_delay)+50);
            processing_state=ProcState::Processing;
        }
        delete data;
    }
    else if(event==EventType::Consider_Send_Back){
        assert(pre_send.size()>0);
        //std::cout<<"6: "<<talking_it->size<<std::endl;
        MemMovRequest movRequest=*talking_it;
        sends.push_back(movRequest);
        if(!movRequest.mem_bus_finished){
            //std::cout<<"***********************Violation****************"<<std::endl;
        }
        pre_send.erase(talking_it);
        delete data;
    }
    if(curState==State::Free){
        //std::cout<<"7"<<std::endl;
        if(sends.size()>0){
            if(subsequent_reads>THRESHOLD && partner->sends.size()>0 && partner->subsequent_reads<=THRESHOLD){
                if(partner->curState==State::Free){
                    partner->call(EventType::General,NULL);
                }
                return;
            }
            process_next_read();
        }
    }
}
MemBus::~MemBus() {
    delete NPU_side;
    delete MA_side;
}
MemBus::MemBus(std::string side1,std::string side2,Sys *generator,Tick L,Tick o,Tick g,double G,bool model_shared_bus,int communication_delay,bool attach){
    NPU_side=new LogGP(side1,generator,L,o,g,G,EventType::MA_to_NPU);
    MA_side=new LogGP(side2,generator,L,o,g,G,EventType::NPU_to_MA);
    NPU_side->partner=MA_side;
    MA_side->partner=NPU_side;
    this->generator=generator;
    this->model_shared_bus=model_shared_bus;
    this->communication_delay=communication_delay;
    if(attach){
        NPU_side->attach_mem_bus(generator,L,o,g,0.0038,model_shared_bus,communication_delay);
    }
    if(generator->id==0){
        std::cout<<"Shared bus modeling enabled? "<<std::boolalpha<<model_shared_bus<<std::endl;
        std::cout<<"LogGP model, the L is:"<<L<<" ,o is: "<<o<<" ,g is: "<<g<<" ,G is: "<<G<<std::endl;
        std::cout<<"communication delay (in the case of disabled shared bus): "<<communication_delay<<std::endl;
    }
}
void MemBus::send_from_NPU_to_MA(MemBus::Transmition transmition,int bytes,bool processed,bool send_back,Callable *callable){
    if(model_shared_bus && transmition==Transmition::Usual){
        NPU_side->request_read(bytes,processed,send_back,callable);
    }
    else{
        if(transmition==Transmition::Fast){
            generator->register_event(callable,EventType::NPU_to_MA,new SharedBusStat(BusType::Shared,0,10,0,0),10);
        }
        else{
            generator->register_event(callable,EventType::NPU_to_MA,new SharedBusStat(BusType::Shared,0,communication_delay,0,0),communication_delay);
        }
    }
}
void MemBus::send_from_MA_to_NPU(MemBus::Transmition transmition,int bytes,bool processed,bool send_back,Callable *callable){
    if(model_shared_bus && transmition==Transmition::Usual) {
        MA_side->request_read(bytes, processed, send_back, callable);
    }
    else{
        if(transmition==Transmition::Fast){
            generator->register_event(callable,EventType::MA_to_NPU,new SharedBusStat(BusType::Shared,0,10,0,0),10);
        }
        else{
            generator->register_event(callable,EventType::MA_to_NPU,new SharedBusStat(BusType::Shared,0,communication_delay,0,0),communication_delay);
        }
    }
}
CollectivePhase::CollectivePhase(Sys *generator, int queue_id, Algorithm *algorithm) {
    this->generator=generator;
    this->queue_id=queue_id;
    this->algorithm=algorithm;
    this->enabled=true;
    this->initial_data_size=algorithm->data_size;
    this->final_data_size=algorithm->final_data_size;
    this->comm_type=algorithm->comType;
    this->enabled=algorithm->enabled;
}
CollectivePhase::CollectivePhase(){
    queue_id=-1;
    generator=NULL;
    algorithm=NULL;
}
void CollectivePhase::init(BaseStream *stream) {
    if(algorithm!=NULL){
        algorithm->init(stream);
    }
}
DMA_Request::DMA_Request(int id,int slots,int latency,int bytes){
    this->slots=slots;
    this->latency=latency;
    this->id=id;
    this->executed=false;
    this->stream_owner=NULL;
    this->bytes=bytes;
}
DMA_Request::DMA_Request(int id,int slots,int latency,int bytes,Callable *stream_owner){
    this->slots=slots;
    this->latency=latency;
    this->id=id;
    this->executed=false;
    this->bytes=bytes;
    this->stream_owner=stream_owner;
}
void BaseStream::changeState(StreamState state) {
    this->state=state;
}

BaseStream::BaseStream(int stream_num,Sys *owner,std::list<CollectivePhase> phases_to_go){
    this->stream_num=stream_num;
    this->owner=owner;
    this->initialized=false;
    this->phases_to_go=phases_to_go;
    if(synchronizer.find(stream_num)!=synchronizer.end()){
        synchronizer[stream_num]++;
    }
    else{
        //std::cout<<"synchronizer set!"<<std::endl;
        synchronizer[stream_num]=1;
        ready_counter[stream_num]=0;
    }
    for(auto &vn:phases_to_go){
        if(vn.algorithm!=NULL){
            vn.init(this);
        }
    }
    state=StreamState::Created;
    preferred_scheduling=SchedulingPolicy::None;
    creation_time=Sys::boostedTick();
    total_packets_sent=0;
    current_queue_id=-1;
    priority=0;
}
void BaseStream::declare_ready(){
    ready_counter[stream_num]++;
    if(ready_counter[stream_num]==owner->total_nodes){
        synchronizer[stream_num]=owner->total_nodes;
        ready_counter[stream_num]=0;
    }
}
bool BaseStream::is_ready(){
    return synchronizer[stream_num]>0;
}
void BaseStream::consume_ready(){
    //std::cout<<"consume ready called!"<<std::endl;
    assert(synchronizer[stream_num]>0);
    synchronizer[stream_num]--;
    resume_ready(stream_num);
}
void BaseStream::suspend_ready(){
    if(suspended_streams.find(stream_num)==suspended_streams.end()){
        std::list<BaseStream*> empty;
        suspended_streams[stream_num]=empty;
    }
    suspended_streams[stream_num].push_back(this);
    return;
}
void BaseStream::resume_ready(int st_num){
    if(suspended_streams[st_num].size()!=(owner->all_generators.size()-1)){
        return;
    }
    int counter=owner->all_generators.size()-1;
    for(int i=0;i<counter;i++){
        StreamBaseline *stream=(StreamBaseline*)suspended_streams[st_num].front();
        suspended_streams[st_num].pop_front();
        owner->all_generators[stream->owner->id]->proceed_to_next_vnet_baseline(stream);
    }
    if(phases_to_go.size()==0){
        destruct_ready();
    }
    return;
}
void BaseStream::destruct_ready() {
    std::map<int,int>::iterator it;
    it=synchronizer.find(stream_num);
    synchronizer.erase(it);
    ready_counter.erase(stream_num);
    suspended_streams.erase(stream_num);
}
PacketBundle::PacketBundle(Sys *generator,BaseStream *stream,std::list<MyPacket*> locked_packets, bool processed, bool send_back, int size,MemBus::Transmition transmition){
this->generator=generator;
this->locked_packets=locked_packets;
this->processed=processed;
this->send_back=send_back;
this->size=size;
this->stream=stream;
this->transmition=transmition;
creation_time=Sys::boostedTick();
}
PacketBundle::PacketBundle(Sys *generator,BaseStream *stream, bool processed, bool send_back, int size,MemBus::Transmition transmition){
    this->generator=generator;
    this->processed=processed;
    this->send_back=send_back;
    this->size=size;
    this->stream=stream;
    this->transmition=transmition;
    creation_time=Sys::boostedTick();
}
void PacketBundle::send_to_MA(){
    generator->memBus->send_from_NPU_to_MA(transmition,size,processed,send_back,this);
}
void PacketBundle::send_to_NPU(){
    generator->memBus->send_from_MA_to_NPU(transmition,size,processed,send_back,this);
}
void PacketBundle::call(EventType event,CallData *data){
    Tick current=Sys::boostedTick();
    for(auto &packet:locked_packets){
        packet->ready_time=current;
    }
    stream->call(EventType::General,data);
    delete this;
}
StreamBaseline::StreamBaseline(Sys *owner,DataSet *dataset,int stream_num,std::list<CollectivePhase> phases_to_go,int priority)
:BaseStream(stream_num,owner,phases_to_go){
        this->owner=owner;
        this->stream_num=stream_num;
        this->phases_to_go=phases_to_go;
        this->dataset=dataset;
        this->priority=priority;
        steps_finished=0;
        initial_data_size=phases_to_go.front().initial_data_size;
}
void StreamBaseline::init(){
    //std::cout<<"stream number: "<<stream_num<<"is inited in node: "<<owner->id<<std::endl;
    initialized= true;
    if(!my_current_phase.enabled){
        return;
    }
    my_current_phase.algorithm->run(EventType::StreamInit,NULL);
    if(steps_finished==1){
        queuing_delay.push_back(last_phase_change-creation_time);
    }
    queuing_delay.push_back(Sys::boostedTick()-last_phase_change);
    total_packets_sent=1;
}
void StreamBaseline::call(EventType event,CallData *data){
    if(event==EventType::WaitForVnetTurn){
        owner->proceed_to_next_vnet_baseline(this);
        return;
    }
    else{
        //std::cout<<"general event called in stream"<<std::endl;
        SharedBusStat *sharedBusStat=(SharedBusStat *)data;
        update_bus_stats(BusType::Both,sharedBusStat);
        my_current_phase.algorithm->run(EventType::General,data);
        if(data!=NULL){
            delete sharedBusStat;
        }
    }
}
void StreamBaseline::consume(RecvPacketEventHadndlerData *message){
    if(message->message_end){
        net_message_latency.back()+=Sys::boostedTick()-message->ready_time;  //not accurate
        net_message_counter++;
        if(steps_finished==1 && owner->id==0){
            //std::cout<<"receiving finshed message flag for stream: "<<stream_num<<std::endl;
        }
    }
    my_current_phase.algorithm->run(EventType::PacketReceived,message);
}
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
    delete scheduler_unit;
    delete vLevels;
    delete memBus;
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

//Sys::Sys(CommonAPI *NI,int id,int num_passes,std::string my_sys,std::string my_workload,int total_stat_rows,int stat_row,std::string path,std::string run_name)
Sys::Sys(CommonAPI *NI,MemoryCommonAPI *MEM,int id,int num_passes,int local_dim, int vertical_dim,int horizontal_dim,
    int perpendicular_dim,int fourth_dim,int local_queus,int vertical_queues,int horizontal_queues,
    int perpendicular_queues,int fourth_queues,std::string my_sys,
    std::string my_workload,float comm_scale, float compute_scale,float injection_scale,int total_stat_rows,int stat_row,
    std::string path,std::string run_name)
{

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

    initialize_sys(my_sys+".txt");

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

    if((id+1)>all_generators.size()){
        all_generators.resize(id+1);
    }
    all_generators[id]=this;

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
    int concurrent_streams=8;
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
    logical_topologies["DBT"]=new DoubleBinaryTreeTopology(id,horizontal_dim,id%local_dim,local_dim,local_dim);          //horizontal_dim,id%local_dim,local_dim);
    logical_topologies["Torus"]=new Torus(id,total_nodes,loc,hor,ver,perp);
    logical_topologies["A2A"]=new AllToAllTopology(id,total_nodes,loc,hor);

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
    workload=new Workload(run_name,this,my_workload+".txt",num_passes,total_stat_rows,stat_row,path);
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
void Sys::parse_var(std::string var, std::string value) {
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
}
void Sys::post_process_inputs() {
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
    return;
}
void Sys::initialize_sys(std::string name) {
    std::ifstream inFile;
    inFile.open("sys_inputs/" + name);
    if (!inFile) {
        std::cout << "Unable to open file: " << name << std::endl;
    } else {
        std::cout << "success in openning file" << std::endl;
    }
    std::string var;
    std::string value;
    while(inFile.peek()!=EOF){
        var="";
        inFile >> var;
        if(inFile.peek()!=EOF){
            inFile >> value;
        }
        parse_var(var,value);
    }
    inFile.close();
    post_process_inputs();
    return;
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
        sys->ask_for_schedule(max);
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
        sys->ask_for_schedule(max);
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
    if(type==ComType::All_to_All){
        chunk_size=size/8;
    }
    if(chunk_size<1024){
        chunk_size=1024;
    }
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
    }
    else{
        return generate_hierarchical_all_reduce(size,local,vertical,horizontal,pref_scheduling,layer);
    }
}
DataSet * Sys::generate_all_gather(int size,bool local, bool vertical, bool horizontal,
                                   SchedulingPolicy pref_scheduling,int layer){
    return generate_hierarchical_all_gather(size,local,vertical,horizontal,pref_scheduling,layer);

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
        std::pair<int,Torus::Direction> local_last;
        if(collectiveOptimization==CollectiveOptimization::LocalBWAware){
            local_last=vLevels->get_next_queue_at_level_first(0);
        }
        else{
            local_last=vLevels->get_next_queue_at_level(0);
        }
        std::pair<int,Torus::Direction> horizontal=vLevels->get_next_queue_at_level(2);
        std::map<int,CollectivePhase*>::iterator  it;
        if(id==0){
            //std::cout<<"initial chunk: "<<tmp<<std::endl;
        }
        if(local_dim>1 && local_run){
            LogicalTopology* A2A=logical_topologies["A2A"]->get_topology();
            PacketRouting pRouting=alltoall_routing;
            if(collectiveImplementation==CollectiveImplementation::DoubleBinaryTreeLocalAllToAll){
                pRouting=PacketRouting::Hardware;
            }
            CollectivePhase vn(this,local_last.first,new AllToAll(AllToAll::Type::AllToAll,id,layer,(AllToAllTopology*)A2A,tmp,AllToAllTopology::Dimension::Local,local_last.second,pRouting,InjectionPolicy::Normal,boost_mode));
            vect.push_back(vn);
            tmp=vn.final_data_size;
            if(id==0){
                //std::cout<<"tmp after phase 1: "<<tmp<<std::endl;
            }
        }
        if(method=="baseline" && horizontal_dim>1 && horizontal_run){
            LogicalTopology* A2A=logical_topologies["A2A"]->get_topology();
            CollectivePhase vn(this,horizontal.first,new AllToAll(AllToAll::Type::AllToAll,id,layer,(AllToAllTopology*)A2A,tmp,AllToAllTopology::Dimension::Horizontal,horizontal.second,PacketRouting::Hardware,InjectionPolicy::Normal,boost_mode));
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
        std::pair<int,Torus::Direction> local_first;
        if(collectiveOptimization==CollectiveOptimization::Baseline){
            local_first=vLevels->get_next_queue_at_level_first(0);
        }
        else{
            local_first=vLevels->get_next_queue_at_level_first(0);
        }
        std::pair<int,Torus::Direction> local_last=vLevels->get_next_queue_at_level_last(0);
        std::pair<int,Torus::Direction> horizontal=vLevels->get_next_queue_at_level(2);
        if(id==0){
            //std::cout<<"initial chunk: "<<tmp<<std::endl;
        }
        if(method=="baseline"){
            if(local_dim>1 && local_run){
                if(collectiveOptimization==CollectiveOptimization::Baseline){
                    LogicalTopology* A2A=logical_topologies["A2A"]->get_topology();
                    CollectivePhase vn(this,local_first.first,new AllToAll(AllToAll::Type::AllReduce,id,layer,(AllToAllTopology*)A2A,tmp,AllToAllTopology::Dimension::Local,local_first.second,alltoall_routing,InjectionPolicy::Normal,boost_mode));
                    vect.push_back(vn);
                    tmp=vn.final_data_size;
                }
                else{
                    LogicalTopology* A2A=logical_topologies["A2A"]->get_topology();
                    CollectivePhase vn(this,local_first.first,new AllToAll(AllToAll::Type::ReduceScatter,id,layer,(AllToAllTopology*)A2A,tmp,AllToAllTopology::Dimension::Local,local_first.second,alltoall_routing,InjectionPolicy::Normal,boost_mode));
                    vect.push_back(vn);
                    tmp=vn.final_data_size;
                }
                if(id==0){
                    //std::cout<<"tmp after phase 1: "<<tmp<<std::endl;
                }
            }
            if(horizontal_dim>1 && horizontal_run){
                LogicalTopology* A2A=logical_topologies["A2A"]->get_topology();
                CollectivePhase vn(this,horizontal.first,new AllToAll(AllToAll::Type::AllReduce,id,layer,(AllToAllTopology*)A2A,tmp,AllToAllTopology::Dimension::Horizontal,horizontal.second,PacketRouting::Hardware,InjectionPolicy::Normal,boost_mode));
                vect.push_back(vn);
                tmp=vn.final_data_size;
                /*LogicalTopology* A2A=logical_topologies["A2A"]->get_topology();
                CollectivePhase vn(this,horizontal.first,new AllToAll(AllToAll::Type::ReduceScatter,id,layer,(AllToAllTopology*)A2A,tmp,AllToAllTopology::Dimension::Horizontal,horizontal.second,PacketRouting::Hardware,InjectionPolicy::Normal,boost_mode));
                vect.push_back(vn);
                tmp=vn.final_data_size;
                if(id==0){
                    //std::cout<<"tmp after phase 2: "<<tmp<<std::endl;
                }
                CollectivePhase vn2(this,horizontal.first,new AllToAll(AllToAll::Type::AllGather,id,layer,(AllToAllTopology*)A2A,tmp,AllToAllTopology::Dimension::Horizontal,horizontal.second,PacketRouting::Hardware,InjectionPolicy::Normal,boost_mode));
                vect.push_back(vn2);
                tmp=vn2.final_data_size;
                if(id==0){
                    //std::cout<<"tmp after phase 3: "<<tmp<<std::endl;
                }*/
            }
            if(local_dim>1 && local_run && collectiveOptimization==CollectiveOptimization::LocalBWAware){
                LogicalTopology* A2A=logical_topologies["A2A"]->get_topology();
                CollectivePhase vn(this,local_last.first,new AllToAll(AllToAll::Type::AllGather,id,layer,(AllToAllTopology*)A2A,tmp,AllToAllTopology::Dimension::Local,local_last.second,alltoall_routing,InjectionPolicy::Normal,boost_mode));
                vect.push_back(vn);
                tmp=vn.final_data_size;
                if(id==0){
                    //std::cout<<"tmp after phase 4: "<<tmp<<std::endl;
                }
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
            std::pair<int,Torus::Direction> local_first;
            if(collectiveOptimization==CollectiveOptimization::Baseline){
                local_first=vLevels->get_next_queue_at_level(0);
                if(collectiveImplementation==CollectiveImplementation::DoubleBinaryTree){
                    CollectivePhase vn(this,local_first.first,new Ring(Ring::Type::AllReduce,id,layer,(Torus*)bt,tmp,Torus::Dimension::Local,local_first.second,PacketRouting::Software,InjectionPolicy::Normal,boost_mode));
                    vect.push_back(vn);
                    tmp=vn.final_data_size;
                }
                else if(collectiveImplementation==CollectiveImplementation::DoubleBinaryTreeLocalAllToAll){
                    LogicalTopology* a2a=logical_topologies["A2A"]->get_topology();
                    CollectivePhase vn(this,local_first.first,new AllToAll(AllToAll::Type::AllReduce,id,layer,(AllToAllTopology*)a2a,tmp,Torus::Dimension::Local,local_first.second,PacketRouting::Hardware,InjectionPolicy::Normal,boost_mode));
                    vect.push_back(vn);
                    tmp=vn.final_data_size;
                }
            }
            else if(collectiveOptimization==CollectiveOptimization::LocalBWAware){
                local_first=vLevels->get_next_queue_at_level_first(0);
                if(collectiveImplementation==CollectiveImplementation::DoubleBinaryTree){
                    CollectivePhase vn(this,local_first.first,new Ring(Ring::Type::ReduceScatter,id,layer,(Torus*)bt,tmp,Torus::Dimension::Local,local_first.second,PacketRouting::Software,InjectionPolicy::Normal,boost_mode));
                    vect.push_back(vn);
                    tmp=vn.final_data_size;
                }
                else if(collectiveImplementation==CollectiveImplementation::DoubleBinaryTreeLocalAllToAll){
                    LogicalTopology* a2a=logical_topologies["A2A"]->get_topology();
                    CollectivePhase vn(this,local_first.first,new AllToAll(AllToAll::Type::ReduceScatter,id,layer,(AllToAllTopology*)a2a,tmp,Torus::Dimension::Local,local_first.second,PacketRouting::Hardware,InjectionPolicy::Normal,boost_mode));
                    vect.push_back(vn);
                    tmp=vn.final_data_size;
                }
            }
        }
        if(id==0){
            //std::cout<<"tmp after phase 1: "<<tmp<<std::endl;
        }
        if(horizontal_dim>1 && horizontal_run){
            std::pair<int,Torus::Direction> tree_queue_id=vLevels->get_next_queue_at_level(2);
            CollectivePhase vn(this, tree_queue_id.first,new DoubleBinaryTreeAllReduce(id, layer, (BinaryTree *) bt, tmp, boost_mode));
            vect.push_back(vn);
            tmp=vn.final_data_size;
        }
        if(local_dim>1 && local_run){
            std::pair<int,Torus::Direction> local_last;
            if(collectiveOptimization==CollectiveOptimization::LocalBWAware){
                local_last=vLevels->get_next_queue_at_level_last(0);
                if(collectiveImplementation==CollectiveImplementation::DoubleBinaryTree){
                    CollectivePhase vn(this,local_last.first,new Ring(Ring::Type::AllGather,id,layer,(Torus*)bt,tmp,Torus::Dimension::Local,local_last.second,PacketRouting::Software,InjectionPolicy::Normal,boost_mode));
                    vect.push_back(vn);
                    tmp=vn.final_data_size;
                }
                else if(collectiveImplementation==CollectiveImplementation::DoubleBinaryTreeLocalAllToAll){
                    LogicalTopology* a2a=logical_topologies["A2A"]->get_topology();
                    CollectivePhase vn(this,local_last.first,new AllToAll(AllToAll::Type::AllGather,id,layer,(AllToAllTopology*)a2a,tmp,Torus::Dimension::Local,local_last.second,PacketRouting::Hardware,InjectionPolicy::Normal,boost_mode));
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
        std::pair<int,Torus::Direction> local_last;
        if(collectiveOptimization==CollectiveOptimization::LocalBWAware){
            local_last=vLevels->get_next_queue_at_level_first(0);
        }
        else{
            local_last=vLevels->get_next_queue_at_level(0);
        }
        std::pair<int,Torus::Direction> vertical=vLevels->get_next_queue_at_level(1);
        std::pair<int,Torus::Direction> horizontal=vLevels->get_next_queue_at_level(2);
        if(id==0){
            //std::cout<<"initial chunk: "<<tmp<<std::endl;
        }
        if(local_dim>1 && local_run){
            LogicalTopology* torus=logical_topologies["Torus"]->get_topology();
            CollectivePhase vn(this,local_last.first,new Ring(Ring::Type::AllToAll,id,layer,(Torus*)torus,tmp,Torus::Dimension::Local,local_last.second,alltoall_routing,InjectionPolicy::Normal,boost_mode));
            vect.push_back(vn);
            tmp=vn.final_data_size;
        }
        if(id==0){
            //std::cout<<"tmp after phase 1: "<<tmp<<std::endl;
        }
        if(vertical_dim>1 && vertical_run){
            LogicalTopology* torus=logical_topologies["Torus"]->get_topology();
            CollectivePhase vn(this,vertical.first,new Ring(Ring::Type::AllToAll,id,layer,(Torus*)torus,tmp,Torus::Dimension::Vertical,vertical.second,alltoall_routing,InjectionPolicy::Normal,boost_mode));
            vect.push_back(vn);
            tmp=vn.final_data_size;
        }
        if(id==0){
            //std::cout<<"tmp after phase 2: "<<tmp<<std::endl;
        }
        if(horizontal_dim>1 && horizontal_run){
            LogicalTopology* torus=logical_topologies["Torus"]->get_topology();
            CollectivePhase vn(this,horizontal.first,new Ring(Ring::Type::AllToAll,id,layer,(Torus*)torus,tmp,Torus::Dimension::Horizontal,horizontal.second,alltoall_routing,InjectionPolicy::Normal,boost_mode));
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
        std::pair<int,Torus::Direction> local_first;
        if(collectiveOptimization==CollectiveOptimization::Baseline){
            local_first=vLevels->get_next_queue_at_level_first(0);
        }
        else{
            local_first=vLevels->get_next_queue_at_level_first(0);
        }
        std::pair<int,Torus::Direction> local_last=vLevels->get_next_queue_at_level_last(0);
        std::pair<int,Torus::Direction> vertical=vLevels->get_next_queue_at_level(1);
        std::pair<int,Torus::Direction> horizontal=vLevels->get_next_queue_at_level(2);

        if(id==0){
            //std::cout<<"initial chunk: "<<tmp<<std::endl;
        }

        if(method=="baseline"){
            if(local_dim>1 && local_run){
                if(collectiveOptimization==CollectiveOptimization::Baseline){
                    LogicalTopology* torus=logical_topologies["Torus"]->get_topology();
                    CollectivePhase vn(this,local_first.first,new Ring(Ring::Type::AllReduce,id,layer,(Torus*)torus,tmp,Torus::Dimension::Local,local_first.second,PacketRouting::Software,InjectionPolicy::Normal,boost_mode));
                    vect.push_back(vn);
                    tmp=vn.final_data_size;
                }
                else{
                    LogicalTopology* torus=logical_topologies["Torus"]->get_topology();
                    CollectivePhase vn(this,local_first.first,new Ring(Ring::Type::ReduceScatter,id,layer,(Torus*)torus,tmp,Torus::Dimension::Local,local_first.second,PacketRouting::Software,InjectionPolicy::Normal,boost_mode));
                    vect.push_back(vn);
                    tmp=vn.final_data_size;
                }
            }
            if(id==0){
                //std::cout<<"tmp after phase 1: "<<tmp<<std::endl;
            }
            //comment should be removed
            if(vertical_dim>1 && vertical_run){
                LogicalTopology* torus=logical_topologies["Torus"]->get_topology();
                CollectivePhase vn(this,vertical.first,new Ring(Ring::Type::AllReduce,id,layer,(Torus*)torus,tmp,Torus::Dimension::Vertical,vertical.second,PacketRouting::Software,InjectionPolicy::Normal,boost_mode));
                vect.push_back(vn);
                tmp=vn.final_data_size;
            }
            if(id==0){
                //std::cout<<"tmp after phase 2: "<<tmp<<std::endl;
            }
            if(horizontal_dim>1 && horizontal_run){
                LogicalTopology* torus=logical_topologies["Torus"]->get_topology();
                CollectivePhase vn(this,horizontal.first,new Ring(Ring::Type::AllReduce,id,layer,(Torus*)torus,tmp,Torus::Dimension::Horizontal,horizontal.second,PacketRouting::Software,InjectionPolicy::Normal,boost_mode));
                vect.push_back(vn);
                tmp=vn.final_data_size;
            }
            if(id==0){
                //std::cout<<"tmp after phase 3: "<<tmp<<std::endl;
            }
            if(local_dim>1 && local_run && collectiveOptimization==CollectiveOptimization::LocalBWAware){
                LogicalTopology* torus=logical_topologies["Torus"]->get_topology();
                CollectivePhase vn(this,local_last.first,new Ring(Ring::Type::AllGather,id,layer,(Torus*)torus,tmp,Torus::Dimension::Local,local_last.second,PacketRouting::Software,InjectionPolicy::Normal,boost_mode));
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

        std::pair<int,Torus::Direction> local_first;
        local_first=vLevels->get_next_queue_at_level_first(0);
        std::pair<int,Torus::Direction> vertical=vLevels->get_next_queue_at_level(1);
        std::pair<int,Torus::Direction> horizontal=vLevels->get_next_queue_at_level(2);

        if(id==0){
            //std::cout<<"initial chunk: "<<tmp<<std::endl;
        }
        if(method=="baseline"){
            if(local_dim>1 && local_run){
                LogicalTopology* torus=logical_topologies["Torus"]->get_topology();
                CollectivePhase vn(this,local_first.first,new Ring(Ring::Type::AllGather,id,layer,(Torus*)torus,tmp,Torus::Dimension::Local,local_first.second,PacketRouting::Software,InjectionPolicy::Normal,boost_mode));
                vect.push_back(vn);
                tmp=vn.final_data_size;
            }
            if(id==0){
                //std::cout<<"tmp after phase 1: "<<tmp<<std::endl;
            }
            //comment should be removed
            if(vertical_dim>1 && vertical_run){
                LogicalTopology* torus=logical_topologies["Torus"]->get_topology();
                CollectivePhase vn(this,vertical.first,new Ring(Ring::Type::AllGather,id,layer,(Torus*)torus,tmp,Torus::Dimension::Vertical,vertical.second,PacketRouting::Software,InjectionPolicy::Normal,boost_mode));
                vect.push_back(vn);
                tmp=vn.final_data_size;
            }
            if(id==0){
                //std::cout<<"tmp after phase 2: "<<tmp<<std::endl;
            }
            if(horizontal_dim>1 && horizontal_run){
                LogicalTopology* torus=logical_topologies["Torus"]->get_topology();
                CollectivePhase vn(this,horizontal.first,new Ring(Ring::Type::AllGather,id,layer,(Torus*)torus,tmp,Torus::Dimension::Horizontal,horizontal.second,PacketRouting::Software,InjectionPolicy::Normal,boost_mode));
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
    if(finished_workloads==1 && event_queue.size()==0){
        //std::cout<<"delete called for node id: "<<id<<std::endl;
        delete this;
    }
}
void Sys::exitSimLoop(std::string msg) {
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
        //std::cout<<"stream: "<<stream->stream_num<<"  scheduled after finishd steps: "<<stream->steps_finished
                 //<<" in  node:"<<id<< " ,remaining: "<<stream->phases_to_go.size()<<" ,at time: "<<boostedTick()
                 //<<" ,initial data size: "<<stream->initial_data_size<<" ,available synchronizer: "<<stream->synchronizer[stream->stream_num]<<std::endl;
    }
    if(!stream->is_ready()){
        stream->suspend_ready();
        return;
    }
    /*if(stream->steps_finished!=0 && (stream->my_current_phase.algorithm->name==Algorithm::Name::AllToAll) && stream->current_com_type==ComType::Reduce_Scatter && ((AllToAll*)stream->my_current_phase.algorithm)->dimension!=AllToAllTopology::Dimension::Local){
        stream_priorities[stream->phases_to_go.front().queue_id].push_front(stream->stream_num);
        if(stream->steps_finished==0){
            std::cout<<"*************************weird thing has happend***********************"<<std::endl;
        }
    }
    if(stream->phases_to_go.size()>0){
        //std::cout<<"priority size: "<<stream_priorities[stream->phases_to_go.front().queue_id].size()<<" ,in node: "<<id<<std::endl;
        if(stream_priorities[stream->phases_to_go.front().queue_id].front()!=stream->stream_num){
            register_event(stream,EventType::WaitForVnetTurn,NULL,1);
            return;
        }
        else{
            stream_priorities[stream->phases_to_go.front().queue_id].pop_front();
        }
    }*/
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
    /*if(stream->preferred_scheduling==SchedulingPolicy::None) {
        if (scheduling_policy == SchedulingPolicy::LIFO) {
            ready_list.push_front(stream);
        } else {
            ready_list.push_back(stream);
        }
    }
    else{
        if (stream->preferred_scheduling == SchedulingPolicy::LIFO) {
            ready_list.push_front(stream);
        } else {
            ready_list.push_back(stream);
        }
    }*/
    scheduler_unit->notify_stream_added_into_ready_list();
}
void Sys::ask_for_schedule(int max){
    if(ready_list.size()==0){return;}
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
    int counter=num;
    while(counter>0){
        //register_phases(ready_list.front(),ready_list.front()->phases_to_go);
        int top_vn=ready_list.front()->phases_to_go.front().queue_id;
        if(method=="proposed"){}//proceed_to_next_vnet((Stream *)ready_list.front());}
        else{proceed_to_next_vnet_baseline((StreamBaseline *)ready_list.front());}
        if(ready_list.front()->current_queue_id==-1){
            Sys::sys_panic("should not happen!");
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
BasicEventHandlerData::BasicEventHandlerData(int nodeId, EventType event) {
    this->nodeId=nodeId;
    this->event=event;
}
RecvPacketEventHadndlerData::RecvPacketEventHadndlerData(BaseStream *owner,int nodeId, EventType event, int vnet, int stream_num)
:BasicEventHandlerData(nodeId,event) {
    this->owner=owner;
    this->vnet=vnet;
    this->stream_num=stream_num;
    this->message_end= true;
    ready_time=Sys::boostedTick();
    //std::cout<<"################## instantiated with nodeID: "<<this->nodeId<<std::endl;
}
Node::Node(int id,Node *parent, Node *left_child, Node *right_child) {
    this->id=id;
    this->parent=parent;
    this->left_child=left_child;
    this->right_child=right_child;
}
LogicalTopology* LogicalTopology::get_topology() {
    return this;
}
int LogicalTopology::get_reminder(int number, int divisible) {
    //int t=(number+divisible)%divisible;
    //std::cout<<"get reminder called with number: "<<number<<" ,and divisible: "<<divisible<<" and test is: "<<t<<std::endl;
    if(number>=0){
        return number%divisible;
    }
    else{
        return (number+divisible)%divisible;
    }
}
Torus::Torus(int id, int total_nodes, int local_dim, int horizontal_dim, int vertical_dim, int perpendicular_dim) {
    this->id=id;
    this->total_nodes=total_nodes;
    this->local_dim=local_dim;
    this->horizontal_dim=horizontal_dim;
    this->vertical_dim=vertical_dim;
    this->perpendicular_dim=perpendicular_dim;
    find_neighbors(id,total_nodes,local_dim,horizontal_dim,vertical_dim,perpendicular_dim);
    std::cout<<"Torus at node: "<<id<<" is created with local: "<<local_node_id<<"  ,right: "<<right_node_id<<" ,left: "<<left_node_id<<std::endl;
}
void Torus::find_neighbors(int id, int total_nodes, int local_dim, int horizontal_dim, int vertical_dim,
                           int perpendicular_dim) {
    //mycode
    //cout<<"my id: "<<m_id<<endl;
    std::map<int,int> temp;
    std::map<int,int> temp2;
    int m_id=id;
    std::string topology="Torus3D";

    int local_cols=local_dim;
    int package_cols=horizontal_dim;

    if((m_id+1)%local_cols!=0){
        local_node_id=m_id+1;
    }
    else{
        local_node_id=m_id-(m_id%local_cols);
    }
    right_node_id=(m_id-(m_id%(local_cols*package_cols)))+((m_id+local_cols)%(local_cols*package_cols));
    left_node_id=(m_id-(m_id%(local_cols*package_cols)))+get_reminder(m_id-local_cols,local_cols*package_cols);//((m_id-local_cols)%(local_cols*package_cols));

    if(topology=="Torus3D") {
        up_node_id = (m_id + (local_cols * package_cols)) % total_nodes;
        down_node_id = get_reminder(m_id - (local_cols * package_cols),
                                    total_nodes); //(m_id-(local_cols*package_cols))%total_nodes;
    }
    else if(topology=="Torus4D"){
        up_node_id=(m_id-(m_id%(local_cols*package_cols*vertical_dim)))+((m_id+(local_cols*package_cols))%(local_cols*package_cols*vertical_dim));
        down_node_id=(m_id-(m_id%(local_cols*package_cols*vertical_dim)))+((m_id-(local_cols*package_cols))%(local_cols*package_cols*vertical_dim));
        Zpositive_node_id=(m_id+(local_cols*package_cols*vertical_dim))%total_nodes;
        Znegative_node_id=(m_id-(local_cols*package_cols*vertical_dim))%total_nodes;
    }
    else if(topology=="Torus5D"){
        up_node_id=(m_id-(m_id%(local_cols*package_cols*vertical_dim)))+
                   ((m_id+(local_cols*package_cols))%(local_cols*package_cols*vertical_dim));
        down_node_id=(m_id-(m_id%(local_cols*package_cols*vertical_dim)))+
                     ((m_id-(local_cols*package_cols))%(local_cols*package_cols*vertical_dim));

        Zpositive_node_id=(m_id-(m_id%(local_cols*package_cols*vertical_dim*perpendicular_dim)))+
                          ((m_id+(local_cols*package_cols*vertical_dim))%(local_cols*package_cols*vertical_dim*perpendicular_dim));
        Znegative_node_id=(m_id-(m_id%(local_cols*package_cols*vertical_dim*perpendicular_dim)))+
                          ((m_id-(local_cols*package_cols*vertical_dim))%(local_cols*package_cols*vertical_dim*perpendicular_dim));

        Fpositive_node_id=(m_id+(local_cols*package_cols*vertical_dim*perpendicular_dim))%vertical_dim;
        Fnegative_node_id=(m_id-(local_cols*package_cols*vertical_dim*perpendicular_dim))%vertical_dim;
    }
    if(topology=="Torus3D"){
        std::cout<<"I'm node: "<<m_id<<"  , local: "<<local_node_id<<" , right: "<<right_node_id<<" , left: "<<left_node_id<<" , up: "<<up_node_id<<" , down: "<<down_node_id<<std::endl;
    }
    if(topology=="Torus4D"){
        std::cout<<"I'm node: "<<m_id<<"  , local: "<<local_node_id<<" , right: "<<right_node_id<<" , left: "<<left_node_id<<" , up: "<<up_node_id
                 <<" , down: "<<down_node_id<<" , Zpositive: "<<Zpositive_node_id<<" , Znegative: "<<Znegative_node_id<<std::endl;
    }
    if(topology=="Torus5D") {
        std::cout << "I'm node: " << m_id << "  , local: " << local_node_id << " , right: " << right_node_id
                  << " , left: " << left_node_id << " , up: " << up_node_id
                  << " , down: " << down_node_id << " , Zpositive: " << Zpositive_node_id << " , Znegative: "
                  << Znegative_node_id
                  << " , Fpositive: " << Fpositive_node_id << " , Fnegative: " << Fnegative_node_id << std::endl;
    }
}
int Torus::get_receiver_node(int id, Torus::Dimension dimension,Direction direction) {
    switch (dimension)
    {
        case Dimension::Local:
            return ((Torus*)(Sys::all_generators[id]->logical_topologies["Torus"]))->local_node_id;
        case Dimension::Horizontal:
            switch (direction)
            {
                case Direction::Clockwise:
                    return ((Torus*)(Sys::all_generators[id]->logical_topologies["Torus"]))->right_node_id;
                case Direction::Anticlockwise:
                    return ((Torus*)(Sys::all_generators[id]->logical_topologies["Torus"]))->left_node_id;
            }
            return -1;
        case Dimension::Vertical:
            switch (direction)
            {
                case Direction::Clockwise:
                    return ((Torus*)(Sys::all_generators[id]->logical_topologies["Torus"]))->up_node_id;
                case Direction::Anticlockwise:
                    return ((Torus*)(Sys::all_generators[id]->logical_topologies["Torus"]))->down_node_id;
            }
            return -1;
        case Dimension::Perpendicular:
            return -1;
        case Dimension::Fourth:
            return -1;
        default:
            return -1;
    }
}
int Torus::get_sender_node(int id, Torus::Dimension dimension,Direction direction) {
    switch (dimension)
    {
        case Dimension::Local:
            for(auto gn:Sys::all_generators){
                if(((Torus*)(gn->logical_topologies["Torus"]))->local_node_id==id){
                    return gn->id;
                }
            }
            return -1;
        case Dimension::Horizontal:
            switch (direction)
            {
                case Direction::Clockwise:
                    return ((Torus*)(Sys::all_generators[id]->logical_topologies["Torus"]))->left_node_id;
                case Direction::Anticlockwise:
                    return ((Torus*)(Sys::all_generators[id]->logical_topologies["Torus"]))->right_node_id;
            }
            return -1;
        case Dimension::Vertical:
            switch (direction)
            {
                case Direction::Clockwise:
                    return ((Torus*)(Sys::all_generators[id]->logical_topologies["Torus"]))->down_node_id;
                case Direction::Anticlockwise:
                    return ((Torus*)(Sys::all_generators[id]->logical_topologies["Torus"]))->up_node_id;
            }
            return -1;
        case Dimension::Perpendicular:
            return -1;
        case Dimension::Fourth:
            return -1;
        default:
            return -1;
    }
}
int Torus::get_nodes_in_ring(Torus::Dimension dimension) {
    switch (dimension){
        case Dimension::Local:
            return local_dim;
        case Dimension::Horizontal:
            return horizontal_dim;
        case Dimension ::Vertical:
            return vertical_dim;
        default:
            return -1;
    }
}
bool Torus::is_enabled(int id, Torus::Dimension dimension) {
    switch (dimension){
        case Dimension::Local:
            if(id<local_dim){
                return true;
            }
            else{
                return false;
            }
        case Dimension::Vertical:
            if(id%(local_dim*horizontal_dim)==0 && id<(local_dim*horizontal_dim*vertical_dim)){
                return true;
            }
            else{
                return false;
            }
        case Dimension::Horizontal:
            if(id%local_dim==0 && id<(local_dim*horizontal_dim)){
                return true;
            }
            else{
                return false;
            }
        default:
            return true;
    }
}
BinaryTree::~BinaryTree() {
    for(auto n:node_list){
        delete n.second;
    }
}
BinaryTree::BinaryTree(int id,TreeType tree_type,int total_tree_nodes,int start,int stride,int local_dim):Torus(id,local_dim*total_tree_nodes,local_dim,total_tree_nodes,1,1) {
    this->total_tree_nodes=total_tree_nodes;
    this->start=start;
    this->tree_type=tree_type;
    this->stride=stride;
    tree=new Node(-1,NULL,NULL,NULL);
    int depth=1;
    int tmp=total_tree_nodes;
    //node_list.resize(total_nodes,NULL);
    while(tmp>1){
        depth++;
        tmp/=2;
    }
    if(tree_type==TreeType::RootMin){
        tree->right_child=initialize_tree(depth-1,tree);
    }
    else{
        tree->left_child=initialize_tree(depth-1,tree);
    }
    build_tree(tree);
    std::cout<<"##############################################"<<std::endl;
    print(tree);
    std::cout<<"##############################################"<<std::endl;

}
Node* BinaryTree::initialize_tree(int depth,Node* parent) {
    Node* tmp=new Node(-1,parent,NULL,NULL);
    if(depth>1){
        tmp->left_child=initialize_tree(depth-1,tmp);
        tmp->right_child=initialize_tree(depth-1,tmp);
    }
    return tmp;
}
void BinaryTree::build_tree(Node *node) {
    if(node->left_child!=NULL){
        build_tree(node->left_child);
    }
    node->id=start;
    node_list[start]=node;
    start+=stride;
    if(node->right_child!=NULL){
        build_tree(node->right_child);
    }
    return;
}
int BinaryTree::get_parent_id(int id) {
    Node *parent=this->node_list[id]->parent;
    if(parent!=NULL){
        return parent->id;
    }
    return -1;
}
int BinaryTree::get_right_child_id(int id) {
    Node *child=this->node_list[id]->right_child;
    if(child!=NULL){
        return child->id;
    }
    return -1;
}
int BinaryTree::get_left_child_id(int id) {
    Node *child=this->node_list[id]->left_child;
    if(child!=NULL){
        return child->id;
    }
    return -1;
}
BinaryTree::Type BinaryTree::get_node_type(int id) {
    Node *node=this->node_list[id];
    if(node->parent==NULL){
        return Type::Root;
    }
    else if(node->left_child==NULL && node->right_child==NULL){
        return Type::Leaf;
    }
    else{
        return Type::Intermediate;
    }
}
void BinaryTree::print(Node *node) {
    std::cout<<"I am node: "<<node->id;
    if(node->left_child!=NULL){
        std::cout<<" and my left child is: "<<node->left_child->id;
    }
    if(node->right_child!=NULL){
        std::cout<<" and my right child is: "<<node->right_child->id;
    }
    if(node->parent!=NULL){
        std::cout<<" and my parent is: "<<node->parent->id;
    }
    BinaryTree::Type typ=get_node_type(node->id);
    if(typ==BinaryTree::Type::Root){
        std::cout<<" and I am Root ";
    }
    else if(typ==BinaryTree::Type::Intermediate){
        std::cout<<" and I am Intermediate ";
    }
    else if(typ==BinaryTree::Type::Leaf){
        std::cout<<" and I am Leaf ";
    }
    std::cout<<std::endl;
    if(node->left_child!=NULL){
        print(node->left_child);
    }
    if(node->right_child!=NULL){
        print(node->right_child);
    }
}
int BinaryTree::get_receiver_node(int id, Torus::Dimension dimension,Direction direction) {
    switch (dimension)
    {
        case Dimension::Local:
            return ((Torus*)((((DoubleBinaryTreeTopology*)(Sys::all_generators[id]->logical_topologies["DBT"])))->DBMIN))->local_node_id;
        case Dimension::Horizontal:
            return -1;
        case Dimension::Vertical:
            return -1;
        case Dimension::Perpendicular:
            return -1;
        case Dimension::Fourth:
            return -1;
        default:
            return -1;
    }
}
int BinaryTree::get_sender_node(int id, Torus::Dimension dimension,Direction direction) {
    switch (dimension)
    {
        case Dimension::Local:
            for(auto gn:Sys::all_generators){
                if(((Torus*)((((DoubleBinaryTreeTopology*)(gn->logical_topologies["DBT"])))->DBMIN))->local_node_id==id){
                    return gn->id;
                }
            }
            return -1;
        case Dimension::Horizontal:
            return -1;
        case Dimension::Vertical:
            return -1;
        case Dimension::Perpendicular:
            return -1;
        case Dimension::Fourth:
            return -1;
        default:
            return -1;
    }
}
DoubleBinaryTreeTopology::~DoubleBinaryTreeTopology() {
    delete DBMIN;
    delete DBMAX;
}
DoubleBinaryTreeTopology::DoubleBinaryTreeTopology(int id,int total_tree_nodes,int start,int stride,int local_dim) {
    std::cout<<"Double binary tree created with total nodes: "<<total_tree_nodes<<" ,start: "<<start<<" ,stride: "<<stride<<std::endl;
    DBMAX=new BinaryTree(id,BinaryTree::TreeType::RootMax,total_tree_nodes,start,stride,local_dim);
    DBMIN=new BinaryTree(id,BinaryTree::TreeType::RootMin,total_tree_nodes,start,stride,local_dim);
    this->counter=0;
}
LogicalTopology* DoubleBinaryTreeTopology::get_topology() {
    //return DBMIN;  //uncomment this and comment the rest lines of this funcion if you want to run allreduce only on one logical tree
    BinaryTree *ans=NULL;
    if(counter++==0){
        ans=DBMAX;
    }
    else{
        ans=DBMIN;
    }
    if(counter>1){
        counter=0;
    }
    return ans;
}
AllToAllTopology::AllToAllTopology(int id, int total_nodes, int local_dim, int alltoall_dim):Torus(id,total_nodes,local_dim,alltoall_dim,1,1){
    return;
}
int AllToAllTopology::get_receiver_node(int id, Torus::Dimension dimension,Direction direction) {
    switch (dimension)
    {
        case Dimension::Local:
            return ((Torus*)(Sys::all_generators[id]->logical_topologies["A2A"]))->local_node_id;
        case Dimension::Horizontal:
            switch (direction)
            {
                case Direction::Clockwise:
                    return ((Torus*)(Sys::all_generators[id]->logical_topologies["A2A"]))->right_node_id;
                case Direction::Anticlockwise:
                    return ((Torus*)(Sys::all_generators[id]->logical_topologies["A2A"]))->left_node_id;
            }
            return -1;
        case Dimension::Vertical:
            return -1;
        case Dimension::Perpendicular:
            return -1;
        case Dimension::Fourth:
            return -1;
        default:
            return -1;
    }
}
int AllToAllTopology::get_sender_node(int id,Torus::Dimension dimension,Direction direction) {
    switch (dimension)
    {
        case Dimension::Local:
            for(auto gn:Sys::all_generators){
                if(((Torus*)(gn->logical_topologies["A2A"]))->local_node_id==id){
                    return gn->id;
                }
            }
            return -1;
        case Dimension::Horizontal:
            switch (direction)
            {
                case Direction::Clockwise:
                    return ((Torus*)(Sys::all_generators[id]->logical_topologies["A2A"]))->left_node_id;
                case Direction::Anticlockwise:
                    return ((Torus*)(Sys::all_generators[id]->logical_topologies["A2A"]))->right_node_id;
            }
            return -1;
        case Dimension::Vertical:
            return -1;
        case Dimension::Perpendicular:
            return -1;
        case Dimension::Fourth:
            return -1;
        default:
            return -1;
    }
}
Algorithm::Algorithm(int layer_num) {
    this->layer_num=layer_num;
    enabled=true;
}
void Algorithm::init(BaseStream *stream) {
    this->stream=stream;
    return;
}
void Algorithm::call(EventType event, CallData *data) {
    return;
}
void Algorithm::exit() {
    //std::cout<<"exiting collective in node: "<<stream->owner->id<<std::endl;
    stream->declare_ready();
    stream->owner->proceed_to_next_vnet_baseline((StreamBaseline*)stream);
    //delete this;
    return;
}
DoubleBinaryTreeAllReduce::DoubleBinaryTreeAllReduce(int id,int layer_num,BinaryTree *tree,int data_size,bool boost_mode):Algorithm(layer_num) {
    this->id=id;
    this->logicalTopology=tree;
    this->data_size=data_size;
    this->state=State::Begin;
    this->reductions=0;
    this->parent=tree->get_parent_id(id);
    this->left_child=tree->get_left_child_id(id);
    this->right_child=tree->get_right_child_id(id);
    this->type=tree->get_node_type(id);
    this->final_data_size=data_size;
    this->comType=ComType::All_Reduce;
    this->name=Name::DoubleBinaryTree;
    this->enabled=true;
    if(boost_mode){
        this->enabled=tree->is_enabled(id,Torus::Dimension::Horizontal);
    }
    //std::cout<<"tree allreduce is configured with id: "<<this->id<<" ,parent: "<<parent<<" ,left child: "<<left_child<<" ,right child: "<<right_child;
    if(type==BinaryTree::Type::Root){
        //std::cout<<" and I am Root ";
    }
    else if(type==BinaryTree::Type::Intermediate){
        //std::cout<<" and I am Intermediate ";
    }
    else if(type==BinaryTree::Type::Leaf){
        //std::cout<<" and I am Leaf ";
    }
    //std::cout<<" ,and the enabled status: "<<this->enabled;
    //std::cout<<std::endl;
}
void DoubleBinaryTreeAllReduce::run(EventType event,CallData *data) {
    if(state==State::Begin && type==BinaryTree::Type::Leaf){  //leaf.1
        (new PacketBundle(stream->owner,stream,false,false,data_size,MemBus::Transmition::Usual))->send_to_MA();
        state=State::SendingDataToParent;
        return;
    }
    else if(state==State::SendingDataToParent && type==BinaryTree::Type::Leaf){ //leaf.3
        //sending
        sim_request snd_req;
        snd_req.srcRank = stream->owner->id;
        snd_req.dstRank = parent;
        snd_req.tag = stream->stream_num;
        snd_req.reqType = UINT8;
        snd_req.vnet=this->stream->current_queue_id;
        snd_req.layerNum=layer_num;
        //std::cout<<"here************"<<std::endl;
        stream->owner->NI->sim_send(Sys::dummy_data,data_size,UINT8,parent,stream->stream_num,&snd_req,&Sys::handleEvent,NULL);
        //receiving
        sim_request rcv_req;
        rcv_req.vnet=this->stream->current_queue_id;
        rcv_req.layerNum=layer_num;
        RecvPacketEventHadndlerData *ehd=new RecvPacketEventHadndlerData(stream,stream->owner->id,EventType::PacketReceived,stream->current_queue_id,stream->stream_num);
        stream->owner->NI->sim_recv(Sys::dummy_data,data_size,UINT8,parent,stream->stream_num,&rcv_req,&Sys::handleEvent,ehd);
        state=State::WaitingDataFromParent;
        return;
    }
    else if(state==State::WaitingDataFromParent && type==BinaryTree::Type::Leaf){ //leaf.4
        (new PacketBundle(stream->owner,stream,false,false,data_size,MemBus::Transmition::Usual))->send_to_NPU();
        state=State::End;
        return;
    }
    else if(state==State::End && type==BinaryTree::Type::Leaf){ //leaf.5
        exit();
        return;
    }

    else if(state==State::Begin && type==BinaryTree::Type::Intermediate){ //int.1
        sim_request rcv_req;
        rcv_req.vnet=this->stream->current_queue_id;
        rcv_req.layerNum=layer_num;
        RecvPacketEventHadndlerData *ehd=new RecvPacketEventHadndlerData(stream,stream->owner->id,EventType::PacketReceived,stream->current_queue_id,stream->stream_num);
        stream->owner->NI->sim_recv(Sys::dummy_data,data_size,UINT8,left_child,stream->stream_num,&rcv_req,&Sys::handleEvent,ehd);
        sim_request rcv_req2;
        rcv_req2.vnet=this->stream->current_queue_id;
        rcv_req2.layerNum=layer_num;
        RecvPacketEventHadndlerData *ehd2=new RecvPacketEventHadndlerData(stream,stream->owner->id,EventType::PacketReceived,stream->current_queue_id,stream->stream_num);
        stream->owner->NI->sim_recv(Sys::dummy_data,data_size,UINT8,right_child,stream->stream_num,&rcv_req2,&Sys::handleEvent,ehd2);
        state=State::WaitingForTwoChildData;
        return;
    }
    else if(state==State::WaitingForTwoChildData && type==BinaryTree::Type::Intermediate && event==EventType::PacketReceived){ //int.2
        (new PacketBundle(stream->owner,stream,true,false,data_size,MemBus::Transmition::Usual))->send_to_NPU();
        state=State::WaitingForOneChildData;
        return;
    }
    else if(state==State::WaitingForOneChildData && type==BinaryTree::Type::Intermediate && event==EventType::PacketReceived){ //int.3
        (new PacketBundle(stream->owner,stream,true,true,data_size,MemBus::Transmition::Usual))->send_to_NPU();
        state=State::SendingDataToParent;
        return;
    }
    else if(reductions<1 && type==BinaryTree::Type::Intermediate && event==EventType::General){ //int.4
        reductions++;
        return;
    }
    else if(state==State::SendingDataToParent && type==BinaryTree::Type::Intermediate){ //int.5
        //sending
        sim_request snd_req;
        snd_req.srcRank = stream->owner->id;
        snd_req.dstRank = parent;
        snd_req.tag = stream->stream_num;
        snd_req.reqType = UINT8;
        snd_req.vnet=this->stream->current_queue_id;
        snd_req.layerNum=layer_num;
        stream->owner->NI->sim_send(Sys::dummy_data,data_size,UINT8,parent,stream->stream_num,&snd_req,&Sys::handleEvent,NULL);
        //receiving
        sim_request rcv_req;
        rcv_req.vnet=this->stream->current_queue_id;
        rcv_req.layerNum=layer_num;
        RecvPacketEventHadndlerData *ehd=new RecvPacketEventHadndlerData(stream,stream->owner->id,EventType::PacketReceived,stream->current_queue_id,stream->stream_num);
        stream->owner->NI->sim_recv(Sys::dummy_data,data_size,UINT8,parent,stream->stream_num,&rcv_req,&Sys::handleEvent,ehd);
        state=State::WaitingDataFromParent;
    }
    else if(state==State::WaitingDataFromParent && type==BinaryTree::Type::Intermediate && event==EventType::PacketReceived){ //int.6
        (new PacketBundle(stream->owner,stream,true,true,data_size,MemBus::Transmition::Usual))->send_to_NPU();
        state=State::SendingDataToChilds;
        return;
    }
    else if(state==State::SendingDataToChilds && type==BinaryTree::Type::Intermediate){
        sim_request snd_req;
        snd_req.srcRank = stream->owner->id;
        snd_req.dstRank = left_child;
        snd_req.tag = stream->stream_num;
        snd_req.reqType = UINT8;
        snd_req.vnet=this->stream->current_queue_id;
        snd_req.layerNum=layer_num;
        stream->owner->NI->sim_send(Sys::dummy_data,data_size,UINT8,left_child,stream->stream_num,&snd_req,&Sys::handleEvent,NULL);
        sim_request snd_req2;
        snd_req2.srcRank = stream->owner->id;
        snd_req2.dstRank = left_child;
        snd_req2.tag = stream->stream_num;
        snd_req2.reqType = UINT8;
        snd_req2.vnet=this->stream->current_queue_id;
        snd_req2.layerNum=layer_num;
        stream->owner->NI->sim_send(Sys::dummy_data,data_size,UINT8,right_child,stream->stream_num,&snd_req2,&Sys::handleEvent,NULL);
        exit();
        return;
    }

    else if(state==State::Begin && type==BinaryTree::Type::Root){ //root.1
        int only_child_id=left_child>=0?left_child:right_child;
        sim_request rcv_req;
        rcv_req.vnet=this->stream->current_queue_id;
        rcv_req.layerNum=layer_num;
        RecvPacketEventHadndlerData *ehd=new RecvPacketEventHadndlerData(stream,stream->owner->id,EventType::PacketReceived,stream->current_queue_id,stream->stream_num);
        stream->owner->NI->sim_recv(Sys::dummy_data,data_size,UINT8,only_child_id,stream->stream_num,&rcv_req,&Sys::handleEvent,ehd);
        state=State::WaitingForOneChildData;
    }
    else if(state==State::WaitingForOneChildData && type==BinaryTree::Type::Root){ //root.2
        (new PacketBundle(stream->owner,stream,true,true,data_size,MemBus::Transmition::Usual))->send_to_NPU();
        state=State::SendingDataToChilds;
        return;
    }
    else if(state==State::SendingDataToChilds && type==BinaryTree::Type::Root){ //root.2
        int only_child_id=left_child>=0?left_child:right_child;
        sim_request snd_req;
        snd_req.srcRank = stream->owner->id;
        snd_req.dstRank = only_child_id;
        snd_req.tag = stream->stream_num;
        snd_req.reqType = UINT8;
        snd_req.vnet=this->stream->current_queue_id;
        //std::cout<<"here************"<<std::endl;
        snd_req.layerNum=layer_num;
        stream->owner->NI->sim_send(Sys::dummy_data,data_size,UINT8,only_child_id,stream->stream_num,&snd_req,&Sys::handleEvent,NULL);
        exit();
        return;
    }
}
QueueLevelHandler::QueueLevelHandler(int level,int start, int end) {
    for(int i=start;i<=end;i++){
        queues.push_back(i);
    }
    allocator=0;
    first_allocator=0;
    last_allocator=queues.size()/2;
    this->level=level;
}
std::pair<int,Torus::Direction> QueueLevelHandler::get_next_queue_id() {
    Torus::Direction dir;
    if(level!=0 && queues.size()>1 && allocator>=(queues.size()/2)){
        dir=Torus::Direction::Anticlockwise;
    }
    else{
        dir=Torus::Direction::Clockwise;
    }
    if(queues.size()==0){
        return std::make_pair(-1,dir);
    }
    int tmp=queues[allocator++];
    if(allocator==queues.size()){
        allocator=0;
    }
    return std::make_pair(tmp,dir);
}
std::pair<int,Torus::Direction> QueueLevelHandler::get_next_queue_id_first() {
    Torus::Direction dir;
    dir=Torus::Direction::Clockwise;
    if(queues.size()==0){
        return std::make_pair(-1,dir);
    }
    int tmp=queues[first_allocator++];
    if(first_allocator==queues.size()/2){
        first_allocator=0;
    }
    return std::make_pair(tmp,dir);
}
std::pair<int,Torus::Direction> QueueLevelHandler::get_next_queue_id_last() {
    Torus::Direction dir;
    dir=Torus::Direction::Anticlockwise;
    if(queues.size()==0){
        return std::make_pair(-1,dir);
    }
    int tmp=queues[last_allocator++];
    if(last_allocator==queues.size()){
        last_allocator=queues.size()/2;
    }
    return std::make_pair(tmp,dir);
}
QueueLevels::QueueLevels(int total_levels, int queues_per_level,int offset) {
    int start=offset;
    //levels.resize(total_levels);
    for(int i=0;i<total_levels;i++){
        QueueLevelHandler tmp(i,start,start+queues_per_level-1);
        levels.push_back(tmp);
        start+=queues_per_level;
    }
}
QueueLevels::QueueLevels(std::vector<int> lv,int offset) {
    int start=offset;
    //levels.resize(total_levels);
    int l=0;
    for(auto &i:lv){
        QueueLevelHandler tmp(l++,start,start+i-1);
        levels.push_back(tmp);
        start+=i;
    }
}
std::pair<int,Torus::Direction> QueueLevels::get_next_queue_at_level(int level) {
    return levels[level].get_next_queue_id();
}
std::pair<int,Torus::Direction> QueueLevels::get_next_queue_at_level_first(int level) {
    return levels[level].get_next_queue_id_first();
}
std::pair<int,Torus::Direction> QueueLevels::get_next_queue_at_level_last(int level) {
    return levels[level].get_next_queue_id_last();
}
Ring::Ring(Type type,int id,int layer_num, Torus *torus, int data_size, Torus::Dimension dimension,
           Torus::Direction direction,PacketRouting routing,InjectionPolicy injection_policy,bool boost_mode):Algorithm(layer_num) {
    //std::cout<<"Ring checkmark 0"<<std::endl;
    this->type=type;
    this->id=id;
    this->logicalTopology=torus;
    this->data_size=data_size;
    this->dimension=dimension;
    this->direction=direction;
    this->nodes_in_ring=torus->get_nodes_in_ring(dimension);
    this->current_receiver=torus->get_receiver_node(id,dimension,direction);
    this->current_sender=torus->get_sender_node(id,dimension,direction);
    this->parallel_reduce=1;
    this->routing=routing;
    this->injection_policy=injection_policy;
    this->total_packets_sent=0;
    this->total_packets_received=0;
    this->free_packets=0;
    this->zero_latency_packets=0;
    this->non_zero_latency_packets=0;
    this->toggle=false;
    this->name=Name::Ring;
    this->enabled=true;
    if(boost_mode){
        this->enabled=torus->is_enabled(id,dimension);
    }
    switch (dimension){
        case Torus::Dimension::Local:
            transmition=MemBus::Transmition::Fast;
            break;
        default:
            transmition=MemBus::Transmition::Usual;
    }
    switch (type){
        case Type::AllReduce:
            stream_count=2*(nodes_in_ring-1);
            break;
        case Type::AllToAll:
            switch (routing){
                case PacketRouting::Software:
                    this->stream_count=((nodes_in_ring-1)*nodes_in_ring)/2;
                    break;
                case PacketRouting::Hardware:
                    this->stream_count=nodes_in_ring-1;
                    break;
            }
            switch (injection_policy){
                case InjectionPolicy::Aggressive:
                    this->parallel_reduce=nodes_in_ring-1;
                    break;
                case InjectionPolicy::Normal:
                    this->parallel_reduce=1>=nodes_in_ring-1?nodes_in_ring-1:1;
                    break;
                default:
                    this->parallel_reduce=1>=nodes_in_ring-1?nodes_in_ring-1:1;
                    break;
            }
            break;
        default:
            stream_count=nodes_in_ring-1;
    }
    if(type==Type::AllToAll || type==Type::AllGather){
        max_count=0;
    }
    else{
        max_count=nodes_in_ring-1;
    }
    remained_packets_per_message=1;
    remained_packets_per_max_count=1;
    switch (type){
        case Type::AllReduce:
            this->final_data_size=data_size;
            this->msg_size=data_size/nodes_in_ring;
            this->comType=ComType::All_Reduce;
            break;
        case Type::AllGather:
            this->final_data_size=data_size*nodes_in_ring;
            //std::cout<<"heeeey! here! final data size: "<<this->final_data_size<<" ,nodes in ring: "<<nodes_in_ring<<std::endl;
            this->msg_size=data_size;
            this->comType=ComType::All_Gatehr;
            break;
        case Type::ReduceScatter:
            this->final_data_size=data_size/nodes_in_ring;
            this->msg_size=data_size/nodes_in_ring;
            this->comType=ComType::Reduce_Scatter;
            break;
        case Type::AllToAll:
            this->final_data_size=data_size;
            this->msg_size=data_size/nodes_in_ring;
            this->comType=ComType::All_to_All;
            break;
        default:
            ;
    }
    //std::cout<<"ring allreduce is configured at node: "<<id<<" ,with sender: "<<current_sender<<" ,and receiver: "<<current_receiver<<" and enable status is: "<<this->enabled<<std::endl;
}
int Ring::get_non_zero_latency_packets() {
    return (nodes_in_ring-1)*parallel_reduce*1;
}
void Ring::run(EventType event, CallData *data) {
    if(event==EventType::General){
        free_packets+=1;
        ready();
        iteratable();
    }
    else if(event==EventType::PacketReceived){
        total_packets_received++;
        insert_packet(NULL);
    }
    else if(event==EventType::StreamInit){
        for (int i = 0;i<parallel_reduce; i++) {
            insert_packet(NULL);
        }
    }
}
void Ring::release_packets(){
    for(auto packet:locked_packets){
        packet->set_notifier(this);
    }
    if(NPU_to_MA==true){
        (new PacketBundle(stream->owner,stream,locked_packets,processed,send_back,msg_size,transmition))->
                send_to_MA();
    }
    else{
        (new PacketBundle(stream->owner,stream,locked_packets,processed,send_back,msg_size,transmition))->
                send_to_NPU();
    }
    locked_packets.clear();
}
void Ring::process_stream_count(){
    if(remained_packets_per_message>0) {
        remained_packets_per_message--;
    }
    if(id==0){
    }
    if(remained_packets_per_message==0 && stream_count>0){
        stream_count--;
        if(stream_count>0){
            remained_packets_per_message=1;
        }
    }
    if(remained_packets_per_message==0 && stream_count==0 && stream->state!=StreamState::Dead){
        stream->changeState(StreamState::Zombie);
        if(id==0){
            //std::cout<<"stream "<<stream_num<<" changed state to zombie"<<std::endl;
        }
    }
    if(id==0){
        //std::cout<<"for stream: "<<stream_num<<" ,total stream count left: "<<stream_count<<std::endl;
    }
}
void Ring::process_max_count(){
    if(remained_packets_per_max_count>0)
        remained_packets_per_max_count--;
    if(remained_packets_per_max_count==0){
        max_count--;
        if(id==0){
            //std::cout<<"max count is now: "<<max_count<<"stream count is: "<<stream_count<<" , free_packets: "<<free_packets<<std::endl;
        }
        release_packets();
        remained_packets_per_max_count=1;
        if(true){
            if(type==Type::AllToAll && routing==PacketRouting::Hardware){  //Should be fixed to include edge cases
                current_receiver=((Torus *)logicalTopology)->get_receiver_node(current_receiver,dimension,direction);
                current_sender=((Torus *)logicalTopology)->get_sender_node(current_sender,dimension,direction);
                if(id==0 && stream_count>0){
                    //std::cout<<"the all_to_all stream: "<<stream_num<<" with fi streams: "<<steps_finished
                    //<<" has changed its dest from: "<<prev_dest<<"  to: "<<my_current_phase.dest_node<<std::endl;
                }
            }
        }
    }
}
void Ring::reduce(){
    process_stream_count();
    packets.pop_front();
    free_packets--;
    total_packets_sent++;
    //not_delivered++;
}
bool Ring::iteratable(){
    if(stream_count==0 && free_packets==(parallel_reduce*1)){ // && not_delivered==0
        exit();
        return false;
    }
    return true;
}
void Ring::insert_packet(Callable *sender){
    if(!enabled){
        return;
    }
    if(zero_latency_packets==0 && non_zero_latency_packets==0){
        zero_latency_packets=parallel_reduce*1;
        non_zero_latency_packets=get_non_zero_latency_packets(); //(nodes_in_ring-1)*parallel_reduce*1;
        toggle=!toggle;
    }
    if(zero_latency_packets>0){
        packets.push_back(MyPacket(stream->current_queue_id,current_sender,current_receiver)); //vnet Must be changed for alltoall topology
        packets.back().sender=sender;
        locked_packets.push_back(&packets.back());
        processed= false;
        send_back=false;
        NPU_to_MA=true;
        process_max_count();
        zero_latency_packets--;
        return;
    }
    else if(non_zero_latency_packets>0){
        packets.push_back(MyPacket(stream->current_queue_id,current_sender,current_receiver));  //vnet Must be changed for alltoall topology
        packets.back().sender=sender;
        locked_packets.push_back(&packets.back());
        if(type==Type::ReduceScatter || (type==Type::AllReduce && toggle)){
            processed=true;
        }
        else{
            processed=false;
        }
        if(non_zero_latency_packets<=parallel_reduce*1){
            send_back=false;
        }
        else{
            send_back=true;
        }
        NPU_to_MA=false;
        process_max_count();
        non_zero_latency_packets--;
        return;
    }
    //std::cout<<"insert packet called"<<std::endl;
    Sys::sys_panic("should not inject nothing!");
    //}
}
bool Ring::ready(){
    //std::cout<<"ready called"<<std::endl;
    if(stream->state==StreamState::Created || stream->state==StreamState::Ready){
        stream->changeState(StreamState::Executing);
        //init(); //should be replaced
    }
    if(!enabled || packets.size()==0 || stream_count==0 || free_packets==0){
        return false;
    }
    MyPacket packet=packets.front();
    sim_request snd_req;
    snd_req.srcRank = id;
    snd_req.dstRank = packet.preferred_dest;
    snd_req.tag = stream->stream_num;
    snd_req.reqType = UINT8;
    snd_req.vnet=this->stream->current_queue_id;
    snd_req.layerNum=layer_num;
    stream->owner->NI->sim_send(Sys::dummy_data,msg_size,UINT8,packet.preferred_dest,stream->stream_num,&snd_req,&Sys::handleEvent,NULL);//stream_num+(packet.preferred_dest*50)
    sim_request rcv_req;
    rcv_req.vnet=this->stream->current_queue_id;
    rcv_req.layerNum=layer_num;
    RecvPacketEventHadndlerData *ehd=new RecvPacketEventHadndlerData(stream,stream->owner->id,EventType::PacketReceived,packet.preferred_vnet,packet.stream_num);
    stream->owner->NI->sim_recv(Sys::dummy_data,msg_size,UINT8,packet.preferred_src,stream->stream_num,&rcv_req,&Sys::handleEvent,ehd); //stream_num+(owner->id*50)
    reduce();
    if(true){
        //std::cout<<"I am node: "<<owner->id<<" and I sent: "<<++test2<<" packets so far, finished steps: "<<
        //steps_finished<<" ,for stream: "<<stream_num<<" ,total received: "<<test
        //<<" at time: "<<Sys::boostedTick()<<" vnet is: "<<my_current_phase.queue_id<<" ,remained: "<<stream_count<<" ,current dest is: "<<packet.preferred_dest
        //<<" ,waiting for packet from: "<<packet.preferred_src<<std::endl;
    }
    return true;
}
void Ring::exit() {
    //std::cout<<"exiting collective in node: "<<stream->owner->id<<std::endl;
    if(packets.size()!=0){
        packets.clear();
    }
    if(locked_packets.size()!=0){
        locked_packets.clear();
    }
    stream->declare_ready();
    stream->owner->proceed_to_next_vnet_baseline((StreamBaseline*)stream);
    //delete this;
    return;
}
AllToAll::AllToAll(Ring::Type type, int id,int layer_num,AllToAllTopology *allToAllTopology, int data_size, Torus::Dimension dimension,Torus::Direction direction, PacketRouting routing,InjectionPolicy injection_policy,bool boost_mode):
        Ring(type,id,layer_num,((Torus *)allToAllTopology),data_size,dimension,direction,routing,injection_policy,boost_mode) {
    this->name=Name::AllToAll;
    this->enabled=true;
    if(boost_mode){
        this->enabled=allToAllTopology->is_enabled(id,dimension);
    }
    switch (routing){
        case PacketRouting::Hardware:
            parallel_reduce=nodes_in_ring-1;
            break;
        case PacketRouting::Software:
            parallel_reduce=1;
            break;
    }
    /*switch(dimension){
        case Torus::Dimension::Local:
            break;
        default :
            switch (type){
                case Type::AllReduce:
                    stream_count=2*(nodes_in_ring-1);
                    break;
                default:
                    stream_count=(nodes_in_ring-1);
            }
    }*/
}
int AllToAll::get_non_zero_latency_packets() {
    if(dimension!=Torus::Dimension::Local){
        return parallel_reduce*1;
    }
    else{
        return (nodes_in_ring-1)*parallel_reduce*1;
    }
}
void AllToAll::process_max_count() {
    if(remained_packets_per_max_count>0)
        remained_packets_per_max_count--;
    if(remained_packets_per_max_count==0){
        max_count--;
        if(id==0){
            //std::cout<<"max count is now: "<<max_count<<"stream count is: "<<stream_count<<" , free_packets: "<<free_packets<<std::endl;
        }
        release_packets();
        remained_packets_per_max_count=1;
        if(routing==PacketRouting::Hardware){
            current_receiver=((Torus *)logicalTopology)->get_receiver_node(current_receiver,dimension,direction);
            if(current_receiver==id){
                current_receiver=((Torus *)logicalTopology)->get_receiver_node(current_receiver,dimension,direction);
            }
            current_sender=((Torus *)logicalTopology)->get_sender_node(current_sender,dimension,direction);
            if(current_sender==id){
                current_sender=((Torus *)logicalTopology)->get_sender_node(current_sender,dimension,direction);
            }
        }
    }
}
void AllToAll::run(EventType event, CallData *data) {
    if(event==EventType::General){
        free_packets+=1;
        if(type==Type::AllReduce && stream_count<=parallel_reduce){
            if(total_packets_received<parallel_reduce){
                return;
            }
            for (int i = 0;i<parallel_reduce; i++) {
                ready();
            }
            iteratable();
        }
        else{
            ready();
            iteratable();
        }
    }
    else if(event==EventType::PacketReceived){
        total_packets_received++;
        insert_packet(NULL);
    }
    else if(event==EventType::StreamInit){
        for (int i = 0;i<parallel_reduce; i++) {
            insert_packet(NULL);
        }
    }
}
