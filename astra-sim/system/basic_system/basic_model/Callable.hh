/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __CALLABLE_HH__
#define __CALLABLE_HH__

#include "astra-sim/common/Common.hh"
#include "astra-sim/system/basic_system/basic_model/CallData.hh"

namespace AstraSim {

class Callable {
  public:
    virtual ~Callable() = default;
    virtual void call(EventType type, CallData* data) = 0;
};

}  // namespace AstraSim

#endif /* __CALLABLE_HH__ */
