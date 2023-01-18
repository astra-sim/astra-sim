/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __CSV_WRITER_HH__
#define __CSV_WRITER_HH__

#include <sys/stat.h>
#include <fstream>
#include <list>
#include <string>

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
  void finalize_csv(std::list<std::list<std::pair<uint64_t, double> > > dims);
  void write_cell(int row, int column, std::string data);
  inline bool exists_test(const std::string& name) {
    struct stat buffer;
    return (stat(name.c_str(), &buffer) == 0);
  }

  std::fstream myFile;
  std::string name;
  std::string path;
};

} // namespace AstraSim

#endif /* __CSV_WRITER_HH__ */
