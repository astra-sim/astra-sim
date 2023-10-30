//
// Created by dkadiyala3 on 10/29/23.
//

#ifndef ASTRASIM_ANALYTICAL_GSTATSIOACTIVITY_HH
#define ASTRASIM_ANALYTICAL_GSTATSIOACTIVITY_HH

#include "GStats.hh"
#include "astra-sim/system/AstraNetworkAPI.hh"
#include <list>
#include <map>

using Time_t = AstraSim::timespec_t;
using Wave = std::list<std::pair<Time_t, Time_t>>;

class GStatsIoActivity : public GStats {
 public:
  GStatsIoActivity(const std::string& format,...);
  GStatsIoActivity();
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
#endif // ASTRASIM_ANALYTICAL_GSTATSIOACTIVITY_HH
