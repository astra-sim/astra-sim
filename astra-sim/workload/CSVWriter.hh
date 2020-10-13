/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __CSVWRITER_HH__
#define __CSVWRITER_HH__


#include <map>
#include <math.h>
#include <fstream>
#include <chrono>
#include <ctime>
#include <tuple>
#include <cstdint>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

namespace AstraSim{
class CSVWriter{
public:
    //std::fstream inFile;
    std::fstream myFile;
    void initialize_csv(int rows, int cols);
    CSVWriter(std::string path,std::string name);
    void write_cell(int row,int column,std::string data);
    std::string path;
    std::string name;
    ~CSVWriter(){
        myFile.close();
    }
    inline bool exists_test(const std::string& name) {
        struct stat buffer;
        return (stat (name.c_str(), &buffer) == 0);
    }
};
}
#endif
