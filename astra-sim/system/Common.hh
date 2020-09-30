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

#ifndef __COMMON_HH__
#define __COMMON_HH__
#include "AstraNetworkAPI.hh"
namespace AstraSim{
    #define CLOCK_PERIOD 1
    typedef unsigned long long Tick;
    enum class ComType {None,Reduce_Scatter, All_Gatehr, All_Reduce,All_to_All,All_Reduce_All_to_All};
    enum class CollectiveBarrier{Blocking,Non_Blocking};
    enum class SchedulingPolicy {LIFO,FIFO,HIGHEST,None};
    enum class InjectionPolicy {Infinite,Aggressive,SemiAggressive,ExtraAggressive,Normal};
    enum class PacketRouting {Hardware,Software};
    enum class BusType {Both,Shared,Mem};
    enum class StreamState {Created,Transferring,Ready,Executing,Zombie,Dead};
    enum class EventType {RendezvousSend,RendezvousRecv,CallEvents,PacketReceived,WaitForVnetTurn,General,TX_DMA,RX_DMA,Wight_Grad_Comm_Finished,Input_Grad_Comm_Finished,Fwd_Comm_Finished,Wight_Grad_Comm_Finished_After_Delay,Input_Grad_Comm_Finished_After_Delay,Fwd_Comm_Finished_After_Delay,Workload_Wait,Reduction_Ready,Rec_Finished,Send_Finished,
        Processing_Finished,Delivered,NPU_to_MA,MA_to_NPU,Read_Port_Free,Write_Port_Free,Apply_Boost,Stream_Transfer_Started,Stream_Ready,Consider_Process,Consider_Retire,Consider_Send_Back,StreamInit,StreamsFinishedIncrease,CommProcessingFinished,NotInitialized};
}
#endif
