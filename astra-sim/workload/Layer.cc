/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "Layer.hh"
#include "astra-sim/system/DataSet.hh"
#include "astra-sim/system/IntData.hh"
namespace AstraSim {
Layer::Layer(
    std::string id,
    int layer_num,
    Sys* generator,
    Workload* workload,
    Tick fwd_pass_compute_time,
    ComType fwd_pass_comm_type,
    uint64_t fwd_pass_comm_size,
    std::vector<bool> fwd_pass_comm_involved_dimensions,
    Tick input_grad_compute_time,
    ComType input_grad_comm_type,
    uint64_t input_grad_comm_size,
    std::vector<bool> input_grad_comm_involved_dimensions,
    Tick weight_grad_compute_time,
    ComType weight_grad_comm_type,
    uint64_t weight_grad_comm_size,
    std::vector<bool> weight_grad_comm_involved_dimensions,
    Tick weight_grad_update_time,
    ParallelismPolicy specific_policy) {
  this->id = id;
  this->layer_num = layer_num;
  this->generator = generator;
  this->workload = workload;
  this->fwd_pass_compute_time = fwd_pass_compute_time;
  this->fwd_pass_comm_type = fwd_pass_comm_type;
  this->fwd_pass_comm_size = fwd_pass_comm_size;
  this->fwd_pass_comm_involved_dimensions = fwd_pass_comm_involved_dimensions;
  this->input_grad_compute_time = input_grad_compute_time;
  this->input_grad_comm_type = input_grad_comm_type;
  this->input_grad_comm_size = input_grad_comm_size;
  this->input_grad_comm_involved_dimensions =
      input_grad_comm_involved_dimensions;
  this->weight_grad_compute_time = weight_grad_compute_time;
  this->weight_grad_comm_type = weight_grad_comm_type;
  this->weight_grad_comm_size = weight_grad_comm_size;
  this->weight_grad_comm_involved_dimensions =
      weight_grad_comm_involved_dimensions;
  this->collective_counter = 0;

  this->weight_grad_update_time = weight_grad_update_time;
  this->fwd_update_time = weight_grad_update_time;
  this->input_grad_update_time = weight_grad_update_time;

  // this->fwd_pass_dataset=NULL;
  //->input_grad_dataset=NULL;
  // this->weight_grad_dataset=NULL;
  this->total_forward_pass_compute = 0;
  this->total_input_grad_compute = 0;
  this->total_weight_grad_compute = 0;
  this->total_weight_grad_comm = 0;
  this->total_input_grad_comm = 0;
  this->total_fwd_comm = 0;
  this->total_waiting_for_wg_comm = 0;
  this->total_waiting_for_ig_comm = 0;
  this->total_waiting_for_fwd_comm = 0;
  this->last_fwd_finished = 0;
  this->last_ig_finished = 0;
  this->last_wg_finished = 0;
  this->needs_fwd_in_bckwd_initiation = false;
  this->is_checkpoint = false;
  this->specific_parallellism = specific_policy;
  assert(generator != NULL);
}

void Layer::call(EventType event, CallData* mdata) {
  if (event == EventType::Wight_Grad_Comm_Finished) {
    last_wg_finished = Sys::boostedTick();
    generator->register_event(
        this,
        EventType::Wight_Grad_Comm_Finished_After_Delay,
        mdata,
        weight_grad_update_time);
    return;
  } else if (event == EventType::Input_Grad_Comm_Finished) {
    last_ig_finished = Sys::boostedTick();
    generator->register_event(
        this,
        EventType::Input_Grad_Comm_Finished_After_Delay,
        mdata,
        input_grad_update_time);
    return;
  } else if (event == EventType::Fwd_Comm_Finished) {
    last_fwd_finished = Sys::boostedTick();
    generator->register_event(
        this, EventType::Fwd_Comm_Finished_After_Delay, mdata, fwd_update_time);
    return;
  }
  int data = ((IntData*)mdata)->data;
  IntData* intData = ((IntData*)mdata);
  if (event == EventType::Wight_Grad_Comm_Finished_After_Delay) {
    if (generator->id == 0) {
      std::cout << "***** info: weight gradient collective for layer: " << id
                << " is finished************" << std::endl;
    }
    weight_grad_datasets[data]->finish_tick += weight_grad_update_time;
    total_weight_grad_comm += weight_grad_datasets[data]->finish_tick -
        weight_grad_datasets[data]->creation_tick;
    if (weight_grad_datasets.size() == 1 &&
        wg_barrier == CollectiveBarrier::Blocking) {
      total_waiting_for_wg_comm += weight_grad_datasets[data]->finish_tick -
          weight_grad_datasets[data]->creation_tick;
      update_stream_stats(weight_grad_datasets[data]);
      int dataset_streams = weight_grad_datasets[data]->total_streams;
      delete weight_grad_datasets[data];
      weight_grad_datasets.erase(data);
      workload->call(EventType::General, NULL);
      generator->increase_finished_streams(dataset_streams);
      delete intData;
      return;
    } else if (started_waiting_for_weight_grad.size() > 0) {
      total_waiting_for_wg_comm += weight_grad_datasets[data]->finish_tick -
          started_waiting_for_weight_grad.front();
      started_waiting_for_weight_grad.pop_front();
      update_stream_stats(weight_grad_datasets[data]);
      int dataset_streams = weight_grad_datasets[data]->total_streams;
      delete weight_grad_datasets[data];
      weight_grad_datasets.erase(data);
      workload->call(EventType::General, NULL);
      generator->increase_finished_streams(dataset_streams);
      delete intData;
      return;
    }
    update_stream_stats(weight_grad_datasets[data]);
    int dataset_streams = weight_grad_datasets[data]->total_streams;
    delete weight_grad_datasets[data];
    weight_grad_datasets.erase(data);
    generator->increase_finished_streams(dataset_streams);
    delete intData;
    return;
  } else if (event == EventType::Input_Grad_Comm_Finished_After_Delay) {
    if (generator->id == 0) {
      std::cout << "***** info: input gradient collective for layer: " << id
                << " is finished************" << std::endl;
    }
    input_grad_datasets[data]->finish_tick += input_grad_update_time;
    total_input_grad_comm += input_grad_datasets[data]->finish_tick -
        input_grad_datasets[data]->creation_tick;
    if (input_grad_datasets.size() == 1 &&
        ig_barrier == CollectiveBarrier::Blocking) {
      total_waiting_for_ig_comm += input_grad_datasets[data]->finish_tick -
          input_grad_datasets[data]->creation_tick;
      update_stream_stats(input_grad_datasets[data]);
      int dataset_streams = input_grad_datasets[data]->total_streams;
      delete input_grad_datasets[data];
      input_grad_datasets.erase(data);
      workload->call(EventType::General, NULL);
      generator->increase_finished_streams(dataset_streams);
      delete intData;
      return;
    } else if (started_waiting_for_input_grad.size() > 0) {
      total_waiting_for_ig_comm += input_grad_datasets[data]->finish_tick -
          started_waiting_for_input_grad.front();
      started_waiting_for_input_grad.pop_front();
      update_stream_stats(input_grad_datasets[data]);
      int dataset_streams = input_grad_datasets[data]->total_streams;
      delete input_grad_datasets[data];
      input_grad_datasets.erase(data);
      workload->call(EventType::General, NULL);
      generator->increase_finished_streams(dataset_streams);
      delete intData;
      return;
    }
    update_stream_stats(input_grad_datasets[data]);
    int dataset_streams = input_grad_datasets[data]->total_streams;
    delete input_grad_datasets[data];
    input_grad_datasets.erase(data);
    generator->increase_finished_streams(dataset_streams);
    delete intData;
    return;
  } else if (event == EventType::Fwd_Comm_Finished_After_Delay) {
    if (generator->id == 0) {
      std::cout << "***** info: fwd pass comm collective for layer: " << id
                << " is finished************" << std::endl;
    }
    fwd_pass_datasets[data]->finish_tick += fwd_update_time;
    total_fwd_comm += fwd_pass_datasets[data]->finish_tick -
        fwd_pass_datasets[data]->creation_tick;
    if (fwd_pass_datasets.size() == 1 &&
        fwd_barrier == CollectiveBarrier::Blocking) {
      total_waiting_for_fwd_comm += fwd_pass_datasets[data]->finish_tick -
          fwd_pass_datasets[data]->creation_tick;
      update_stream_stats(fwd_pass_datasets[data]);
      int dataset_streams = fwd_pass_datasets[data]->total_streams;
      delete fwd_pass_datasets[data];
      fwd_pass_datasets.erase(data);
      workload->call(EventType::General, NULL);
      generator->increase_finished_streams(dataset_streams);
      delete intData;
      return;
    } else if (started_waiting_for_fwd_pass.size() > 0) {
      if (generator->id == 0) {
        // std::cout<<"***** info: fwd pass comm collective for layer: "<<id<<"
        // is finished and callback called************"<<std::endl;
      }
      total_waiting_for_fwd_comm += fwd_pass_datasets[data]->finish_tick -
          started_waiting_for_fwd_pass.front();
      started_waiting_for_fwd_pass.pop_front();
      update_stream_stats(fwd_pass_datasets[data]);
      int dataset_streams = fwd_pass_datasets[data]->total_streams;
      delete fwd_pass_datasets[data];
      fwd_pass_datasets.erase(data);
      workload->call(EventType::General, NULL);
      generator->increase_finished_streams(dataset_streams);
      delete intData;
      return;
    }
    update_stream_stats(fwd_pass_datasets[data]);
    int dataset_streams = fwd_pass_datasets[data]->total_streams;
    delete fwd_pass_datasets[data];
    fwd_pass_datasets.erase(data);
    generator->increase_finished_streams(dataset_streams);
    delete intData;
    return;
  }
}

Tick Layer::get_fwd_pass_compute() {
  total_forward_pass_compute += fwd_pass_compute_time;
  return fwd_pass_compute_time;
}
Tick Layer::get_input_grad_compute() {
  total_input_grad_compute += input_grad_compute_time;
  return input_grad_compute_time;
}
Tick Layer::get_weight_grad_compute() {
  total_weight_grad_compute += weight_grad_compute_time;
  return weight_grad_compute_time;
}
void Layer::increment_waiting_for_wg() {
  total_waiting_for_wg_comm++;
}
void Layer::increment_waiting_for_ig() {
  total_waiting_for_ig_comm++;
}
void Layer::increment_waiting_for_fwd() {
  total_waiting_for_fwd_comm++;
}
bool Layer::is_fwd_pass_comm_finished() {
  if (fwd_pass_datasets.size() ==
      0) { // && Sys::boostedTick()-last_fwd_finished>=fwd_update_time
    return true;
  }
  return false;
}
bool Layer::is_fwd_pass_comm_finished_blocking() {
  if (fwd_pass_datasets.size() == 0) {
    return true;
  }
  if (started_waiting_for_fwd_pass.size() == 0) {
    started_waiting_for_fwd_pass.push_back(Sys::boostedTick());
  }
  return false;
}
bool Layer::is_input_grad_comm_finished() {
  if (input_grad_datasets.size() ==
      0) { //&& Sys::boostedTick()-last_ig_finished>=input_grad_update_time
    return true;
  }
  return false;
}
bool Layer::is_input_grad_comm_finished_blocking() {
  if (input_grad_datasets.size() ==
      0) { //&& Sys::boostedTick()-last_ig_finished>=input_grad_update_time
    return true;
  }
  if (started_waiting_for_input_grad.size() == 0) {
    started_waiting_for_input_grad.push_back(Sys::boostedTick());
  }
  return false;
}
bool Layer::is_weight_grad_comm_finished() {
  if (weight_grad_datasets.size() ==
      0) { //&& Sys::boostedTick()-last_wg_finished>=weight_grad_update_time
    return true;
  }
  return false;
}
bool Layer::is_weight_grad_comm_finished_blocking() {
  if (weight_grad_datasets.size() == 0) {
    return true;
  }
  if (started_waiting_for_weight_grad.size() == 0) {
    this->started_waiting_for_weight_grad.push_back(Sys::boostedTick());
  }
  return false;
}
void Layer::print_involved_dimensions(std::vector<bool>& involved_dimensions) {
  std::cout << " involved dimensions: ";
  for (int i = 0; i < involved_dimensions.size(); i++) {
    if (involved_dimensions[i] == true) {
      std::cout << " 1,";
    } else {
      std::cout << " 0,";
    }
  }
  std::cout << std::endl;
}
LayerData Layer::report(
    std::string run_name,
    int layer_num,
    int total_rows,
    int stat_row,
    CSVWriter* detailed,
    CSVWriter* EndToEnd,
    double& total_compute,
    double& total_exposed,
    bool seprate_log) {
  LayerData layerData;
  take_stream_stats_average();
  total_compute += (total_forward_pass_compute / FREQ);
  total_compute += (total_weight_grad_compute / FREQ);
  total_compute += (total_input_grad_compute / FREQ);
  total_exposed += (total_waiting_for_fwd_comm / FREQ);
  total_exposed += (total_waiting_for_wg_comm / FREQ);
  total_exposed += (total_waiting_for_ig_comm / FREQ);
  layerData.layer_name = id;
  layerData.total_forward_pass_compute = total_forward_pass_compute / FREQ;
  layerData.total_weight_grad_compute = total_weight_grad_compute / FREQ;
  layerData.total_input_grad_compute = total_input_grad_compute / FREQ;
  layerData.total_waiting_for_fwd_comm = total_waiting_for_fwd_comm / FREQ;
  layerData.total_waiting_for_wg_comm = total_waiting_for_wg_comm / FREQ;
  layerData.total_waiting_for_ig_comm = total_waiting_for_ig_comm / FREQ;
  layerData.total_fwd_comm = total_fwd_comm / FREQ;
  layerData.total_weight_grad_comm = total_weight_grad_comm / FREQ;
  layerData.total_input_grad_comm = total_input_grad_comm / FREQ;
  int i = 0;
  for (auto& qd : queuing_delay) {
    layerData.avg_queuing_delay.push_back(std::make_pair(i, qd / FREQ));
  }
  i = 1;
  for (auto& ml : net_message_latency) {
    layerData.avg_network_message_dealy.push_back(std::make_pair(i, ml / FREQ));
  }
  if (seprate_log) {
    std::cout << "*******************" << std::endl;
    std::cout << "Layer id: " << id << std::endl;
    std::cout << "Total collectives issued for this layer: "
              << collective_counter << std::endl;
    if (stat_row == 0) {
      EndToEnd->write_cell(layer_num * total_rows + 1, 0, id);
      detailed->write_cell(layer_num * total_rows + 1, 0, id);
    }
    EndToEnd->write_cell(layer_num * total_rows + 1 + stat_row, 1, run_name);
    detailed->write_cell(layer_num * total_rows + 1 + stat_row, 1, run_name);

    std::cout << "*************************  Workload stats  "
                 "************************* "
              << id << std::endl;

    std::cout << "id: " << id << " ,Total cycles spent on fwd pass compute: "
              << total_forward_pass_compute << std::endl;
    if (stat_row == 0 && layer_num == 0) {
      EndToEnd->write_cell(0, 2, "fwd compute");
    }
    EndToEnd->write_cell(
        layer_num * total_rows + 1 + stat_row,
        2,
        std::to_string(total_forward_pass_compute / FREQ));

    std::cout << "id: " << id << " ,Total cycles spent on weight grad compute: "
              << total_weight_grad_compute << std::endl;
    if (stat_row == 0 && layer_num == 0) {
      EndToEnd->write_cell(0, 3, "wg compute");
    }
    EndToEnd->write_cell(
        layer_num * total_rows + 1 + stat_row,
        3,
        std::to_string(total_weight_grad_compute / FREQ));

    std::cout << "id: " << id << " ,Total cycles spent on input grad compute: "
              << total_input_grad_compute << std::endl;
    if (stat_row == 0 && layer_num == 0) {
      EndToEnd->write_cell(0, 4, "ig compute");
    }
    EndToEnd->write_cell(
        layer_num * total_rows + 1 + stat_row,
        4,
        std::to_string(total_input_grad_compute / FREQ));

    std::cout << "id: " << id
              << " ,Total cycles spent idle waiting for fwd finish: "
              << total_waiting_for_fwd_comm << std::endl;
    if (stat_row == 0 && layer_num == 0) {
      EndToEnd->write_cell(0, 5, "fwd exposed comm");
    }
    EndToEnd->write_cell(
        layer_num * total_rows + 1 + stat_row,
        5,
        std::to_string(total_waiting_for_fwd_comm / FREQ));

    std::cout << "id: " << id
              << " ,Total cycles spent idle waiting for weight grad finish: "
              << total_waiting_for_wg_comm << std::endl;
    if (stat_row == 0 && layer_num == 0) {
      EndToEnd->write_cell(0, 6, "wg exposed comm");
    }
    EndToEnd->write_cell(
        layer_num * total_rows + 1 + stat_row,
        6,
        std::to_string(total_waiting_for_wg_comm / FREQ));

    std::cout << "id: " << id
              << " ,Total cycles spent idle waiting for input grad finish: "
              << total_waiting_for_ig_comm << std::endl;
    if (stat_row == 0 && layer_num == 0) {
      EndToEnd->write_cell(0, 7, "ig exposed comm");
    }
    EndToEnd->write_cell(
        layer_num * total_rows + 1 + stat_row,
        7,
        std::to_string(total_waiting_for_ig_comm / FREQ));

    std::cout << "id: " << id
              << " ,Total cycles spent on fwd pass comm: " << total_fwd_comm
              << std::endl;
    if (stat_row == 0 && layer_num == 0) {
      EndToEnd->write_cell(0, 8, "fwd total comm");
    }
    EndToEnd->write_cell(
        layer_num * total_rows + 1 + stat_row,
        8,
        std::to_string(total_fwd_comm / FREQ));

    std::cout << "id: " << id << " ,Total cycles spent on weight grad comm: "
              << total_weight_grad_comm << std::endl;
    if (stat_row == 0 && layer_num == 0) {
      EndToEnd->write_cell(0, 9, "wg total comm");
    }
    EndToEnd->write_cell(
        layer_num * total_rows + 1 + stat_row,
        9,
        std::to_string(total_weight_grad_comm / FREQ));

    std::cout << "id: " << id << " ,Total cycles spent on input grad comm: "
              << total_input_grad_comm << std::endl;
    if (stat_row == 0 && layer_num == 0) {
      EndToEnd->write_cell(0, 10, "ig total comm");
    }
    EndToEnd->write_cell(
        layer_num * total_rows + 1 + stat_row,
        10,
        std::to_string(total_input_grad_comm / FREQ));

    if (stat_row == 0 && layer_num == 0) {
      EndToEnd->write_cell(0, 11, "workload finished at");
    }
    EndToEnd->write_cell(
        layer_num * total_rows + 1 + stat_row,
        11,
        std::to_string(((double)Sys::boostedTick()) / FREQ));
    if (layer_num == workload->SIZE - 1) {
      if (stat_row == 0) {
        EndToEnd->write_cell(0, 12, "total comp");
        EndToEnd->write_cell(0, 13, "total exposed comm");
      }
      total_exposed = (((double)Sys::boostedTick()) / FREQ) - total_compute;
      EndToEnd->write_cell(1 + stat_row, 12, std::to_string(total_compute));
      EndToEnd->write_cell(1 + stat_row, 13, std::to_string(total_exposed));
    }

    /*std::cout<<"*************************  Shared bus stats
    ************************* "<<id<<std::endl; std::cout<<"id: "<<id<<"
    ,Average cycles spent on shared bus queue delay for transfer (per message):
    "<<total_shared_bus_transfer_queue_delay<<std::endl; if(stat_row==0 &&
    layer_num==0){ detailed->write_cell(0,2,"shared bus transfer queue delay");
    }
    detailed->write_cell(layer_num*total_rows+1+stat_row,2,std::to_string(total_shared_bus_transfer_queue_delay/FREQ));

    std::cout<<"id: "<<id<<" ,Average cycles spent on shared bus delay for
    transfer (per message): "<<total_shared_bus_transfer_delay<<std::endl;
    if(stat_row==0 && layer_num==0){
        detailed->write_cell(0,3,"shared bus transfer delay");
    }
    detailed->write_cell(layer_num*total_rows+1+stat_row,3,std::to_string(total_shared_bus_transfer_delay/FREQ));

    std::cout<<"id: "<<id<<" ,Average cycles spent on shared bus queue delay for
    processing (per message):
    "<<total_shared_bus_processing_queue_delay<<std::endl; if(stat_row==0 &&
    layer_num==0){ detailed->write_cell(0,4,"shared bus processing queue
    delay");
    }
    detailed->write_cell(layer_num*total_rows+1+stat_row,4,std::to_string(total_shared_bus_processing_queue_delay/FREQ));

    std::cout<<"id: "<<id<<" ,Average cycles spent on shared bus delay for
    processing (per message): "<<total_shared_bus_processing_delay<<std::endl;
    if(stat_row==0 && layer_num==0){
        detailed->write_cell(0,5,"shared bus processing delay");
    }
    detailed->write_cell(layer_num*total_rows+1+stat_row,5,std::to_string(total_shared_bus_processing_delay/FREQ));

    std::cout<<"*************************  Mem bus stats
    ************************* "<<id<<std::endl; std::cout<<"id: "<<id<<"
    ,Average cycles spent on mem bus queue delay for transfer (per message):
    "<<total_mem_bus_transfer_queue_delay<<std::endl; if(stat_row==0 &&
    layer_num==0){ detailed->write_cell(0,6,"mem bus queue delay");
    }
    detailed->write_cell(layer_num*total_rows+1+stat_row,6,std::to_string(total_mem_bus_transfer_queue_delay/FREQ));

    std::cout<<"id: "<<id<<" ,Average cycles spent on mem bus delay for transfer
    (per message): "<<total_mem_bus_transfer_delay<<std::endl; if(stat_row==0 &&
    layer_num==0){ detailed->write_cell(0,7,"mem bus delay");
    }
    detailed->write_cell(layer_num*total_rows+1+stat_row,7,std::to_string(total_mem_bus_transfer_delay/FREQ));*/

    std::cout << "*************************  Queuing stats  "
                 "************************* "
              << id << std::endl;
    int count = 2;
    int i = 0;
    for (auto& qd : queuing_delay) {
      std::cout << "id: " << id
                << " ,Average cycles spent on queuing for phase " << i++
                << " of algorithm (per chunk): " << qd << std::endl;
      if (stat_row == 0 && layer_num == 0) {
        detailed->write_cell(
            0, count, "queuing delay phase " + std::to_string(i - 1));
      }
      detailed->write_cell(
          layer_num * total_rows + 1 + stat_row,
          count++,
          std::to_string(qd / FREQ));
    }
    std::cout << "*************************  Network stats  "
                 "************************* "
              << id << std::endl;
    i = 1;
    for (auto& ml : net_message_latency) {
      std::cout << "id: " << id
                << " ,Average cycles spent on network for phase " << i++
                << " of algorithm (per message): " << ml << std::endl;
      if (stat_row == 0 && layer_num == 0) {
        detailed->write_cell(
            0, count, "network delay phase " + std::to_string(i - 1));
      }
      detailed->write_cell(
          layer_num * total_rows + 1 + stat_row,
          count++,
          std::to_string(ml / FREQ));
    }
    if (layer_num == workload->SIZE - 1) {
      std::cout
          << "*************************  Chunk Stats Per Logical Dimension (for all layers) "
             "************************* "
          << id << std::endl;
      i = 1;
      std::vector<double> avg_chunk_latency_per_dimension =
          generator->scheduler_unit->get_average_latency_per_dimension();
      for (auto& cl : avg_chunk_latency_per_dimension) {
        std::cout << " ,Average chunk latency for logical dimension  " << i++
                  << " of topology: " << cl << std::endl;
        if (stat_row == 0) {
          detailed->write_cell(
              0, count, "avg chunk delay dimension " + std::to_string(i - 1));
        }
        detailed->write_cell(1 + stat_row, count++, std::to_string(cl / FREQ));
      }
    }
  }
  return layerData;
}
void Layer::issue_forward_pass_comm(
    SchedulingPolicy pref_scheduling,
    CollectiveBarrier barrier) {
  DataSet* fp = NULL;
  fwd_barrier = barrier;
  collective_counter++;
  if (fwd_pass_comm_type == ComType::All_Reduce) {
    fp = generator->generate_all_reduce(
        fwd_pass_comm_size,
        fwd_pass_comm_involved_dimensions,
        pref_scheduling,
        layer_num);
    if (!fp->active) {
      if (generator->id == 0) {
        std::cout
            << "info: all dims disabled, no forward pass collective for layer: "
            << id << std::endl;
      }
      collective_counter--;
      delete fp;
      if (barrier == CollectiveBarrier::Blocking) {
        workload->call(EventType::General, NULL);
      }
      return;
    }
    if (generator->id == 0) {
      std::cout << "info: all-reduce forward pass collective issued for layer: "
                << id << ",";
      print_involved_dimensions(fwd_pass_comm_involved_dimensions);
    }
  } else if (fwd_pass_comm_type == ComType::All_to_All) {
    fp = generator->generate_all_to_all(
        fwd_pass_comm_size,
        fwd_pass_comm_involved_dimensions,
        pref_scheduling,
        layer_num);
    if (!fp->active) {
      if (generator->id == 0) {
        std::cout
            << "info: all dims disabled, no forward pass collective for layer: "
            << id << std::endl;
      }
      collective_counter--;
      delete fp;
      if (barrier == CollectiveBarrier::Blocking) {
        workload->call(EventType::General, NULL);
      }
      return;
    }
    if (generator->id == 0) {
      std::cout << "info: all-to-all forward pass collective issued for layer: "
                << id << ",";
      print_involved_dimensions(fwd_pass_comm_involved_dimensions);
    }
  } else if (fwd_pass_comm_type == ComType::All_Gatehr) {
    fp = generator->generate_all_gather(
        fwd_pass_comm_size,
        fwd_pass_comm_involved_dimensions,
        pref_scheduling,
        layer_num);
    if (!fp->active) {
      if (generator->id == 0) {
        std::cout
            << "info: all dims disabled, no forward pass collective for layer: "
            << id << std::endl;
      }
      collective_counter--;
      delete fp;
      if (barrier == CollectiveBarrier::Blocking) {
        workload->call(EventType::General, NULL);
      }
      return;
    }
    if (generator->id == 0) {
      std::cout << "info: all-gather forward pass collective issued for layer: "
                << id << ",";
      print_involved_dimensions(fwd_pass_comm_involved_dimensions);
    }
  } else if (fwd_pass_comm_type == ComType::Reduce_Scatter) {
    fp = generator->generate_reduce_scatter(
        fwd_pass_comm_size,
        fwd_pass_comm_involved_dimensions,
        pref_scheduling,
        layer_num);
    if (!fp->active) {
      if (generator->id == 0) {
        std::cout
            << "info: all dims disabled, no forward pass collective for layer: "
            << id << std::endl;
      }
      collective_counter--;
      delete fp;
      if (barrier == CollectiveBarrier::Blocking) {
        workload->call(EventType::General, NULL);
      }
      return;
    }
    if (generator->id == 0) {
      std::cout
          << "info: reduce-scatter forward pass collective issued for layer: "
          << id << ",";
      print_involved_dimensions(fwd_pass_comm_involved_dimensions);
    }
  } else if (fwd_pass_comm_type == ComType::None) {
    collective_counter--;
    if (generator->id == 0) {
      std::cout << "info: no forward pass collective for layer: " << id
                << std::endl;
    }
    if (barrier == CollectiveBarrier::Blocking) {
      workload->call(EventType::General, NULL);
    }
    return;
  } else {
    Sys::sys_panic("no known collective operation! ");
  }
  fwd_pass_datasets[fp->my_id] = fp;
  fp->set_notifier(this, EventType::Fwd_Comm_Finished);
}
void Layer::issue_input_grad_comm(
    SchedulingPolicy pref_scheduling,
    CollectiveBarrier barrier) {
  DataSet* ig = NULL;
  ig_barrier = barrier;
  collective_counter++;
  if (input_grad_comm_type == ComType::All_Reduce) {
    ig = generator->generate_all_reduce(
        input_grad_comm_size,
        input_grad_comm_involved_dimensions,
        pref_scheduling,
        layer_num);
    if (!ig->active) {
      if (generator->id == 0) {
        std::cout
            << "info: all dims disabled, no input grad collective for layer: "
            << id << std::endl;
      }
      collective_counter--;
      delete ig;
      if (barrier == CollectiveBarrier::Blocking) {
        workload->call(EventType::General, NULL);
      }
      return;
    }
    if (generator->id == 0) {
      std::cout << "info: all-reduce input grad collective issued for layer: "
                << id << ",";
      print_involved_dimensions(input_grad_comm_involved_dimensions);
    }
  } else if (input_grad_comm_type == ComType::All_to_All) {
    ig = generator->generate_all_to_all(
        input_grad_comm_size,
        input_grad_comm_involved_dimensions,
        pref_scheduling,
        layer_num);
    if (!ig->active) {
      if (generator->id == 0) {
        std::cout
            << "info: all dims disabled, no input grad collective for layer: "
            << id << std::endl;
      }
      collective_counter--;
      delete ig;
      if (barrier == CollectiveBarrier::Blocking) {
        workload->call(EventType::General, NULL);
      }
      return;
    }
    if (generator->id == 0) {
      std::cout << "info: all-to-all input grad collective issued for layer: "
                << id << ",";
      print_involved_dimensions(input_grad_comm_involved_dimensions);
    }
  } else if (input_grad_comm_type == ComType::All_Gatehr) {
    ig = generator->generate_all_gather(
        input_grad_comm_size,
        input_grad_comm_involved_dimensions,
        pref_scheduling,
        layer_num);
    if (!ig->active) {
      if (generator->id == 0) {
        std::cout
            << "info: all dims disabled, no input grad collective for layer: "
            << id << std::endl;
      }
      collective_counter--;
      delete ig;
      if (barrier == CollectiveBarrier::Blocking) {
        workload->call(EventType::General, NULL);
      }
      return;
    }
    if (generator->id == 0) {
      std::cout << "info: all-gather input grad collective issued for layer: "
                << id << ",";
      print_involved_dimensions(input_grad_comm_involved_dimensions);
    }
  } else if (input_grad_comm_type == ComType::Reduce_Scatter) {
    ig = generator->generate_reduce_scatter(
        input_grad_comm_size,
        input_grad_comm_involved_dimensions,
        pref_scheduling,
        layer_num);
    if (!ig->active) {
      if (generator->id == 0) {
        std::cout
            << "info: all dims disabled, no input grad collective for layer: "
            << id << std::endl;
      }
      collective_counter--;
      delete ig;
      if (barrier == CollectiveBarrier::Blocking) {
        workload->call(EventType::General, NULL);
      }
      return;
    }
    if (generator->id == 0) {
      std::cout
          << "info: reduce-scatter input grad collective issued for layer: "
          << id << ",";
      print_involved_dimensions(input_grad_comm_involved_dimensions);
    }
  } else if (input_grad_comm_type == ComType::None) {
    collective_counter--;
    if (generator->id == 0) {
      std::cout << "info: no input grad collective for layer: " << id
                << std::endl;
    }
    if (barrier == CollectiveBarrier::Blocking) {
      workload->call(EventType::General, NULL);
    }
    return;
  } else {
    std::cout << "no known collective operation! for layer: " << id
              << std::endl;
    Sys::sys_panic("no known collective operation! ");
  }
  input_grad_datasets[ig->my_id] = ig;
  ig->set_notifier(this, EventType::Input_Grad_Comm_Finished);
}
void Layer::issue_weight_grad_comm(
    SchedulingPolicy pref_scheduling,
    CollectiveBarrier barrier) {
  // if(weight_grad_dataset!=NULL)
  // delete weight_grad_dataset;
  DataSet* wg = NULL;
  wg_barrier = barrier;
  collective_counter++;
  if (weight_grad_comm_type == ComType::All_Reduce) {
    wg = generator->generate_all_reduce(
        weight_grad_comm_size,
        weight_grad_comm_involved_dimensions,
        pref_scheduling,
        layer_num);
    if (!wg->active) {
      if (generator->id == 0) {
        std::cout
            << "info: all dims disabled, no weight grad collective for layer: "
            << id << std::endl;
      }
      collective_counter--;
      delete wg;
      if (barrier == CollectiveBarrier::Blocking) {
        workload->call(EventType::General, NULL);
      }
      return;
    }
    if (generator->id == 0) {
      std::cout << "info: all-reduce weight grad collective issued for layer: "
                << id << " with size: " << weight_grad_comm_size << ",";
      print_involved_dimensions(weight_grad_comm_involved_dimensions);
    }
  } else if (weight_grad_comm_type == ComType::All_to_All) {
    wg = generator->generate_all_to_all(
        weight_grad_comm_size,
        weight_grad_comm_involved_dimensions,
        pref_scheduling,
        layer_num);
    if (!wg->active) {
      if (generator->id == 0) {
        std::cout
            << "info: all dims disabled, no weight grad collective for layer: "
            << id << std::endl;
      }
      collective_counter--;
      delete wg;
      if (barrier == CollectiveBarrier::Blocking) {
        workload->call(EventType::General, NULL);
      }
      return;
    }
    if (generator->id == 0) {
      std::cout << "info: all-to-all weight grad collective issued for layer: "
                << id << " with size: " << weight_grad_comm_size << ",";
      print_involved_dimensions(weight_grad_comm_involved_dimensions);
    }
  } else if (weight_grad_comm_type == ComType::All_Gatehr) {
    wg = generator->generate_all_gather(
        weight_grad_comm_size,
        weight_grad_comm_involved_dimensions,
        pref_scheduling,
        layer_num);
    if (!wg->active) {
      if (generator->id == 0) {
        std::cout
            << "info: all dims disabled, no weight grad collective for layer: "
            << id << std::endl;
      }
      collective_counter--;
      delete wg;
      if (barrier == CollectiveBarrier::Blocking) {
        workload->call(EventType::General, NULL);
      }
      return;
    }
    if (generator->id == 0) {
      std::cout << "info: all-gather weight grad collective issued for layer: "
                << id << ",";
      print_involved_dimensions(weight_grad_comm_involved_dimensions);
    }
  } else if (weight_grad_comm_type == ComType::Reduce_Scatter) {
    wg = generator->generate_reduce_scatter(
        weight_grad_comm_size,
        weight_grad_comm_involved_dimensions,
        pref_scheduling,
        layer_num);
    if (!wg->active) {
      if (generator->id == 0) {
        std::cout
            << "info: all dims disabled, no weight grad collective for layer: "
            << id << std::endl;
      }
      collective_counter--;
      delete wg;
      if (barrier == CollectiveBarrier::Blocking) {
        workload->call(EventType::General, NULL);
      }
      return;
    }
    if (generator->id == 0) {
      std::cout
          << "info: reduce-scatter weight grad collective issued for layer: "
          << id << ",";
      print_involved_dimensions(weight_grad_comm_involved_dimensions);
    }
  } else if (weight_grad_comm_type == ComType::None) {
    collective_counter--;
    if (generator->id == 0) {
      std::cout << "info: no weight grad collective for layer: " << id
                << std::endl;
    }
    if (barrier == CollectiveBarrier::Blocking) {
      workload->call(EventType::General, NULL);
    }
    return;
  } else {
    Sys::sys_panic("no known collective operation! ");
  }
  weight_grad_datasets[wg->my_id] = wg;
  wg->set_notifier(this, EventType::Wight_Grad_Comm_Finished);
}
} // namespace AstraSim
