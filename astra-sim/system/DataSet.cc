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

#include "DataSet.hh"
#include "Sys.hh"
#include "IntData.hh"
namespace AstraSim{
    int DataSet::id_auto_increment=0;
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
}