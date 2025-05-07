#pragma once

#include <cstdint>
#include <ios>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace HTSim {

enum class HTSimProto {
    None,
    Tcp
};

std::stringstream& operator>> (std::stringstream& is, HTSimProto& proto);

typedef uint32_t flowid_t;
typedef uint32_t triggerid_t;
typedef uint64_t simtime_picosec;

enum class Dir { Send, Receive };

// Temporary struct to pass trace info
class FlowInfo {
    public:
    FlowInfo(int _src, int _dst, uint64_t _size, int _tag)
     : src(_src), dst(_dst), size(_size), tag(_tag) {}
    int src, dst;
    int size;
    // Tag for all flows belonging to a collective
    int tag;
};

struct HTSimConf {
    // When this option is true, a flow will be marked as finished for the receiver
    // as soon as the last packet is received instead of waiting for sender to get ack.
    bool recv_flow_finish;
};

class MsgEvent {
public:
    int src_id;
    int dst_id;
    Dir type;
    // Indicates the number of bytes remaining to be sent or received.
    // Initialized with the original size of the message, and
    // incremented/decremented depending on how many bytes were sent/received.
    // Eventually, this value will reach 0 when the event has completed.
    int remaining_msg_bytes;
    void* fun_arg;
    void (*msg_handler)(void* fun_arg);
    MsgEvent(int _src_id,
             int _dst_id,
             Dir _type,
             int _remaining_msg_bytes,
             void* _fun_arg,
             void (*_msg_handler)(void* fun_arg))
        : src_id(_src_id),
          dst_id(_dst_id),
          type(_type),
          remaining_msg_bytes(_remaining_msg_bytes),
          fun_arg(_fun_arg),
          msg_handler(_msg_handler) {}

    // Default constructor to prevent compile errors. When looking up MsgEvents
    // from maps such as send_waiting, we should always check that a MsgEvent exists
    // for the given key. (i.e. this default constructor should not be called in
    // runtime.)
    MsgEvent()
        : src_id(0),
          dst_id(0),
          type(Dir::Send),
          remaining_msg_bytes(0),
          fun_arg(nullptr),
          msg_handler(nullptr) {}

    // CallHandler will call the callback handler associated with this MsgEvent.
    void callHandler() {
        msg_handler(fun_arg);
        return;
    }
};

// MsgEventKey is a key to uniquely identify each MsgEvent.
//  - Pair <Tag, Pair <src_id, dst_id>>
typedef std::pair<int, std::pair<int, int>> MsgEventKey;

struct tm_info {
    int nodes;
    // We don't need connections/triggers information,
    // they're generated on the fly.
    // Can add link failures or astra-sim specific config here.
};

typedef void (*EventHandler)(void*);

class HTSimSession {
    public:
        static HTSimSession& instance();
        static HTSimSession& init(const HTSim::tm_info* const tm, int argc, char** argv, HTSimProto proto = HTSimProto::None);
        void run(const HTSim::tm_info* const tm);
        void finish();
        void stop_simulation();
        void send_flow(HTSim::FlowInfo flow,
                       int flow_id,
                       EventHandler msg_handler,
                       void* fun_arg);
        double get_time_ns();
        double get_time_us();
        void schedule_astra_event(long double delta, EventHandler msg_handler, void* fun_arg);

        // Used to count how many bytes were sent/received by this node.
        // Refer to sim_finish().
        //   - key: Pair <node_id, send/receive>. Where 'send/receive' indicates if the
        //   value is for send or receive
        //   - value: Number of bytes this node has sent (if send/receive is 0) and
        //   received (if send/receive is 1)
        static std::map<std::pair<int, HTSim::Dir>, int> node_bytes_sent;

        // SentHash stores a MsgEvent for sim_send events and its callback handler.
        //   - key: A pair of <MsgEventKey, port_id>.
        //          A single collective phase can be split into multiple sim_send messages, which all
        //          have the same MsgEventKey.
        //          TODO: Adding port_id as key is a hacky solution. The real solution would be to split
        //          this map, similar to recv_waiting and msg_standby.
        //   - value: A MsgEvent instance that indicates that Sys layer is waiting for a
        //   send event to finish
        static std::map<std::pair<HTSim::MsgEventKey, int>, HTSim::MsgEvent> send_waiting;

        //   - recv_waiting holds messages where sim_recv has been called but HTSim has
        //   not yet simulated the message arriving,
        //   - msg_standby holds messages which HTSim has simulated the arrival, but sim_recv
        //   has not yet been called.

        //   - key: A MsgEventKey instance.
        //   - value: A MsgEvent instance that indicates that Sys layer is waiting for a
        //   receive event to finish
        static std::map<HTSim::MsgEventKey, HTSim::MsgEvent> recv_waiting;

        //   - key: A MsgEventKey instance.
        //   - value: The number of bytes that HTSim has simulated completed, but the
        //   System layer has not yet called sim_recv
        static std::map<HTSim::MsgEventKey, int> msg_standby;

        static std::map<int, int> flow_id_to_tag;

        static void notify_receiver_receive_data(int src_id,
                                                 int dst_id,
                                                 int message_size,
                                                 int tag,
                                                 int flow_id);
        static void notify_sender_sending_finished(int src_id,
                                                   int dst_id,
                                                   int message_size,
                                                   int tag,
                                                   int flow_id);
        static void flow_finish_send(int src_id, int dst_id, int msg_size, int tag);
        static void flow_finish_recv(int src_id, int dst_id, int msg_size, int tag);
        static HTSimConf conf;
        HTSimSession(const HTSimSession&) = delete;    // disable Copy Constructor
        void operator=(const HTSimSession&) = delete;  // disable Assign Constructor
        class HTSimSessionImpl;
    private:
        // Singleton pattern
        HTSimSession(const tm_info* const tm, int argc, char** argv, HTSimProto proto);
        ~HTSimSession();
        static HTSimSession* session;
        // PImpl pattern
        std::unique_ptr<HTSimSessionImpl> impl;
    };

}  // namespace HTSim