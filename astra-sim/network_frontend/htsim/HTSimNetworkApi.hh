#pragma once

#include "common/CommonNetworkApi.hh"
#include "HTSimSession.hh"
#include <astra-network-analytical/common/Type.h>
#include <astra-network-analytical/congestion_unaware/Topology.h>
#include <cstdint>
#include <vector>
using namespace AstraSim;
using namespace AstraSimAnalytical;
using namespace NetworkAnalytical;
using namespace NetworkAnalyticalCongestionUnaware;

namespace HTSim {

class CompletionTracker {
  public:
    CompletionTracker(int num_ranks) {
            num_unfinished_ranks_ = num_ranks;
            completion_tracker = std::vector<int>(num_ranks, 0);
    }
    bool all_finished();
    void mark_rank_as_finished(int rank);

  private:
    int num_unfinished_ranks_;
    std::vector<int> completion_tracker;
};

/**
 * HTSimApi is a AstraNetworkAPI
 * implemented for HTSim network backend.
 */
class HTSimNetworkApi final : public CommonNetworkApi {
  public:

     static void set_completion_tracker(std::shared_ptr<CompletionTracker> completion_tracker_ptr) noexcept;
    /**
     * Constructor.
     *
     * @param rank id of the API
     */
    explicit HTSimNetworkApi(int rank) noexcept;

    /**
     * Implement sim_send of AstraNetworkAPI.
     */
    int sim_send(void* buffer,
                 uint64_t count,
                 int type,
                 int dst,
                 int tag,
                 sim_request* request,
                 void (*msg_handler)(void* fun_arg),
                 void* fun_arg) override;

    int sim_recv(void* buffer,
                 uint64_t message_size,
                 int type,
                 int src,
                 int tag,
                 AstraSim::sim_request* request,
                 void (*msg_handler)(void* fun_arg),
                 void* fun_arg) override;

    AstraSim::timespec_t sim_get_time() override;

    void sim_schedule(timespec_t delta, void (*fun_ptr)(void* fun_arg), void* fun_arg) override;

    void sim_notify_finished() override;

  private:
    /// topology
    static std::shared_ptr<Topology> topology;
    static std::shared_ptr<CompletionTracker> completion_tracker;

  public:
    inline static HTSim::tm_info htsim_info;
};

}  // namespace HTSim
