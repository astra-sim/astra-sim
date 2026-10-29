#pragma once

#include <HTSimSession.hh>
#include <astra-network-analytical/common/Type.h>
#include <astra-network-analytical/congestion_unaware/Topology.h>
#include <astra-sim/common/AstraNetworkAPI.hh>
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
class HTSimNetworkApi final : public AstraNetworkAPI {
  public:
    /**
     * Set the topology to be used.
     *
     * @param topology_ptr pointer to the to
     */
    static void set_topology(std::shared_ptr<Topology> topology_ptr) noexcept;

    static void set_completion_tracker(
        std::shared_ptr<CompletionTracker> completion_tracker_ptr) noexcept;
    /**
     * Constructor.
     *
     * @param rank id of the API
     */
    explicit HTSimNetworkApi(int rank) noexcept;

    /**
     * Implement sim_cmd of AstraNetworkAPI.
     */
    int sim_cmd(CommandType cmd_type,
                const sim_request& request,
                void (*msg_handler)(void* fun_arg),
                void* fun_arg) override;

    AstraSim::timespec_t sim_get_time() override;

    void sim_schedule(timespec_t delta,
                      void (*fun_ptr)(void* fun_arg),
                      void* fun_arg) override;

    void sim_notify_finished() override;

    /**
     * Implement get_BW_at_dimension of AstraNetworkAPI.
     */
    double get_BW_at_dimension(int dim) const override;

  private:
    int sim_send_impl(uint64_t count,
                      int dst,
                      int tag,
                      void (*msg_handler)(void* fun_arg),
                      void* fun_arg);

    int sim_recv_impl(uint64_t message_size,
                      int src,
                      int tag,
                      void (*msg_handler)(void* fun_arg),
                      void* fun_arg);

    /// topology
    static std::shared_ptr<Topology> topology;
    static unsigned flow_id;
    static std::shared_ptr<CompletionTracker> completion_tracker;

    // dimension-related values, formerly inherited from CommonNetworkApi --
    // htsim derives from AstraNetworkAPI directly, so it carries its own copy.
    static std::vector<Bandwidth> bandwidth_per_dim;
    static int dims_count;

  public:
    inline static HTSim::tm_info htsim_info;
};

}  // namespace HTSim
