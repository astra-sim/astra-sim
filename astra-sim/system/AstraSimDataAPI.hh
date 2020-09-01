#ifndef __ASTRASIMDATAAPI_HH__
#define __ASTRASIMDATAAPI_HH__
#include <list>
#include <iostream>
class LayerData{
    public:
        std::string layer_name;
        double total_forward_pass_compute;
        double total_weight_grad_compute;
        double total_input_grad_compute;
        double total_waiting_for_fwd_comm;
        double total_waiting_for_wg_comm;
        double total_waiting_for_ig_comm;
        double total_fwd_comm;
        double total_weight_grad_comm;
        double total_input_grad_comm;
        std::list<std::pair<int,double>> avg_queuing_delay; //int is phase #, double is latency
        std::list<std::pair<int,double>> avg_network_message_dealy; //int is phase #, double is latency

};
class AstraSimDataAPI{
    public:
      std::string run_name;
      std::list<LayerData> layers_stats;
      double workload_finished_time;
      double total_compute;
      double total_exposed_comm;
};
#endif