#ifndef __LOCAL_MEM_USAGE_TRACKER__
#define __LOCAL_MEM_USAGE_TRACKER__

#include "astra-sim/common/Common.hh"
#include <json/json.hpp>
#include <map>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "extern/graph_frontend/chakra/src/feeder_v3/et_feeder.h"

using json = nlohmann::json;

namespace AstraSim {

typedef struct {
    Tick start;
    Tick end;
    std::string nodeName;
    uint64_t nodeId;
} MemActivity;

typedef std::string TensorId;

class LocalMemUsageTracker {
  public:
    LocalMemUsageTracker(uint64_t sysId) : sysId(sysId) {}
    ~LocalMemUsageTracker();
    void recordStart(const std::shared_ptr<Chakra::ETFeederNode> node,
                     Tick tick);
    void recordEnd(const std::shared_ptr<Chakra::ETFeederNode> node, Tick tick);
    void buildMemoryTrace();
    void dumpMemoryTrace(const std::string& filename);
    void buildMemoryTimeline();
    void buildTensorLifetimeHeatmap();
    std::tuple<float, std::string> getPeakMemUsageFormatted() const;
    uint64_t getPeakMemUsage() const;

    uint64_t sysId;

  private:
    void recordReads(const std::shared_ptr<Chakra::ETFeederNode> node,
                     Tick start,
                     Tick end);
    void recordWrites(const std::shared_ptr<Chakra::ETFeederNode> node,
                      Tick start,
                      Tick end);

    uint64_t parseIOInfos(
        const google::protobuf::RepeatedPtrField<std::string>& values,
        std::vector<std::tuple<TensorId, uint64_t>>& IOinfos);
    std::unordered_map<TensorId, std::vector<MemActivity>> memReads;
    std::unordered_map<TensorId, MemActivity> memWrites;
    std::unordered_map<TensorId, uint64_t> tensorSize;
    std::unordered_map<uint64_t, Tick> activityStartTime;
    std::vector<json> serializedMemoryTrace;
    std::map<Tick, std::unordered_set<TensorId>> memoryContents;
    std::map<Tick, uint64_t> memoryUsage;
    std::unordered_map<TensorId, uint64_t> tensorMapId;
};

}  // namespace AstraSim

#endif
