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

#include "Algorithm.hh"
namespace AstraSim {
Algorithm::Algorithm(int layer_num) {
  this->layer_num = layer_num;
  enabled = true;
}
void Algorithm::init(BaseStream* stream) {
  this->stream = stream;
  return;
}
void Algorithm::call(EventType event, CallData* data) {
  return;
}
void Algorithm::exit() {
  // std::cout<<"exiting collective in node: "<<stream->owner->id<<std::endl;
  stream->declare_ready();
  stream->owner->proceed_to_next_vnet_baseline((StreamBaseline*)stream);
  // delete this;
  return;
}
} // namespace AstraSim