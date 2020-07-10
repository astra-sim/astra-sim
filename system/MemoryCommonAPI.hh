#ifndef __MEMORYCOMMONAPI_HH__
#define __MEMORYCOMMONAPI_HH__
#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
#include <cstdint>

class MemoryCommonAPI{
    public:
        virtual  uint64_t mem_read(uint64_t size)=0;
        virtual  uint64_t mem_write(uint64_t size)=0;
        virtual ~MemoryCommonAPI()= default;
};
#endif
