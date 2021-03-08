/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "OfflineGreedy.hh"
namespace AstraSim {

std::map<long long,std::vector<int>> OfflineGreedy::chunk_schedule;
std::map<long long,int> OfflineGreedy::schedule_consumer;

DimElapsedTime::DimElapsedTime(int dim_num) {
  this->dim_num=dim_num;
  this->elapsed_time=0;
}
OfflineGreedy::OfflineGreedy(Sys *sys) {
   this->sys=sys;
   this->dim_size=sys->physical_dims;
   this->dim_BW.resize(this->dim_size.size());
   for(int i=0;i<this->dim_size.size();i++){
     this->dim_BW[i]=sys->NI->get_BW_at_dimension(i);
     this->dim_elapsed_time.push_back(DimElapsedTime(i));
   }
}
std::vector<int> OfflineGreedy::get_chunk_scheduling(long long chunk_id, uint64_t chunk_size,std::vector<bool> &dimensions_involved) {
  if (chunk_schedule.find(chunk_id)!=chunk_schedule.end()) {
    schedule_consumer[chunk_id]++;
    if(schedule_consumer[chunk_id]==sys->all_generators.size()){
      std::vector<int> res=chunk_schedule[chunk_id];
      chunk_schedule.erase(chunk_id);
      schedule_consumer.erase(chunk_id);
      return res;
    }
    return chunk_schedule[chunk_id];
  }
  if(sys->id!=0) {
    return sys->all_generators[0]->offline_greedy->get_chunk_scheduling(chunk_id,chunk_size,dimensions_involved);
  }
  else{
    std::sort(dim_elapsed_time.begin(),dim_elapsed_time.end());
    std::vector<int> result;
    for(auto &dim:dim_elapsed_time){
      if(!dimensions_involved[dim.dim_num] || dim_size[dim.dim_num]==1){
        result.push_back(dim.dim_num);
        continue;
      }
      result.push_back(dim.dim_num);
      dim.elapsed_time+=((((double)chunk_size)/1048576)*
          (((double)(dim_size[dim.dim_num]-1))/(dim_size[dim.dim_num]-1)))/
          (dim_BW[dim.dim_num]/dim_BW[0]);
      chunk_size/=dim_size[dim.dim_num];
    }
    chunk_schedule[chunk_id]=result;
    schedule_consumer[chunk_id]=1;
    /*std::cout<<"scheduling for chunk: "<<chunk_id<<" is: ";
    for(auto a:result){
      std::cout<<a<<" ,";
    }
    std::cout<<std::endl;*/
    return result;
  }
}
} // namespace AstraSim