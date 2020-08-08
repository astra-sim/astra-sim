#ifndef __ASTRAMEMORYAPI_HH__
#define __ASTRAMEMORYAPI_HH__
#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
#include <cstdint>
class AstraMemoryAPI{
    public:
        virtual  uint64_t mem_read(uint64_t size)=0;
        virtual  uint64_t mem_write(uint64_t size)=0;
        virtual  uint64_t npu_mem_read(uint64_t size)=0;
        virtual  uint64_t npu_mem_write(uint64_t size)=0;
        virtual  uint64_t nic_mem_read(uint64_t size)=0;
        virtual  uint64_t nic_mem_write(uint64_t size)=0;
        virtual ~AstraMemoryAPI()= default;
};
#endif
