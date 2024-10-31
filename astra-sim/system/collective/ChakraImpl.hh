/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __CHAKRAIMPL__HH
#define __CHAKRAIMPL__HH

#include <stdlib.h>
#include <unistd.h>

#include "astra-sim/system/MemBus.hh"
#include "astra-sim/system/MyPacket.hh"
#include "astra-sim/system/collective/Algorithm.hh"
#include "extern/graph_frontend/chakra/src/feeder/et_feeder.h"

namespace AstraSim {

class HardwareResourceChakra {
  public:
    HardwareResourceChakra();
    void occupy(const std::shared_ptr<Chakra::ETFeederNode> node);
    void release(const std::shared_ptr<Chakra::ETFeederNode> node);
    bool is_available(const std::shared_ptr<Chakra::ETFeederNode> node) const;

    uint32_t num_in_flight_cpu_ops;
    uint32_t num_in_flight_gpu_comp_ops;
    uint32_t num_in_flight_gpu_comm_ops;
};

/*
 * ChakraImpl contains the logic to parse and execute a Chakra ET representation
 * of a collective implementation. (instead of the typical way of implementing
 * collectives within the system layer and executing the code.)
 *
 * This Chakra trace consists of only COMM_SEND, COMM_RECV, COMP nodes to
 * describe the messages that make up a Collective. (note: no verifier!) This
 * allows us to easily define different implementations of the same algorithm,
 * or, even define our own custom algorithms using tools such as MSCCLang. To
 * use this implementation, write the "ABSOLUTE" path to the Chakra trace file
 * in the system layer input (instead of traditional `Ring`, etc.), under
 * `{all-reduce|all-to-all|all-gather}-implementation-chakra`.
 *
 * For a detailed description on using a Chakra ET based representation, refer
 * to the documentation.
 * TODO: Add a verifier to verify correct communication behavior.
 */
class ChakraImpl : public Algorithm {
 public:
  ChakraImpl(std::string et_filename, int id);

  // Runs the collective algorithm. This function is only called once to start
  // the algorithm.
  virtual void run(EventType event, CallData* data);
  void call(EventType event, CallData* data);

 private:
  /*
   * The following functions move through the Chakra ET and issues nodes whose
   * dependencies are resolved, Similar to how the Workload layer moves through
   * the workload Chakra ET.
   * TODO: merge with impl in Workload layer.
   */
  void issue(std::shared_ptr<Chakra::ETFeederNode> node);
  void issue_dep_free_nodes();

  // Rank Id
  int id;
  // ET Feeder for the Chakra ET for this specific rank.
  Chakra::ETFeeder* et_feeder;
  // Tracks availability of hardware resources (e.g. prevent two send ET nodes
  // at same time).
  // TODO: merge with impl in Workload layer.
  HardwareResourceChakra* hw_resource;
};

}  // namespace AstraSim

#endif /* __CHAKRAIMPL__HH */
