//
// Created by Saeed Rashidi on 2/21/23.
//

#include "InSwitch.hh"
#include <assert.h>
#include "astra-sim/system/Common.hh"
#include "astra-sim/system/RecvPacketEventHandlerData.hh"

namespace AstraSim {
InSwitch::InSwitch(int id, Switch* sw, uint64_t data_size, ComType type) {
    this->id=id;
    this->sw=sw;
    this->data_size=data_size;
    this->type=type;
    this->state=State::npu_sending;
    this->num_msg_received=0;

  }
  void InSwitch::run(EventType event, CallData* data) {
    if(type==ComType::All_Reduce)
      run_all_reduce(event,data);
    else if(type==ComType::Reduce_Scatter)
      run_reduce_scatter(event,data);
    else if(type==ComType::All_Gather)
      run_all_gather(event,data);
    else if(type==ComType::All_to_All)
      run_all_to_all(event,data);
    else
      assert(false);
  }
  void InSwitch::run_reduce_scatter(EventType event, CallData* data) {
    if(state==State::npu_sending){
      //sending
      sim_request snd_req;
      snd_req.srcRank = stream->owner->id;
      snd_req.dstRank = sw->sw->id;
      snd_req.tag = stream->stream_id;
      snd_req.reqType = UINT8;
      snd_req.vnet = this->stream->current_queue_id;
      stream->owner->front_end_sim_send(
          0,
          Sys::dummy_data,
          data_size,
          UINT8,
          sw->sw->id,
          stream->stream_id,
          &snd_req,
          &Sys::handleEvent,
          nullptr);
      // receiving
      sim_request rcv_req;
      rcv_req.vnet = this->stream->current_queue_id;
      RecvPacketEventHandlerData* ehd = new RecvPacketEventHandlerData(
          stream,
          stream->owner->id,
          EventType::PacketReceived,
          stream->current_queue_id,
          stream->stream_id);
      stream->owner->front_end_sim_recv(
          0,
          Sys::dummy_data,
          data_size / sw->attached_NPUs.size(),
          UINT8,
          sw->sw->id,
          stream->stream_id,
          &rcv_req,
          &Sys::handleEvent,
          ehd);
    }
    if(stream->owner->id!=sw->attached_NPUs[0]->id){
      if(state==State::npu_sending){
        state=State::npu_receiving;
      }
      else{
        assert(event==EventType::PacketReceived);
        exit();
        return;
      }
    }
    else{
      if(state==State::npu_sending){
        // switch receiving
        for(int i=0;i<sw->attached_NPUs.size();i++){
          sim_request rcv_req;
          rcv_req.vnet = this->stream->current_queue_id;
          RecvPacketEventHandlerData* ehd = new RecvPacketEventHandlerData(
              stream,
              stream->owner->id,
              EventType::PacketReceived,
              stream->current_queue_id,
              stream->stream_id);
          sw->sw->front_end_sim_recv(
              0,
              Sys::dummy_data,
              data_size,
              UINT8,
              sw->attached_NPUs[i]->id,
              stream->stream_id,
              &rcv_req,
              &Sys::handleEvent,
              ehd);
        }
        state=State::switch_receiving;
      }
      else if(state==State::switch_receiving){
        assert(event==EventType::PacketReceived);
        num_msg_received++;
        if(num_msg_received==sw->attached_NPUs.size()){
          state=State::switch_processing;
          //std::cout<<"switch processing started at: "<<Sys::boostedTick()<<std::endl;
          stream->owner->register_event(stream,EventType::Switch_Processing, nullptr,100);
          return;
        }
      }
      else if(state==State::switch_processing){
        //switch sending
        for(int i=0;i<sw->attached_NPUs.size();i++){
          sim_request snd_req;
          snd_req.srcRank = stream->owner->id;
          snd_req.dstRank = sw->sw->id;
          snd_req.tag = stream->stream_id;
          snd_req.reqType = UINT8;
          snd_req.vnet = this->stream->current_queue_id;
          sw->sw->front_end_sim_send(
              0,
              Sys::dummy_data,
              data_size / sw->attached_NPUs.size(),
              UINT8,
              sw->attached_NPUs[i]->id,
              stream->stream_id,
              &snd_req,
              &Sys::handleEvent,
              nullptr);
        }
        state=State::npu_receiving;
      }
      else if(state==State::npu_receiving){
        assert(event==EventType::PacketReceived);
        exit();
        return;
      }
      else{
        assert(false);
      }
    }
  }
  void InSwitch::run_all_reduce(EventType event, CallData* data) {
    if(state==State::npu_sending){
      //sending
      sim_request snd_req;
      snd_req.srcRank = stream->owner->id;
      snd_req.dstRank = sw->sw->id;
      snd_req.tag = stream->stream_id;
      snd_req.reqType = UINT8;
      snd_req.vnet = this->stream->current_queue_id;
      stream->owner->front_end_sim_send(
          0,
          Sys::dummy_data,
          data_size,
          UINT8,
          sw->sw->id,
          stream->stream_id,
          &snd_req,
          &Sys::handleEvent,
          nullptr);
      // receiving
      sim_request rcv_req;
      rcv_req.vnet = this->stream->current_queue_id;
      RecvPacketEventHandlerData* ehd = new RecvPacketEventHandlerData(
          stream,
          stream->owner->id,
          EventType::PacketReceived,
          stream->current_queue_id,
          stream->stream_id);
      stream->owner->front_end_sim_recv(
          0,
          Sys::dummy_data,
          data_size,
          UINT8,
          sw->sw->id,
          stream->stream_id,
          &rcv_req,
          &Sys::handleEvent,
          ehd);
    }
    if(stream->owner->id!=sw->attached_NPUs[0]->id){
      if(state==State::npu_sending){
        state=State::npu_receiving;
      }
      else{
        assert(event==EventType::PacketReceived);
        exit();
        return;
      }
    }
    else{
      if(state==State::npu_sending){
        // switch receiving
        for(int i=0;i<sw->attached_NPUs.size();i++){
          sim_request rcv_req;
          rcv_req.vnet = this->stream->current_queue_id;
          RecvPacketEventHandlerData* ehd = new RecvPacketEventHandlerData(
              stream,
              stream->owner->id,
              EventType::PacketReceived,
              stream->current_queue_id,
              stream->stream_id);
          sw->sw->front_end_sim_recv(
              0,
              Sys::dummy_data,
              data_size,
              UINT8,
              sw->attached_NPUs[i]->id,
              stream->stream_id,
              &rcv_req,
              &Sys::handleEvent,
              ehd);
        }
        state=State::switch_receiving;
      }
      else if(state==State::switch_receiving){
        assert(event==EventType::PacketReceived);
        num_msg_received++;
        if(num_msg_received==sw->attached_NPUs.size()){
          state=State::switch_processing;
          //std::cout<<"switch processing started at: "<<Sys::boostedTick()<<std::endl;
          stream->owner->register_event(stream,EventType::Switch_Processing, nullptr,100);
          return;
        }
      }
      else if(state==State::switch_processing){
        //switch sending
        for(int i=0;i<sw->attached_NPUs.size();i++){
          sim_request snd_req;
          snd_req.srcRank = stream->owner->id;
          snd_req.dstRank = sw->sw->id;
          snd_req.tag = stream->stream_id;
          snd_req.reqType = UINT8;
          snd_req.vnet = this->stream->current_queue_id;
          sw->sw->front_end_sim_send(
              0,
              Sys::dummy_data,
              data_size,
              UINT8,
              sw->attached_NPUs[i]->id,
              stream->stream_id,
              &snd_req,
              &Sys::handleEvent,
              nullptr);
        }
        state=State::npu_receiving;
      }
      else if(state==State::npu_receiving){
        assert(event==EventType::PacketReceived);
        exit();
        return;
      }
      else{
        assert(false);
      }
    }
  }
  void InSwitch::run_all_gather(EventType event, CallData* data) {
    if(state==State::npu_sending){
      //sending
      sim_request snd_req;
      snd_req.srcRank = stream->owner->id;
      snd_req.dstRank = sw->sw->id;
      snd_req.tag = stream->stream_id;
      snd_req.reqType = UINT8;
      snd_req.vnet = this->stream->current_queue_id;
      stream->owner->front_end_sim_send(
          0,
          Sys::dummy_data,
          data_size,
          UINT8,
          sw->sw->id,
          stream->stream_id,
          &snd_req,
          &Sys::handleEvent,
          nullptr);
      // receiving
      sim_request rcv_req;
      rcv_req.vnet = this->stream->current_queue_id;
      RecvPacketEventHandlerData* ehd = new RecvPacketEventHandlerData(
          stream,
          stream->owner->id,
          EventType::PacketReceived,
          stream->current_queue_id,
          stream->stream_id);
      stream->owner->front_end_sim_recv(
          0,
          Sys::dummy_data,
          (sw->attached_NPUs.size()-1)*data_size,
          UINT8,
          sw->sw->id,
          stream->stream_id,
          &rcv_req,
          &Sys::handleEvent,
          ehd);
    }
    if(stream->owner->id!=sw->attached_NPUs[0]->id){
      if(state==State::npu_sending){
        state=State::npu_receiving;
      }
      else{
        assert(event==EventType::PacketReceived);
        exit();
        return;
      }
    }
    else{
      if(state==State::npu_sending){
        // switch receiving
        for(int i=0;i<sw->attached_NPUs.size();i++){
          sim_request rcv_req;
          rcv_req.vnet = this->stream->current_queue_id;
          RecvPacketEventHandlerData* ehd = new RecvPacketEventHandlerData(
              stream,
              stream->owner->id,
              EventType::PacketReceived,
              stream->current_queue_id,
              stream->stream_id);
          sw->sw->front_end_sim_recv(
              0,
              Sys::dummy_data,
              data_size,
              UINT8,
              sw->attached_NPUs[i]->id,
              stream->stream_id,
              &rcv_req,
              &Sys::handleEvent,
              ehd);
        }
        state=State::switch_receiving;
      }
      else if(state==State::switch_receiving){
        assert(event==EventType::PacketReceived);
        num_msg_received++;
        if(num_msg_received==sw->attached_NPUs.size()){
          state=State::npu_receiving;
          //switch sending
          for(int i=0;i<sw->attached_NPUs.size();i++){
            sim_request snd_req;
            snd_req.srcRank = stream->owner->id;
            snd_req.dstRank = sw->sw->id;
            snd_req.tag = stream->stream_id;
            snd_req.reqType = UINT8;
            snd_req.vnet = this->stream->current_queue_id;
            sw->sw->front_end_sim_send(
                0,
                Sys::dummy_data,
                (sw->attached_NPUs.size()-1)*data_size,
                UINT8,
                sw->attached_NPUs[i]->id,
                stream->stream_id,
                &snd_req,
                &Sys::handleEvent,
                nullptr);
          }
          state=State::npu_receiving;
        }
      }
      else if(state==State::npu_receiving){
        assert(event==EventType::PacketReceived);
        exit();
        return;
      }
      else{
        assert(false);
      }
    }
  }
  void InSwitch::run_all_to_all(EventType event, CallData* data) {
    if(state==State::npu_sending){
      //sending
      sim_request snd_req;
      snd_req.srcRank = stream->owner->id;
      snd_req.dstRank = sw->sw->id;
      snd_req.tag = stream->stream_id;
      snd_req.reqType = UINT8;
      snd_req.vnet = this->stream->current_queue_id;
      stream->owner->front_end_sim_send(
          0,
          Sys::dummy_data,
          (sw->attached_NPUs.size()-1)*(data_size/sw->attached_NPUs.size()),
          UINT8,
          sw->sw->id,
          stream->stream_id,
          &snd_req,
          &Sys::handleEvent,
          nullptr);
      // receiving
      sim_request rcv_req;
      rcv_req.vnet = this->stream->current_queue_id;
      RecvPacketEventHandlerData* ehd = new RecvPacketEventHandlerData(
          stream,
          stream->owner->id,
          EventType::PacketReceived,
          stream->current_queue_id,
          stream->stream_id);
      stream->owner->front_end_sim_recv(
          0,
          Sys::dummy_data,
          (sw->attached_NPUs.size()-1)*(data_size/sw->attached_NPUs.size()),
          UINT8,
          sw->sw->id,
          stream->stream_id,
          &rcv_req,
          &Sys::handleEvent,
          ehd);
    }
    if(stream->owner->id!=sw->attached_NPUs[0]->id){
      if(state==State::npu_sending){
        state=State::npu_receiving;
      }
      else{
        assert(event==EventType::PacketReceived);
        exit();
        return;
      }
    }
    else{
      if(state==State::npu_sending){
        // switch receiving
        for(int i=0;i<sw->attached_NPUs.size();i++){
          sim_request rcv_req;
          rcv_req.vnet = this->stream->current_queue_id;
          RecvPacketEventHandlerData* ehd = new RecvPacketEventHandlerData(
              stream,
              stream->owner->id,
              EventType::PacketReceived,
              stream->current_queue_id,
              stream->stream_id);
          sw->sw->front_end_sim_recv(
              0,
              Sys::dummy_data,
              (sw->attached_NPUs.size()-1)*(data_size/sw->attached_NPUs.size()),
              UINT8,
              sw->attached_NPUs[i]->id,
              stream->stream_id,
              &rcv_req,
              &Sys::handleEvent,
              ehd);
        }
        state=State::switch_receiving;
      }
      else if(state==State::switch_receiving){
        assert(event==EventType::PacketReceived);
        num_msg_received++;
        if(num_msg_received==sw->attached_NPUs.size()){
          state=State::npu_receiving;
          //switch sending
          for(int i=0;i<sw->attached_NPUs.size();i++){
            sim_request snd_req;
            snd_req.srcRank = stream->owner->id;
            snd_req.dstRank = sw->sw->id;
            snd_req.tag = stream->stream_id;
            snd_req.reqType = UINT8;
            snd_req.vnet = this->stream->current_queue_id;
            sw->sw->front_end_sim_send(
                0,
                Sys::dummy_data,
                (sw->attached_NPUs.size()-1)*(data_size/sw->attached_NPUs.size()),
                UINT8,
                sw->attached_NPUs[i]->id,
                stream->stream_id,
                &snd_req,
                &Sys::handleEvent,
                nullptr);
          }
          state=State::npu_receiving;
        }
      }
      else if(state==State::npu_receiving){
        assert(event==EventType::PacketReceived);
        exit();
        return;
      }
      else{
        assert(false);
      }
    }
  }
}

