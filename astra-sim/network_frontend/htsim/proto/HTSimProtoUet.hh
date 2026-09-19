#pragma once

#include "config.h"
#include "clock.h"
#include <climits>
#include <cstdint>

#include "fat_tree_topology.h"
#include "fat_tree_switch.h"
#include "uec.h"
#include "uec_logger.h"
#include "uec_pdcses.h"

#include "HTSimSessionImpl.hh"

namespace HTSim {

#define HOST_NIC 100000

typedef enum {
    UNDEFINED,
    RANDOM,
    ECN,
    COMPOSITE,
    PRIORITY,
    CTRL_PRIO,
    FAIR_PRIO,
    LOSSLESS,
    LOSSLESS_INPUT,
    LOSSLESS_INPUT_ECN,
    COMPOSITE_ECN,
    COMPOSITE_ECN_LB,
    SWIFT_SCHEDULER,
    ECN_PRIO,
    AEOLUS,
    AEOLUS_ECN
} queue_type;

class HTSimProtoUet final : public HTSimSession::HTSimSessionImpl {
    public:
        HTSimProtoUet(const HTSim::tm_info* const tm, int argc, char** argv);
        ~HTSimProtoUet() override;
        void run(const HTSim::tm_info* const tm);
        void stop_simulation();
        void finish();
        void send_flow(HTSim::FlowInfo flow,
                       int flow_id,
                       void (*msg_handler)(void* fun_arg),
                       void* fun_arg);
        void schedule_htsim_event(HTSim::FlowInfo flow, int flow_id, std::optional<uint32_t> msgid);

    private:
        uint32_t cwnd = 0;
        mem_b cwnd_b = 0;
        uint32_t no_of_nodes = 0;
        bool param_queuesize_set = false;
        uint32_t queuesize_pkt = 0;
        int packet_size = 4150;
        uint32_t path_entropy_size = 64;
        uint32_t tiers = 3;         // we support 2 and 3 tier fattrees
        uint32_t planes = 1;        // multi-plane topologies
        uint32_t ports = 1;         // ports per NIC
        bool disable_trim = false;  // Disable trimming, drop instead
        uint16_t trimsize = 64;     // size of a trimmed packet
        std::stringstream filename;
        queue_type qt = COMPOSITE;
        const double ecn_thresh = 0.5;  // default marking threshold for ECN load balancing
        simtime_picosec target_Qdelay = 0;
        simtime_picosec network_max_unloaded_rtt = 0;

        enum LoadBalancing_Algo {BITMAP, REPS, OBLIVIOUS, MIXED, REPS_LEGACY};
        LoadBalancing_Algo load_balancing_algo = MIXED;

        bool param_ecn_set = false;
        bool ecn = true;
        uint32_t ecn_low = 0;
        uint32_t ecn_high = 0;
        uint32_t queue_size_bdp_factor = 0;

        bool receiver_driven = true;
        bool sender_driven = false;

        int seed = 13;
        double pcie_rate = 1.1;

        int end_time = 0;  // in microseconds. 0 means simulation runs until all flows are finished.
        bool force_disable_oversubscribed_cc = false;
        bool enable_accurate_base_rtt = false;

        // unsure how to set this.
        queue_type snd_type = FAIR_PRIO;

        float ar_sticky_delta = 10;

        char* topo_file = NULL;
        int8_t qa_gate = 1;

        bool log_sink = false;
        bool log_nic = false;
        bool log_flow_events = true;
        bool log_tor_downqueue = false;
        bool log_tor_upqueue = false;
        bool log_traffic = true;
        bool log_switches = false;
        bool log_queue_usage = false;

        RouteStrategy route_strategy = NOT_SET;
        FatTreeSwitch::sticky_choices ar_sticky = FatTreeSwitch::PER_PACKET;
        simtime_picosec hop_latency = timeFromUs((uint32_t)1);
        simtime_picosec switch_latency = timeFromUs((uint32_t)0);
        simtime_picosec logtime = timeFromMs(0.25);  // ms;
        linkspeed_bps linkspeed = speedFromMbps((double)HOST_NIC);
        std::vector<UecSrc*> uec_srcs;

        std::unique_ptr<QueueLoggerFactory> qlf;
        std::unique_ptr<UecSinkLoggerSampling> sink_logger;
        std::unique_ptr<NicLoggerSampling> nic_logger;
        std::unique_ptr<TrafficLoggerSimple> traffic_logger;
        std::unique_ptr<FlowEventLoggerSimple> event_logger;
        std::unique_ptr<Logfile> logfile;
        std::unique_ptr<Clock> c;

        UecSrc* uec_src;
        UecSink* uec_snk;

        std::vector<std::unique_ptr<Topology>> topo;
        unique_ptr<FatTreeTopologyCfg> topo_cfg;

        std::vector<UecPullPacer*> pacers;
        std::vector<PCIeModel*> pcie_models;
        std::vector<OversubscribedCC*> oversubscribed_ccs;

        std::vector<UecNIC*> nics;

        map<flowid_t, pair<UecSrc*, UecSink*>> flowmap;
        map<flowid_t, UecPdcSes*> flow_pdc_map;

        uint32_t default_trimming_queuesize_factor = 1;
        uint32_t default_nontrimming_queuesize_factor = 5;
        uint32_t topo_num_failed = 0;
        bool conn_reuse = false;
};

} // namespace HTSim