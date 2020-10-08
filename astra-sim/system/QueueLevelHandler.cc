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

#include "QueueLevelHandler.hh"
namespace AstraSim{
    QueueLevelHandler::QueueLevelHandler(int level,int start, int end) {
        for(int i=start;i<=end;i++){
            queues.push_back(i);
        }
        allocator=0;
        first_allocator=0;
        last_allocator=queues.size()/2;
        this->level=level;
    }
    std::pair<int,RingTopology::Direction> QueueLevelHandler::get_next_queue_id() {
        RingTopology::Direction dir;
        if(level!=0 && queues.size()>1 && allocator>=(queues.size()/2)){
            dir=RingTopology::Direction::Anticlockwise;
        }
        else{
            dir=RingTopology::Direction::Clockwise;
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
    std::pair<int,RingTopology::Direction> QueueLevelHandler::get_next_queue_id_first() {
        RingTopology::Direction dir;
        dir=RingTopology::Direction::Clockwise;
        if(queues.size()==0){
            return std::make_pair(-1,dir);
        }
        int tmp=queues[first_allocator++];
        if(first_allocator==queues.size()/2){
            first_allocator=0;
        }
        return std::make_pair(tmp,dir);
    }
    std::pair<int,RingTopology::Direction> QueueLevelHandler::get_next_queue_id_last() {
        RingTopology::Direction dir;
        dir=RingTopology::Direction::Anticlockwise;
        if(queues.size()==0){
            return std::make_pair(-1,dir);
        }
        int tmp=queues[last_allocator++];
        if(last_allocator==queues.size()){
            last_allocator=queues.size()/2;
        }
        return std::make_pair(tmp,dir);
    }
}