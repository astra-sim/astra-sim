// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <astra-sim/system/simple/CommunicatorGroup.hh>

#include <algorithm>

#include <astra-sim/system/simple/CollectivePlan.hh>
#include <astra-sim/system/simple/SimpleSys.hh>

using namespace AstraSim;

CommunicatorGroup::CommunicatorGroup(int id,
                                     std::vector<int> involved_NPUs,
                                     SimpleSys* generator) {
    set_id(id);
    this->involved_NPUs = involved_NPUs;
    this->generator = generator;
    std::sort(involved_NPUs.begin(), involved_NPUs.end());

    // -1 means the rank is not in the comm group.
    int position = -1;
    for (int i = 0; i < involved_NPUs.size(); ++i) {
        if (involved_NPUs[i] == generator->id) {
            position = i;
            break;
        }
    }
    pos_in_group = position;
    setLDSTImpl(false);  // default is bulk DMA based implementation
}

CommunicatorGroup::~CommunicatorGroup() {
    for (auto cg : comm_plans) {
        CollectivePlan* cp = cg.second;
        delete cp;
    }
}

void CommunicatorGroup::set_id(int id) {
    assert(id > 0);
    this->id = id;
    this->num_streams = id * 1000000;
}

CollectivePlan* CommunicatorGroup::get_collective_plan(ComType comm_type) {
    if (comm_plans.find(comm_type) != comm_plans.end()) {
        return comm_plans[comm_type];
    }

    if (static_cast<uint64_t>(generator->total_nodes) == involved_NPUs.size()) {
        LogicalTopology* logical_topology =
            generator->get_logical_topology(comm_type);
        std::vector<CollectiveImpl*> collective_implementation =
            generator->get_collective_implementation(comm_type);
        std::vector<bool> dimensions_involved(10, true);
        bool should_be_removed = false;
        comm_plans[comm_type] =
            new CollectivePlan(logical_topology, collective_implementation,
                               dimensions_involved, should_be_removed);
        return comm_plans[comm_type];
    } else {
        std::vector<CollectiveImpl*> collective_implementation =
            generator->get_collective_implementation(comm_type);
        if (collective_implementation.size() > 1) {
            // This means that everything fell through and we got a native
            // collective that is multi-dimensional. The current logic
            // requires that, for a comm group that is smaller than the whole
            // cluster, we reduce everything to one dimension since the
            // logical dimension no longer matches/matters.
            collective_implementation = std::vector<CollectiveImpl*>{
                new CollectiveImpl(CollectiveImplType::Ring)};
        }
        LogicalTopology* logical_topology = new RingTopology(
            RingTopology::Dimension::Local, generator->id, involved_NPUs);
        std::vector<bool> dimensions_involved(1, true);
        bool should_be_removed = true;
        comm_plans[comm_type] =
            new CollectivePlan(logical_topology, collective_implementation,
                               dimensions_involved, should_be_removed);
        return comm_plans[comm_type];
    }
    assert(false);
    return nullptr;
}

int CommunicatorGroup::get_position_in_group() {
    return pos_in_group;
}
