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

#include "SimSendCaller.hh"
#include "Sys.hh"
namespace AstraSim {
SimSendCaller::SimSendCaller(
    Sys* generator,
    void* buffer,
    int count,
    int type,
    int dst,
    int tag,
    sim_request request,
    void (*msg_handler)(void* fun_arg),
    void* fun_arg) {
  this->generator = generator;
  this->buffer = buffer;
  this->count = count;
  this->type = type;
  this->dst = dst;
  this->tag = tag;
  this->request = request;
  this->msg_handler = msg_handler;
  this->fun_arg = fun_arg;
}
void SimSendCaller::call(EventType type, CallData* data) {
  generator->NI->sim_send(
      this->buffer,
      this->count,
      this->type,
      this->dst,
      this->tag,
      &this->request,
      this->msg_handler,
      this->fun_arg);
  delete this;
}
} // namespace AstraSim