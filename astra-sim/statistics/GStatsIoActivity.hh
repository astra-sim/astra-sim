/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#pragma once
#include "GStats.hh"
#include <astra-sim/system/Common.hh>
#include <list>
#include <map>

using Time_t = AstraSim::timespec_t;
using Wave = std::list<std::pair<Time_t, Time_t>>;

class GStatsIoActivity : public GStats {
 public:
  explicit GStatsIoActivity(const std::string& format,...);
  GStatsIoActivity()= default;
  ~GStatsIoActivity();

  void setNameStr(const std::string& format,...);
  void reportValue() const override;
  void resetValue() override;

  void prepareReport() override;

  void recordEntry(std::string key, std::pair<Time_t, Time_t>& entry);
  int report();

 private:
  std::map<std::string, Wave> activityMap;
  std::map<std::string, Wave>::iterator mapIter;
};
