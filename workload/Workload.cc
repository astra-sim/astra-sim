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
CSVWriter::CSVWriter(std::string path,std::string name) {
    this->path=path;
    this->name=name;
}
void CSVWriter::initialize_csv(int rows, int cols) {
    //if(exists_test(path+name)){
        //return;
    //}

    std::cout<<"path to create csvs is: "<<path<<std::endl;
    do{
       std::cout << "trying to open: " << path << std::endl;
       myFile.open(path+name, std::fstream::out);
    }while (!myFile.is_open());
    do{
        myFile.close();
    }while (myFile.is_open());

    do{
        std::cout << "trying to open: " << path << std::endl;
        myFile.open(path+name, std::fstream::out | std::fstream::in);
    }while (!myFile.is_open());

    if (!myFile) {
        std::cout << "Unable to open file: " << path << std::endl;
    } else {
        std::cout << "success in openning file" << std::endl;
    }

    myFile.seekp(0,std::ios_base::beg);
    myFile.seekg(0,std::ios_base::beg);
    /*if(!File.eof()){
        std::cout<<"eof reached"<<std::endl;
        return;
    }*/
    for(int i=0;i<rows;i++){
        for(int j=0;j<cols-1;j++){
            myFile<<',';
        }
        myFile<<'\n';
    }
    do{
        myFile.close();
    }while (myFile.is_open());

}
void CSVWriter::write_cell(int row, int column,std::string data) {
    std::string str="";
    std::string tmp;

    int status=1;
    int fildes=-1;
    do {
        fildes = open((path+name).c_str(), O_RDWR);
    }while(fildes==-1);
    
    do{
        status = lockf(fildes, F_TLOCK, (off_t)1000000);
    }while(status!=0);
    char buf[1];
    while(row>0){
        status=read(fildes,buf,1);
        str=str+(*buf);
        if(*buf=='\n'){
            row--;
        }
    }
    while(column>0){
        status=read(fildes,buf,1);
        str=str+(*buf);
        if(*buf==','){
            column--;
        }
        if(*buf=='\n'){
            std::cout<<"fatal error in inserting cewll!"<<std::endl;
        }
    }
    str=str+data;
    while(read(fildes,buf,1)){
        str=str+(*buf);
    }

    do{
        lseek(fildes,0,SEEK_SET);
       status=write(fildes,str.c_str(),str.length());
    }while (status!=str.length());

    do {
        status=lseek(fildes, 0, SEEK_SET);
    }while(status==-1);

    do{
        status = lockf(fildes, F_ULOCK, (off_t)1000000);
    }while(status!=0);

    do {
        status = close(fildes);
    }while(status==-1);
    return;
}
/*void CSVWriter::write_cell(int row, int column,std::string data) {
    std::string str="";
    std::string tmp;

    int status=1;
    int fildes=-1;
    do {
        fildes = open((path+name).c_str(), O_RDWR);
    }while(fildes==-1);

    do{
        status = lockf(fildes, F_TLOCK, (off_t)1000000);
    }while(status!=0);

    do{
        //std::cout << "trying to open: " << path << std::endl;
        myFile.open(path+name, std::fstream::out | std::fstream::in);
    }while (!myFile.is_open());
    myFile.seekg(0,std::ios_base::beg);
    char ctmp='h';
    for(int i=0;i<row;i++){
        std::getline(myFile,tmp);
        str=str+tmp+'\n';
    }
    while(column>0){
        ctmp=myFile.get();
        if(ctmp==','){
            column--;
        }
        if(ctmp=='\n'){
            std::cout<<"fatal error in inserting cewll!"<<std::endl;
        }
        str=str+ctmp;
    }
    str=str+data;
    while((ctmp=myFile.get())!=EOF){
        str=str+ctmp;
        //std::cout<<ctmp<<std::endl;
    }
    //std::cout<<str<<std::endl;
    do{
        myFile.close();
    }while (myFile.is_open());

    do{
        //std::cout << "trying to open: " << path << std::endl;
        myFile.open(path+name, std::fstream::out);
    }while (!myFile.is_open());
    myFile.seekp(0,std::ios_base::beg);
    myFile.write(str.c_str(), str.size());
    do{
        myFile.close();
    }while (myFile.is_open());


    do{
        status = lockf(fildes, F_ULOCK, (off_t)1000000);
    }while(status!=0);

    do {
        status = close(fildes);
    }while(status==-1);

    return;
}*/
int DataSet::id_auto_increment=0;
Layer::Layer(std::string id,int layer_num,Sys *generator,Workload *workload,int fwd_pass_compute_time,ComType fwd_pass_comm_type,
    int fwd_pass_comm_size,int input_grad_compute_time,ComType input_grad_comm_type,int input_grad_comm_size,
    int weight_grad_compute_time,ComType weight_grad_comm_type,int weight_grad_comm_size,int weight_grad_update_time){
    this->id=id;
    this->layer_num=layer_num;
    this->generator=generator;
    this->workload=workload;
    this->fwd_pass_compute_time=fwd_pass_compute_time;
    this->fwd_pass_comm_type=fwd_pass_comm_type;
    this->fwd_pass_comm_size=fwd_pass_comm_size;
    this->input_grad_compute_time=input_grad_compute_time;
    this->input_grad_comm_type=input_grad_comm_type;
    this->input_grad_comm_size=input_grad_comm_size;
    this->weight_grad_compute_time=weight_grad_compute_time;
    this->weight_grad_comm_type=weight_grad_comm_type;
    this->weight_grad_comm_size=weight_grad_comm_size;
    this->collective_counter=0;

    this->weight_grad_update_time=weight_grad_update_time;
    this->fwd_update_time=weight_grad_update_time;
    this->input_grad_update_time=weight_grad_update_time;

    //this->fwd_pass_dataset=NULL;
    //->input_grad_dataset=NULL;
    //this->weight_grad_dataset=NULL;
    this->total_forward_pass_compute=0;
    this->total_input_grad_compute=0;
    this->total_weight_grad_compute=0;
    this->total_weight_grad_comm=0;
    this->total_input_grad_comm=0;
    this->total_fwd_comm=0;
    this->total_waiting_for_wg_comm=0;
    this->total_waiting_for_ig_comm=0;
    this->total_waiting_for_fwd_comm=0;
    this->last_fwd_finished=0;
    this->last_ig_finished=0;
    this->last_wg_finished=0;
    assert(generator!=NULL);
}
//void call(EventType event,CallData *data){
/*if(event==EventType::Wight_Grad_Comm_Finished){
    total_weight_grad_comm+=weight_grad_dataset->finish_tick-weight_grad_dataset->creation_tick;
}*/
//    panic("sould not be called!");
//    return;
//}
void Layer::call(EventType event,CallData *mdata){
    if(event==EventType::Wight_Grad_Comm_Finished){
        last_wg_finished=Sys::boostedTick();
        generator->register_event(this,EventType::Wight_Grad_Comm_Finished_After_Delay,mdata,weight_grad_update_time);
        return;
    }
    else if(event==EventType::Input_Grad_Comm_Finished){
        last_ig_finished=Sys::boostedTick();
        generator->register_event(this,EventType::Input_Grad_Comm_Finished_After_Delay,mdata,input_grad_update_time);
        return;
    }
    else if(event==EventType::Fwd_Comm_Finished){
        last_fwd_finished=Sys::boostedTick();
        generator->register_event(this,EventType::Fwd_Comm_Finished_After_Delay,mdata,fwd_update_time);
        return;
    }
    int data=((IntData*)mdata)->data;
    IntData *intData=((IntData*)mdata);
    if(event==EventType::Wight_Grad_Comm_Finished_After_Delay){
        if(generator->id==0){
            std::cout<<"***** info: weight gradient collective for layer: "<<id<<" is finished************"<<std::endl;
        }
        total_weight_grad_comm+=weight_grad_datasets[data]->finish_tick-weight_grad_datasets[data]->creation_tick;
        if(weight_grad_datasets.size()==1 && wg_barrier==CollectiveBarrier::Blocking){
            total_waiting_for_wg_comm+=weight_grad_datasets[data]->finish_tick-weight_grad_datasets[data]->creation_tick;
            update_stream_stats(weight_grad_datasets[data]);
            int dataset_streams=weight_grad_datasets[data]->total_streams;
            delete weight_grad_datasets[data];
            weight_grad_datasets.erase(data);
            workload->call(EventType::General,NULL);
            generator->increase_finished_streams(dataset_streams);
            delete intData;
            return;
        }
        else if(started_waiting_for_weight_grad.size()>0){
            total_waiting_for_wg_comm+=weight_grad_datasets[data]->finish_tick-started_waiting_for_weight_grad.front();
            started_waiting_for_weight_grad.pop_front();
            update_stream_stats(weight_grad_datasets[data]);
            int dataset_streams=weight_grad_datasets[data]->total_streams;
            delete weight_grad_datasets[data];
            weight_grad_datasets.erase(data);
            workload->call(EventType::General,NULL);
            generator->increase_finished_streams(dataset_streams);
            delete intData;
            return;
        }
        update_stream_stats(weight_grad_datasets[data]);
        int dataset_streams=weight_grad_datasets[data]->total_streams;
        delete weight_grad_datasets[data];
        weight_grad_datasets.erase(data);
        generator->increase_finished_streams(dataset_streams);
        delete intData;
        return;
    }
    else if(event==EventType::Input_Grad_Comm_Finished_After_Delay){
        if(generator->id==0){
            std::cout<<"***** info: input gradient collective for layer: "<<id<<" is finished************"<<std::endl;
        }
        total_input_grad_comm+=input_grad_datasets[data]->finish_tick-input_grad_datasets[data]->creation_tick;
        if(input_grad_datasets.size()==1 && ig_barrier==CollectiveBarrier::Blocking){
            total_waiting_for_ig_comm+=input_grad_datasets[data]->finish_tick-input_grad_datasets[data]->creation_tick;
            update_stream_stats(input_grad_datasets[data]);
            int dataset_streams=input_grad_datasets[data]->total_streams;
            delete input_grad_datasets[data];
            input_grad_datasets.erase(data);
            workload->call(EventType::General,NULL);
            generator->increase_finished_streams(dataset_streams);
            delete intData;
            return;
        }
        else if(started_waiting_for_input_grad.size()>0){
            total_waiting_for_ig_comm+=input_grad_datasets[data]->finish_tick-started_waiting_for_input_grad.front();
            started_waiting_for_input_grad.pop_front();
            update_stream_stats(input_grad_datasets[data]);
            int dataset_streams=input_grad_datasets[data]->total_streams;
            delete input_grad_datasets[data];
            input_grad_datasets.erase(data);
            workload->call(EventType::General,NULL);
            generator->increase_finished_streams(dataset_streams);
            delete intData;
            return;
        }
        update_stream_stats(input_grad_datasets[data]);
        int dataset_streams=input_grad_datasets[data]->total_streams;
        delete input_grad_datasets[data];
        input_grad_datasets.erase(data);
        generator->increase_finished_streams(dataset_streams);
        delete intData;
        return;
    }
    else if(event==EventType::Fwd_Comm_Finished_After_Delay){
        if(generator->id==0){
            std::cout<<"***** info: fwd pass comm collective for layer: "<<id<<" is finished************"<<std::endl;
        }
        total_fwd_comm+=fwd_pass_datasets[data]->finish_tick-fwd_pass_datasets[data]->creation_tick;
        if(fwd_pass_datasets.size()==1 && fwd_barrier==CollectiveBarrier::Blocking){
            total_waiting_for_fwd_comm+=fwd_pass_datasets[data]->finish_tick-fwd_pass_datasets[data]->creation_tick;
            update_stream_stats(fwd_pass_datasets[data]);
            int dataset_streams=fwd_pass_datasets[data]->total_streams;
            delete fwd_pass_datasets[data];
            fwd_pass_datasets.erase(data);
            workload->call(EventType::General,NULL);
            generator->increase_finished_streams(dataset_streams);
            delete intData;
            return;
        }
        else if(started_waiting_for_fwd_pass.size()>0){
            if(generator->id==0){
                //std::cout<<"***** info: fwd pass comm collective for layer: "<<id<<" is finished and callback called************"<<std::endl;
            }
            total_waiting_for_fwd_comm+=fwd_pass_datasets[data]->finish_tick-started_waiting_for_fwd_pass.front();
            started_waiting_for_fwd_pass.pop_front();
            update_stream_stats(fwd_pass_datasets[data]);
            int dataset_streams=fwd_pass_datasets[data]->total_streams;
            delete fwd_pass_datasets[data];
            fwd_pass_datasets.erase(data);
            workload->call(EventType::General,NULL);
            generator->increase_finished_streams(dataset_streams);
            delete intData;
            return;
        }
        update_stream_stats(fwd_pass_datasets[data]);
        int dataset_streams=fwd_pass_datasets[data]->total_streams;
        delete fwd_pass_datasets[data];
        fwd_pass_datasets.erase(data);
        generator->increase_finished_streams(dataset_streams);
        delete intData;
        return;
    }
}

Tick Layer::get_fwd_pass_compute(){
    total_forward_pass_compute+=fwd_pass_compute_time;
    return fwd_pass_compute_time;
}
Tick Layer::get_input_grad_compute(){
    total_input_grad_compute+=input_grad_compute_time;
    return input_grad_compute_time;
}
Tick Layer::get_weight_grad_compute(){
    total_weight_grad_compute+=weight_grad_compute_time;
    return weight_grad_compute_time;
}
void Layer::increment_waiting_for_wg(){
    total_waiting_for_wg_comm++;
}
void Layer::increment_waiting_for_ig(){
    total_waiting_for_ig_comm++;
}
void Layer::increment_waiting_for_fwd(){
    total_waiting_for_fwd_comm++;
}
bool Layer::is_fwd_pass_comm_finished(){
    if(fwd_pass_datasets.size()==0){ // && Sys::boostedTick()-last_fwd_finished>=fwd_update_time
        return true;
    }
    return false;
}
bool Layer::is_fwd_pass_comm_finished_blocking(){
    if(fwd_pass_datasets.size()==0){
        return true;
    }
    started_waiting_for_fwd_pass.push_back(Sys::boostedTick());
    return false;
}
bool Layer::is_input_grad_comm_finished(){
    if(input_grad_datasets.size()==0){ //&& Sys::boostedTick()-last_ig_finished>=input_grad_update_time
        return true;
    }
    return false;
}
bool Layer::is_input_grad_comm_finished_blocking(){
    if(input_grad_datasets.size()==0){ //&& Sys::boostedTick()-last_ig_finished>=input_grad_update_time
        return true;
    }
    started_waiting_for_input_grad.push_back(Sys::boostedTick());
    return false;
}
bool Layer::is_weight_grad_comm_finished(){
    if(weight_grad_datasets.size()==0){ //&& Sys::boostedTick()-last_wg_finished>=weight_grad_update_time
        return true;
    }
    return false;
}
bool Layer::is_weight_grad_comm_finished_blocking(){
    if(weight_grad_datasets.size()==0){
        return true;
    }
    this->started_waiting_for_weight_grad.push_back(Sys::boostedTick());
    return false;
}
void Layer::report(std::string run_name,int layer_num,int total_rows,int stat_row,CSVWriter *detailed,CSVWriter *EndToEnd,double &total_compute,double &total_exposed){
    take_stream_stats_average();
    std::cout<<"*******************"<<std::endl;
    std::cout<<"Layer id: "<<id<<std::endl;
    std::cout<<"Total collectives issued for this layer: "<<collective_counter<<std::endl;
    if(stat_row==0){
        EndToEnd->write_cell(layer_num*total_rows+1,0,id);
        detailed->write_cell(layer_num*total_rows+1,0,id);
    }

    EndToEnd->write_cell(layer_num*total_rows+1+stat_row,1,run_name);
    detailed->write_cell(layer_num*total_rows+1+stat_row,1,run_name);


    std::cout<<"*************************  Workload stats  ************************* "<<id<<std::endl;

    std::cout<<"id: "<<id<<" ,Total cycles spent on fwd pass compute: "<<total_forward_pass_compute<<std::endl;
    if(stat_row==0 && layer_num==0){
        EndToEnd->write_cell(0,2,"fwd compute");
    }
    EndToEnd->write_cell(layer_num*total_rows+1+stat_row,2,std::to_string(total_forward_pass_compute/FREQ));
    total_compute+=(total_forward_pass_compute/FREQ);

    std::cout<<"id: "<<id<<" ,Total cycles spent on weight grad compute: "<<total_weight_grad_compute<<std::endl;
    if(stat_row==0 && layer_num==0){
        EndToEnd->write_cell(0,3,"wg compute");
    }
    EndToEnd->write_cell(layer_num*total_rows+1+stat_row,3,std::to_string(total_weight_grad_compute/FREQ));
    total_compute+=(total_weight_grad_compute/FREQ);

    std::cout<<"id: "<<id<<" ,Total cycles spent on input grad compute: "<<total_input_grad_compute<<std::endl;
    if(stat_row==0 && layer_num==0){
        EndToEnd->write_cell(0,4,"ig compute");
    }
    EndToEnd->write_cell(layer_num*total_rows+1+stat_row,4,std::to_string(total_input_grad_compute/FREQ));
    total_compute+=(total_input_grad_compute/FREQ);

    std::cout<<"id: "<<id<<" ,Total cycles spent idle waiting for fwd finish: "<<total_waiting_for_fwd_comm<<std::endl;
    if(stat_row==0 && layer_num==0){
        EndToEnd->write_cell(0,5,"fwd exposed comm");
    }
    EndToEnd->write_cell(layer_num*total_rows+1+stat_row,5,std::to_string(total_waiting_for_fwd_comm/FREQ));
    total_exposed+=(total_waiting_for_fwd_comm/FREQ);

    std::cout<<"id: "<<id<<" ,Total cycles spent idle waiting for weight grad finish: "<<total_waiting_for_wg_comm<<std::endl;
    if(stat_row==0 && layer_num==0){
        EndToEnd->write_cell(0,6,"wg exposed comm");
    }
    EndToEnd->write_cell(layer_num*total_rows+1+stat_row,6,std::to_string(total_waiting_for_wg_comm/FREQ));
    total_exposed+=(total_waiting_for_wg_comm/FREQ);

    std::cout<<"id: "<<id<<" ,Total cycles spent idle waiting for input grad finish: "<<total_waiting_for_ig_comm<<std::endl;
    if(stat_row==0 && layer_num==0){
        EndToEnd->write_cell(0,7,"ig exposed comm");
    }
    EndToEnd->write_cell(layer_num*total_rows+1+stat_row,7,std::to_string(total_waiting_for_ig_comm/FREQ));
    total_exposed+=(total_waiting_for_ig_comm/FREQ);

    std::cout<<"id: "<<id<<" ,Total cycles spent on fwd pass comm: "<<total_fwd_comm<<std::endl;
    if(stat_row==0 && layer_num==0){
        EndToEnd->write_cell(0,8,"fwd total comm");
    }
    EndToEnd->write_cell(layer_num*total_rows+1+stat_row,8,std::to_string(total_fwd_comm/FREQ));

    std::cout<<"id: "<<id<<" ,Total cycles spent on weight grad comm: "<<total_weight_grad_comm<<std::endl;
    if(stat_row==0 && layer_num==0){
        EndToEnd->write_cell(0,9,"wg total comm");
    }
    EndToEnd->write_cell(layer_num*total_rows+1+stat_row,9,std::to_string(total_weight_grad_comm/FREQ));

    std::cout<<"id: "<<id<<" ,Total cycles spent on input grad comm: "<<total_input_grad_comm<<std::endl;
    if(stat_row==0 && layer_num==0){
        EndToEnd->write_cell(0,10,"ig total comm");
    }
    EndToEnd->write_cell(layer_num*total_rows+1+stat_row,10,std::to_string(total_input_grad_comm/FREQ));

    if(stat_row==0 && layer_num==0){
        EndToEnd->write_cell(0,11,"workload finished at");
    }
    EndToEnd->write_cell(layer_num*total_rows+1+stat_row,11,std::to_string(((double)Sys::boostedTick())/FREQ));
    if(layer_num==workload->SIZE-1){
        if(stat_row==0){
            EndToEnd->write_cell(0,12,"total comp");
            EndToEnd->write_cell(0,13,"total exposed comm");
        }
        EndToEnd->write_cell(1+stat_row,12,std::to_string(total_compute));
        EndToEnd->write_cell(1+stat_row,13,std::to_string(total_exposed));
    }

    /*std::cout<<"*************************  Shared bus stats  ************************* "<<id<<std::endl;
    std::cout<<"id: "<<id<<" ,Average cycles spent on shared bus queue delay for transfer (per message): "<<total_shared_bus_transfer_queue_delay<<std::endl;
    if(stat_row==0 && layer_num==0){
        detailed->write_cell(0,2,"shared bus transfer queue delay");
    }
    detailed->write_cell(layer_num*total_rows+1+stat_row,2,std::to_string(total_shared_bus_transfer_queue_delay/FREQ));

    std::cout<<"id: "<<id<<" ,Average cycles spent on shared bus delay for transfer (per message): "<<total_shared_bus_transfer_delay<<std::endl;
    if(stat_row==0 && layer_num==0){
        detailed->write_cell(0,3,"shared bus transfer delay");
    }
    detailed->write_cell(layer_num*total_rows+1+stat_row,3,std::to_string(total_shared_bus_transfer_delay/FREQ));

    std::cout<<"id: "<<id<<" ,Average cycles spent on shared bus queue delay for processing (per message): "<<total_shared_bus_processing_queue_delay<<std::endl;
    if(stat_row==0 && layer_num==0){
        detailed->write_cell(0,4,"shared bus processing queue delay");
    }
    detailed->write_cell(layer_num*total_rows+1+stat_row,4,std::to_string(total_shared_bus_processing_queue_delay/FREQ));

    std::cout<<"id: "<<id<<" ,Average cycles spent on shared bus delay for processing (per message): "<<total_shared_bus_processing_delay<<std::endl;
    if(stat_row==0 && layer_num==0){
        detailed->write_cell(0,5,"shared bus processing delay");
    }
    detailed->write_cell(layer_num*total_rows+1+stat_row,5,std::to_string(total_shared_bus_processing_delay/FREQ));

    std::cout<<"*************************  Mem bus stats  ************************* "<<id<<std::endl;
    std::cout<<"id: "<<id<<" ,Average cycles spent on mem bus queue delay for transfer (per message): "<<total_mem_bus_transfer_queue_delay<<std::endl;
    if(stat_row==0 && layer_num==0){
        detailed->write_cell(0,6,"mem bus queue delay");
    }
    detailed->write_cell(layer_num*total_rows+1+stat_row,6,std::to_string(total_mem_bus_transfer_queue_delay/FREQ));

    std::cout<<"id: "<<id<<" ,Average cycles spent on mem bus delay for transfer (per message): "<<total_mem_bus_transfer_delay<<std::endl;
    if(stat_row==0 && layer_num==0){
        detailed->write_cell(0,7,"mem bus delay");
    }
    detailed->write_cell(layer_num*total_rows+1+stat_row,7,std::to_string(total_mem_bus_transfer_delay/FREQ));*/

    std::cout<<"*************************  Queuing stats  ************************* "<<id<<std::endl;
    int count=2;
    int i=0;
    for(auto &qd:queuing_delay){
        std::cout<<"id: "<<id<<" ,Average cycles spent on queuing for phase "<<i++<<" of algorithm (per chunk): "<<qd<<std::endl;
        if(stat_row==0 && layer_num==0){
            detailed->write_cell(0,count,"queuing delay phase "+std::to_string(i-1));
        }
        detailed->write_cell(layer_num*total_rows+1+stat_row,count++,std::to_string(qd/FREQ));
    }
    std::cout<<"*************************  Network stats  ************************* "<<id<<std::endl;
    i=1;
    for(auto &ml:net_message_latency){
        std::cout<<"id: "<<id<<" ,Average cycles spent on network for phase "<<i++<<" of algorithm (per message): "<<ml<<std::endl;
        if(stat_row==0 && layer_num==0){
            detailed->write_cell(0,count,"network delay phase "+std::to_string(i-1));
        }
        detailed->write_cell(layer_num*total_rows+1+stat_row,count++,std::to_string(ml/FREQ));
    }
}
void Layer::issue_forward_pass_comm(bool local,bool vertical,bool horizontal,SchedulingPolicy pref_scheduling,
                             CollectiveBarrier barrier){
    DataSet *fp=NULL;
    DataSet *fp2=NULL;
    fwd_barrier=barrier;
    collective_counter++;
    if(fwd_pass_comm_type==ComType::All_Reduce){
        fp=generator->generate_all_reduce(fwd_pass_comm_size,local,vertical,horizontal,pref_scheduling,layer_num);
        if(generator->id==0) {
            std::cout<<"info: allreduce forward pass collective issued for layer: "<<id<<std::endl;
        }
    }
    else if(fwd_pass_comm_type==ComType::All_to_All){
        fp=generator->generate_all_to_all(fwd_pass_comm_size,local,vertical,horizontal,pref_scheduling,layer_num);
    }
    else if(fwd_pass_comm_type==ComType::All_Gatehr){
        fp=generator->generate_all_gather(fwd_pass_comm_size,local,vertical,horizontal,pref_scheduling,layer_num);
        if(generator->id==0) {
            std::cout<<"info: allgather forward pass collective issued for layer: "<<id<<std::endl;
        }
    }
    else if(fwd_pass_comm_type==ComType::All_Reduce_All_to_All){
        fp=generator->generate_all_reduce(fwd_pass_comm_size,local,vertical,horizontal,pref_scheduling,layer_num);
        fp2=generator->generate_all_to_all(lookup_table_size,local,vertical,horizontal,pref_scheduling,layer_num);
    }
    else if(fwd_pass_comm_type==ComType::None){
        collective_counter--;
        if(generator->id==0) {
            std::cout<<"info: no forward pass collective for layer: "<<id<<std::endl;
        }
        if(barrier==CollectiveBarrier::Blocking){
            workload->call(EventType::General,NULL);
        }
        return;
    }
    else{
        Sys::sys_panic("no known collective operation! ");
    }
    fwd_pass_datasets[fp->my_id]=fp;
    fp->set_notifier(this,EventType::Fwd_Comm_Finished);
    if(fp2!=NULL){
        fwd_pass_datasets[fp2->my_id]=fp2;
        fp2->set_notifier(this,EventType::Fwd_Comm_Finished);
    }
}
void Layer::issue_input_grad_comm(bool local,bool vertical,bool horizontal,SchedulingPolicy pref_scheduling,
                           CollectiveBarrier barrier){
    DataSet *ig=NULL;
    DataSet *ig2=NULL;
    ig_barrier=barrier;
    collective_counter++;
    if(input_grad_comm_type==ComType::All_Reduce){
        ig=generator->generate_all_reduce(input_grad_comm_size,local,vertical,horizontal,pref_scheduling,layer_num);
        if(generator->id==0) {
            std::cout<<"info: allreduce input grad collective issued for layer: "<<id<<std::endl;
        }
    }
    else if(input_grad_comm_type==ComType::All_to_All){
        ig=generator->generate_all_to_all(input_grad_comm_size,local,vertical,horizontal,pref_scheduling,layer_num);
    }
    else if(input_grad_comm_type==ComType::All_Gatehr){
        ig=generator->generate_all_gather(input_grad_comm_size,local,vertical,horizontal,pref_scheduling,layer_num);
        if(generator->id==0) {
            std::cout<<"info: allgather input grad collective issued for layer: "<<id<<std::endl;
        }
    }
    else if(input_grad_comm_type==ComType::All_Reduce_All_to_All){
        ig=generator->generate_all_reduce(input_grad_comm_size,local,vertical,horizontal,pref_scheduling,layer_num);
        ig2=generator->generate_all_to_all(lookup_table_size,local,vertical,horizontal,pref_scheduling,layer_num);
    }
    else if(input_grad_comm_type==ComType::None){
        collective_counter--;
        if(generator->id==0) {
            std::cout<<"info: no input grad collective for layer: "<<id<<std::endl;
        }
        if(barrier==CollectiveBarrier::Blocking){
            workload->call(EventType::General,NULL);
        }
        return;
    }
    else{
        std::cout<<"no known collective operation! for layer: "<<id<<std::endl;
        Sys::sys_panic("no known collective operation! ");
    }
    input_grad_datasets[ig->my_id]=ig;
    ig->set_notifier(this,EventType::Input_Grad_Comm_Finished);
    if(ig2!=NULL){
        input_grad_datasets[ig2->my_id]=ig2;
        ig2->set_notifier(this,EventType::Input_Grad_Comm_Finished);
    }
}
void Layer::issue_weight_grad_comm(bool local,bool vertical,bool horizontal,SchedulingPolicy pref_scheduling,
                            CollectiveBarrier barrier){
    //if(weight_grad_dataset!=NULL)
    //delete weight_grad_dataset;
    DataSet *wg=NULL;
    DataSet *wg2=NULL;
    wg_barrier=barrier;
    collective_counter++;
    if(weight_grad_comm_type==ComType::All_Reduce){
        wg=generator->generate_all_reduce(weight_grad_comm_size,local,vertical,horizontal,pref_scheduling,layer_num);
        if(generator->id==0) {
            std::cout<<"info: allreduce weight grad collective issued for layer: "<<id<<" with size: "<<weight_grad_comm_size<<std::endl;
        }
    }
    else if(weight_grad_comm_type==ComType::All_to_All){
        wg=generator->generate_all_to_all(weight_grad_comm_size,local,vertical,horizontal,pref_scheduling,layer_num);
    }
    else if(weight_grad_comm_type==ComType::All_Gatehr){
        wg=generator->generate_all_gather(weight_grad_comm_size,local,vertical,horizontal,pref_scheduling,layer_num);
        if(generator->id==0) {
            std::cout<<"info: allgather weight grad collective issued for layer: "<<id<<std::endl;
        }
    }
    else if(weight_grad_comm_type==ComType::All_Reduce_All_to_All){
        wg=generator->generate_all_reduce(weight_grad_comm_size,local,vertical,horizontal,pref_scheduling,layer_num);
        wg2=generator->generate_all_to_all(lookup_table_size,local,vertical,horizontal,pref_scheduling,layer_num);
    }
    else if(weight_grad_comm_type==ComType::None){
        collective_counter--;
        if(generator->id==0) {
            std::cout<<"info: no weight grad collective for layer: "<<id<<std::endl;
        }
        if(barrier==CollectiveBarrier::Blocking){
            workload->call(EventType::General,NULL);
        }
        return;
    }
    else{
        Sys::sys_panic("no known collective operation! ");
    }
    weight_grad_datasets[wg->my_id]=wg;
    wg->set_notifier(this,EventType::Wight_Grad_Comm_Finished);
    if(wg2!=NULL){
        weight_grad_datasets[wg2->my_id]=wg2;
        wg2->set_notifier(this,EventType::Wight_Grad_Comm_Finished);
    }
}
Workload::~Workload() {
    if(end_to_end!=NULL){
        delete end_to_end;
    }
    if(detailed!=NULL){
        delete detailed;
    }
    for(int i=0;i<SIZE;i++){
        delete layers[i];
    }
    if(layers!=NULL){
        delete[] layers;
    }
}
Workload::Workload(std::string run_name,Sys *generator,std::string name, int TOTAL_PASS,int total_rows,int stat_row, std::string path) {
    this->initialized=false;
    this->layers = NULL;
    this->SIZE = 0;
    this->counter = 0;
    this->delay_loaded = false;
    this->collective_issued = false;
    this->current_state = LoopState::Forward_Pass;
    this->generator = generator;
    this->TOTAL_PASS = TOTAL_PASS;
    this->pass_counter = 0;
    this->index = 0;
    this->waiting_for_comm=0;
    end_to_end=NULL;
    detailed=NULL;
    this->path=path;
    this->stat_row=stat_row;
    this->initialized=initialize_workload(name);
    if(this->initialized== false){
        return;
    }
    this->total_rows=total_rows;
    this->run_name=run_name;
    this->registered_for_finished_streams= false;
    if(generator->id==0){
        std::cout<<"stat path: "<<path<<" ,total rows: "<<total_rows<<" ,stat row: "<<stat_row<<std::endl;
        detailed=new CSVWriter(path,"detailed.csv");
        end_to_end=new CSVWriter(path,"EndToEnd.csv");
        if(stat_row==0){
            initialize_stat_files();
        }
        //detailed->write_cell(0,0,"23");
        //detailed->write_cell(3,4,"46");
    }
}
void Workload::initialize_stat_files() {
    detailed->initialize_csv(SIZE*total_rows+20,50);
    end_to_end->initialize_csv(SIZE*total_rows+20,50);
}
void Workload::iterate() {
    if (parallelismPolicy == ParallelismPolicy::Data) {
        iterate_data_parallel();
    } else if (parallelismPolicy == ParallelismPolicy::Transformer) {
        iterate_hybrid_parallel_Transformer();
    } else if (parallelismPolicy == ParallelismPolicy::DLRM || parallelismPolicy==ParallelismPolicy::DLRMEnhanced) {
        iterate_hybrid_parallel_DLRM();
    } else if (parallelismPolicy == ParallelismPolicy::MicroBenchmark) {
        iterate_micro_benchmark();
    } else if (parallelismPolicy == ParallelismPolicy::Model) {
        iterate_model_parallel();
    } else if (parallelismPolicy == ParallelismPolicy::HybridDataModel) {
        iterate_hybrid_parallel_data_model();
    }else if (parallelismPolicy == ParallelismPolicy::HybridModelData) {
        iterate_hybrid_parallel_model_data();
    }else if (parallelismPolicy == ParallelismPolicy::DistributedInference) {
        iterate_distributed_inference();
    }
    else {
        Sys::sys_panic("No known parallelism!");
    }
}

void Workload::call(EventType event, CallData *data) {
    if (counter > 0) {
        generator->try_register_event(this, EventType::Workload_Wait, NULL, counter);
        return;
    }
    if (parallelismPolicy == ParallelismPolicy::Data) {
        iterate_data_parallel();
    } else if (parallelismPolicy == ParallelismPolicy::Transformer) {
        iterate_hybrid_parallel_Transformer();
    } else if (parallelismPolicy == ParallelismPolicy::DLRM || parallelismPolicy==ParallelismPolicy::DLRMEnhanced) {
        iterate_hybrid_parallel_DLRM();
    } else if (parallelismPolicy == ParallelismPolicy::MicroBenchmark) {
        iterate_micro_benchmark();
    } else if (parallelismPolicy == ParallelismPolicy::Model) {
        iterate_model_parallel();
    } else if (parallelismPolicy == ParallelismPolicy::HybridDataModel) {
        iterate_hybrid_parallel_data_model();
    }else if (parallelismPolicy == ParallelismPolicy::HybridModelData) {
        iterate_hybrid_parallel_model_data();
    }else if (parallelismPolicy == ParallelismPolicy::DistributedInference) {
        iterate_distributed_inference();
    }
    else {
        Sys::sys_panic("No known parallelism!");
    }
}
void Workload::report() {
    double total_compute=0;
    double total_exposed=0;
    for (int i = 0; i < SIZE; i++) {
        layers[i]->report(run_name,i,total_rows,stat_row,detailed,end_to_end,total_compute,total_exposed);
    }
    std::cout << "*************************" << std::endl;
    std::cout << "all passes finished at time: " << Sys::boostedTick()
              << ", id of first layer: " << layers[0]->id << std::endl;
    //std::cout << "Total cycles waiting for communication to be finished: " << waiting_for_comm << std::endl;
}
void Workload::check_for_sim_end() {
    if (pass_counter == TOTAL_PASS) {
        current_state=LoopState::Wait_For_Sim_Finish;
        if (generator->streams_finished != generator->streams_injected && registered_for_finished_streams==false) {
            generator->register_for_finished_stream(this);
            registered_for_finished_streams= true;
            //generator->register_event(this, EventType::General, NULL, 1);
            return;
        }
        if(generator->streams_finished== generator->streams_injected){
            if(generator->id == 0){
                report();
            }
            //std::cout<<"workload of node: "<<generator->id<<" has been finished"<<std::endl;
            generator->workload_finished();
            return;
        }
    }
    return;
}
void Workload::iterate_micro_benchmark() {
    assert(index >= 0);
    assert(index < SIZE);
    if(current_state!=LoopState::Wait_For_Sim_Finish){
        for (pass_counter = 0; pass_counter < TOTAL_PASS; pass_counter++) {
            layers[index]->issue_weight_grad_comm(true, true, true, SchedulingPolicy::None,
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
            //layers[index]->increment_waiting_for_wg();
            //waiting_for_comm++;
            //generator->register_event(this, EventType::General, NULL, 1);
            return;
        }
        if (delay_loaded == false) {
            counter = layers[index]->get_fwd_pass_compute();
            if (generator->id == 0) {
                //std::cout<<"layer: "<<index<<" delay in cycles: "<<counter<<std::endl;
            }
            delay_loaded = true;
        }
        if (counter > 0) {
            if (generator->id == 0) {
                //std::cout<<"i have been called in cycles: "<<Sys::boostedTick()<<std::endl;
            }
            generator->try_register_event(this, EventType::Workload_Wait, NULL, counter);
            return;
        }
        if (generator->id == 0) {
            //std::cout<<"moving to the fwp layer:"<<index<<" ,at time: "<<Sys::boostedTick()<<std::endl;
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
            generator->try_register_event(this, EventType::Workload_Wait, NULL, counter);
            return;
        }
        delay_loaded = false;
        layers[index]->issue_weight_grad_comm(true, true, true, SchedulingPolicy::None,
                                              CollectiveBarrier::Non_Blocking);
        if (index == 0) {
            if (generator->id == 0) {
                std::cout << "pass: " << pass_counter << " finished at time: " << Sys::boostedTick()
                          << std::endl;
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
            generator->try_register_event(this, EventType::Workload_Wait, NULL, counter);
            return;
        }
        delay_loaded = false;
        index--;
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
                //std::cout<<"layer: "<<index<<" delay in cycles: "<<counter<<std::endl;
            }
            delay_loaded = true;
        }
        if (counter > 0) {
            if (generator->id == 0) {
                //std::cout<<"i have been called in cycles: "<<Sys::boostedTick()<<std::endl;
            }
            generator->try_register_event(this, EventType::Workload_Wait, NULL, counter);
            return;
        }
        if (!collective_issued) {
            collective_issued = true;
            layers[index]->issue_forward_pass_comm(true, false, false, SchedulingPolicy::None,
                                                   CollectiveBarrier::Blocking);
            return;
        }
        if (generator->id == 0) {
            //std::cout<<"moving to the fwp layer:"<<index<<" ,at time: "<<Sys::boostedTick()<<std::endl;
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
            generator->try_register_event(this, EventType::Workload_Wait, NULL, counter);
            return;
        }
        if (!collective_issued) {
            collective_issued = true;
            layers[index]->issue_weight_grad_comm(false, true, true, SchedulingPolicy::FIFO,
                                                  CollectiveBarrier::Non_Blocking);
        }
        if (!layers[index]->is_input_grad_comm_finished_blocking()) {
            //layers[index]->increment_waiting_for_ig();
            //generator->register_event(this, EventType::General, NULL, 1);
            return;
        }
        collective_issued = false;
        delay_loaded = false;
        if (index >= 0) {
            index--;
        }
        if (index == -1) {
            index=0;
            if (generator->id == 0) {
                std::cout << "pass: " << pass_counter << " finished at time: " << Sys::boostedTick()
                          << std::endl;
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
            generator->try_register_event(this, EventType::Workload_Wait, NULL, counter);
            return;
        }
        if (!collective_issued && index > 0) {
            collective_issued = true;
            layers[index]->issue_input_grad_comm(true, false, false, SchedulingPolicy::LIFO,
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
                //std::cout<<"layer: "<<index<<" delay in cycles: "<<counter<<std::endl;
            }
            delay_loaded = true;
        }
        if (counter > 0) {
            if (generator->id == 0) {
                //std::cout<<"i have been called in cycles: "<<Sys::boostedTick()<<std::endl;
            }
            generator->try_register_event(this, EventType::Workload_Wait, NULL, counter);
            return;
        }
        if (!collective_issued) {
            collective_issued = true;
            layers[index]->issue_forward_pass_comm(false, true, true, SchedulingPolicy::None,
                                                   CollectiveBarrier::Blocking);
            return;
        }
        if (generator->id == 0) {
            //std::cout<<"moving to the fwp layer:"<<index<<" ,at time: "<<Sys::boostedTick()<<std::endl;
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
            generator->try_register_event(this, EventType::Workload_Wait, NULL, counter);
            return;
        }
        if (!collective_issued) {
            collective_issued = true;
            layers[index]->issue_weight_grad_comm(true, false, false, SchedulingPolicy::FIFO,
                                                  CollectiveBarrier::Non_Blocking);
        }
        if (!layers[index]->is_input_grad_comm_finished_blocking()) {
            //layers[index]->increment_waiting_for_ig();
            //generator->register_event(this, EventType::General, NULL, 1);
            return;
        }
        collective_issued = false;
        delay_loaded = false;
        if (index >= 0) {
            index--;
        }
        if (index == -1) {
            index=0;
            if (generator->id == 0) {
                std::cout << "pass: " << pass_counter << " finished at time: " << Sys::boostedTick()
                          << std::endl;
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
            generator->try_register_event(this, EventType::Workload_Wait, NULL, counter);
            return;
        }
        if (!collective_issued && index > 0) {
            collective_issued = true;
            layers[index]->issue_input_grad_comm(false, true, true, SchedulingPolicy::LIFO,
                                                 CollectiveBarrier::Non_Blocking);
        }
        collective_issued = false;
        delay_loaded = false;
        current_state = LoopState::Weight_Gradient;
        generator->register_event(this, EventType::General, NULL, 1);
        return;
    }
}
void Workload::iterate_distributed_inference(){
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
                //std::cout<<"layer: "<<index<<" delay in cycles: "<<counter<<std::endl;
            }
            delay_loaded = true;
        }
        if (counter > 0) {
            if (generator->id == 0) {
                //std::cout<<"i have been called in cycles: "<<Sys::boostedTick()<<std::endl;
            }
            generator->try_register_event(this, EventType::Workload_Wait, NULL, counter);
            return;
        }
        if (!collective_issued) {
            collective_issued = true;
            layers[index]->issue_forward_pass_comm(true, true, true, SchedulingPolicy::None,
                                                   CollectiveBarrier::Blocking);
            return;
        }
        if (generator->id == 0) {
            //std::cout<<"moving to the fwp layer:"<<index<<" ,at time: "<<Sys::boostedTick()<<std::endl;
        }
        index++;
        delay_loaded = false;
        collective_issued = false;
        if (index >= SIZE) {
            index=0;
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
                //std::cout<<"layer: "<<index<<" delay in cycles: "<<counter<<std::endl;
            }
            delay_loaded = true;
        }
        if (counter > 0) {
            if (generator->id == 0) {
                //std::cout<<"i have been called in cycles: "<<Sys::boostedTick()<<std::endl;
            }
            generator->try_register_event(this, EventType::Workload_Wait, NULL, counter);
            return;
        }
        if (!collective_issued) {
            collective_issued = true;
            layers[index]->issue_forward_pass_comm(true, true, true, SchedulingPolicy::None,
                                                   CollectiveBarrier::Blocking);
            return;
        }
        if (generator->id == 0) {
            //std::cout<<"moving to the fwp layer:"<<index<<" ,at time: "<<Sys::boostedTick()<<std::endl;
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
            generator->try_register_event(this, EventType::Workload_Wait, NULL, counter);
            return;
        }
        if (!layers[index]->is_input_grad_comm_finished_blocking()) {
            //layers[index]->increment_waiting_for_ig();
            //generator->register_event(this, EventType::General, NULL, 1);
            return;
        }
        collective_issued = false;
        delay_loaded = false;
        if (index >= 0) {
            index--;
        }
        if (index == -1) {
            index=0;
            if (generator->id == 0) {
                std::cout << "pass: " << pass_counter << " finished at time: " << Sys::boostedTick()
                          << std::endl;
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
            generator->try_register_event(this, EventType::Workload_Wait, NULL, counter);
            return;
        }
        if (!collective_issued && index > 0) {
            collective_issued = true;
            layers[index]->issue_input_grad_comm(true, true, true, SchedulingPolicy::LIFO,
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
                //std::cout<<"layer: "<<index<<" delay in cycles: "<<counter<<std::endl;
            }
            delay_loaded = true;
        }
        if (counter > 0) {
            if (generator->id == 0) {
                //std::cout<<"i have been called in cycles: "<<Sys::boostedTick()<<std::endl;
            }
            generator->try_register_event(this, EventType::Workload_Wait, NULL, counter);
            return;
        }
        if (!collective_issued) {
            collective_issued = true;
            layers[index]->issue_forward_pass_comm(true, false, true, SchedulingPolicy::None,
                                                   CollectiveBarrier::Blocking);
            return;
        }
        if (generator->id == 0) {
            //std::cout<<"moving to the fwp layer:"<<index<<" ,at time: "<<Sys::boostedTick()<<std::endl;
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
            generator->try_register_event(this, EventType::Workload_Wait, NULL, counter);
            return;
        }
        if (!collective_issued) {
            collective_issued = true;
            layers[index]->issue_weight_grad_comm(false, true, false, SchedulingPolicy::FIFO,
                                                  CollectiveBarrier::Non_Blocking);
        }
        if (!layers[index]->is_input_grad_comm_finished_blocking()) {
            //layers[index]->increment_waiting_for_ig();
            //generator->register_event(this, EventType::General, NULL, 1);
            return;
        }
        collective_issued = false;
        delay_loaded = false;
        if (index >= 0) {
            index--;
        }
        if (index == -1) {
            index=0;
            if (generator->id == 0) {
                std::cout << "pass: " << pass_counter << " finished at time: " << Sys::boostedTick()
                          << std::endl;
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
            generator->try_register_event(this, EventType::Workload_Wait, NULL, counter);
            return;
        }
        if (!collective_issued && index > 0) {
            collective_issued = true;
            layers[index]->issue_input_grad_comm(true, false, true, SchedulingPolicy::LIFO,
                                                 CollectiveBarrier::Non_Blocking);
        }
        collective_issued = false;
        delay_loaded = false;
        current_state = LoopState::Weight_Gradient;
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
            //layers[index]->increment_waiting_for_wg();
            //waiting_for_comm++;
            if(pass_counter==1 && generator->id==0 && generator->streams_finished==106){
                //std::cout<<"still waiting for copleteness of layer: "<<layers[index]->id<<std::endl;
            }
            //generator->register_event(this, EventType::General, NULL, 1);
            return;
        }
        if (delay_loaded == false) {
            counter = layers[index]->get_fwd_pass_compute();
            if (generator->id == 0) {
                //std::cout<<"layer: "<<index<<" delay in cycles: "<<counter<<std::endl;
            }
            delay_loaded = true;
        }
        if (counter > 0) {
            if (generator->id == 0) {
                //std::cout<<"i have been called in cycles: "<<Sys::boostedTick()<<std::endl;
            }
            generator->try_register_event(this, EventType::Workload_Wait, NULL, counter);
            return;
        }
        if (!collective_issued && layers[index]->fwd_pass_comm_type == ComType::All_to_All) {
            collective_issued = true;
            layers[index]->issue_forward_pass_comm(true, true, true, SchedulingPolicy::HIGHEST,
                                                   CollectiveBarrier::Non_Blocking);

        } else if (index == DLRM_LAST_BOTTOM_LAYER) {
            if (!layers[0]->is_fwd_pass_comm_finished_blocking()) {
                //layers[0]->increment_waiting_for_fwd();
                //generator->register_event(this, EventType::General, NULL, 1);
                return;
            }
        }
        if (generator->id == 0) {
            //std::cout<<"moving to the fwp layer:"<<index<<" ,at time: "<<Sys::boostedTick()<<std::endl;
        }
        index++;
        delay_loaded = false;
        collective_issued = false;
        if (index >= SIZE) {
            current_state = LoopState::Weight_Gradient;
            index--;
        }
        if (generator->id == 0) {
            std::cout << "*************************layer changed to: " << index << std::endl;
        }
        generator->register_event(this, EventType::General, NULL, 1);
        return;
    } else if (current_state == LoopState::Weight_Gradient) {
        if (delay_loaded == false) {
            counter = layers[index]->get_weight_grad_compute();
            delay_loaded = true;
        }
        if (counter > 0) {
            generator->try_register_event(this, EventType::Workload_Wait, NULL, counter);
            return;
        }
        if (!collective_issued) {
            collective_issued = true;
            layers[index]->issue_weight_grad_comm(true, true, true, SchedulingPolicy::None,
                                                  CollectiveBarrier::Non_Blocking);
        }
        if (parallelismPolicy==ParallelismPolicy::DLRM && !layers[index]->is_input_grad_comm_finished_blocking()) {
            //layers[index]->increment_waiting_for_ig();
            //generator->register_event(this, EventType::General, NULL, 1);
            return;
        }
        if (index == 0) {
            if (generator->id == 0) {
                std::cout << "pass: " << pass_counter << " finished at time: " << Sys::boostedTick()
                          << std::endl;
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
            generator->try_register_event(this, EventType::Workload_Wait, NULL, counter);
            return;
        }
        if (index == DLRM_LAST_BOTTOM_LAYER+1) {
            layers[0]->issue_input_grad_comm(true, true, true, SchedulingPolicy::HIGHEST,
                                             CollectiveBarrier::Non_Blocking);
        }
        index--;
        if (generator->id == 0) {
            std::cout<<"*************************layer changed to: "<<index<<" in ig"<<std::endl;
        }
        current_state = LoopState::Weight_Gradient;
        collective_issued = false;
        delay_loaded = false;
        generator->register_event(this, EventType::General, NULL, 1);
    }
}
int Workload::get_layer_numbers(std::string workload_input) {
    std::ifstream inFile;
    inFile.open("workload_inputs/" + workload_input +".txt");
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
bool Workload::initialize_workload(std::string name) {
    std::ifstream inFile;
    inFile.open("workload_inputs/" + name);
    if (!inFile) {
        std::cout << "Unable to open file: " << name << std::endl;
        std::cout<<"######### Exiting because unable to open the workload input file #########"<< std::endl;
        return false;
    } else {
        std::cout << "success in openning file" << std::endl;
    }
    std::string type;
    int lines;
    inFile >> type;
    if (type == "DATA") {
        parallelismPolicy = ParallelismPolicy::Data;
    } else if (type == "HYBRID_TRANSFORMER") {
        parallelismPolicy = ParallelismPolicy::Transformer;
    } else if (type == "HYBRID_DLRM" || type == "HYBRID_DLRM_ENHANCED") {
        if(type == "HYBRID_DLRM"){
            parallelismPolicy = ParallelismPolicy::DLRM;
        }
        else if(type == "HYBRID_DLRM_ENHANCED"){
            parallelismPolicy=ParallelismPolicy ::DLRMEnhanced;
        }
        inFile >> DLRM_LAST_BOTTOM_LAYER;
        if(generator->id==0){
            std::cout<<"****************** info: DLRM workload last bottom layer is: "<<DLRM_LAST_BOTTOM_LAYER<<std::endl;
        }
    }
    else if (type == "MODEL") {
        parallelismPolicy = ParallelismPolicy::Model;
    }else if (type == "HYBRID_DATA_MODEL") {
        parallelismPolicy = ParallelismPolicy::HybridDataModel;
    }else if (type == "HYBRID_MODEL_DATA") {
        parallelismPolicy = ParallelismPolicy::HybridModelData;
    }else if (type == "MICRO") {
        parallelismPolicy = ParallelismPolicy::MicroBenchmark;
    }
    else if (type == "DISTRIBUTED_INFERENCE") {
        parallelismPolicy = ParallelismPolicy::DistributedInference;
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
        //wg_comm_size=2048;
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
        }else if (wg_comm_type_s == "ALLGATHER") {
            wg_type = ComType::All_Gatehr;
        }

        if (ig_comm_type_s == "ALLREDUCE") {
            ig_type = ComType::All_Reduce;
        } else if (ig_comm_type_s == "ALLTOALL") {
            ig_type = ComType::All_to_All;
        } else if (ig_comm_type_s == "ALLREDUCEALLTOALL") {
            ig_type = ComType::All_Reduce_All_to_All;
        }else if (ig_comm_type_s == "ALLGATHER") {
            ig_type = ComType::All_Gatehr;
        }

        if (fp_comm_type_s == "ALLREDUCE") {
            fp_type = ComType::All_Reduce;
        } else if (fp_comm_type_s == "ALLTOALL") {
            fp_type = ComType::All_to_All;
        } else if (fp_comm_type_s == "ALLREDUCEALLTOALL") {
            fp_type = ComType::All_Reduce_All_to_All;
        }else if (fp_comm_type_s == "ALLGATHER") {
            fp_type = ComType::All_Gatehr;
        }

        if (generator->id == 0) {
            std::cout << "id: " << id << " , depen: " << depen << " , wg_comp_time: " << wg_compute_time
                      << std::endl;
        }
        Layer *l = new Layer(id,i, generator,this ,fp_compute_time * generator->compute_scale, fp_type,
                             fp_comm_size * generator->comm_scale, ig_compute_time * generator->compute_scale,
                             ig_type,
                             ig_comm_size * generator->comm_scale, wg_compute_time * generator->compute_scale,
                             wg_type,
                             wg_comm_size * generator->comm_scale, wg_update_time);

        layers[i] = l;
    }
    if (generator->id == 0) {
        std::cout << "type: " << type << " ,num passes: " << TOTAL_PASS << " ,lines: " << lines
                  << " compute scale: " << generator->compute_scale << " ,comm scale: " << generator->comm_scale
                  << std::endl;
    }
    inFile.close();
    return true;
}
void Workload::fire() {
    call(EventType::General, NULL);
}
