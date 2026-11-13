#include "HTSimProtoUet.hh"

#include "HTSimSession.hh"

#include <cstring>
#include <optional>

#include "compositequeue.h"
#include "connection_matrix.h"
#include "logfile.h"
#include "network.h"
#include "oversubscribed_cc.h"
#include "pciemodel.h"
#include "pipe.h"
#include "topology.h"
#include "uec_mp.h"
#include "uec_base.h"
#include "uec_pdcses.h"

#include "fat_tree_topology.h"

// Simulation params

#include "main.h"

namespace HTSim {

enum reason { PCIE = 0, OVERSUBSCRIBED_CC = 1 };

static void exit_error(char* progr) {
    std::cout
        << "Usage " << progr
        << " [-nodes N]\n\t[-conns C]\n\t[-cwnd cwnd_size]\n\t[-q "
            "queue_size]\n\t[-recv_oversub_cc] Use receiver-driven AIMD to reduce total window when "
            "trims are not last hop\n\t[-queue_type "
            "composite|random|lossless|lossless_input|]\n\t[-tm traffic_matrix_file]\n\t[-strat "
            "route_strategy (single,rand,perm,pull,ecmp,\n\tecmp_host "
            "path_count,ecmp_ar,ecmp_rr,\n\tecmp_host_ar ar_thresh)]\n\t[-log log_level]\n\t[-seed "
            "random_seed]\n\t[-end end_time_in_usec]\n\t[-mtu MTU]\n\t[-hop_latency x] per hop wire "
            "latency in us,default 1 \n\t[-disable_fd] disable fair decrease to get higher "
            "throughput, \n\t[-target_q_delay x] target_queuing_delay in us, default is 6us "
            "\n\t[-switch_latency x] switching latency in us, default 0\n\t[-host_queue_type  "
            "swift|prio|fair_prio]\n\t[-logtime dt] sample time for sinklogger, etc"
            "\n\t[-logtime dt] sample time for sinklogger, etc\n\t[-conn_reuse] enable connection reuse"
        << std::endl;
    exit(1);
}

static simtime_picosec calculate_rtt(FatTreeTopologyCfg* t_cfg, linkspeed_bps host_linkspeed) { 
    /*
    Using the host linkspeed here is not very accurate, but hopefully good enough for this usecase.
    */
    simtime_picosec rtt = 2 * t_cfg->get_diameter_latency()
                + (Packet::data_packet_size() * 8 / speedAsGbps(host_linkspeed) * t_cfg->get_diameter() * 1000) 
                + (UecBasePacket::get_ack_size() * 8 / speedAsGbps(host_linkspeed) * t_cfg->get_diameter() * 1000);

    return rtt;
};

static uint32_t calculate_bdp_pkt(FatTreeTopologyCfg* t_cfg, linkspeed_bps host_linkspeed) {
    simtime_picosec rtt = calculate_rtt(t_cfg, host_linkspeed);
    uint32_t bdp_pkt = ceil((timeAsSec(rtt) * (host_linkspeed/8)) / (double)Packet::data_packet_size()); 

    return bdp_pkt;
}

// Impl constructor that loads config for session
HTSimProtoUet::HTSimProtoUet(const HTSim::tm_info* const tm, int argc, char** argv) {
    c = std::make_unique<Clock>(timeFromSec(5 / 100.), eventlist);
    no_of_nodes = tm->nodes;

    filename << "logout.dat";

    HTSimSession::conf.recv_flow_finish = false;

    int i = 1;
    while (i < argc) {
        if (!strcmp(argv[i], "-o")) {
            filename.str(std::string());
            filename << argv[i + 1];
            i++;
        } else if (!strcmp(argv[i],"-conn_reuse")){
            conn_reuse = true;
            cout << "Enabling connection reuse" << endl;
        } else if (!strcmp(argv[i], "-end")) {
            end_time = atoi(argv[i + 1]);
            cout << "endtime(us) " << end_time << endl;
            i++;
        } else if (!strcmp(argv[i], "-nodes")) {
            no_of_nodes = atoi(argv[i + 1]);
            cout << "no_of_nodes " << no_of_nodes << endl;
            i++;
        } else if (!strcmp(argv[i], "-tiers")) {
            tiers = atoi(argv[i + 1]);
            cout << "tiers " << tiers << endl;
            assert(tiers == 2 || tiers == 3);
            i++;
        } else if (!strcmp(argv[i], "-planes")) {
            planes = atoi(argv[i + 1]);
            ports = planes;
            cout << "planes " << planes << endl;
            cout << "ports per NIC " << ports << endl;
            assert(planes >= 1 && planes <= 8);
            i++;
        } else if (!strcmp(argv[i],"-receiver_cc_only")) {
            UecSrc::_sender_based_cc = false;
            UecSrc::_receiver_based_cc = true;
            UecSink::_oversubscribed_cc = false;
            sender_driven = false;
            receiver_driven = true;
            cout << "receiver based CC enabled ONLY" << endl;
        } else if (!strcmp(argv[i], "-sender_cc_only")) {
            UecSrc::_sender_based_cc = true;
            UecSrc::_receiver_based_cc = false;
            UecSink::_oversubscribed_cc = false;
            sender_driven = true;
            receiver_driven = false;
            cout << "sender based CC enabled ONLY" << endl;
        } else if (!strcmp(argv[i], "-qa_gate")) {
            qa_gate = atof(argv[i+1]);
            cout << "qa_gate 2^" << qa_gate << endl;
            i++;
        } else if (!strcmp(argv[i], "-target_q_delay")) {
            target_Qdelay = timeFromUs(atof(argv[i + 1]));
            cout << "target_q_delay" << atof(argv[i + 1]) << " us" << endl;
            i++;
        } else if (!strcmp(argv[i],"-queue_size_bdp_factor")) {
            queue_size_bdp_factor = atoi(argv[i+1]);
            cout << "Setting queue size to "<< queue_size_bdp_factor << "x BDP." << endl;
            i++;
        } else if (!strcmp(argv[i], "-sender_cc_algo")) {
            UecSrc::_sender_based_cc = true;
            sender_driven = true;

            if (!strcmp(argv[i + 1], "dctcp"))
                UecSrc::_sender_cc_algo = UecSrc::DCTCP;
            else if (!strcmp(argv[i + 1], "nscc"))
                UecSrc::_sender_cc_algo = UecSrc::NSCC;
            else if (!strcmp(argv[i + 1], "constant"))
                UecSrc::_sender_cc_algo = UecSrc::CONSTANT;
            else {
                cout << "UNKNOWN CC ALGO " << argv[i + 1] << endl;
                exit(1);
            }
            cout << "sender based algo " << argv[i + 1] << endl;
            i++;
        } else if (!strcmp(argv[i], "-sender_cc")) {
            UecSrc::_sender_based_cc = true;
            UecSink::_oversubscribed_cc = false;
            sender_driven = true;
            cout << "sender based CC enabled " << endl;
        } else if (!strcmp(argv[i],"-receiver_cc")) {
            UecSrc::_receiver_based_cc = true;
            receiver_driven = true;
            cout << "receiver based CC enabled " << endl;
        } else if (!strcmp(argv[i], "-load_balancing_algo")) {
            if (!strcmp(argv[i + 1], "bitmap")) {
                load_balancing_algo = BITMAP;
            } else if (!strcmp(argv[i + 1], "reps")) {
                load_balancing_algo = REPS;
            } else if (!strcmp(argv[i+1], "reps_legacy")) {
                load_balancing_algo = REPS_LEGACY;
            } else if (!strcmp(argv[i + 1], "oblivious")) {
                load_balancing_algo = OBLIVIOUS;
            } else if (!strcmp(argv[i + 1], "mixed")) {
                load_balancing_algo = MIXED;
            } else {
                cout << "Unknown load balancing algorithm of type " << argv[i + 1]
                     << ", expecting bitmap, reps or reps2" << endl;
                exit_error(argv[0]);
            }
            cout << "Load balancing algorithm set to  " << argv[i + 1] << endl;
            i++;
        } else if (!strcmp(argv[i], "-queue_type")) {
            if (!strcmp(argv[i + 1], "composite")) {
                qt = COMPOSITE;
            } else if (!strcmp(argv[i + 1], "composite_ecn")) {
                qt = COMPOSITE_ECN;
            } else if (!strcmp(argv[i + 1], "aeolus")) {
                qt = AEOLUS;
            } else if (!strcmp(argv[i + 1], "aeolus_ecn")) {
                qt = AEOLUS_ECN;
            } else {
                cout << "Unknown queue type " << argv[i + 1] << endl;
                exit_error(argv[0]);
            }
            cout << "queue_type " << qt << endl;
            i++;
        } else if (!strcmp(argv[i], "-debug")) {
            UecSrc::_debug = true;
            UecPdcSes::_debug = true;
        } else if (!strcmp(argv[i], "-host_queue_type")) {
            if (!strcmp(argv[i + 1], "swift")) {
                snd_type = SWIFT_SCHEDULER;
            } else if (!strcmp(argv[i + 1], "prio")) {
                snd_type = PRIORITY;
            } else if (!strcmp(argv[i + 1], "fair_prio")) {
                snd_type = FAIR_PRIO;
            } else {
                cout << "Unknown host queue type " << argv[i + 1]
                     << " expecting one of swift|prio|fair_prio" << endl;
                exit_error(argv[0]);
            }
            cout << "host queue_type " << snd_type << endl;
            i++;
        } else if (!strcmp(argv[i], "-log")) {
            auto should_log = true;
            if (!strcmp(argv[i + 1], "no")) {
                should_log = false;
                i++;
            }
            if (!strcmp(argv[i + 1], "flow_events")) {
                log_flow_events = should_log;
            } else if (!strcmp(argv[i + 1], "sink")) {
                cout << "logging sinks\n";
                log_sink = should_log;
            } else if (!strcmp(argv[i + 1], "nic")) {
                cout << "logging nics\n";
                log_nic = should_log;
            } else if (!strcmp(argv[i + 1], "tor_downqueue")) {
                cout << "logging tor downqueues\n";
                log_tor_downqueue = should_log;
            } else if (!strcmp(argv[i + 1], "tor_upqueue")) {
                cout << "logging tor upqueues\n";
                log_tor_upqueue = should_log;
            } else if (!strcmp(argv[i + 1], "switch")) {
                cout << "logging total switch queues\n";
                log_switches = should_log;
            } else if (!strcmp(argv[i + 1], "traffic")) {
                cout << "logging traffic\n";
                log_traffic = should_log;
            } else if (!strcmp(argv[i + 1], "queue_usage")) {
                cout << "logging queue usage\n";
                log_queue_usage = should_log;
            } else {
                exit_error(argv[0]);
            }
            i++;
        } else if (!strcmp(argv[i], "-cwnd")) {
            cwnd = atoi(argv[i + 1]);
            cout << "cwnd " << cwnd << endl;
            i++;
        } else if (!strcmp(argv[i], "-topo")) {
            topo_file = argv[i + 1];
            cout << "FatTree topology input file: " << topo_file << endl;
            i++;
        } else if (!strcmp(argv[i], "-q")) {
            param_queuesize_set = true;
            queuesize_pkt = atoi(argv[i+1]);
            cout << "Setting queuesize to " << queuesize_pkt << " packets " << endl;
            i++;
        } else if (!strcmp(argv[i], "-sack_threshold")) {
            UecSink::_bytes_unacked_threshold = atoi(argv[i + 1]);
            cout << "Setting receiver SACK bytes threshold to " << UecSink::_bytes_unacked_threshold
                 << " bytes " << endl;
            i++;
        } else if (!strcmp(argv[i], "-oversubscribed_cc")) {
            UecSink::_oversubscribed_cc = true;
            cout << "Using receiver oversubscribed CC " << endl;
        } else if (!strcmp(argv[i], "-Ai")) {
            OversubscribedCC::_Ai = atof(argv[i + 1]);
            cout << "Using Ai " << OversubscribedCC::_Ai << endl;
            i += 1;
        } else if (!strcmp(argv[i], "-Md")) {
            OversubscribedCC::_Md = atof(argv[i + 1]);
            cout << "Using Md " << OversubscribedCC::_Md << endl;
            i += 1;
        } else if (!strcmp(argv[i], "-alpha")) {
            OversubscribedCC::_alpha = atof(argv[i + 1]);
            cout << "Using Alpha " << OversubscribedCC::_alpha << endl;
            i += 1;
        } else if (!strcmp(argv[i], "-force_disable_oversubscribed_cc")) {
            UecSink::_oversubscribed_cc = false;
            force_disable_oversubscribed_cc = true;
            cout << "Disabling receiver oversubscribed CC even with OS topology" << endl;
        } else if (!strcmp(argv[i], "-enable_accurate_base_rtt")) {
            enable_accurate_base_rtt = true;
            cout << "Enable accurate base rtt configuration, each flow uses the accurate "
                    "end-to-end delay for the current sender/receiver pair as rtt upper bound."
                 << endl;
        } else if (!strcmp(argv[i],"-disable_base_rtt_update_on_nack")){
          UecSrc::update_base_rtt_on_nack = false;
          cout << "Disables using NACKs to update the base RTT." << endl;
        } else if (!strcmp(argv[i],"-sleek")){
            UecSrc::_enable_sleek = true;
            cout << "Using SLEEK, the sender-based fast loss recovery heuristic " << endl;
        } else if (!strcmp(argv[i], "-ecn")) {
            // fraction of queuesize, between 0 and 1
            param_ecn_set = true;
            ecn = true;
            ecn_low = atoi(argv[i + 1]);
            ecn_high = atoi(argv[i + 2]);
            i += 2;
        } else if (!strcmp(argv[i], "-disable_trim")) {
            disable_trim = true;
            cout << "Trimming disabled, dropping instead." << endl;
        } else if (!strcmp(argv[i], "-trimsize")) {
            // size of trimmed packet in bytes
            trimsize = atoi(argv[i + 1]);
            cout << "trimmed packet size: " << trimsize << " bytes\n";
            i += 1;
        } else if (!strcmp(argv[i], "-logtime")) {
            double log_ms = atof(argv[i + 1]);
            logtime = timeFromMs(log_ms);
            cout << "logtime " << logtime << " ms" << endl;
            i++;
        } else if (!strcmp(argv[i], "-logtime_us")) {
            double log_us = atof(argv[i + 1]);
            logtime = timeFromUs(log_us);
            cout << "logtime " << log_us << " us" << endl;
            i++;
        } else if (!strcmp(argv[i], "-failed")) {
            // number of failed links (failed to 25% linkspeed)
            topo_num_failed = atoi(argv[i+1]);
            i++;
        } else if (!strcmp(argv[i], "-linkspeed")) {
            // linkspeed specified is in Mbps
            linkspeed = speedFromMbps(atof(argv[i + 1]));
            i++;
        } else if (!strcmp(argv[i], "-seed")) {
            seed = atoi(argv[i + 1]);
            cout << "random seed " << seed << endl;
            i++;
        } else if (!strcmp(argv[i], "-mtu")) {
            packet_size = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-paths")) {
            path_entropy_size = atoi(argv[i + 1]);
            cout << "no of paths " << path_entropy_size << endl;
            i++;
        } else if (!strcmp(argv[i], "-hop_latency")) {
            hop_latency = timeFromUs(atof(argv[i + 1]));
            cout << "Hop latency set to " << timeAsUs(hop_latency) << endl;
            i++;
        } else if (!strcmp(argv[i], "-pcie")) {
            UecSink::_model_pcie = true;
            pcie_rate = atof(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-switch_latency")) {
            switch_latency = timeFromUs(atof(argv[i + 1]));
            cout << "Switch latency set to " << timeAsUs(switch_latency) << endl;
            i++;
        } else if (!strcmp(argv[i], "-ar_sticky_delta")) {
            ar_sticky_delta = atof(argv[i + 1]);
            cout << "Adaptive routing sticky delta " << ar_sticky_delta << "us" << endl;
            i++;
        } else if (!strcmp(argv[i], "-ar_granularity")) {
            if (!strcmp(argv[i + 1], "packet"))
                ar_sticky = FatTreeSwitch::PER_PACKET;
            else if (!strcmp(argv[i + 1], "flow"))
                ar_sticky = FatTreeSwitch::PER_FLOWLET;
            else {
                cout << "Expecting -ar_granularity packet|flow, found " << argv[i + 1] << endl;
                exit(1);
            }
            i++;
        } else if (!strcmp(argv[i], "-ar_method")) {
            if (!strcmp(argv[i + 1], "pause")) {
                cout << "Adaptive routing based on pause state " << endl;
                FatTreeSwitch::fn = &FatTreeSwitch::compare_pause;
            } else if (!strcmp(argv[i + 1], "queue")) {
                cout << "Adaptive routing based on queue size " << endl;
                FatTreeSwitch::fn = &FatTreeSwitch::compare_queuesize;
            } else if (!strcmp(argv[i + 1], "bandwidth")) {
                cout << "Adaptive routing based on bandwidth utilization " << endl;
                FatTreeSwitch::fn = &FatTreeSwitch::compare_bandwidth;
            } else if (!strcmp(argv[i + 1], "pqb")) {
                cout << "Adaptive routing based on pause, queuesize and bandwidth utilization "
                     << endl;
                FatTreeSwitch::fn = &FatTreeSwitch::compare_pqb;
            } else if (!strcmp(argv[i + 1], "pq")) {
                cout << "Adaptive routing based on pause, queuesize" << endl;
                FatTreeSwitch::fn = &FatTreeSwitch::compare_pq;
            } else if (!strcmp(argv[i + 1], "pb")) {
                cout << "Adaptive routing based on pause, bandwidth utilization" << endl;
                FatTreeSwitch::fn = &FatTreeSwitch::compare_pb;
            } else if (!strcmp(argv[i + 1], "qb")) {
                cout << "Adaptive routing based on queuesize, bandwidth utilization" << endl;
                FatTreeSwitch::fn = &FatTreeSwitch::compare_qb;
            } else {
                cout
                    << "Unknown AR method expecting one of pause, queue, bandwidth, pqb, pq, pb, qb"
                    << endl;
                exit(1);
            }
            i++;
        } else if (!strcmp(argv[i], "-strat")) {
            if (!strcmp(argv[i + 1], "ecmp_host")) {
                route_strategy = ECMP_FIB;
                FatTreeSwitch::set_strategy(FatTreeSwitch::ECMP);
            } else if (!strcmp(argv[i + 1], "rr_ecmp")) {
                // this is the host route strategy;
                route_strategy = ECMP_FIB_ECN;
                qt = COMPOSITE_ECN_LB;
                // this is the switch route strategy.
                FatTreeSwitch::set_strategy(FatTreeSwitch::RR_ECMP);
            } else if (!strcmp(argv[i + 1], "ecmp_host_ecn")) {
                route_strategy = ECMP_FIB_ECN;
                FatTreeSwitch::set_strategy(FatTreeSwitch::ECMP);
                qt = COMPOSITE_ECN_LB;
            } else if (!strcmp(argv[i + 1], "reactive_ecn")) {
                // Jitu's suggestion for something really simple
                // One path at a time, but switch whenever we get a trim or ecn
                // this is the host route strategy;
                route_strategy = REACTIVE_ECN;
                FatTreeSwitch::set_strategy(FatTreeSwitch::ECMP);
                qt = COMPOSITE_ECN_LB;
            } else if (!strcmp(argv[i + 1], "ecmp_ar")) {
                route_strategy = ECMP_FIB;
                path_entropy_size = 1;
                FatTreeSwitch::set_strategy(FatTreeSwitch::ADAPTIVE_ROUTING);
            } else if (!strcmp(argv[i + 1], "ecmp_host_ar")) {
                route_strategy = ECMP_FIB;
                FatTreeSwitch::set_strategy(FatTreeSwitch::ECMP_ADAPTIVE);
                // the stuff below obsolete
                // FatTreeSwitch::set_ar_fraction(atoi(argv[i+2]));
                // cout << "AR fraction: " << atoi(argv[i+2]) << endl;
                // i++;
            } else if (!strcmp(argv[i + 1], "ecmp_rr")) {
                // switch round robin
                route_strategy = ECMP_FIB;
                path_entropy_size = 1;
                FatTreeSwitch::set_strategy(FatTreeSwitch::RR);
            }
            i++;
        } else {
            cout << "Unknown parameter " << argv[i] << endl;
            exit_error(argv[0]);
        }
        i++;
    }

    if (end_time > 0 && logtime >= timeFromUs((uint32_t)end_time)){
        cout << "Logtime set to endtime" << endl;
        logtime = timeFromUs((uint32_t)end_time) - 1;
    }

    assert(trimsize >= 64 && trimsize <= (uint32_t)packet_size);
    cout << "Packet size (MTU) is " << packet_size << endl;

    std::srand(seed);
    srandom(seed);
    std::cout << "Parsed args\n";
    Packet::set_packet_size(packet_size);

    UecSrc::_mtu = Packet::data_packet_size();
    UecSrc::_mss = UecSrc::_mtu - UecSrc::_hdr_size;

    if (route_strategy == NOT_SET) {
        route_strategy = ECMP_FIB;
        FatTreeSwitch::set_strategy(FatTreeSwitch::ECMP);
    }

    FatTreeSwitch::_ar_sticky = ar_sticky;
    FatTreeSwitch::_sticky_delta = timeFromUs(ar_sticky_delta);
    FatTreeSwitch::_ecn_threshold_fraction = ecn_thresh;
    FatTreeSwitch::_disable_trim = disable_trim;
    FatTreeSwitch::_trim_size = trimsize;

    eventlist.setEndtime(timeFromUs((uint32_t)end_time));

    switch (route_strategy) {
        case ECMP_FIB_ECN:
        case REACTIVE_ECN:
            if (qt != COMPOSITE_ECN_LB) {
                fprintf(stderr, "Route Strategy is ECMP ECN.  Must use an ECN queue\n");
                exit(1);
            } else if (ecn_thresh <= 0 || ecn_thresh >= 1) {
                fprintf(stderr,
                        "Route Strategy is ECMP ECN.  ecn_thresh must be between 0 and 1\n");
                exit(1);
            }
            [[fallthrough]];
        case ECMP_FIB:
            if (path_entropy_size > 10000) {
                fprintf(stderr, "Route Strategy is ECMP.  Must specify path count using -paths\n");
                exit(1);
            }
            break;
        case NOT_SET:
            fprintf(stderr,
                    "Route Strategy not set.  Use the -strat param.  \nValid values are perm, "
                    "rand, pull, rg and single\n");
            exit(1);
        default:
            break;
    }

    // prepare the loggers

    cout << "Logging to " << filename.str() << endl;
    logfile = std::make_unique<Logfile>(filename.str(), eventlist);

    cout << "Linkspeed set to " << linkspeed / 1000000000 << "Gbps" << endl;
    logfile->setStartTime(timeFromSec(0));

    if (log_sink) {
        sink_logger = std::make_unique<UecSinkLoggerSampling>(logtime, eventlist);
        logfile->addLogger(*sink_logger);
    }
    if (log_nic) {
        nic_logger = std::make_unique<NicLoggerSampling>(logtime, eventlist);
        logfile->addLogger(*nic_logger);
    }
    if (log_traffic) {
        traffic_logger = std::make_unique<TrafficLoggerSimple>();
        logfile->addLogger(*traffic_logger);
    }
    if (log_flow_events) {
        event_logger = std::make_unique<FlowEventLoggerSimple>();
        logfile->addLogger(*event_logger);
    }

    if (log_tor_downqueue || log_tor_upqueue) {
        qlf = std::make_unique<QueueLoggerFactory>(logfile.get(),
                                                   QueueLoggerFactory::LOGGER_SAMPLING, eventlist);
        qlf->set_sample_period(logtime);
    } else if (log_queue_usage) {
        qlf = std::make_unique<QueueLoggerFactory>(logfile.get(), QueueLoggerFactory::LOGGER_EMPTY,
                                                   eventlist);
        qlf->set_sample_period(logtime);
    }

    if (!param_queuesize_set) {
        cout << "Automatic queue sizing enabled ";
        if (queue_size_bdp_factor==0) {
            if (disable_trim) {
                queue_size_bdp_factor = default_nontrimming_queuesize_factor;
                cout << "non-trimming";
            } else {
                queue_size_bdp_factor = default_trimming_queuesize_factor;
                cout << "trimming";
            }
        }
        cout << " queue-size-to-bdp-factor is " << queue_size_bdp_factor << "xBDP"
             << endl;
    }

    if (topo_file) {
        topo_cfg = FatTreeTopologyCfg::load(topo_file, memFromPkt(queuesize_pkt), (::queue_type)qt, (::queue_type)snd_type);
        if (topo_cfg->no_of_nodes() != no_of_nodes) {
            cerr << "Mismatch between connection matrix (" << no_of_nodes << " nodes) and topology ("
                    << topo_cfg->no_of_nodes() << " nodes)" << endl;
            exit(1);
        }
    } else {
        topo_cfg = make_unique<FatTreeTopologyCfg>(tiers, no_of_nodes, linkspeed, memFromPkt(queuesize_pkt),
                                                    hop_latency, switch_latency, (::queue_type)qt, (::queue_type)snd_type);
    }

    network_max_unloaded_rtt = calculate_rtt(topo_cfg.get(), linkspeed);
    cout << "network_max_unloaded_rtt " << timeAsUs(network_max_unloaded_rtt) << endl;

    mem_b queuesize = 0;
    if (!param_queuesize_set) {
        uint32_t bdp_pkt = calculate_bdp_pkt(topo_cfg.get(), linkspeed);
        mem_b queuesize_pkt = bdp_pkt * queue_size_bdp_factor;
        queuesize = memFromPkt(queuesize_pkt);
    } else {
        queuesize = memFromPkt(queuesize_pkt);
    }
    topo_cfg->set_queue_sizes(queuesize);

    if (topo_num_failed > 0) {
        topo_cfg->set_failed_links(topo_num_failed);
    }

    if (topo_cfg->get_oversubscription_ratio() > 1 && !UecSrc::_sender_based_cc && !force_disable_oversubscribed_cc) {
        UecSink::_oversubscribed_cc = true;
        OversubscribedCC::setOversubscriptionRatio(topo_cfg->get_oversubscription_ratio());
        cout << "Using simple receiver oversubscribed CC. Oversubscription ratio is " << topo_cfg->get_oversubscription_ratio() << endl;
    }

    //2 priority queues; 3 hops for incast
    UecSrc::_min_rto = timeFromUs(15 + queuesize * 6.0 * 8 * 1000000 / linkspeed);
    cout << "Setting min RTO to " << timeAsUs(UecSrc::_min_rto) << endl;

    if (ecn){
        uint32_t bdp_pkt = calculate_bdp_pkt(topo_cfg.get(), linkspeed);
        if (!param_ecn_set) {
            ecn_low = memFromPkt(ceil(bdp_pkt * 0.2));
            ecn_high = memFromPkt(ceil(bdp_pkt * 0.8));
        } else {
            ecn_low = memFromPkt(ecn_low);
            ecn_high = memFromPkt(ecn_high);
        }
        cout << "Setting ECN to parameters low " << ecn_low << " high " << ecn_high <<  " enable on tor downlink " << !receiver_driven << endl;
        topo_cfg->set_ecn_parameters(true, !receiver_driven, ecn_low, ecn_high);
        assert(ecn_low <= ecn_high);
        assert(ecn_high <= queuesize);
    }

    cout << *topo_cfg << endl;

    for (uint32_t p = 0; p < planes; p++) {
        topo.push_back(make_unique<FatTreeTopology>(topo_cfg.get(), qlf.get(), &eventlist, nullptr));

        if (log_switches) {
            topo.at(p)->add_switch_loggers(*logfile, logtime);
        }
    }

    if (UecSink::_oversubscribed_cc) {
        OversubscribedCC::_base_rtt = network_max_unloaded_rtt;
    }

    // Initialize congestion control algorithms
    if (receiver_driven) {
        // TBD
    }
    if (sender_driven) {
        // UecSrc::parameterScaleToTargetQ();
        bool trimming_enabled = !disable_trim;
        UecSrc::initNsccParams(network_max_unloaded_rtt, linkspeed, target_Qdelay,
                               qa_gate, trimming_enabled);
    }

    for (size_t ix = 0; ix < no_of_nodes; ix++) {
        auto pull_pacer = new UecPullPacer(linkspeed, 0.99,
                                          UecBasePacket::unquantize(UecSink::_credit_per_pull),
                                          eventlist, ports);
        pacers.push_back(pull_pacer);

        if (UecSink::_model_pcie)
            pcie_models.push_back(
                new PCIeModel(linkspeed * pcie_rate, UecSrc::_mtu, eventlist, pacers[ix]));

        if (UecSink::_oversubscribed_cc)
            oversubscribed_ccs.push_back(new OversubscribedCC(eventlist, pacers[ix]));

        UecNIC* nic = new UecNIC(ix, eventlist, linkspeed, ports);
        nics.push_back(nic);
        if (log_nic) {
            nic_logger->monitorNic(nic);
        }
    }

    if (planes != 1) {
        cout << "We are taking the plane 0 to calculate the network rtt; If all the planes have "
                "the same tiers, you can remove this check."
             << endl;
        assert(false);
    }

    cwnd_b = cwnd * Packet::data_packet_size();
}

HTSimProtoUet::~HTSimProtoUet() {
    for (auto& [flow_id, endpoints] : flowmap) {
        delete endpoints.first;
        delete endpoints.second;
    }
    flowmap.clear();

    for (auto& pair : flow_pdc_map) {
        delete pair.second;
    }
    flow_pdc_map.clear();

    for (auto pacer : pacers) {
        delete pacer;
    }
    pacers.clear();

    for (auto nic : nics) {
        delete nic;
    }
    nics.clear();
}

// Schedule_htsim_event creates a new connection and schedules it in HTSim.
// Adapted from main connections loop
void HTSimProtoUet::schedule_htsim_event(FlowInfo flow, int flow_id, optional<uint32_t> msgid) {
    auto src = flow.src;
    auto dst = flow.dst;
    auto msg_size = flow.size;

    if (!conn_reuse and msgid.has_value()) {
        cout << "msg keyword can only be used when conn_reuse is enabled.\n";
        abort();
    }

    assert(planes > 0);
    double start = eventlist.now();
    simtime_picosec transmission_delay = 0;
    simtime_picosec base_rtt_bw_two_points = 0;

    if (enable_accurate_base_rtt) {
        auto diameter = topo_cfg->get_diameter();
        auto two_point_diameter_latency =
            topo_cfg->get_two_point_diameter_latency(src, dst);
        transmission_delay =
            (Packet::data_packet_size() * 8 / speedAsGbps(linkspeed) * diameter * 1000)
            + (UecBasePacket::get_ack_size() * 8 / speedAsGbps(linkspeed) * diameter * 1000);
        base_rtt_bw_two_points = 2*two_point_diameter_latency + transmission_delay;
    }

    if (!conn_reuse || (flow_id and flowmap.find(flow_id) == flowmap.end())) {
        // Create new connection for this message
        unique_ptr<UecMultipath> mp = nullptr;
            if (load_balancing_algo == BITMAP){
                mp = make_unique<UecMpBitmap>(path_entropy_size, UecSrc::_debug);
            } else if (load_balancing_algo == REPS){
                mp = make_unique<UecMpReps>(path_entropy_size, UecSrc::_debug, !disable_trim);
            } else if (load_balancing_algo == REPS_LEGACY){
              mp = make_unique<UecMpRepsLegacy>(path_entropy_size, UecSrc::_debug);
            } else if (load_balancing_algo == OBLIVIOUS){
                mp = make_unique<UecMpOblivious>(path_entropy_size, UecSrc::_debug);
            } else if (load_balancing_algo == MIXED){
                mp = make_unique<UecMpMixed>(path_entropy_size, UecSrc::_debug);
            } else {
                cout << "ERROR: Failed to set multipath algorithm, abort." << endl;
                abort();
        }

        // Connections in rack use upper no_of_nodes NICs
        auto src_nic = nics.at(src);
        auto dst_nic = nics.at(dst);

        uec_src = new UecSrc(traffic_logger.get(), eventlist, move(mp), *src_nic, ports);
        uec_src->_debug_srcid = src;
        uec_src->_debug_dstid = dst;
        uec_src->astrasim_flow_finish_send_cb = &HTSimSession::flow_finish_send;

        if (flow_id) {
            uec_src->setFlowId(flow_id);
            assert(flowmap.find(flow_id) == flowmap.end()); // don't have dups
        }

        if (conn_reuse) {
            stringstream uec_src_dbg_tag;
            uec_src_dbg_tag << "flow_id " << uec_src->flowId();
            UecPdcSes* pdc = new UecPdcSes(uec_src, EventList::getTheEventList(), UecSrc::_mss, UecSrc::_hdr_size, uec_src_dbg_tag.str());
            pdc->_debug_srcid = src;
            pdc->_debug_dstid = dst;
            pdc->astrasim_flow_finish_send_cb = &HTSimSession::flow_finish_send;
            pdc->astrasim_flow_finish_recv_cb = &HTSimSession::flow_finish_recv;
            uec_src->makeReusable(pdc);
            flow_pdc_map[uec_src->flowId()] = pdc;
        }

        if (receiver_driven) {
            uec_snk = new UecSink(NULL,pacers[dst],*dst_nic, ports);
        } else { //each connection has its own pacer, so receiver driven mode does not kick in!
            uec_snk = new UecSink(NULL,linkspeed,1.1,UecBasePacket::unquantize(UecSink::_credit_per_pull),eventlist,*dst_nic, ports);
        }
        flowmap[uec_src->flowId()] = { uec_src, uec_snk };

        if (flow_id) {
            uec_snk->setFlowId(flow_id);
        }

        // If cwnd is 0 initXXcc will set a sensible default value
        if (receiver_driven) {
            // uec_src->setCwnd(cwnd*Packet::data_packet_size());
            // uec_src->setMaxWnd(cwnd*Packet::data_packet_size());
            if (enable_accurate_base_rtt) {
                uec_src->initRccc(cwnd_b, base_rtt_bw_two_points);
            } else {
                uec_src->initRccc(cwnd_b, network_max_unloaded_rtt);
            }
        }
        if (sender_driven) {
            if (enable_accurate_base_rtt) {
                uec_src->initNscc(cwnd_b, base_rtt_bw_two_points);
            } else {
                uec_src->initNscc(cwnd_b, network_max_unloaded_rtt);
            }
        }
        uec_srcs.push_back(uec_src);
        uec_src->setDst(dst);

        if (log_flow_events) {
            uec_src->logFlowEvents(*event_logger);
        }

        uec_src->setName("Uec_" + ntoa(src) + "_" + ntoa(dst));
        logfile->writeName(*uec_src);
        uec_snk->setSrc(src);
        uec_snk->_debug_srcid = src;
        uec_snk->_debug_dstid = dst;
        uec_snk->astrasim_flow_finish_recv_cb = &HTSimSession::flow_finish_recv;

        if (UecSink::_model_pcie) {
            uec_snk->setPCIeModel(pcie_models[dst]);
        }

        if (UecSink::_oversubscribed_cc) {
            uec_snk->setOversubscribedCC(oversubscribed_ccs[dst]);
        }

        uec_snk->setName("Uec_sink_" + ntoa(src) + "_" + ntoa(dst));
        logfile->writeName(*uec_snk);

        if (!conn_reuse) {
            if (msg_size > 0) {
                uec_src->setFlowsize(msg_size);
            }
        } else {
            assert(msg_size > 0);
            UecPdcSes* pdc = flow_pdc_map.find(flow_id)->second;
            // In enque function, 0 means the event is scheduled without delay.
            optional<simtime_picosec> start_ts = std::make_optional(0);
            pdc->enque(msg_size, start_ts, true);
        }

        for (uint32_t p = 0; p < planes; p++) {
            switch (route_strategy) {
                case ECMP_FIB:
                case ECMP_FIB_ECN:
                case REACTIVE_ECN: {
                    Route* srctotor = new Route();
                    Route* dsttotor = new Route();
                    auto topo_p = dynamic_cast<FatTreeTopology*>(topo[p].get());
                    srctotor->push_back(topo_p->queues_ns_nlp[src][topo_cfg->HOST_POD_SWITCH(src)][0]);
                    srctotor->push_back(topo_p->pipes_ns_nlp[src][topo_cfg->HOST_POD_SWITCH(src)][0]);
                    srctotor->push_back(topo_p
                                            ->queues_ns_nlp[src][topo_cfg->HOST_POD_SWITCH(src)][0]
                                            ->getRemoteEndpoint());

                    dsttotor->push_back(topo_p->queues_ns_nlp[dst][topo_cfg->HOST_POD_SWITCH(dst)][0]);
                    dsttotor->push_back(topo_p->pipes_ns_nlp[dst][topo_cfg->HOST_POD_SWITCH(dst)][0]);
                    dsttotor->push_back(topo_p
                                            ->queues_ns_nlp[dst][topo_cfg->HOST_POD_SWITCH(dst)][0]
                                            ->getRemoteEndpoint());
                    std::cerr << "Route through fat tree" << std::endl;
                    uec_src->connectPort(p, *srctotor, *dsttotor, *uec_snk, start);
                    // uec_src->setPaths(path_entropy_size);
                    // uec_snk->setPaths(path_entropy_size);

                    // register src and snk to receive packets from their respective TORs.
                    assert(topo_p->switches_lp[topo_cfg->HOST_POD_SWITCH(src)]);
                    assert(topo_p->switches_lp[topo_cfg->HOST_POD_SWITCH(src)]);
                    topo_p->switches_lp[topo_cfg->HOST_POD_SWITCH(src)]->addHostPort(
                        src, uec_snk->flowId(), uec_src->getPort(p));
                    topo_p->switches_lp[topo_cfg->HOST_POD_SWITCH(dst)]->addHostPort(
                        dst, uec_src->flowId(), uec_snk->getPort(p));
                    break;
                }
                default:
                    abort();
            }
        }

        if (log_sink) {
            sink_logger->monitorSink(uec_snk);
        }
    } else {
        // Use existing connection for this message
        assert(msgid.has_value());

        UecPdcSes* pdc = flow_pdc_map.find(flow_id)->second;
        uec_src = nullptr;
        uec_snk = nullptr;

        pdc->enque(msg_size, 0, true);
    }
}

void HTSimProtoUet::run(const HTSim::tm_info* const tm) {
    // Record the setup
    int pktsize = Packet::data_packet_size();
    logfile->write("# pktsize=" + ntoa(pktsize) + " bytes");
    logfile->write("# hostnicrate = " + ntoa(linkspeed / 1000000) + " Mbps");

    // GO!
    cout << "Starting simulation" << std::endl;
    while (eventlist.doNextEvent()) {}

    cout << std::endl << "Simulation of events finished:" << std::endl;

    Logged::dump_idmap();

    return;
}

void HTSimProtoUet::finish() {
    cout << "Done" << endl;
    int new_pkts = 0, rtx_pkts = 0, bounce_pkts = 0, rts_pkts = 0, ack_pkts = 0, nack_pkts = 0,
        pull_pkts = 0, sleek_pkts = 0;
    for (size_t ix = 0; ix < uec_srcs.size(); ix++) {
        const struct UecSrc::Stats& s = uec_srcs[ix]->stats();
        new_pkts += s.new_pkts_sent;
        rtx_pkts += s.rtx_pkts_sent;
        rts_pkts += s.rts_pkts_sent;
        bounce_pkts += s.bounces_received;
        ack_pkts += s.acks_received;
        nack_pkts += s.nacks_received;
        pull_pkts += s.pulls_received;
        sleek_pkts += s._sleek_counter;
    }
    cout << "New: " << new_pkts << " Rtx: " << rtx_pkts << " RTS: " << rts_pkts
         << " Bounced: " << bounce_pkts << " ACKs: " << ack_pkts << " NACKs: " << nack_pkts
         << " Pulls: " << pull_pkts << " sleek_pkts: " << sleek_pkts << endl;
}

} // namespace HTSim