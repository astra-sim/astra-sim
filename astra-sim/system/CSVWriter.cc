/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "astra-sim/system/CSVWriter.hh"

#include "astra-sim/common/Logging.hh"
#include "astra-sim/system/Common.hh"

#include <fcntl.h>
#include <unistd.h>
#include <cassert>
#include <cmath>
#include <iostream>
#include <utility>
#include <vector>

using namespace std;
using namespace AstraSim;

CSVWriter::CSVWriter(std::string path, std::string name) {
  this->path = path;
  this->name = name;
}

void CSVWriter::initialize_csv(int rows, int cols) {
  auto logger = LoggerFactory::get_logger("system::CSVWriter");
  logger->info("CSV path and filename: {}{}", path, name);
  int trial = 10000;
  do {
    myFile.open(path + name, std::fstream::out);
    trial--;
  } while (!myFile.is_open() && trial > 0);
  if (trial == 0) {
    logger->critical("Unable to create file: {}", path);
    logger->critical(
        "This error is fatal. Please make sure the CSV write path exists.");
    exit(1);
  }
  do {
    myFile.close();
  } while (myFile.is_open());

  do {
    myFile.open(path + name, std::fstream::out | std::fstream::in);
  } while (!myFile.is_open());

  if (!myFile) {
    logger->critical("Unable to create file: {}", path);
    logger->critical(
        "This error is fatal. Please make sure the CSV write path exists.");
    exit(1);
  } else {
    logger->info("Success in opening CSV file for writing the report.");
  }

  myFile.seekp(0, std::ios_base::beg);
  myFile.seekg(0, std::ios_base::beg);
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < cols - 1; j++) {
      myFile << ',';
    }
    myFile << '\n';
  }
  do {
    myFile.close();
  } while (myFile.is_open());
}

void CSVWriter::finalize_csv(
    std::list<std::list<std::pair<uint64_t, double>>> dims) {
  auto logger = LoggerFactory::get_logger("system::CSVWriter");
  logger->info("path to create csvs is: {}", path);
  int trial = 10000;
  do {
    myFile.open(path + name, std::fstream::out);
    trial--;
  } while (!myFile.is_open() && trial > 0);
  if (trial == 0) {
    logger->critical("Unable to create file: {}{}", path, name);
    logger->critical(
        "This error is fatal. Please make sure the CSV write path exists.");
    exit(1);
  }
  do {
    myFile.close();
  } while (myFile.is_open());

  do {
    myFile.open(path + name, std::fstream::out | std::fstream::in);
  } while (!myFile.is_open());

  if (!myFile) {
    logger->critical("Unable to create file: {}{}", path, name);
    logger->critical(
        "This error is fatal. Please make sure the CSV write path exists.");
    exit(1);
  } else {
    logger->info("success in openning file");
  }
  myFile.seekp(0, std::ios_base::beg);
  myFile.seekg(0, std::ios_base::beg);
  std::vector<std::list<std::pair<uint64_t, double>>::iterator> dims_it;
  std::vector<std::list<std::pair<uint64_t, double>>::iterator> dims_it_end;
  for (auto& dim : dims) {
    dims_it.push_back(dim.begin());
    dims_it_end.push_back(dim.end());
  }
  int dim_num = 1;
  myFile << " time (us) ";
  myFile << ",";
  for (uint64_t i = 0; i < dims.size(); i++) {
    myFile << "dim" + std::to_string(dim_num) + " util";
    myFile << ',';
    dim_num++;
  }
  myFile << '\n';
  while (true) {
    uint64_t finished = 0;
    uint64_t compare;
    for (uint64_t i = 0; i < dims_it.size(); i++) {
      if (dims_it[i] != dims_it_end[i]) {
        if (i == 0) {
          myFile << std::to_string((*dims_it[i]).first / FREQ);
          myFile << ",";
          compare = (*dims_it[i]).first;
        } else {
          assert(compare == (*dims_it[i]).first);
        }
      }
      if (dims_it[i] == dims_it_end[i]) {
        finished++;
        myFile << ",";
        continue;
      } else {
        myFile << std::to_string((*dims_it[i]).second);
        myFile << ',';
        std::advance(dims_it[i], 1);
      }
    }
    myFile << '\n';
    if (finished == dims_it_end.size()) {
      break;
    }
  }
  myFile.close();
}

void CSVWriter::write_cell(int row, int column, std::string data) {
  std::string str = "";
  std::string tmp;

  int status = 1;
  int fildes = -1;
  do {
    fildes = open((path + name).c_str(), O_RDWR);
  } while (fildes == -1);

  do {
    status = lockf(fildes, F_TLOCK, (off_t)1000000);
  } while (status != 0);
  char buf[1];
  while (row > 0) {
    status = read(fildes, buf, 1);
    str = str + (*buf);
    if (*buf == '\n') {
      row--;
    }
  }
  while (column > 0) {
    status = read(fildes, buf, 1);
    str = str + (*buf);
    if (*buf == ',') {
      column--;
    }
    if (*buf == '\n') {
      LoggerFactory::get_logger("system::CSVWriter")
          ->critical("fatal error in inserting cewll!");
      exit(1);
    }
  }
  str = str + data;
  while (read(fildes, buf, 1)) {
    str = str + (*buf);
  }

  do {
    lseek(fildes, 0, SEEK_SET);
    status = write(fildes, str.c_str(), str.length());
  } while (status != str.length());

  do {
    status = lseek(fildes, 0, SEEK_SET);
  } while (status == -1);

  do {
    status = lockf(fildes, F_ULOCK, (off_t)1000000);
  } while (status != 0);

  do {
    status = close(fildes);
  } while (status == -1);
  return;
}
