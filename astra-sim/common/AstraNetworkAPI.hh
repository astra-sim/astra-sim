/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __ASTRA_NETWORK_API_HH__
#define __ASTRA_NETWORK_API_HH__

#include "astra-sim/system/Common.hh"

namespace AstraSim {

class AstraNetworkAPI {
 public:
  enum class BackendType { NotSpecified = 0, Garnet, NS3, Analytical };

  AstraNetworkAPI(int rank) : rank(rank) {};
  virtual ~AstraNetworkAPI() {};

  virtual int sim_send(
      void* buffer,
      uint64_t count,
      int type,
      int dst,
      int tag,
      sim_request* request,
      void (*msg_handler)(void* fun_arg),
      void* fun_arg) = 0;

  virtual int sim_recv(
      void* buffer,
      uint64_t count,
      int type,
      int src,
      int tag,
      sim_request* request,
      void (*msg_handler)(void* fun_arg),
      void* fun_arg) = 0;

  /*
   * sim_schedule is used when ASTRA-sim wants to schedule an event on the
   * network backend. delta: The relative time difference between the current
   * time and the time when the event is triggered, as observed by the network
   * simulator. fun_ptr: The event handler to be triggered at the scheduled
   * time. fun_arg: Arguments to pass into fun_ptr.
   */
  virtual void sim_schedule(
      timespec_t delta,
      void (*fun_ptr)(void* fun_arg),
      void* fun_arg) = 0;

  virtual BackendType get_backend_type() {
    return BackendType::NotSpecified;
  };

  virtual int sim_comm_get_rank() {
    return rank;
  };

  virtual int sim_comm_set_rank(int rank) {
    this->rank = rank;
    return this->rank;
  };

  virtual timespec_t sim_get_time() = 0;

  virtual double get_BW_at_dimension(int dim) {
    return -1;
  };

  int rank;
};

} // namespace AstraSim

#endif /* __ASTRA_NETWORK_API_HH__ */
