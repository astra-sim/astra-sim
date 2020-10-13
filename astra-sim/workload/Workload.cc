/******************************************************************************
Copyright (c) 2020 Georgia Institute of Technology
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Author : Saeed Rashidi (saeed.rashidi@gatech.edu)
*******************************************************************************/
#include "Workload.hh"
#include "CSVWriter.hh"
#include "Layer.hh"

namespace AstraSim {
Workload::~Workload() {
  if (end_to_end != NULL) {
    delete end_to_end;
  }
  if (detailed != NULL) {
    delete detailed;
  }
  for (int i = 0; i < SIZE; i++) {
    delete layers[i];
  }
  if (layers != NULL) {
    delete[] layers;
  }
}
Workload::Workload(std::string run_name, Sys *generator, std::string name,
                   int TOTAL_PASS, int total_rows, int stat_row,
                   std::string path, bool seprate_log) {
  this->initialized = false;
  this->layers = NULL;
  this->SIZE = 0;
  this->counter = 0;
  this->delay_loaded = false;
  this->checkpoint_initiated = false;
  this->collective_issued = false;
  this->current_state = LoopState::Forward_Pass;
  this->generator = generator;
  this->TOTAL_PASS = TOTAL_PASS;
  this->pass_counter = 0;
  this->index = 0;
  this->waiting_for_comm = 0;
  end_to_end = NULL;
  detailed = NULL;
  this->path = path;
  this->stat_row = stat_row;
  this->seprate_log = seprate_log;
  this->initialized = initialize_workload(name);
  if (this->initialized == false) {
    return;
  }
  this->total_rows = total_rows;
  this->run_name = run_name;
  this->registered_for_finished_streams = false;
  if (generator->id == 0 && seprate_log) {
    std::cout << "stat path: " << path << " ,total rows: " << total_rows
              << " ,stat row: " << stat_row << std::endl;
    detailed = new CSVWriter(path, "detailed.csv");
    end_to_end = new CSVWriter(path, "EndToEnd.csv");
    if (stat_row == 0) {
      initialize_stat_files();
    }
    // detailed->write_cell(0,0,"23");
    // detailed->write_cell(3,4,"46");
  }
}
void Workload::initialize_stat_files() {
  detailed->initialize_csv(SIZE * total_rows + 20, 50);
  end_to_end->initialize_csv(SIZE * total_rows + 20, 50);
}
void Workload::call(EventType event, CallData *data) {
  if (counter > 0) {
    generator->try_register_event(this, EventType::Workload_Wait, NULL,
                                  counter);
    return;
  }
  if (parallelismPolicy == ParallelismPolicy::Data) {
    iterate_data_parallel();
  } else if (parallelismPolicy == ParallelismPolicy::Transformer) {
    iterate_hybrid_parallel_Transformer();
  } else if (parallelismPolicy == ParallelismPolicy::DLRM ||
             parallelismPolicy == ParallelismPolicy::DLRMEnhanced) {
    iterate_hybrid_parallel_DLRM();
  } else if (parallelismPolicy == ParallelismPolicy::MicroBenchmark) {
    iterate_micro_benchmark();
  } else if (parallelismPolicy == ParallelismPolicy::Model) {
    iterate_model_parallel();
  } else if (parallelismPolicy == ParallelismPolicy::HybridDataModel) {
    iterate_hybrid_parallel_data_model();
  } else if (parallelismPolicy == ParallelismPolicy::HybridModelData) {
    iterate_hybrid_parallel_model_data();
  } else if (parallelismPolicy == ParallelismPolicy::DistributedInference) {
    iterate_distributed_inference();
  } else if (parallelismPolicy == ParallelismPolicy::TransformerFwdInBckwd) {
    iterate_hybrid_parallel_Transformer_fwd_in_bckwd();
  } else if (parallelismPolicy == ParallelismPolicy::HybridCustomized) {
    iterate_hybrid_parallel_customized();
  } else {
    Sys::sys_panic("No known parallelism!");
  }
}
void Workload::report() {
  double total_compute = 0;
  double total_exposed = 0;
  AstraSimDataAPI astraSimDataAPI;
  astraSimDataAPI.run_name = run_name;
  astraSimDataAPI.workload_finished_time = ((double)Sys::boostedTick()) / FREQ;
  for (int i = 0; i < SIZE; i++) {
    astraSimDataAPI.layers_stats.push_back(layers[i]->report(
        run_name, i, total_rows, stat_row, detailed, end_to_end, total_compute,
        total_exposed, this->seprate_log));
  }
  astraSimDataAPI.total_compute = total_compute;
  astraSimDataAPI.total_exposed_comm = total_exposed;
  std::cout << "*************************" << std::endl;
  std::cout << "all passes finished at time: " << Sys::boostedTick()
            << ", id of first layer: " << layers[0]->id << std::endl;
  generator->NI->pass_front_end_report(astraSimDataAPI);
  // std::cout << "Total cycles waiting for communication to be finished: " <<
  // waiting_for_comm << std::endl;
}
void Workload::check_for_sim_end() {
  if (pass_counter == TOTAL_PASS) {
    current_state = LoopState::Wait_For_Sim_Finish;
    if (generator->streams_finished != generator->streams_injected &&
        registered_for_finished_streams == false) {
      generator->register_for_finished_stream(this);
      registered_for_finished_streams = true;
      // generator->register_event(this, EventType::General, NULL, 1);
      return;
    }
    if (generator->streams_finished == generator->streams_injected) {
      if (generator->id == 0) {
        report();
      }
      // std::cout<<"workload of node: "<<generator->id<<" has been
      // finished"<<std::endl;
      generator->workload_finished();
      return;
    }
  }
  return;
}
void Workload::iterate_micro_benchmark() {
  assert(index >= 0);
  assert(index < SIZE);
  if (current_state != LoopState::Wait_For_Sim_Finish) {
    for (pass_counter = 0; pass_counter < TOTAL_PASS; pass_counter++) {
      layers[index]->issue_weight_grad_comm(true, true, true,
                                            SchedulingPolicy::None,
                                            CollectiveBarrier::Non_Blocking);
    }
  }
  check_for_sim_end();
}
void Workload::iterate_data_parallel() {
  assert(index >= 0);
  assert(index < SIZE);
  check_for_sim_end();
  if (current_state == LoopState::Forward_Pass) {
    if (!layers[index]->is_weight_grad_comm_finished_blocking()) {
      // layers[index]->increment_waiting_for_wg();
      // waiting_for_comm++;
      // generator->register_event(this, EventType::General, NULL, 1);
      return;
    }
    if (delay_loaded == false) {
      counter = layers[index]->get_fwd_pass_compute();
      if (generator->id == 0) {
        // std::cout<<"layer: "<<index<<" delay in cycles:
        // "<<counter<<std::endl;
      }
      delay_loaded = true;
    }
    if (counter > 0) {
      if (generator->id == 0) {
        // std::cout<<"i have been called in cycles:
        // "<<Sys::boostedTick()<<std::endl;
      }
      generator->try_register_event(this, EventType::Workload_Wait, NULL,
                                    counter);
      return;
    }
    if (generator->id == 0) {
      // std::cout<<"moving to the fwp layer:"<<index<<" ,at time:
      // "<<Sys::boostedTick()<<std::endl;
    }
    index++;
    delay_loaded = false;
    if (index >= SIZE) {
      current_state = LoopState::Weight_Gradient;
      index--;
    }
    generator->register_event(this, EventType::General, NULL, 1);
    return;
  } else if (current_state == LoopState::Weight_Gradient) {
    if (delay_loaded == false) {
      counter = layers[index]->get_weight_grad_compute();
      delay_loaded = true;
    }
    if (counter > 0) {
      generator->try_register_event(this, EventType::Workload_Wait, NULL,
                                    counter);
      return;
    }
    delay_loaded = false;
    layers[index]->issue_weight_grad_comm(true, true, true,
                                          SchedulingPolicy::None,
                                          CollectiveBarrier::Non_Blocking);
    if (index == 0) {
      if (generator->id == 0) {
        std::cout << "pass: " << pass_counter
                  << " finished at time: " << Sys::boostedTick() << std::endl;
      }
      pass_counter++;
      current_state = LoopState::Forward_Pass;
    } else {
      current_state = LoopState::Input_Gradient;
    }
    generator->register_event(this, EventType::General, NULL, 1);
    return;
  } else if (current_state == LoopState::Input_Gradient) {
    if (delay_loaded == false) {
      counter = layers[index]->get_input_grad_compute();
      delay_loaded = true;
    }
    if (counter > 0) {
      generator->try_register_event(this, EventType::Workload_Wait, NULL,
                                    counter);
      return;
    }
    delay_loaded = false;
    index--;
    current_state = LoopState::Weight_Gradient;
    generator->register_event(this, EventType::General, NULL, 1);
    return;
  }
}
void Workload::iterate_hybrid_parallel_customized() {
  assert(index >= 0);
  assert(index < SIZE);
  check_for_sim_end();
  if (current_state == LoopState::Forward_Pass) {
    if (!layers[index]->is_weight_grad_comm_finished_blocking()) {
      return;
    }
    if (delay_loaded == false) {
      counter = layers[index]->get_fwd_pass_compute();
      if (generator->id == 0) {
        // std::cout<<"layer: "<<index<<" delay in cycles:
        // "<<counter<<std::endl;
      }
      delay_loaded = true;
    }
    if (counter > 0) {
      if (generator->id == 0) {
        // std::cout<<"i have been called in cycles:
        // "<<Sys::boostedTick()<<std::endl;
      }
      generator->try_register_event(this, EventType::Workload_Wait, NULL,
                                    counter);
      return;
    }
    if (!collective_issued) {
      collective_issued = true;
      bool first = false;
      bool second = false;
      bool third = false;
      if (layers[index]->specific_parallellism == ParallelismPolicy::Data) {
        first = false;
        second = false;
        third = false;
      } else if (layers[index]->specific_parallellism ==
                 ParallelismPolicy::Model) {
        first = true;
        second = true;
        third = true;
      } else if (layers[index]->specific_parallellism ==
                 ParallelismPolicy::HybridDataModel) {
        first = true;
        second = false;
        third = false;
      } else if (layers[index]->specific_parallellism ==
                 ParallelismPolicy::HybridModelData) {
        first = false;
        second = true;
        third = true;
      }
      layers[index]->issue_forward_pass_comm(first, second, third,
                                             SchedulingPolicy::None,
                                             CollectiveBarrier::Blocking);
      return;
    }
    if (generator->id == 0) {
      // std::cout<<"moving to the fwp layer:"<<index<<" ,at time:
      // "<<Sys::boostedTick()<<std::endl;
    }
    index++;
    delay_loaded = false;
    collective_issued = false;
    if (index >= SIZE) {
      current_state = LoopState::Input_Gradient;
      index--;
    }
    generator->register_event(this, EventType::General, NULL, 1);
    return;
  } else if (current_state == LoopState::Weight_Gradient) {
    if (delay_loaded == false) {
      counter = layers[index]->get_weight_grad_compute();
      delay_loaded = true;
    }
    if (counter > 0) {
      generator->try_register_event(this, EventType::Workload_Wait, NULL,
                                    counter);
      return;
    }
    if (!collective_issued) {
      collective_issued = true;
      bool first = false;
      bool second = false;
      bool third = false;
      if (layers[index]->specific_parallellism == ParallelismPolicy::Data) {
        first = true;
        second = true;
        third = true;
      } else if (layers[index]->specific_parallellism ==
                 ParallelismPolicy::Model) {
        first = false;
        second = false;
        third = false;
      } else if (layers[index]->specific_parallellism ==
                 ParallelismPolicy::HybridDataModel) {
        first = false;
        second = true;
        third = true;
      } else if (layers[index]->specific_parallellism ==
                 ParallelismPolicy::HybridModelData) {
        first = true;
        second = false;
        third = false;
      }
      layers[index]->issue_weight_grad_comm(first, second, third,
                                            SchedulingPolicy::FIFO,
                                            CollectiveBarrier::Non_Blocking);
    }
    if (!layers[index]->is_input_grad_comm_finished_blocking()) {
      // layers[index]->increment_waiting_for_ig();
      // generator->register_event(this, EventType::General, NULL, 1);
      return;
    }
    collective_issued = false;
    delay_loaded = false;
    if (index >= 0) {
      index--;
    }
    if (index == -1) {
      index = 0;
      if (generator->id == 0) {
        std::cout << "pass: " << pass_counter
                  << " finished at time: " << Sys::boostedTick() << std::endl;
      }
      pass_counter++;
      current_state = LoopState::Forward_Pass;
    } else {
      current_state = LoopState::Input_Gradient;
    }
    generator->register_event(this, EventType::General, NULL, 1);
    return;
  } else if (current_state == LoopState::Input_Gradient) {
    if (delay_loaded == false) {
      counter = layers[index]->get_input_grad_compute();
      delay_loaded = true;
    }
    if (counter > 0) {
      generator->try_register_event(this, EventType::Workload_Wait, NULL,
                                    counter);
      return;
    }
    if (!collective_issued && index > 0) {
      collective_issued = true;
      bool first = false;
      bool second = false;
      bool third = false;
      if (layers[index]->specific_parallellism == ParallelismPolicy::Data) {
        first = false;
        second = false;
        third = false;
      } else if (layers[index]->specific_parallellism ==
                 ParallelismPolicy::Model) {
        first = true;
        second = true;
        third = true;
      } else if (layers[index]->specific_parallellism ==
                 ParallelismPolicy::HybridDataModel) {
        first = true;
        second = false;
        third = false;
      } else if (layers[index]->specific_parallellism ==
                 ParallelismPolicy::HybridModelData) {
        first = false;
        second = true;
        third = true;
      }
      layers[index]->issue_input_grad_comm(first, second, third,
                                           SchedulingPolicy::LIFO,
                                           CollectiveBarrier::Non_Blocking);
    }
    collective_issued = false;
    delay_loaded = false;
    current_state = LoopState::Weight_Gradient;
    generator->register_event(this, EventType::General, NULL, 1);
    return;
  }
}
void Workload::iterate_hybrid_parallel_data_model() {
  assert(index >= 0);
  assert(index < SIZE);
  check_for_sim_end();
  if (current_state == LoopState::Forward_Pass) {
    if (!layers[index]->is_weight_grad_comm_finished_blocking()) {
      return;
    }
    if (delay_loaded == false) {
      counter = layers[index]->get_fwd_pass_compute();
      if (generator->id == 0) {
        // std::cout<<"layer: "<<index<<" delay in cycles:
        // "<<counter<<std::endl;
      }
      delay_loaded = true;
    }
    if (counter > 0) {
      if (generator->id == 0) {
        // std::cout<<"i have been called in cycles:
        // "<<Sys::boostedTick()<<std::endl;
      }
      generator->try_register_event(this, EventType::Workload_Wait, NULL,
                                    counter);
      return;
    }
    if (!collective_issued) {
      collective_issued = true;
      layers[index]->issue_forward_pass_comm(true, false, false,
                                             SchedulingPolicy::None,
                                             CollectiveBarrier::Blocking);
      return;
    }
    if (generator->id == 0) {
      // std::cout<<"moving to the fwp layer:"<<index<<" ,at time:
      // "<<Sys::boostedTick()<<std::endl;
    }
    index++;
    delay_loaded = false;
    collective_issued = false;
    if (index >= SIZE) {
      current_state = LoopState::Input_Gradient;
      index--;
    }
    generator->register_event(this, EventType::General, NULL, 1);
    return;
  } else if (current_state == LoopState::Weight_Gradient) {
    if (delay_loaded == false) {
      counter = layers[index]->get_weight_grad_compute();
      delay_loaded = true;
    }
    if (counter > 0) {
      generator->try_register_event(this, EventType::Workload_Wait, NULL,
                                    counter);
      return;
    }
    if (!collective_issued) {
      collective_issued = true;
      layers[index]->issue_weight_grad_comm(false, true, true,
                                            SchedulingPolicy::FIFO,
                                            CollectiveBarrier::Non_Blocking);
    }
    if (!layers[index]->is_input_grad_comm_finished_blocking()) {
      // layers[index]->increment_waiting_for_ig();
      // generator->register_event(this, EventType::General, NULL, 1);
      return;
    }
    collective_issued = false;
    delay_loaded = false;
    if (index >= 0) {
      index--;
    }
    if (index == -1) {
      index = 0;
      if (generator->id == 0) {
        std::cout << "pass: " << pass_counter
                  << " finished at time: " << Sys::boostedTick() << std::endl;
      }
      pass_counter++;
      current_state = LoopState::Forward_Pass;
    } else {
      current_state = LoopState::Input_Gradient;
    }
    generator->register_event(this, EventType::General, NULL, 1);
    return;
  } else if (current_state == LoopState::Input_Gradient) {
    if (delay_loaded == false) {
      counter = layers[index]->get_input_grad_compute();
      delay_loaded = true;
    }
    if (counter > 0) {
      generator->try_register_event(this, EventType::Workload_Wait, NULL,
                                    counter);
      return;
    }
    if (!collective_issued && index > 0) {
      collective_issued = true;
      layers[index]->issue_input_grad_comm(true, false, false,
                                           SchedulingPolicy::LIFO,
                                           CollectiveBarrier::Non_Blocking);
    }
    collective_issued = false;
    delay_loaded = false;
    current_state = LoopState::Weight_Gradient;
    generator->register_event(this, EventType::General, NULL, 1);
    return;
  }
}
void Workload::iterate_hybrid_parallel_model_data() {
  assert(index >= 0);
  assert(index < SIZE);
  check_for_sim_end();
  if (current_state == LoopState::Forward_Pass) {
    if (!layers[index]->is_weight_grad_comm_finished_blocking()) {
      return;
    }
    if (delay_loaded == false) {
      counter = layers[index]->get_fwd_pass_compute();
      if (generator->id == 0) {
        // std::cout<<"layer: "<<index<<" delay in cycles:
        // "<<counter<<std::endl;
      }
      delay_loaded = true;
    }
    if (counter > 0) {
      if (generator->id == 0) {
        // std::cout<<"i have been called in cycles:
        // "<<Sys::boostedTick()<<std::endl;
      }
      generator->try_register_event(this, EventType::Workload_Wait, NULL,
                                    counter);
      return;
    }
    if (!collective_issued) {
      collective_issued = true;
      layers[index]->issue_forward_pass_comm(false, true, true,
                                             SchedulingPolicy::None,
                                             CollectiveBarrier::Blocking);
      return;
    }
    if (generator->id == 0) {
      // std::cout<<"moving to the fwp layer:"<<index<<" ,at time:
      // "<<Sys::boostedTick()<<std::endl;
    }
    index++;
    delay_loaded = false;
    collective_issued = false;
    if (index >= SIZE) {
      current_state = LoopState::Input_Gradient;
      index--;
    }
    generator->register_event(this, EventType::General, NULL, 1);
    return;
  } else if (current_state == LoopState::Weight_Gradient) {
    if (delay_loaded == false) {
      counter = layers[index]->get_weight_grad_compute();
      delay_loaded = true;
    }
    if (counter > 0) {
      generator->try_register_event(this, EventType::Workload_Wait, NULL,
                                    counter);
      return;
    }
    if (!collective_issued) {
      collective_issued = true;
      layers[index]->issue_weight_grad_comm(true, false, false,
                                            SchedulingPolicy::FIFO,
                                            CollectiveBarrier::Non_Blocking);
    }
    if (!layers[index]->is_input_grad_comm_finished_blocking()) {
      // layers[index]->increment_waiting_for_ig();
      // generator->register_event(this, EventType::General, NULL, 1);
      return;
    }
    collective_issued = false;
    delay_loaded = false;
    if (index >= 0) {
      index--;
    }
    if (index == -1) {
      index = 0;
      if (generator->id == 0) {
        std::cout << "pass: " << pass_counter
                  << " finished at time: " << Sys::boostedTick() << std::endl;
      }
      pass_counter++;
      current_state = LoopState::Forward_Pass;
    } else {
      current_state = LoopState::Input_Gradient;
    }
    generator->register_event(this, EventType::General, NULL, 1);
    return;
  } else if (current_state == LoopState::Input_Gradient) {
    if (delay_loaded == false) {
      counter = layers[index]->get_input_grad_compute();
      delay_loaded = true;
    }
    if (counter > 0) {
      generator->try_register_event(this, EventType::Workload_Wait, NULL,
                                    counter);
      return;
    }
    if (!collective_issued && index > 0) {
      collective_issued = true;
      layers[index]->issue_input_grad_comm(false, true, true,
                                           SchedulingPolicy::LIFO,
                                           CollectiveBarrier::Non_Blocking);
    }
    collective_issued = false;
    delay_loaded = false;
    current_state = LoopState::Weight_Gradient;
    generator->register_event(this, EventType::General, NULL, 1);
    return;
  }
}
void Workload::iterate_distributed_inference() {
  assert(index >= 0);
  assert(index < SIZE);
  check_for_sim_end();
  if (current_state == LoopState::Forward_Pass) {
    if (!layers[index]->is_weight_grad_comm_finished_blocking()) {
      return;
    }
    if (delay_loaded == false) {
      counter = layers[index]->get_fwd_pass_compute();
      if (generator->id == 0) {
        // std::cout<<"layer: "<<index<<" delay in cycles:
        // "<<counter<<std::endl;
      }
      delay_loaded = true;
    }
    if (counter > 0) {
      if (generator->id == 0) {
        // std::cout<<"i have been called in cycles:
        // "<<Sys::boostedTick()<<std::endl;
      }
      generator->try_register_event(this, EventType::Workload_Wait, NULL,
                                    counter);
      return;
    }
    if (!collective_issued) {
      collective_issued = true;
      layers[index]->issue_forward_pass_comm(true, true, true,
                                             SchedulingPolicy::None,
                                             CollectiveBarrier::Blocking);
      return;
    }
    if (generator->id == 0) {
      // std::cout<<"moving to the fwp layer:"<<index<<" ,at time:
      // "<<Sys::boostedTick()<<std::endl;
    }
    index++;
    delay_loaded = false;
    collective_issued = false;
    if (index >= SIZE) {
      index = 0;
      pass_counter++;
    }
    generator->register_event(this, EventType::General, NULL, 1);
    return;
  }
}
void Workload::iterate_model_parallel() {
  assert(index >= 0);
  assert(index < SIZE);
  check_for_sim_end();
  if (current_state == LoopState::Forward_Pass) {
    if (!layers[index]->is_weight_grad_comm_finished_blocking()) {
      return;
    }
    if (delay_loaded == false) {
      counter = layers[index]->get_fwd_pass_compute();
      if (generator->id == 0) {
        // std::cout<<"layer: "<<index<<" delay in cycles:
        // "<<counter<<std::endl;
      }
      delay_loaded = true;
    }
    if (counter > 0) {
      if (generator->id == 0) {
        // std::cout<<"i have been called in cycles:
        // "<<Sys::boostedTick()<<std::endl;
      }
      generator->try_register_event(this, EventType::Workload_Wait, NULL,
                                    counter);
      return;
    }
    if (!collective_issued) {
      collective_issued = true;
      layers[index]->issue_forward_pass_comm(true, true, true,
                                             SchedulingPolicy::None,
                                             CollectiveBarrier::Blocking);
      return;
    }
    if (generator->id == 0) {
      // std::cout<<"moving to the fwp layer:"<<index<<" ,at time:
      // "<<Sys::boostedTick()<<std::endl;
    }
    index++;
    delay_loaded = false;
    collective_issued = false;
    if (index >= SIZE) {
      current_state = LoopState::Input_Gradient;
      index--;
    }
    generator->register_event(this, EventType::General, NULL, 1);
    return;
  } else if (current_state == LoopState::Weight_Gradient) {
    if (delay_loaded == false) {
      counter = layers[index]->get_weight_grad_compute();
      delay_loaded = true;
    }
    if (counter > 0) {
      generator->try_register_event(this, EventType::Workload_Wait, NULL,
                                    counter);
      return;
    }
    if (!layers[index]->is_input_grad_comm_finished_blocking()) {
      // layers[index]->increment_waiting_for_ig();
      // generator->register_event(this, EventType::General, NULL, 1);
      return;
    }
    collective_issued = false;
    delay_loaded = false;
    if (index >= 0) {
      index--;
    }
    if (index == -1) {
      index = 0;
      if (generator->id == 0) {
        std::cout << "pass: " << pass_counter
                  << " finished at time: " << Sys::boostedTick() << std::endl;
      }
      pass_counter++;
      current_state = LoopState::Forward_Pass;
    } else {
      current_state = LoopState::Input_Gradient;
    }
    generator->register_event(this, EventType::General, NULL, 1);
    return;
  } else if (current_state == LoopState::Input_Gradient) {
    if (delay_loaded == false) {
      counter = layers[index]->get_input_grad_compute();
      delay_loaded = true;
    }
    if (counter > 0) {
      generator->try_register_event(this, EventType::Workload_Wait, NULL,
                                    counter);
      return;
    }
    if (!collective_issued && index > 0) {
      collective_issued = true;
      layers[index]->issue_input_grad_comm(true, true, true,
                                           SchedulingPolicy::LIFO,
                                           CollectiveBarrier::Non_Blocking);
    }
    collective_issued = false;
    delay_loaded = false;
    current_state = LoopState::Weight_Gradient;
    generator->register_event(this, EventType::General, NULL, 1);
    return;
  }
}
void Workload::iterate_hybrid_parallel_Transformer() {
  assert(index >= 0);
  assert(index < SIZE);
  check_for_sim_end();
  if (current_state == LoopState::Forward_Pass) {
    if (!layers[index]->is_weight_grad_comm_finished_blocking()) {
      return;
    }
    if (delay_loaded == false) {
      counter = layers[index]->get_fwd_pass_compute();
      if (generator->id == 0) {
        // std::cout<<"layer: "<<index<<" delay in cycles:
        // "<<counter<<std::endl;
      }
      delay_loaded = true;
    }
    if (counter > 0) {
      if (generator->id == 0) {
        // std::cout<<"i have been called in cycles:
        // "<<Sys::boostedTick()<<std::endl;
      }
      generator->try_register_event(this, EventType::Workload_Wait, NULL,
                                    counter);
      return;
    }
    if (!collective_issued) {
      collective_issued = true;
      layers[index]->issue_forward_pass_comm(true, false, true,
                                             SchedulingPolicy::None,
                                             CollectiveBarrier::Blocking);
      return;
    }
    if (generator->id == 0) {
      // std::cout<<"moving to the fwp layer:"<<index<<" ,at time:
      // "<<Sys::boostedTick()<<std::endl;
    }
    index++;
    delay_loaded = false;
    collective_issued = false;
    if (index >= SIZE) {
      current_state = LoopState::Input_Gradient;
      index--;
    }
    generator->register_event(this, EventType::General, NULL, 1);
    return;
  } else if (current_state == LoopState::Weight_Gradient) {
    if (delay_loaded == false) {
      counter = layers[index]->get_weight_grad_compute();
      delay_loaded = true;
    }
    if (counter > 0) {
      generator->try_register_event(this, EventType::Workload_Wait, NULL,
                                    counter);
      return;
    }
    if (!collective_issued) {
      collective_issued = true;
      layers[index]->issue_weight_grad_comm(false, true, false,
                                            SchedulingPolicy::FIFO,
                                            CollectiveBarrier::Non_Blocking);
    }
    if (!layers[index]->is_input_grad_comm_finished_blocking()) {
      // layers[index]->increment_waiting_for_ig();
      // generator->register_event(this, EventType::General, NULL, 1);
      return;
    }
    collective_issued = false;
    delay_loaded = false;
    if (index >= 0) {
      index--;
    }
    if (index == -1) {
      index = 0;
      if (generator->id == 0) {
        std::cout << "pass: " << pass_counter
                  << " finished at time: " << Sys::boostedTick() << std::endl;
      }
      pass_counter++;
      current_state = LoopState::Forward_Pass;
    } else {
      current_state = LoopState::Input_Gradient;
    }
    generator->register_event(this, EventType::General, NULL, 1);
    return;
  } else if (current_state == LoopState::Input_Gradient) {
    if (delay_loaded == false) {
      counter = layers[index]->get_input_grad_compute();
      delay_loaded = true;
    }
    if (counter > 0) {
      generator->try_register_event(this, EventType::Workload_Wait, NULL,
                                    counter);
      return;
    }
    if (!collective_issued && index > 0) {
      collective_issued = true;
      layers[index]->issue_input_grad_comm(true, false, true,
                                           SchedulingPolicy::LIFO,
                                           CollectiveBarrier::Non_Blocking);
    }
    collective_issued = false;
    delay_loaded = false;
    current_state = LoopState::Weight_Gradient;
    generator->register_event(this, EventType::General, NULL, 1);
    return;
  }
}
void Workload::iterate_hybrid_parallel_Transformer_fwd_in_bckwd() {
  assert(index >= 0);
  assert(index < SIZE);
  check_for_sim_end();
  if (current_state == LoopState::Forward_Pass) {
    if (!layers[index]->is_weight_grad_comm_finished_blocking()) {
      return;
    }
    if (delay_loaded == false) {
      counter = layers[index]->get_fwd_pass_compute();
      if (generator->id == 0) {
        // std::cout<<"layer: "<<index<<" delay in cycles:
        // "<<counter<<std::endl;
      }
      delay_loaded = true;
    }
    if (counter > 0) {
      if (generator->id == 0) {
        // std::cout<<"i have been called in cycles:
        // "<<Sys::boostedTick()<<std::endl;
      }
      generator->try_register_event(this, EventType::Workload_Wait, NULL,
                                    counter);
      return;
    }
    if (!collective_issued) {
      collective_issued = true;
      layers[index]->issue_forward_pass_comm(true, false, true,
                                             SchedulingPolicy::None,
                                             CollectiveBarrier::Blocking);
      return;
    }
    if (generator->id == 0) {
      // std::cout<<"moving to the fwp layer:"<<index<<" ,at time:
      // "<<Sys::boostedTick()<<std::endl;
    }
    index++;
    delay_loaded = false;
    collective_issued = false;
    if (index >= SIZE) {
      current_state = LoopState::Input_Gradient;
      index--;
    }
    generator->register_event(this, EventType::General, NULL, 1);
    return;
  } else if (current_state == LoopState::Weight_Gradient) {
    if (delay_loaded == false) {
      counter = layers[index]->get_weight_grad_compute();
      delay_loaded = true;
    }
    if (counter > 0) {
      generator->try_register_event(this, EventType::Workload_Wait, NULL,
                                    counter);
      return;
    }
    if (!collective_issued) {
      collective_issued = true;
      layers[index]->issue_weight_grad_comm(false, true, false,
                                            SchedulingPolicy::FIFO,
                                            CollectiveBarrier::Non_Blocking);
    }
    if (!layers[index]->is_input_grad_comm_finished_blocking()) {
      // layers[index]->increment_waiting_for_ig();
      // generator->register_event(this, EventType::General, NULL, 1);
      return;
    }
    collective_issued = false;
    delay_loaded = false;
    if (index >= 0) {
      index--;
    }
    if (index == -1) {
      index = 0;
      if (generator->id == 0) {
        std::cout << "pass: " << pass_counter
                  << " finished at time: " << Sys::boostedTick() << std::endl;
      }
      pass_counter++;
      current_state = LoopState::Forward_Pass;
    } else {
      current_state = LoopState::Input_Gradient;
    }
    generator->register_event(this, EventType::General, NULL, 1);
    return;
  } else if (current_state == LoopState::Input_Gradient) {
    if (layers[index]->needs_fwd_in_bckwd_initiation && !checkpoint_initiated) {
      int tmp = index;
      while (!layers[index--]->is_checkpoint)
        ;
      index++;
      current_state = LoopState::Forward_In_BackPass;
      checkpoint_initiated = true;
      generator->register_event(this, EventType::General, NULL, 1);
      if (generator->id == 0) {
        std::cout << "***** info, initiating fwd_in_bkwd starting from layer:"
                  << index << " to layer: " << tmp
                  << " ,at time: " << Sys::boostedTick() << std::endl;
      }
      return;
    }
    if (delay_loaded == false) {
      counter = layers[index]->get_input_grad_compute();
      delay_loaded = true;
    }
    if (counter > 0) {
      generator->try_register_event(this, EventType::Workload_Wait, NULL,
                                    counter);
      return;
    }
    if (!collective_issued && index > 0) {
      collective_issued = true;
      layers[index]->issue_input_grad_comm(true, false, true,
                                           SchedulingPolicy::LIFO,
                                           CollectiveBarrier::Non_Blocking);
    }
    checkpoint_initiated = false;
    collective_issued = false;
    delay_loaded = false;
    current_state = LoopState::Weight_Gradient;
    generator->register_event(this, EventType::General, NULL, 1);
    return;
  } else if (current_state == LoopState::Forward_In_BackPass) {
    if (!layers[index]->is_weight_grad_comm_finished_blocking()) {
      return;
    }
    if (delay_loaded == false) {
      counter = layers[index]->get_fwd_pass_compute();
      if (generator->id == 0) {
        // std::cout<<"layer: "<<index<<" delay in cycles:
        // "<<counter<<std::endl;
      }
      delay_loaded = true;
    }
    if (counter > 0) {
      if (generator->id == 0) {
        // std::cout<<"i have been called in cycles:
        // "<<Sys::boostedTick()<<std::endl;
      }
      generator->try_register_event(this, EventType::Workload_Wait, NULL,
                                    counter);
      return;
    }
    if (!collective_issued) {
      collective_issued = true;
      layers[index]->issue_forward_pass_comm(true, false, true,
                                             SchedulingPolicy::None,
                                             CollectiveBarrier::Blocking);
      return;
    }
    if (generator->id == 0) {
      // std::cout<<"moving to the fwp layer:"<<index<<" ,at time:
      // "<<Sys::boostedTick()<<std::endl;
    }
    index++;
    delay_loaded = false;
    collective_issued = false;
    if (layers[index]->needs_fwd_in_bckwd_initiation) {
      current_state = LoopState::Input_Gradient;
    }
    generator->register_event(this, EventType::General, NULL, 1);
    return;
  }
}
void Workload::iterate_hybrid_parallel_DLRM() {
  assert(index >= 0);
  assert(index < SIZE);
  check_for_sim_end();
  if (current_state == LoopState::Forward_Pass) {
    if (!layers[index]->is_weight_grad_comm_finished_blocking()) {
      // layers[index]->increment_waiting_for_wg();
      // waiting_for_comm++;
      if (pass_counter == 1 && generator->id == 0 &&
          generator->streams_finished == 106) {
        // std::cout<<"still waiting for copleteness of layer:
        // "<<layers[index]->id<<std::endl;
      }
      // generator->register_event(this, EventType::General, NULL, 1);
      return;
    }
    if (delay_loaded == false) {
      counter = layers[index]->get_fwd_pass_compute();
      if (generator->id == 0) {
        // std::cout<<"layer: "<<index<<" delay in cycles:
        // "<<counter<<std::endl;
      }
      delay_loaded = true;
    }
    if (counter > 0) {
      if (generator->id == 0) {
        // std::cout<<"i have been called in cycles:
        // "<<Sys::boostedTick()<<std::endl;
      }
      generator->try_register_event(this, EventType::Workload_Wait, NULL,
                                    counter);
      return;
    }
    if (!collective_issued &&
        layers[index]->fwd_pass_comm_type == ComType::All_to_All) {
      collective_issued = true;
      layers[index]->issue_forward_pass_comm(true, true, true,
                                             SchedulingPolicy::HIGHEST,
                                             CollectiveBarrier::Non_Blocking);

    } else if (index == DLRM_LAST_BOTTOM_LAYER) {
      if (!layers[0]->is_fwd_pass_comm_finished_blocking()) {
        // layers[0]->increment_waiting_for_fwd();
        // generator->register_event(this, EventType::General, NULL, 1);
        return;
      }
    }
    if (generator->id == 0) {
      // std::cout<<"moving to the fwp layer:"<<index<<" ,at time:
      // "<<Sys::boostedTick()<<std::endl;
    }
    index++;
    delay_loaded = false;
    collective_issued = false;
    if (index >= SIZE) {
      current_state = LoopState::Weight_Gradient;
      index--;
    }
    if (generator->id == 0) {
      std::cout << "*************************layer changed to: " << index
                << std::endl;
    }
    generator->register_event(this, EventType::General, NULL, 1);
    return;
  } else if (current_state == LoopState::Weight_Gradient) {
    if (delay_loaded == false) {
      counter = layers[index]->get_weight_grad_compute();
      delay_loaded = true;
    }
    if (counter > 0) {
      generator->try_register_event(this, EventType::Workload_Wait, NULL,
                                    counter);
      return;
    }
    if (!collective_issued) {
      collective_issued = true;
      layers[index]->issue_weight_grad_comm(true, true, true,
                                            SchedulingPolicy::None,
                                            CollectiveBarrier::Non_Blocking);
    }
    if (parallelismPolicy == ParallelismPolicy::DLRM &&
        !layers[index]->is_input_grad_comm_finished_blocking()) {
      // layers[index]->increment_waiting_for_ig();
      // generator->register_event(this, EventType::General, NULL, 1);
      return;
    }
    if (index == 0) {
      if (generator->id == 0) {
        std::cout << "pass: " << pass_counter
                  << " finished at time: " << Sys::boostedTick() << std::endl;
      }
      pass_counter++;
      current_state = LoopState::Forward_Pass;
    } else {
      current_state = LoopState::Input_Gradient;
    }
    delay_loaded = false;
    collective_issued = false;
    generator->register_event(this, EventType::General, NULL, 1);
  } else if (current_state == LoopState::Input_Gradient) {
    if (delay_loaded == false) {
      counter = layers[index]->get_input_grad_compute();
      delay_loaded = true;
    }
    if (counter > 0) {
      generator->try_register_event(this, EventType::Workload_Wait, NULL,
                                    counter);
      return;
    }
    if (index == DLRM_LAST_BOTTOM_LAYER + 1) {
      layers[0]->issue_input_grad_comm(true, true, true,
                                       SchedulingPolicy::HIGHEST,
                                       CollectiveBarrier::Non_Blocking);
    }
    index--;
    if (generator->id == 0) {
      std::cout << "*************************layer changed to: " << index
                << " in ig" << std::endl;
    }
    current_state = LoopState::Weight_Gradient;
    collective_issued = false;
    delay_loaded = false;
    generator->register_event(this, EventType::General, NULL, 1);
  }
}
int Workload::get_layer_numbers(std::string workload_input) {
  std::ifstream inFile;
  inFile.open("workload_inputs/" + workload_input + ".txt");
  if (!inFile) {
    std::cout << "Unable to open file: " << workload_input << std::endl;
  } else {
    std::cout << "success in openning file" << std::endl;
  }
  std::string dummyLine;
  std::getline(inFile, dummyLine);
  int layers;
  inFile >> layers;
  inFile.close();
  return layers;
}
ParallelismPolicy Workload::decode_parallelsim(std::string parallelism) {
  if (parallelism == "DATA")
    return ParallelismPolicy::Data;
  else if (parallelism == "HYBRID_TRANSFORMER")
    return ParallelismPolicy::Transformer;
  else if (parallelism == "HYBRID_TRANSFORMER_FWD_IN_BCKWD")
    return ParallelismPolicy::TransformerFwdInBckwd;
  else if (parallelism == "HYBRID_DLRM")
    return ParallelismPolicy::DLRM;
  else if (parallelism == "HYBRID_DLRM_ENHANCED")
    return ParallelismPolicy ::DLRMEnhanced;
  else if (parallelism == "MODEL")
    return ParallelismPolicy::Model;
  else if (parallelism == "HYBRID_DATA_MODEL")
    return ParallelismPolicy::HybridDataModel;
  else if (parallelism == "HYBRID_MODEL_DATA")
    return ParallelismPolicy::HybridModelData;
  else if (parallelism == "HYBRID_CUSTOMIZED")
    return ParallelismPolicy::HybridCustomized;
  else if (parallelism == "MICRO")
    return ParallelismPolicy::MicroBenchmark;
  else if (parallelism == "DISTRIBUTED_INFERENCE")
    return ParallelismPolicy::DistributedInference;
  else
    return ParallelismPolicy::None;
}
bool Workload::initialize_workload(std::string name) {
  std::map<int, bool> chekpoints;
  std::map<int, bool> need_checkpoint_initiation;
  std::ifstream inFile;
  inFile.open(name);
  if (!inFile) {
    std::cout << "Unable to open file: " << name << std::endl;
    std::cout << "######### Exiting because unable to open the workload input "
                 "file #########"
              << std::endl;
    return false;
  } else {
    std::cout << "success in openning file" << std::endl;
  }
  std::string type;
  int lines;
  inFile >> type;
  parallelismPolicy = decode_parallelsim(type);
  if (parallelismPolicy == ParallelismPolicy::TransformerFwdInBckwd) {
    std::string tmp;
    int i;
    inFile >> tmp;
    inFile >> i;
    std::cout << "checkpoints layers are: ";
    while (i-- > 0) {
      int layer;
      inFile >> layer;
      chekpoints[layer] = true;
      std::cout << layer << ", ";
    }
    std::cout << std::endl;
    std::cout << "layers initiating fwd_in_bckwd are: ";
    inFile >> tmp;
    inFile >> i;
    while (i-- > 0) {
      int layer;
      inFile >> layer;
      need_checkpoint_initiation[layer] = true;
      std::cout << layer << ", ";
    }
    std::cout << std::endl;
  } else if (parallelismPolicy == ParallelismPolicy::DLRM ||
             parallelismPolicy == ParallelismPolicy::DLRMEnhanced) {
    inFile >> DLRM_LAST_BOTTOM_LAYER;
    if (generator->id == 0) {
      std::cout
          << "****************** info: DLRM workload last bottom layer is: "
          << DLRM_LAST_BOTTOM_LAYER << std::endl;
    }
  } else if (parallelismPolicy == ParallelismPolicy::None) {
    std::cout << "######### Exiting because unable to decode the workload "
                 "parallelization strategy #########"
              << std::endl;
    inFile.close();
    return false;
  }
  inFile >> lines;
  run_type = type;
  SIZE = lines;
  layers = new Layer *[SIZE];
  for (int i = 0; i < lines; i++) {
    std::string id;
    inFile >> id;
    int depen;
    inFile >> depen;

    int fp_compute_time;
    inFile >> fp_compute_time;
    std::string fp_comm_type_s;
    inFile >> fp_comm_type_s;
    int fp_comm_size;
    inFile >> fp_comm_size;

    int ig_compute_time;
    inFile >> ig_compute_time;
    std::string ig_comm_type_s;
    inFile >> ig_comm_type_s;
    int ig_comm_size;
    inFile >> ig_comm_size;

    int wg_compute_time;
    inFile >> wg_compute_time;
    std::string wg_comm_type_s;
    inFile >> wg_comm_type_s;
    int wg_comm_size;
    inFile >> wg_comm_size;
    // wg_comm_size=2048;
    int wg_update_time;
    inFile >> wg_update_time;

    ComType fp_type = ComType::None;
    ComType ig_type = ComType::None;
    ComType wg_type = ComType::None;

    if (wg_comm_type_s == "ALLREDUCE") {
      wg_type = ComType::All_Reduce;
    } else if (wg_comm_type_s == "ALLTOALL") {
      wg_type = ComType::All_to_All;
    } else if (wg_comm_type_s == "ALLREDUCEALLTOALL") {
      wg_type = ComType::All_Reduce_All_to_All;
    } else if (wg_comm_type_s == "ALLGATHER") {
      wg_type = ComType::All_Gatehr;
    } else if (wg_comm_type_s == "REDUCESCATTER") {
      wg_type = ComType::Reduce_Scatter;
    }

    if (ig_comm_type_s == "ALLREDUCE") {
      ig_type = ComType::All_Reduce;
    } else if (ig_comm_type_s == "ALLTOALL") {
      ig_type = ComType::All_to_All;
    } else if (ig_comm_type_s == "ALLREDUCEALLTOALL") {
      ig_type = ComType::All_Reduce_All_to_All;
    } else if (ig_comm_type_s == "ALLGATHER") {
      ig_type = ComType::All_Gatehr;
    } else if (ig_comm_type_s == "REDUCESCATTER") {
      ig_type = ComType::Reduce_Scatter;
    }

    if (fp_comm_type_s == "ALLREDUCE") {
      fp_type = ComType::All_Reduce;
    } else if (fp_comm_type_s == "ALLTOALL") {
      fp_type = ComType::All_to_All;
    } else if (fp_comm_type_s == "ALLREDUCEALLTOALL") {
      fp_type = ComType::All_Reduce_All_to_All;
    } else if (fp_comm_type_s == "ALLGATHER") {
      fp_type = ComType::All_Gatehr;
    } else if (fp_comm_type_s == "REDUCESCATTER") {
      fp_type = ComType::Reduce_Scatter;
    }

    if (generator->id == 0) {
      std::cout << "id: " << id << " , depen: " << depen
                << " , wg_comp_time: " << wg_compute_time << std::endl;
    }
    Layer *l = new Layer(id, i, generator, this,
                         fp_compute_time * generator->compute_scale, fp_type,
                         fp_comm_size * generator->comm_scale,
                         ig_compute_time * generator->compute_scale, ig_type,
                         ig_comm_size * generator->comm_scale,
                         wg_compute_time * generator->compute_scale, wg_type,
                         wg_comm_size * generator->comm_scale, wg_update_time);
    if (chekpoints.find(i) != chekpoints.end()) {
      l->is_checkpoint = true;
    }
    if (need_checkpoint_initiation.find(i) !=
        need_checkpoint_initiation.end()) {
      l->needs_fwd_in_bckwd_initiation = true;
    }
    if (parallelismPolicy == ParallelismPolicy::HybridCustomized) {
      std::string specific_parallelsim;
      inFile >> specific_parallelsim;
      l->specific_parallellism = decode_parallelsim(specific_parallelsim);
    }
    layers[i] = l;
  }
  if (generator->id == 0) {
    std::cout << "type: " << type << " ,num passes: " << TOTAL_PASS
              << " ,lines: " << lines
              << " compute scale: " << generator->compute_scale
              << " ,comm scale: " << generator->comm_scale << std::endl;
  }
  inFile.close();
  return true;
}
void Workload::fire() { call(EventType::General, NULL); }
} // namespace AstraSim
