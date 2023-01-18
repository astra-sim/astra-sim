/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __DOUBLE_BINARY_TREE_ALL_REDUCE_HH__
#define __DOUBLE_BINARY_TREE_ALL_REDUCE_HH__

#include "astra-sim/system/collective/Algorithm.hh"
#include "astra-sim/system/CallData.hh"
#include "astra-sim/system/topology/BinaryTree.hh"

namespace AstraSim {

class DoubleBinaryTreeAllReduce : public Algorithm {
 public:
  enum class State {
    Begin = 0,
    WaitingForTwoChildData,
    WaitingForOneChildData,
    SendingDataToParent,
    WaitingDataFromParent,
    SendingDataToChilds,
    End
  };

  DoubleBinaryTreeAllReduce(
      int id, BinaryTree* tree, uint64_t data_size);
  void run(EventType event, CallData* data);

  BinaryTree::Type type;
  State state;
  int parent;
  int left_child;
  int reductions;
  int right_child;
};

} // namespace AstraSim

#endif /* __DOUBLE_BINARY_TREE_ALL_REDUCE_HH__ */
