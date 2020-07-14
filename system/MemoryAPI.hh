#ifndef __MEMORYAPI_HH__
#define __MEMORYAPI_HH__
#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
#include <cstdint>

class MemoryAPI{
    public:
        virtual  uint64_t mem_read(uint64_t size)=0;
        virtual  uint64_t mem_write(uint64_t size)=0;
        virtual ~MemoryAPI()= default;
};
#endif
