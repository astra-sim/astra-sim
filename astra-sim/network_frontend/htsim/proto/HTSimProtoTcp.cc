#include "HTSimProtoTcp.hh"
#include "HTSimSession.hh"

// Adapted from HTSim main_tcp.cpp

#include "network.h"
#include "randomqueue.h"

#include "pipe.h"
#include "eventlist.h"
#include "logfile.h"
#include "mtcp.h"
#include "tcp.h"
#include "tcp_transfer.h"
#include "cbr.h"
#include "firstfit.h"

#include <cstring>
// Simulation params

#define PRINT_PATHS 0

#define PERIODIC 0
#include "main.h"

static FirstFit* ff = NULL;
static size_t subflow_count = 1;

#define USE_FIRST_FIT 0
#define FIRST_FIT_INTERVAL 100

namespace HTSim {

static void exit_error(char* progr) {
    std::cout << "Usage " << progr << " [UNCOUPLED(DEFAULT)|COUPLED_INC|FULLY_COUPLED|COUPLED_EPSILON] [epsilon][COUPLED_SCALABLE_TCP" << std::endl;
    exit(1);
}

static void print_path(std::ofstream &paths,const Route* rt){
    for (uint32_t i=1;i<rt->size()-1;i+=2){
        RandomQueue* q = (RandomQueue*)rt->at(i);
        if (q!=NULL) {
            paths << q->str() << " ";
        }
        else {
            paths << "NULL ";
        }
    }

    paths << std::endl;
}

// Impl constructor that loads config for session
HTSimProtoTcp::HTSimProtoTcp(const HTSim::tm_info* const tm, int argc, char** argv) {
    eventlist.setEndtime(timeFromSec(4));
    c = std::make_unique<Clock>(timeFromSec(50 / 100.), eventlist);
    no_of_nodes = tm->nodes;
    linkspeed = speedFromMbps((double)HOST_NIC);

    int i = 1;
    filename << "logout.dat";

    while (i<argc) {
        if (!strcmp(argv[i],"-o")){
            filename.str(std::string());
            filename << argv[i+1];
            i++;
        }
        else if (!strcmp(argv[i],"-sub")){
            subflow_count = atoi(argv[i+1]);
            i++;
        } else if (!strcmp(argv[i],"-nodes")){
            no_of_nodes = atoi(argv[i+1]);
            std::cout << "no_of_nodes "<<no_of_nodes << std::endl;
            i++;
        } else if (!strcmp(argv[i], "-topo")) {
            topo_file = argv[i + 1];
            cout << "FatTree topology input file: " << topo_file << endl;
            i++;
        } else if (!strcmp(argv[i], "UNCOUPLED"))
            algo = UNCOUPLED;
        else if (!strcmp(argv[i], "COUPLED_INC"))
            algo = COUPLED_INC;
        else if (!strcmp(argv[i], "FULLY_COUPLED"))
            algo = FULLY_COUPLED;
        else if (!strcmp(argv[i], "COUPLED_TCP"))
            algo = COUPLED_TCP;
        else if (!strcmp(argv[i], "COUPLED_SCALABLE_TCP"))
            algo = COUPLED_SCALABLE_TCP;
        else if (!strcmp(argv[i], "COUPLED_EPSILON")) {
            algo = COUPLED_EPSILON;
            if (argc > i+1){
                epsilon = atof(argv[i+1]);
                i++;
            }
            printf("Using epsilon %f\n", epsilon);
        } else
            exit_error(argv[0]);

        i++;
    }
    srand(time(NULL));

    std::cout << "Using subflow count " << subflow_count << std::endl;
    std::cout << "requested nodes " << no_of_nodes << std::endl;


    std::cout <<  "Using algo="<<algo<< " epsilon=" << epsilon << std::endl;
    // prepare the loggers

    std::cout << "Logging to " << filename.str() << std::endl;
    logfile = std::make_unique<Logfile>(filename.str(), eventlist);

#if PRINT_PATHS
    filename << ".paths";
    std::cout << "Logging path choices to " << filename.str() << std::endl;
    std::ofstream paths(filename.str().c_str());
    if (!paths){
        std::cout << "Can't open for writing paths file!"<< std::endl;
        exit(1);
    }
#endif

    logfile->setStartTime(timeFromSec(0));
    sinkLogger = std::make_unique<TcpSinkLoggerSampling>(timeFromMs(1000), eventlist);
    logfile->addLogger(*sinkLogger);

    tcpRtxScanner = std::make_unique<TcpRtxTimerScanner>(timeFromMs(10), eventlist);
    qlf = std::make_unique<QueueLoggerFactory>(logfile.get(), QueueLoggerFactory::LOGGER_SAMPLING, eventlist);
    qlf->set_sample_period(timeFromUs(1000.0));

#if USE_FIRST_FIT
    if (subflow_count==1){
        ff = new FirstFit(timeFromMs(FIRST_FIT_INTERVAL),eventlist);
    }
#endif

#ifdef FAT_TREE

if (topo_file) {

    FatTreeTopology* top_ = FatTreeTopology::load(topo_file, qlf.get(), eventlist, memFromPkt(8),
    RANDOM, FAIR_PRIO);
    top = std::unique_ptr<FatTreeTopology>(top_);

    if (top->no_of_nodes() != no_of_nodes) {
        std::cerr << "Mismatch between connection matrix (" << no_of_nodes
        << " nodes) and topology (" << top->no_of_nodes() << " nodes)" << endl;
        exit(1);
    }
} else {
        top = std::make_unique<FatTreeTopology>(no_of_nodes, linkspeed, memFromPkt(8), qlf.get(), &eventlist,ff,RANDOM,0);
    }
#endif

#ifdef OV_FAT_TREE
    top = std::make_unique<OversubscribedFatTreeTopology>(logfile.get(), &eventlist,ff);
#endif

#ifdef MH_FAT_TREE
    top = std::make_unique<MultihomedFatTreeTopology>(logfile.get(), &eventlist,ff);
#endif

#ifdef STAR
    top = std::make_unique<StarTopology>(logfile.get(), &eventlist,ff);
#endif

#ifdef BCUBE
    top = std::make_unique<BCubeTopology>(logfile.get(),&eventlist,ff);
    std::cout << "BCUBE " << K << std::endl;
#endif

#ifdef VL2
    top = std::make_unique<VL2Topology>(logfile.get(),&eventlist,ff);
#endif
    no_of_nodes = top->no_of_nodes();
    std::cout << "actual nodes " << no_of_nodes << std::endl;

    net_paths = new vector<const Route*>**[no_of_nodes];
    is_dest = new int[no_of_nodes];

    for (uint32_t i=0;i<no_of_nodes;i++){
        is_dest[i] = 0;
        net_paths[i] = new vector<const Route*>*[no_of_nodes];
        for (uint32_t j = 0;j<no_of_nodes;j++)
            net_paths[i][j] = NULL;
    }

    if (ff)
        ff->net_paths = net_paths;
}

// Schedule_htsim_event creates a new connection and schedules it in HTSim.
// Adapted from main connections loop
void HTSimProtoTcp::schedule_htsim_event(FlowInfo flow, int flow_id) {
    auto src = flow.src;
    auto dst = flow.dst;
    auto msg_size = flow.size;
    double start = eventlist.now();
    connID++;

    if (!net_paths[src][dst]) {
        net_paths[src][dst] = top->get_paths(src,dst);
    }


    if (algo == COUPLED_EPSILON) {
        mtcp = new MultipathTcpSrc(algo, eventlist, NULL, epsilon);
    }
    else {
        mtcp = new MultipathTcpSrc(algo, eventlist, NULL);
    }

    uint32_t it_sub;
    size_t crt_subflow_count = subflow_count;
    tot_subs += crt_subflow_count;
    cnt_con++;

    it_sub = crt_subflow_count > net_paths[src][dst]->size()?net_paths[src][dst]->size():crt_subflow_count;

#ifdef MH_FAT_TREE
    int use_all = it_sub==net_paths[src][dst]->size();
#endif

    for (uint32_t inter = 0; inter < it_sub; inter++) {
        tcpSrc = new TcpSrc(NULL, NULL, eventlist);
        tcpSrc->_debug_srcid = src;
        tcpSrc->_debug_dstid = dst;
        tcpSrc->astrasim_flow_finish_send_cb = &HTSimSession::flow_finish_send;
        tcpSrc->set_flowsize(msg_size);
        tcpSnk = new TcpSink();
        tcpSnk->_debug_srcid = src;
        tcpSnk->_debug_dstid = dst;
        tcpSnk->astrasim_flow_finish_recv_cb = &HTSimSession::flow_finish_recv;

        tcpSrc->setName("mtcp_" + ntoa(src) + "_" + ntoa(inter) + "_" + ntoa(dst));
        logfile->writeName(*tcpSrc);

        tcpSnk->setName("mtcp_sink_" + ntoa(src) + "_" + ntoa(inter) + "_" + ntoa(dst));
        logfile->writeName(*tcpSnk);

        tcpRtxScanner->registerTcp(*tcpSrc);
        size_t choice = 0;

#ifdef FAT_TREE
        choice = rand()%net_paths[src][dst]->size();
#endif

#ifdef OV_FAT_TREE
        choice = rand()%net_paths[src][dst]->size();
#endif

#ifdef MH_FAT_TREE
        if (use_all)
            choice = inter;
        else
            choice = rand()%net_paths[src][dst]->size();
#endif

#ifdef VL2
        choice = rand()%net_paths[src][dst]->size();
#endif

#ifdef STAR
        choice = 0;
#endif

#ifdef BCUBE
        //choice = inter;

        int min = -1, max = -1,minDist = 1000,maxDist = 0;
        if (subflow_count==1){
            //find shortest and longest path
            for (uint32_t dd=0;dd<net_paths[src][dst]->size();dd++){
                if (net_paths[src][dst]->at(dd)->size()<minDist){
                    minDist = net_paths[src][dst]->at(dd)->size();
                    min = dd;
                }
                if (net_paths[src][dst]->at(dd)->size()>maxDist){
                    maxDist = net_paths[src][dst]->at(dd)->size();
                    max = dd;
                }
            }
            choice = min;
        } else
            choice = rand()%net_paths[src][dst]->size();
#endif
        if (choice>=net_paths[src][dst]->size()){
            printf("Weird path choice %lu out of %lu\n",choice,net_paths[src][dst]->size());
            exit(1);
        }

#if PRINT_PATHS
        paths << "Route from "<< ntoa(src) << " to " << ntoa(dst) << "  (" << choice << ") -> " ;
        print_path(paths,net_paths[src][dst]->at(choice));
#endif

        routeout = new Route(*(net_paths[src][dst]->at(choice)));
        routeout->push_back(tcpSnk);

        routein = new Route();
        routein->push_back(tcpSrc);
        extrastarttime = 0 * drand();

        //join multipath connection

        mtcp->addSubflow(tcpSrc);

        if (inter == 0) {
            mtcp->setName("multipath" + ntoa(src) + "_" + ntoa(dst));
            logfile->writeName(*mtcp);
        }

        tcpSrc->connect(*routeout, *routein, *tcpSnk, start + timeFromMs(extrastarttime));

        if (flow_id) {
            tcpSrc->setFlowId(flow_id);
            tcpSnk->setFlowId(flow_id);
        }

#ifdef PACKET_SCATTER
        tcpSrc->set_paths(net_paths[src][dst]);
        cout << "Using PACKET SCATTER!!!!"<<endl;
#endif

        if (ff&&!inter)
            ff->add_flow(src,dst,tcpSrc);

        sinkLogger->monitorMultipathSink(tcpSnk);
    }
}

void HTSimProtoTcp::run(const HTSim::tm_info* const tm) {
    Logged::dump_idmap();
    // Record the setup
    int pktsize = Packet::data_packet_size();
    logfile->write("# pktsize=" + ntoa(pktsize) + " bytes");
    logfile->write("# subflows=" + ntoa(subflow_count));
    logfile->write("# hostnicrate = " + ntoa(linkspeed/1000000) + " Mbps");
    logfile->write("# corelinkrate = " + ntoa(HOST_NIC*CORE_TO_HOST) + " pkt/sec");
    double rtt = timeAsSec(timeFromUs(RTT));
    logfile->write("# rtt =" + ntoa(rtt));

    // GO!
    while (eventlist.doNextEvent()) {
    }
}

void HTSimProtoTcp::finish() {
#if USE_FIRST_FIT
    delete ff
#endif
    for (uint32_t i=0;i<no_of_nodes;i++){
        delete net_paths[i];
    }
    delete[] net_paths;
    delete[] is_dest;
    std::cout << std::endl << "Simulation of events finished" << std::endl;
}

} // namespace HTSim