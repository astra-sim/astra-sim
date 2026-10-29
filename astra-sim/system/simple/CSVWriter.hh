// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <cstdint>
#include <fstream>
#include <list>
#include <string>
#include <sys/stat.h>

namespace AstraSim {

class CSVWriter {
  public:
    CSVWriter(std::string path, std::string name);
    ~CSVWriter() {
        if (myFile.is_open()) {
            myFile.close();
        }
    }
    void initialize_csv(int rows, int cols);
    void finalize_csv(std::list<std::list<std::pair<uint64_t, double>>> dims);
    void write_cell(int row, int column, std::string data);
    inline bool exists_test(const std::string& name) {
        struct stat buffer;
        return (stat(name.c_str(), &buffer) == 0);
    }

    std::fstream myFile;
    std::string name;
    std::string path;
};

}  // namespace AstraSim
