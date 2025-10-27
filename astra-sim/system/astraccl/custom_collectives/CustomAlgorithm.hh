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
#include "astra-sim/system/CommunicatorGroup.hh"

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
    CustomAlgorithm(std::string et_filename, int id, int pos_in_comm, CommunicatorGroup* comm_group);

    // Runs the collective algorithm. This function is only called once to start
    // the algorithm.
    virtual void run(EventType event, CallData* data);
    void call(EventType event, CallData* data);
    // When running a e.g. 4 rank algorithm on a comm group of ranks 1, 3, 5, 7,
    // The et file for the algorithm will have ranks 0~3.
    // This function converts 0,1,2,3 to 1,3,5,7, respectively.
    int convert_algo_rank_to_real_rank(int algo_rank);

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
    CommunicatorGroup* comm_group;
};

}  // namespace AstraSim

#endif /* __CUSTOMALGORITHM__HH */
