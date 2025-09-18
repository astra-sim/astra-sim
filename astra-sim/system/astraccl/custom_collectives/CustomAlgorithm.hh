/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __CUSTOMALGORITHM__HH
#define __CUSTOMALGORITHM__HH

#include <stdlib.h>
#include <unistd.h>

#include "astra-sim/system/MemBus.hh"
#include "astra-sim/system/MyPacket.hh"
#include "astra-sim/system/astraccl/Algorithm.hh"
#include "extern/graph_frontend/chakra/src/feeder_v3/et_feeder.h"

namespace AstraSim {

/*
 * CustomAlgorithm class allows users to simulate their own custom algorithms.
 * This class implements the Algorithm interface. The current implementation
 * parses a Chakra ET representation of the custom algorithm.
 *
 * The Chakra ET representation is a file that contains the nodes of the
 * algorithm. The nodes are either COMM_SEND, COMM_RECV, or COMP.
 *
 * To use this implementation, write the "ABSOLUTE" path to the Chakra trace
 * file in the system layer input (instead of traditional `Ring`, etc.), under
 * `{all-reduce|all-to-all|all-gather}-implementation-custom`.
 *
 * For a detailed description on using a Chakra ET based representation, refer
 * to the documentation in the public wiki.
 * TODO: Add a verifier to verify correct communication behavior.
 */
class CustomAlgorithm : public Algorithm {
  public:
    CustomAlgorithm(std::string et_filename, std::string pernode_config_filename, int id);

    // Runs the collective algorithm. This function is only called once to start
    // the algorithm.
    virtual void run(EventType event, CallData* data);
    void call(EventType event, CallData* data);

  private:
    /*
     * The following functions move through the Chakra ET and issues nodes whose
     * dependencies are resolved, Similar to how the Workload layer moves
     * through the workload Chakra ET.
     * TODO: merge with impl in Workload layer.
     */
    void issue(std::shared_ptr<Chakra::FeederV3::ETFeederNode> node);
    void issue_dep_free_nodes();

    // Rank Id
    int id;
    // ET Feeder for the Chakra ET for this specific communication & rank.
    // This is separate from the ET Feeder in the Workload layer, which is used
    // to traverse the whole workload Chakra ET.
    Chakra::ETFeeder* et_feeder;
};

}  // namespace AstraSim

#endif /* __CUSTOMALGORITHM__HH */
