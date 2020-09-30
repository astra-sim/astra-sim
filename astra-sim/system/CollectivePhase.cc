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

#include "CollectivePhase.hh"
#include "Algorithm.hh"
namespace AstraSim{
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
}