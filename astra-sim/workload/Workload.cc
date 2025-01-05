/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "astra-sim/workload/Workload.hh"

#include "astra-sim/common/Logging.hh"
#include "astra-sim/system/IntData.hh"
#include "astra-sim/system/MemEventHandlerData.hh"
#include "astra-sim/system/RecvPacketEventHandlerData.hh"
#include "astra-sim/system/SendPacketEventHandlerData.hh"
#include "astra-sim/system/WorkloadLayerHandlerData.hh"
#include <json/json.hpp>

#include <iostream>
#include <stdlib.h>
#include <unistd.h>

using namespace std;
using namespace AstraSim;
using namespace Chakra;
using json = nlohmann::json;

typedef ChakraProtoMsg::NodeType ChakraNodeType;
typedef ChakraProtoMsg::CollectiveCommType ChakraCollectiveCommType;

Workload::Workload(Sys* sys, string et_filename, string comm_group_filename) {
    string workload_filename = et_filename + "." + to_string(sys->id) + ".et";
    // Check if workload filename exists
    if (access(workload_filename.c_str(), R_OK) < 0) {
        string error_msg;
        if (errno == ENOENT) {
            error_msg =
                "workload file: " + workload_filename + " does not exist";
        } else if (errno == EACCES) {
            error_msg = "workload file: " + workload_filename +
                        " exists but is not readable";
        } else {
            error_msg =
                "Unknown workload file: " + workload_filename + " access error";
        }
        LoggerFactory::get_logger("workload")->critical(error_msg);
        exit(EXIT_FAILURE);
    }
    this->et_feeder = new ETFeeder(workload_filename);
    this->comm_group = nullptr;
    // TODO: parametrize the number of available hardware resources
    this->hw_resource = new HardwareResource(1);
    this->sys = sys;
    initialize_comm_group(comm_group_filename);
    this->is_finished = false;
}

Workload::~Workload() {
    if (this->comm_group != nullptr) {
        delete this->comm_group;
    }
    if (this->et_feeder != nullptr) {
        delete this->et_feeder;
    }
    if (this->hw_resource != nullptr) {
        delete this->hw_resource;
    }
}

void Workload::initialize_comm_group(string comm_group_filename) {
    // communicator group input file is not given
    if (comm_group_filename.find("empty") != std::string::npos) {
        comm_group = nullptr;
        return;
    }

    ifstream inFile;
    json j;
    inFile.open(comm_group_filename);
    inFile >> j;

    for (json::iterator it = j.begin(); it != j.end(); ++it) {
        bool in_comm_group = false;

        for (auto id : it.value()) {
            if (id == sys->id) {
                in_comm_group = true;
            }
        }

        if (in_comm_group) {
            std::vector<int> involved_NPUs;
            for (auto id : it.value()) {
                involved_NPUs.push_back(id);
            }
            comm_group = new CommunicatorGroup(1, involved_NPUs, sys);
            // Note: All NPUs should create comm group with identical ids if
            // they want to communicate with each other
        }
    }
}

void Workload::initialize_process_group(std::shared_ptr<Chakra::ETFeederNode> node) {

    if (!this->process_group_map.empty()) return;

    std::string pg_info = node->get_inputs_values();
    if (pg_info.empty()) {
        std::cerr << "Process Group Information is not encoded" << std::endl;
        return;
    }
    pg_info = pg_info.substr(2, pg_info.size() - 4); // Strip first and last character
    try {
        json valuesRoot = json::parse(pg_info);

        for (const auto& item : valuesRoot) {
            std::string pgName = item.at("pg_name").get<std::string>();
            std::vector<int> involved_NPUs = item.at("ranks").get<std::vector<int>>();

            if (involved_NPUs.empty()) {
                for (int i=0; i<sys->total_nodes; i++)
                    involved_NPUs.push_back(i);
            }

            // To ensure pgName > 0
            CommunicatorGroup* cg = new CommunicatorGroup(std::stoi(pgName)+1, involved_NPUs, sys);
            process_group_map.insert(std::pair<std::string, CommunicatorGroup*>(pgName, cg));
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing or processing JSON: " << e.what() << std::endl;
    }
}

void Workload::issue_dep_free_nodes() {
    std::queue<shared_ptr<Chakra::ETFeederNode>> push_back_queue;
    shared_ptr<Chakra::ETFeederNode> node = et_feeder->getNextIssuableNode();
    while (node != nullptr) {
        if (hw_resource->is_available(node)) {
            issue(node);
        } else {
            push_back_queue.push(node);
        }
        node = et_feeder->getNextIssuableNode();
    }

    while (!push_back_queue.empty()) {
        shared_ptr<Chakra::ETFeederNode> node = push_back_queue.front();
        et_feeder->pushBackIssuableNode(node->id());
        push_back_queue.pop();
    }
}

void Workload::issue(shared_ptr<Chakra::ETFeederNode> node) {
    auto logger = LoggerFactory::get_logger("workload");
    if (sys->replay_only) {
        hw_resource->occupy(node);
        issue_replay(node);
    } else {
        if ((node->type() == ChakraNodeType::METADATA_NODE)) {
            initialize_process_group(node);
        }
        if ((node->type() == ChakraNodeType::MEM_LOAD_NODE) ||
            (node->type() == ChakraNodeType::MEM_STORE_NODE)) {
            if (sys->trace_enabled) {
                logger->debug("issue,sys->id={}, tick={}, node->id={}, "
                              "node->name={}, node->type={}",
                              sys->id, Sys::boostedTick(), node->id(),
                              node->name(),
                              static_cast<uint64_t>(node->type()));
            }
            issue_remote_mem(node);
        } else if (node->is_cpu_op() ||
                   (!node->is_cpu_op() &&
                    node->type() == ChakraNodeType::COMP_NODE)) {
            if ((node->runtime() == 0) && (node->num_ops() == 0)) {
                skip_invalid(node);
            } else {
                if (sys->trace_enabled) {
                    logger->debug("issue,sys->id={}, tick={}, node->id={}, "
                                  "node->name={}, node->type={}",
                                  sys->id, Sys::boostedTick(), node->id(),
                                  node->name(),
                                  static_cast<uint64_t>(node->type()));
                }
                issue_comp(node);
            }
        } else if (!node->is_cpu_op() &&
                   (node->type() == ChakraNodeType::COMM_COLL_NODE ||
                    (node->type() == ChakraNodeType::COMM_SEND_NODE) ||
                    (node->type() == ChakraNodeType::COMM_RECV_NODE))) {
            if (sys->trace_enabled) {
                if (sys->trace_enabled) {
                    logger->debug("issue,sys->id={}, tick={}, node->id={}, "
                                  "node->name={}, node->type={}",
                                  sys->id, Sys::boostedTick(), node->id(),
                                  node->name(),
                                  static_cast<uint64_t>(node->type()));
                }
            }
            issue_comm(node);
        } else if (node->type() == ChakraNodeType::INVALID_NODE) {
            skip_invalid(node);
        }
    }
}

void Workload::issue_replay(shared_ptr<Chakra::ETFeederNode> node) {
    WorkloadLayerHandlerData* wlhd = new WorkloadLayerHandlerData;
    wlhd->node_id = node->id();
    uint64_t runtime = 1ul;
    if (node->runtime() != 0ul) {
        // chakra runtimes are in microseconds and we should convert it into
        // nanoseconds
        runtime = node->runtime() * 1000;
    }
    if (node->is_cpu_op()) {
        hw_resource->tics_cpu_ops += runtime;
    } else {
        hw_resource->tics_gpu_ops += runtime;
    }
    sys->register_event(this, EventType::General, wlhd, runtime);
}

void Workload::issue_remote_mem(shared_ptr<Chakra::ETFeederNode> node) {
    hw_resource->occupy(node);

    WorkloadLayerHandlerData* wlhd = new WorkloadLayerHandlerData;
    wlhd->sys_id = sys->id;
    wlhd->workload = this;
    wlhd->node_id = node->id();
    sys->remote_mem->issue(node->tensor_size(), wlhd);
}

void Workload::issue_comp(shared_ptr<Chakra::ETFeederNode> node) {
    hw_resource->occupy(node);

    if (sys->roofline_enabled) {
        WorkloadLayerHandlerData* wlhd = new WorkloadLayerHandlerData;
        wlhd->node_id = node->id();

        double operational_intensity = static_cast<double>(node->num_ops()) /
                                       static_cast<double>(node->tensor_size());
        double perf = sys->roofline->get_perf(operational_intensity);
        double elapsed_time =
            static_cast<double>(node->num_ops()) / perf;  // sec
        uint64_t runtime =
            static_cast<uint64_t>(elapsed_time * 1e9);  // sec -> ns
        if (node->is_cpu_op()) {
            hw_resource->tics_cpu_ops += runtime;
        } else {
            hw_resource->tics_gpu_ops += runtime;
        }
        sys->register_event(this, EventType::General, wlhd, runtime);
    } else {
        // advance this node forward the recorded "replayed" time specificed in
        // the ET.
        issue_replay(node);
    }
}

void Workload::issue_comm(shared_ptr<Chakra::ETFeederNode> node) {
    hw_resource->occupy(node);

    if (node->pg_name().empty())
        std::cerr << "Error: pg name is not specified "
                  << node->name() << ":::" << node->pg_name() << endl;

    auto it = process_group_map.find(node->pg_name());
    comm_group = (it != process_group_map.end()) ? it->second : nullptr;

    // Security
    // broadcast colelctive has not been implemented in ASTRA-SIM yet.
    // So, we just use its real system mesurements

    int comm_index = node->comm_type();
    if(node->type() == ChakraNodeType::COMM_SEND_NODE){
        comm_index = 10;
    }
    else if (node->type() == ChakraNodeType::COMM_RECV_NODE) {
        comm_index = 11;
    }
    else if (node->comm_type() < 0 || node->comm_type() > 9) {
        // Code to handle the case where comm_type is out of range
        std::cerr << "Error: comm_type is out of range (0-9), got "
                  << node->name() << " time: " << node->runtime() * 1000 << std::endl;
        return;
    }

    if (comm_index == 10 || comm_index == 11) {
        std::cout << "Replaying : " << node->name() << std::endl;
        uint64_t runtime = 1ul;
        if (node->runtime() != 0ul) {
            // chakra runtimes are in microseconds and we should convert it
            // into nanoseconds
            runtime = node->runtime() * 1000;
        }
        DataSet* fp = new DataSet(1);
        //fp->set_notifier(this, EventType::CollectiveCommunicationFinished);
        collective_comm_node_id_map[fp->my_id] = node->id();
        collective_comm_wrapper_map[fp->my_id] = fp;
        sys->register_event(fp, EventType::General, nullptr,
        // chakra runtimes are in microseconds and we
        // should convert it into nanoseconds
                runtime);
        fp->set_notifier(this, EventType::CollectiveCommunicationFinished);
        return;
    }

    vector<bool> involved_dim;

    // We prioritize involved_dim if exists.
    if (node->has_other_attr("involved_dim")) {
        const ChakraProtoMsg::AttributeProto& attr =
            node->get_other_attr("involved_dim");

        // Ensure the attribute is of type bool_list before accessing
        if (attr.has_bool_list()) {
            const ChakraProtoMsg::BoolList& bool_list = attr.bool_list();

            // Traverse bool_list and add values to involved_dim
            for (int i = 0; i < bool_list.values_size(); ++i) {
                involved_dim.push_back(bool_list.values(i));
            }
        } else {
            cerr << "Expected bool_list in involved_dim but found another type."
                 << endl;
            exit(EXIT_FAILURE);
        }
    } else if (comm_group == nullptr) {
        // involved_dim and comm_group do not exist in ETFeeder.
        // Assume involved_dim = [1,1,1,1] which we could simulate at most 4-Dimension.

    	for(int i = 0; i < 4; i++)
            involved_dim.push_back(true);
    }

    if (!node->is_cpu_op() &&
        (node->type() == ChakraNodeType::COMM_COLL_NODE)) {
        if (node->comm_type() == ChakraCollectiveCommType::ALL_REDUCE) {
            DataSet* fp =
                sys->generate_all_reduce(node->comm_size(), involved_dim,
                                         comm_group, node->comm_priority());
            collective_comm_node_id_map[fp->my_id] = node->id();
            collective_comm_wrapper_map[fp->my_id] = fp;
            fp->set_notifier(this, EventType::CollectiveCommunicationFinished);

        } else if (node->comm_type() == ChakraCollectiveCommType::ALL_TO_ALL) {
            DataSet* fp =
                sys->generate_all_to_all(node->comm_size(), involved_dim,
                                         comm_group, node->comm_priority());
            collective_comm_node_id_map[fp->my_id] = node->id();
            collective_comm_wrapper_map[fp->my_id] = fp;
            fp->set_notifier(this, EventType::CollectiveCommunicationFinished);

        } else if (node->comm_type() == ChakraCollectiveCommType::ALL_GATHER) {
            DataSet* fp =
                sys->generate_all_gather(node->comm_size(), involved_dim,
                                         comm_group, node->comm_priority());
            collective_comm_node_id_map[fp->my_id] = node->id();
            collective_comm_wrapper_map[fp->my_id] = fp;
            fp->set_notifier(this, EventType::CollectiveCommunicationFinished);

        } else if (node->comm_type() ==
                   ChakraCollectiveCommType::REDUCE_SCATTER) {
            DataSet* fp =
                sys->generate_reduce_scatter(node->comm_size(), involved_dim,
                                             comm_group, node->comm_priority());
            collective_comm_node_id_map[fp->my_id] = node->id();
            collective_comm_wrapper_map[fp->my_id] = fp;
            fp->set_notifier(this, EventType::CollectiveCommunicationFinished);

        } else if (node->comm_type() == ChakraCollectiveCommType::BROADCAST) {
            // broadcast colelctive has not been implemented in ASTRA-SIM yet.
            // So, we just use its real system mesurements
            uint64_t runtime = 1ul;
            if (node->runtime() != 0ul) {
                // chakra runtimes are in microseconds and we should convert it
                // into nanoseconds
                runtime = node->runtime() * 1000;
            }
            DataSet* fp = new DataSet(1);
            fp->set_notifier(this, EventType::CollectiveCommunicationFinished);
            collective_comm_node_id_map[fp->my_id] = node->id();
            collective_comm_wrapper_map[fp->my_id] = fp;
            sys->register_event(fp, EventType::General, nullptr,
                                // chakra runtimes are in microseconds and we
                                // should convert it into nanoseconds
                                runtime);
            fp->set_notifier(this, EventType::CollectiveCommunicationFinished);
        }
    } else if (node->type() == ChakraNodeType::COMM_SEND_NODE) {
        sim_request snd_req;
        snd_req.srcRank = node->comm_src();
        snd_req.dstRank = node->comm_dst();
        snd_req.reqType = UINT8;
        SendPacketEventHandlerData* sehd = new SendPacketEventHandlerData;
        sehd->callable = this;
        sehd->wlhd = new WorkloadLayerHandlerData;
        sehd->wlhd->node_id = node->id();
        sehd->event = EventType::PacketSent;
        sys->front_end_sim_send(0, Sys::dummy_data, node->comm_size(), UINT8,
                                node->comm_dst(), node->comm_tag(), &snd_req,
                                Sys::FrontEndSendRecvType::NATIVE,
                                &Sys::handleEvent, sehd);
    } else if (node->type() == ChakraNodeType::COMM_RECV_NODE) {
        sim_request rcv_req;
        RecvPacketEventHandlerData* rcehd = new RecvPacketEventHandlerData;
        rcehd->wlhd = new WorkloadLayerHandlerData;
        rcehd->wlhd->node_id = node->id();
        rcehd->workload = this;
        rcehd->event = EventType::PacketReceived;
        sys->front_end_sim_recv(0, Sys::dummy_data, node->comm_size(), UINT8,
                                node->comm_src(), node->comm_tag(), &rcv_req,
                                Sys::FrontEndSendRecvType::NATIVE,
                                &Sys::handleEvent, rcehd);
    } else {
        LoggerFactory::get_logger("workload")
            ->critical("Unknown communication node type");
        exit(EXIT_FAILURE);
    }
}

void Workload::skip_invalid(shared_ptr<Chakra::ETFeederNode> node) {
    et_feeder->freeChildrenNodes(node->id());
    et_feeder->removeNode(node->id());
}

void Workload::call(EventType event, CallData* data) {
    if (is_finished) {
        return;
    }

    if (event == EventType::CollectiveCommunicationFinished) {
        IntData* int_data = (IntData*)data;
        hw_resource->tics_gpu_comms += int_data->execution_time;
        uint64_t node_id = collective_comm_node_id_map[int_data->data];
        shared_ptr<Chakra::ETFeederNode> node = et_feeder->lookupNode(node_id);

        if (sys->trace_enabled) {
            LoggerFactory::get_logger("workload")
                ->debug("callback,sys->id={}, tick={}, node->id={}, "
                        "node->name={}, node->type={}",
                        sys->id, Sys::boostedTick(), node->id(), node->name(),
                        static_cast<uint64_t>(node->type()));
        }

        hw_resource->release(node);

        et_feeder->freeChildrenNodes(node_id);

        issue_dep_free_nodes();

        et_feeder->removeNode(node_id);

        // The Dataset class provides statistics that should be used later to
        // dump more statistics in the workload layer
        delete collective_comm_wrapper_map[node_id];
        collective_comm_wrapper_map.erase(node_id);

    } else {
        if (data == nullptr) {
            issue_dep_free_nodes();
        } else {
            WorkloadLayerHandlerData* wlhd = (WorkloadLayerHandlerData*)data;
            shared_ptr<Chakra::ETFeederNode> node =
                et_feeder->lookupNode(wlhd->node_id);

            if (sys->trace_enabled) {
                LoggerFactory::get_logger("workload")
                    ->debug("callback,sys->id={}, tick={}, node->id={}, "
                            "node->name={}, node->type={}",
                            sys->id, Sys::boostedTick(), node->id(),
                            node->name(), static_cast<uint64_t>(node->type()));
            }

            hw_resource->release(node);

            et_feeder->freeChildrenNodes(node->id());

            issue_dep_free_nodes();

            et_feeder->removeNode(wlhd->node_id);
            delete wlhd;
        }
    }

    if (!et_feeder->hasNodesToIssue() &&
        (hw_resource->num_in_flight_cpu_ops == 0) &&
        (hw_resource->num_in_flight_gpu_comp_ops == 0) &&
        (hw_resource->num_in_flight_gpu_comm_ops == 0)) {
        report();
        is_finished = true;
    }
}

void Workload::fire() {
    call(EventType::General, NULL);
}

void Workload::report() {
    Tick curr_tick = Sys::boostedTick();
    LoggerFactory::get_logger("workload")
        ->info("sys[{}] finished, {} cycles, exposed communication {} cycles.",
               sys->id, curr_tick, curr_tick - hw_resource->tics_gpu_ops);
}
