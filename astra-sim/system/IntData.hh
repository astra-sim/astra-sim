/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __INT_DATA_HH__
#define __INT_DATA_HH__

namespace AstraSim {

class IntData : public CallData {
 public:
  IntData(int d) {
    data = d;
  }
  int data;
};

} // namespace AstraSim

#endif /* __INT_DATA_HH__ */
