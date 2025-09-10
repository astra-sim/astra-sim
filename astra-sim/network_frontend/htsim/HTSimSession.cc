#include "HTSimSession.hh"
#include "HTSimSessionImpl.hh"
#include "HTSimProtoTcp.hh"
#include "HTSimProtoUet.hh"

#include <iostream>

namespace HTSim {

std::map<std::pair<int, HTSim::Dir>, uint64_t> HTSimSession::node_bytes_sent;
std::map<std::pair<HTSim::MsgEventKey, FlowMsgId>, HTSim::MsgEvent>
    HTSimSession::send_waiting;
std::map<HTSim::MsgEventKey, HTSim::MsgEvent> HTSimSession::recv_waiting;
std::map<HTSim::MsgEventKey, uint64_t> HTSimSession::msg_standby;
std::map<FlowMsgId, int> HTSimSession::id_to_tag;
HTSimSession* HTSimSession::session = nullptr;
HTSimConf HTSimSession::conf;

typedef void (*EventHandler)(void*);

class AstraEventSrc : public EventSource {
public:
    AstraEventSrc(EventHandler msg_handler, void* fun_arg, EventList& eventList);
    void doNextEvent();

private:
    EventHandler _msg_handler;
    void* _fun_arg;
};

// Event source for scheduling callbacks to be executed by HTSim
std::vector<AstraEventSrc*> astra_events;

AstraEventSrc::AstraEventSrc(EventHandler msg_handler,
                             void* fun_arg,
                             EventList& eventList)
    : EventSource(eventList, "astraSimSrc"), _msg_handler(msg_handler), _fun_arg(fun_arg) {
}

void AstraEventSrc::doNextEvent() {
    // Run the handler
    _msg_handler(_fun_arg);
}

std::stringstream& operator>> (std::stringstream& is, HTSimProto& proto) {
    std::string s;
    is >> s;
    if (s == "uet") {
        proto = HTSimProto::Uet;
    } else if (s == "tcp") {
        proto = HTSimProto::Tcp;
    } else {
        proto = HTSimProto::None;
    }
    return is;
}

// Send_flow commands the HTSim simulator to schedule a message to be sent
// between two pair of nodes. send_flow is triggered by sim_send.
void HTSimSession::send_flow(FlowInfo flow,
                            FlowMsgId id,
                            void (*msg_handler)(void* fun_arg),
                            void* fun_arg) {
    // Create a MsgEvent instance and register callback function.
    MsgEvent send_event = MsgEvent(flow.src, flow.dst, Dir::Send, flow.size, fun_arg, msg_handler);
    id_to_tag[id] = flow.tag;
    std::pair<MsgEventKey, FlowMsgId> send_event_key = std::make_pair(
        std::make_pair(flow.tag, std::make_pair(send_event.src_id, send_event.dst_id)), id);
    HTSimSession::send_waiting[send_event_key] = send_event;

    // Create a queue pair and schedule within the HTSim simulator.
    if (!conf.conn_reuse) {
        impl->schedule_htsim_event(flow, id.first, std::nullopt);
    } else {
        impl->schedule_htsim_event(flow, id.first, id.second);
    }
}

// notify_receiver_receive_data looks at whether the astra-sim has issued
// sim_recv for this message. If the system layer is waiting for this message,
// call the callback handler for the MsgEvent. If the system layer is not *yet*
// waiting for this message, register that this message has arrived,
// so that the system layer can later call the callback handler when sim_recv
// is called.
void HTSimSession::notify_receiver_receive_data(int src_id,
                                                int dst_id,
                                                uint64_t message_size,
                                                int tag) {
    MsgEventKey recv_expect_event_key = make_pair(tag, std::make_pair(src_id, dst_id));

    if (HTSimSession::recv_waiting.find(recv_expect_event_key) !=
        HTSimSession::recv_waiting.end()) {
        // The Sys object is waiting for packets to arrive.
        MsgEvent recv_expect_event = recv_waiting[recv_expect_event_key];
        if (message_size == recv_expect_event.remaining_msg_bytes) {
            // We received exactly the amount of data what Sys object was expecting.
            HTSimSession::recv_waiting.erase(recv_expect_event_key);
            recv_expect_event.callHandler();
        } else if (message_size > recv_expect_event.remaining_msg_bytes) {
            // We received more packets than the Sys object is expecting.
            // Place task in received_msg_standby_hash and wait for Sys object to issue more sim_recv
            // calls. Call callback handler for the amount Sys object was waiting for.
            HTSimSession::msg_standby[recv_expect_event_key] = message_size - recv_expect_event.remaining_msg_bytes;
            HTSimSession::recv_waiting.erase(recv_expect_event_key);
            recv_expect_event.callHandler();
        } else {
            // There are still packets to arrive.
            // Reduce the number of packets we are waiting for. Do not call callback
            // handler.
            recv_expect_event.remaining_msg_bytes -= message_size;
            HTSimSession::recv_waiting[recv_expect_event_key] = recv_expect_event;
        }
    } else {
        // The Sys object is not yet waiting for packets to arrive.
        if (HTSimSession::msg_standby.find(recv_expect_event_key) ==
            HTSimSession::msg_standby.end()) {
            // Place task in msg_standby and wait for Sys object to issue more sim_recv
            // calls.
            HTSimSession::msg_standby[recv_expect_event_key] = message_size;
        } else {
            // Sys object is still waiting. Add number of bytes we are waiting for.
            HTSimSession::msg_standby[recv_expect_event_key] += message_size;
        }
    }

    // Add to the number of total bytes received.
    if (HTSimSession::node_bytes_sent.find(std::make_pair(dst_id, Dir::Receive)) ==
        HTSimSession::node_bytes_sent.end()) {
        HTSimSession::node_bytes_sent[std::make_pair(dst_id, Dir::Receive)] = message_size;
    } else {
        HTSimSession::node_bytes_sent[std::make_pair(dst_id, Dir::Receive)] += message_size;
    }
}

void HTSimSession::notify_sender_sending_finished(int src_id,
                                                  int dst_id,
                                                  uint64_t message_size,
                                                  int tag,
                                                  FlowMsgId id) {
    // Lookup the send_event registered at send_flow().
    std::pair<MsgEventKey, FlowMsgId> send_event_key =
        std::make_pair(std::make_pair(tag, std::make_pair(src_id, dst_id)), id);
    if (HTSimSession::send_waiting.find(send_event_key) == HTSimSession::send_waiting.end()) {
        std::cerr << "Cannot find send_event in sent_hash. Something is wrong. "
             << "src_id, dst_id: " << src_id << " " << dst_id << " : " << tag << " - " << id.first
             << " - " << id.second << "\n";
        assert(0 && "notify_sender_sending_finished failed");
    }

    // Verify that the (HTSim identified) sent message size matches what was
    // expected by the system layer.
    MsgEvent send_event = HTSimSession::send_waiting[send_event_key];
    HTSimSession::send_waiting.erase(send_event_key);

    // Add to the number of total bytes sent.
    if (HTSimSession::node_bytes_sent.find(std::make_pair(src_id, Dir::Send)) ==
        HTSimSession::node_bytes_sent.end()) {
        HTSimSession::node_bytes_sent[std::make_pair(src_id, Dir::Send)] = message_size;
    } else {
        HTSimSession::node_bytes_sent[std::make_pair(src_id, Dir::Send)] += message_size;
    }
    send_event.callHandler();
}

// flow_finish is triggered by HTSim to indicate that a flow has finished.
// Registered as the callback handler for the Uet
// instance created at send_flow. This registration is done at
// common.h::SetupNetwork().
void HTSimSession::flow_finish_send(int src_id, int dst_id, uint64_t msg_size, unsigned flow_id, unsigned msg_id) {
    FlowMsgId id = {flow_id, msg_id};
    int tag = id_to_tag[id];

    // Let sender knows that the flow has finished.
    notify_sender_sending_finished(src_id, dst_id, msg_size, tag, id);

    if (!conf.recv_flow_finish) {
        // Let receiver knows that it has received packets.
        notify_receiver_receive_data(src_id, dst_id, msg_size, tag);
    }
}

void HTSimSession::flow_finish_recv(int src_id, int dst_id, uint64_t msg_size, unsigned flow_id, unsigned msg_id) {
    FlowMsgId id = {flow_id, msg_id};

    if (conf.recv_flow_finish) {
        int tag = id_to_tag[id];
        // Let receiver knows that it has received packets.
        notify_receiver_receive_data(src_id, dst_id, msg_size, tag);
    }

}

void HTSimSession::finish() {
    impl->finish();
}

void HTSimSession::stop_simulation() {
    impl->stop_simulation();
}

void HTSimSession::HTSimSessionImpl::stop_simulation() {
    std::cout << "HTSim stopping simulation..." << std::endl;
    auto now = eventlist.now();
    eventlist.setEndtime(now);
}

// Constructor creates inner impl
HTSimSession& HTSimSession::init(const HTSim::tm_info* const tm, const int argc, char** argv) {
    if (session != nullptr)
        assert(0 && "HTSim session initialized twice");
    static HTSimSession session_(tm, argc, argv);
    session = &session_;
    session->conf.conn_reuse = tm->conn_reuse;
    return session_;
};

// Instance getter
HTSimSession& HTSimSession::instance() {
    if (session == nullptr)
        assert(0 && "HTSim session not initialized");
    return (*session);
}

HTSimSession::HTSimSession(const HTSim::tm_info* const tm, int argc, char** argv) {
    switch(tm->proto) {
        case HTSimProto::Uet:
            impl = std::make_unique<HTSimProtoUet>(tm, argc, argv);
            break;
        case HTSimProto::Tcp:
            impl = std::make_unique<HTSimProtoTcp>(tm, argc, argv);
            break;
        default:
            std::cerr << "Unknown HTSim protocol" << std::endl;
            abort();
    }
}

void HTSimSession::schedule_astra_event(long double when_ns,
                                        void (*msg_handler)(void* fun_arg),
                                        void* fun_arg) {
    AstraEventSrc* src = new AstraEventSrc(msg_handler, fun_arg, impl->eventlist);
    astra_events.push_back(src);
    impl->eventlist.sourceIsPendingRel(*src, timeFromNs(when_ns));
}

// Wrapper functions

void HTSimSession::run(const HTSim::tm_info* const tm) {
    impl->run(tm);
}

double HTSimSession::get_time_ns() {
    return timeAsNs(impl->eventlist.now());
}

double HTSimSession::get_time_us() {
    return timeAsUs(impl->eventlist.now());
}

FlowMsgId HTSimSession::get_flow_id(FlowInfo flow) {
    FlowMsgId id;
    static unsigned flow_id_tracker = 0;
    static std::map<std::pair<unsigned, unsigned>, unsigned> pair_to_flow_id;
    static std::map<unsigned, unsigned> flow_id_to_msg_id;
    if (!conf.conn_reuse) {
        flow_id_tracker += 1;
        id.first = flow_id_tracker;
        id.second = 0;
    } else {
        auto flow_pair = std::make_pair(flow.src, flow.dst);
        if (pair_to_flow_id.find(flow_pair) == pair_to_flow_id.end()) {
            flow_id_tracker += 1;
            id.first = flow_id_tracker;
            // HTSim starts msg_id at 1
            id.second = 1;
            pair_to_flow_id[flow_pair] = id.first;
            flow_id_to_msg_id[id.first] = id.second;
        } else {
            id.first = pair_to_flow_id[flow_pair];
            flow_id_to_msg_id[id.first] += 1;
            id.second = flow_id_to_msg_id[id.first];
        }
    }
    return id;
}

HTSimSession::~HTSimSession() {
    for (auto* event: astra_events) {
        delete event;
    }
    astra_events.clear();
}

} // namespace HTSim