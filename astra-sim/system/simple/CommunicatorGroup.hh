// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <assert.h>
#include <map>
#include <vector>

#include <astra-sim/common/Common.hh>

namespace AstraSim {

class SimpleSys;
class CollectivePlan;
class CommunicatorGroup {
  public:
    CommunicatorGroup(int id,
                      std::vector<int> involved_NPUs,
                      SimpleSys* generator);
    CollectivePlan* get_collective_plan(ComType comm_type);
    void set_id(int id);
    void setLDSTImpl(bool _isLDST) {
        isLDST = _isLDST;
    }
    bool isLDSTImpl() {
        return isLDST;
    }
    int get_position_in_group();
    ~CommunicatorGroup();

    std::vector<int> involved_NPUs;
    int num_streams;

  private:
    int id;
    bool isLDST;
    // Position of the generating NPU within involved_NPUs, or -1 if it is
    // not a member of this comm group.
    int pos_in_group;
    SimpleSys* generator;
    std::map<ComType, CollectivePlan*> comm_plans;
};

}  // namespace AstraSim
