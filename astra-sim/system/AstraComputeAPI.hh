#ifndef __COMPUTE_HH__
#define __COMPUTE_HH__
#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
#include <cstdint>
#include "AstraNetworkAPI.hh"
namespace AstraSim{
class ComputeMetaData{
    public:
        uint64_t compute_delay; //delay in nanoseconds
};
class ComputeAPI{
    public:
        int array_height; //scale-sim properties
        double memBW;
        virtual timespec_t compute(uint64_t M, uint64_t K, uint64_t N,double MemBWPercentage)=0;
};
}
#endif
