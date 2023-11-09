//
// Created by dkadiyala3 on 10/29/23.
//
#include "GStatsIoActivity.hh"
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <iostream>

GStatsIoActivity::GStatsIoActivity(const std::string& format,...) {
 assert(!format.empty()); // Mandatory to pass a non-empty description

  va_list ap;
  va_start(ap, format);
  this->name = getText(format, ap);
  va_end(ap);
  subscribe();
}

GStatsIoActivity::~GStatsIoActivity(){
  unsubscribe();
}


void GStatsIoActivity::setNameStr(const std::string& format,...){
  assert(!format.empty()); // Mandatory to pass a non-empty description
  va_list ap;

  va_start(ap, format);
  this->name = getText(format, ap);
  va_end(ap);
  subscribe();
}

void GStatsIoActivity::recordEntry(std::string key, std::pair<Time_t, Time_t>& entry) {

  // check if the key exists in the map.
  mapIter = activityMap.find(key);
  if(mapIter != activityMap.end()){
    // get the last timestamps of ioActivity block
    Wave::iterator blockPair = mapIter->second.end();
    --blockPair; // move the iterator to the start of last entry in the list.
    long double prevStart = blockPair->first.time_val;
    long double prevStop  = blockPair->second.time_val;
    long double currStart = entry.first.time_val;
    long double currStop  = entry.second.time_val;

    if(currStart > prevStop){
      // New blockPair into the wave.
      activityMap[key].push_back(entry);
    }else if( prevStart <= currStart && currStart <= prevStop){
      // check the curr stop
      if(currStop > prevStop){
        // Extend the previous stop to current stop time
        blockPair->second.time_val = currStop;
      }else{
        // ignore the current block as it is covered by previous block
      }
    }else{
      assert((currStart>prevStart) && "\n\tError: block rearrangment\n");
    }

  }else{
    // new entry.
    activityMap[key].push_back(entry);
  }
}

void GStatsIoActivity::reportValue() const {
  // print the output ioActivity of the link to a file.
  std::string filename = this->name + ".txt";

  // Open the file for writing
  std::ofstream outputFile(filename);

  // Check if the file opened successfully
  if (!outputFile.is_open()) {
    std::cerr << "Failed to open the file." << std::endl;
  }

  if(!activityMap.empty()) {
    for (const auto& entry : activityMap) {
      std::string key = entry.first;
      const Wave& wave = entry.second;

      outputFile << key;
      for (const auto& block : wave) {
        outputFile << ",(" << block.first.time_val << ":"
                   << block.second.time_val << ")";
      }
      outputFile << std::endl;
    }
  }

  // Close the file
  outputFile.close();
  std::string cmd = "bzip2 -9 -f " + filename;
  std::system(cmd.c_str());
  // printf("Name of this container: %s\n",(this->name).c_str());
}

void GStatsIoActivity::resetValue() {

}

void GStatsIoActivity::prepareReport() {

}