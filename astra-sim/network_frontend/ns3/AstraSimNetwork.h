//change style to take cpp classes
#include "/home/lightkhan/ns3-interface/astra-sim/astra-sim/system/AstraNetworkAPI.hh"
#include<iostream>
using namespace std;
class ASTRASimNetwork: AstraSim::AstraNetworkAPI{
    public:
        ASTRASimNetwork(int rank):AstraNetworkAPI(rank){
            cout<<"hello constructor\n";
        }
        ~ASTRASimNetwork(){}
        int sim_comm_size(AstraSim::sim_comm comm, int* size){
            return 0;
        }
        int sim_finish(){
            cout<<"sim finish\n";
            return 0;
        }
        double sim_time_resolution(){
            return 0;
        }
        int sim_init(AstraSim::AstraMemoryAPI* MEM){
            return 0;
        }
        AstraSim::timespec_t sim_get_time(){
            AstraSim::timespec_t timeSpec;
            // timeSpec.time_type_e = "SE";
            timeSpec.time_val = 0.0;
            return timeSpec;
        }
        virtual void sim_schedule(
            AstraSim::timespec_t delta,
            void (*fun_ptr)(void* fun_arg),
            void* fun_arg){
                return;
            }
        virtual int sim_send(
            void* buffer,
            uint64_t count,
            int type,
            int dst,
            int tag,
            AstraSim::sim_request* request,
            void (*msg_handler)(void* fun_arg),
            void* fun_arg){
                system("./waf --run  scratch/myTCPMultiple"); //add arguments to pass src, dst, count
                return 0;
            }
        virtual int sim_recv(
            void* buffer,
            uint64_t count,
            int type,
            int src,
            int tag,
            AstraSim::sim_request* request,
            void (*msg_handler)(void* fun_arg),
            void* fun_arg){
                return 0;
            }

};
