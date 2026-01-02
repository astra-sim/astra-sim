#include "astra-sim/workload/LocalMemUsageTracker.hh"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_set>
#include <vector>

#include "astra-sim/common/Common.hh"
#include "astra-sim/common/Logging.hh"

// Using the new feeder v3 APIs.
using namespace AstraSim;
using namespace Chakra;
using namespace Chakra::FeederV3;  // Bring ChakraAttr and other FeederV3 types
                                   // into scope

uint64_t LocalMemUsageTracker::parseIOInfos(
    const google::protobuf::RepeatedPtrField<std::string>& values,
    std::vector<std::tuple<TensorId, uint64_t>>& IOinfos) {
    if (values.size() % 2 != 0) {
        throw std::runtime_error("IO infos list size is not even.");
    }
    uint64_t parsedCnt = 0;
    for (int i = 0; i < values.size(); i += 2) {
        const TensorId& tensorName = values.Get(i);
        const std::string& sizeStr = values.Get(i + 1);
        uint64_t size = std::stoull(sizeStr);
        IOinfos.emplace_back(tensorName, size);
        ++parsedCnt;
    }
    return parsedCnt;
}

void LocalMemUsageTracker::recordReads(
    const std::shared_ptr<Chakra::ETFeederNode> node, Tick start, Tick end) {
    static std::vector<std::tuple<TensorId, uint64_t>> IOinfos;
    IOinfos.clear();
    uint64_t nodeId = node->id();

    if (!node->has_attr("inputs")) {
        throw std::runtime_error("Attribute 'inputs' not found in node " +
                                 std::to_string(nodeId) +
                                 " (LocalMemUsageTracker::recordReads)");
    }

    // use the explicit template type.
    auto attr = node->get_attr<ChakraAttr>("inputs");
    if (!attr.has_string_list() || attr.string_list().values().empty()) {
        return;
    }
    const auto& values = attr.string_list().values();
    uint64_t parsedCount = parseIOInfos(values, IOinfos);

    for (const auto& iter : IOinfos) {
        TensorId tensorName = std::get<0>(iter);
        uint64_t tensorSize = std::get<1>(iter);

        // Ignore tensors with size 0
        if (tensorSize == 0) {
            continue;
        }

        MemActivity readActivity;
        readActivity.start = start;
        readActivity.end = end;
        readActivity.nodeName = node->name();
        readActivity.nodeId = node->id();

        if (this->tensorSize.find(tensorName) == this->tensorSize.end()) {
            AstraSim::LoggerFactory::get_logger(
                "workload::LocalMemUsageTracker")
                ->trace("tracker record read before write node.id={} "
                        "tensor.name={} start={} end={}",
                        node->id(), tensorName, start, end);
            MemActivity writeActivity;
            writeActivity.start = 0ul;
            writeActivity.end = 10ul;
            writeActivity.nodeName = "UNDEFINED";
            writeActivity.nodeId = UINT64_MAX;
            this->tensorSize.insert({tensorName, tensorSize});
            this->memWrites.insert({tensorName, writeActivity});
        }
        if (this->memReads.find(tensorName) == this->memReads.end()) {
            this->memReads.emplace(tensorName, std::vector<MemActivity>());
        }
        AstraSim::LoggerFactory::get_logger("workload::LocalMemUsageTracker")
            ->trace(
                "tracker record read node.id={} tensor.name={} start={} end={}",
                node->id(), tensorName, start, end);
        this->memReads[tensorName].emplace_back(readActivity);
    }
}

void LocalMemUsageTracker::recordWrites(
    const std::shared_ptr<Chakra::ETFeederNode> node, Tick start, Tick end) {
    static std::vector<std::tuple<TensorId, uint64_t>> IOinfos;
    IOinfos.clear();
    uint64_t nodeId = node->id();

    if (!node->has_attr("outputs")) {
        throw std::runtime_error("Attribute 'outputs' not found in node " +
                                 std::to_string(nodeId) +
                                 " (LocalMemUsageTracker::recordWrites)");
    }

    auto attr = node->get_attr<ChakraAttr>("outputs");
    if (!attr.has_string_list() || attr.string_list().values().empty()) {
        return;
    }

    const auto& values = attr.string_list().values();
    uint64_t parsedCount = parseIOInfos(values, IOinfos);

    for (const auto& iter : IOinfos) {
        TensorId tensorName = std::get<0>(iter);
        uint64_t tensorSize = std::get<1>(iter);

        // Ignore tensors with size 0
        if (tensorSize == 0) {
            continue;
        }

        MemActivity writeActivity;
        writeActivity.start = start;
        writeActivity.end = end;
        writeActivity.nodeName = node->name();
        writeActivity.nodeId = node->id();
        AstraSim::LoggerFactory::get_logger("workload::LocalMemUsageTracker")
            ->trace("tracker record write node.id={} tensor.name={} start={} "
                    "end={}",
                    node->id(), tensorName, start, end);
        AstraSim::LoggerFactory::get_logger("workload::LocalMemUsageTracker")
            ->flush();
        if (this->tensorSize.find(tensorName) == this->tensorSize.end()) {
            // first write
            this->tensorSize.insert({tensorName, tensorSize});
            this->memWrites.insert({tensorName, writeActivity});
        } else {
            // each tensor should only be written once.
            assert(false);
        }
    }
}

void LocalMemUsageTracker::recordStart(
    const std::shared_ptr<Chakra::ETFeederNode> node, Tick tick) {
    uint64_t nodeId = node->id();
    AstraSim::LoggerFactory::get_logger("workload::LocalMemUsageTracker")
        ->trace("tracker record start of node.id={} at tick={}", nodeId, tick);
    this->activityStartTime[nodeId] = tick;
}

void LocalMemUsageTracker::recordEnd(
    const std::shared_ptr<Chakra::ETFeederNode> node, Tick tick) {
    uint64_t nodeId = node->id();
    if (this->activityStartTime.find(nodeId) == this->activityStartTime.end()) {
        return;
    }
    Tick start = this->activityStartTime[nodeId];
    Tick end = tick;
    AstraSim::LoggerFactory::get_logger("workload::LocalMemUsageTracker")
        ->trace("tracker record end of node.id={} at start={} end={}", nodeId,
                start, end);
    this->recordReads(node, start, end);
    this->recordWrites(node, start, end);
    this->activityStartTime.erase(nodeId);
}

void LocalMemUsageTracker::buildMemoryTrace() {
    this->tensorMapId.clear();
    uint64_t idCnt = 0ul;
    for (const auto& item : tensorSize) {
        const TensorId& tensorName = item.first;
        uint64_t id = idCnt++;
        tensorMapId.insert(std::make_pair(tensorName, id));
    }
    this->serializedMemoryTrace.clear();
    for (const auto& item : tensorMapId) {
        const TensorId& tensorName = item.first;
        if (this->memReads.find(tensorName) == this->memReads.end()) {
            continue;
        }
        for (const auto& readActivity : this->memReads.at(tensorName)) {
            std::string nodeName = readActivity.nodeName;
            uint64_t nodeId = readActivity.nodeId;
            json objStart = {
                {"name", tensorName},
                {"cat", "tensorRead"},
                {"ph", "B"},
                {"ts", 1e-3 * readActivity.start},
                {"pid", this->sysId},
                {"tid", this->tensorMapId.at(tensorName)},
                {"args", json{{"size", this->tensorSize.at(tensorName)},
                              {"node_name", nodeName},
                              {"node_id", nodeId}}}};
            json objEnd = {
                {"name", tensorName},
                {"cat", "tensorRead"},
                {"ph", "E"},
                {"ts", 1e-3 * readActivity.end},
                {"pid", this->sysId},
                {"tid", this->tensorMapId.at(tensorName)},
                {"args", json{{"size", this->tensorSize.at(tensorName)},
                              {"node_name", nodeName},
                              {"node_id", nodeId}}}};
            this->serializedMemoryTrace.push_back(std::move(objStart));
            this->serializedMemoryTrace.push_back(std::move(objEnd));
        }
    }
    for (const auto& item : tensorMapId) {
        const TensorId& tensorName = item.first;
        auto writeActivity = this->memWrites.at(tensorName);
        std::string nodeName = writeActivity.nodeName;
        uint64_t nodeId = writeActivity.nodeId;
        json objStart = {
            {"name", tensorName},
            {"cat", "tensorWrite"},
            {"ph", "B"},
            {"ts", 1e-3 * writeActivity.start},
            {"pid", this->sysId},
            {"tid", this->tensorMapId.at(tensorName)},
            {"args", json{{"size", this->tensorSize.at(tensorName)},
                          {"node_name", nodeName},
                          {"node_id", nodeId}}}};
        json objEnd = {{"name", tensorName},
                       {"cat", "tensorWrite"},
                       {"ph", "E"},
                       {"ts", 1e-3 * writeActivity.end},
                       {"pid", this->sysId},
                       {"tid", this->tensorMapId.at(tensorName)},
                       {"args", json{{"size", this->tensorSize.at(tensorName)},
                                     {"node_name", nodeName},
                                     {"node_id", nodeId}}}};
        this->serializedMemoryTrace.push_back(std::move(objStart));
        this->serializedMemoryTrace.push_back(std::move(objEnd));
    }
}

void LocalMemUsageTracker::dumpMemoryTrace(const std::string& filename) {
    std::string local_mem_trace_filename =
        fmt::format(filename + ".{}.json", this->sysId);
    std::ofstream file(local_mem_trace_filename);
    if (!file.is_open()) {
        AstraSim::LoggerFactory::get_logger("workload::LocalMemUsageTracker")
            ->error("failed to open file {}", local_mem_trace_filename);
        return;
    }
    json trace;
    trace["traceEvents"] = this->serializedMemoryTrace;
    file << trace.dump(2);
    file.close();
}

void LocalMemUsageTracker::buildMemoryTimeline() {
    this->memoryContents.clear();
    this->memoryUsage.clear();
    std::set<Tick> ticks;
    std::map<Tick, std::vector<TensorId>> tensorWrites;
    std::map<Tick, std::vector<TensorId>> tensorLastReads;
    for (const auto& item : this->memWrites) {
        const TensorId& tensorName = item.first;
        const MemActivity& writeActivity = item.second;
        ticks.insert(writeActivity.start);
        tensorWrites[writeActivity.start].push_back(tensorName);
        json tensorWriteEvent = {
            {"name", tensorName},
            {"cat", "tensorLifetime"},
            {"ph", "B"},
            {"ts", 1e-3 * writeActivity.start},
            {"pid", this->sysId + 1000000ul},
            {"tid", this->tensorMapId.at(tensorName)},
            {"args", json{{"size", this->tensorSize.at(tensorName)}}}};
        this->serializedMemoryTrace.push_back(std::move(tensorWriteEvent));
    }
    for (const auto& item : this->memReads) {
        const TensorId& tensorName = item.first;
        const MemActivity& latestReadActivity = item.second.back();
        ticks.insert(latestReadActivity.end);
        tensorLastReads[latestReadActivity.end].push_back(tensorName);
        json tensorLastReadEvent = {
            {"name", tensorName},
            {"cat", "tensorLifetime"},
            {"ph", "E"},
            {"ts", 1e-3 * latestReadActivity.end},
            {"pid", this->sysId + 1000000ul},
            {"tid", this->tensorMapId.at(tensorName)},
            {"args", json{{"size", this->tensorSize.at(tensorName)}}}};
        this->serializedMemoryTrace.push_back(std::move(tensorLastReadEvent));
    }

    // Build the memory timeline as a counter event so that it appears as a line
    // chart.
    for (auto it = ticks.begin(); it != ticks.end(); it++) {
        if (it != ticks.begin()) {
            auto prevIt = std::prev(it);
            this->memoryContents.emplace(*it, this->memoryContents.at(*prevIt));
        } else {
            this->memoryContents.emplace(*it, std::unordered_set<TensorId>());
        }
        if (tensorWrites.find(*it) != tensorWrites.end()) {
            for (const auto& tensorName : tensorWrites.at(*it)) {
                this->memoryContents.at(*it).insert(tensorName);
            }
        }
        if (tensorLastReads.find(*it) != tensorLastReads.end()) {
            for (const auto& tensorName : tensorLastReads.at(*it)) {
                this->memoryContents.at(*it).erase(tensorName);
            }
        }

        uint64_t totalSizeBytes = 0ul;
        for (const auto& tensorName : this->memoryContents.at(*it)) {
            totalSizeBytes += this->tensorSize.at(tensorName);
        }

        // Convert bytes to megabytes (1 MB = 1024*1024 bytes)
        double totalSizeMB =
            static_cast<double>(totalSizeBytes) / (1024.0 * 1024.0);
        json memoryTimelineEvent = {{"name", "GPU Memory Usage (MB)"},
                                    {"cat", "GPU Memory"},
                                    {"ph", "C"},
                                    {"ts", 1e-3 * (*it)},
                                    {"pid", this->sysId + 2000000ul},
                                    {"args", json{{"Memory_MB", totalSizeMB}}}};
        this->serializedMemoryTrace.push_back(std::move(memoryTimelineEvent));
        this->memoryUsage.insert({*it, totalSizeBytes});
    }

    // Build the tensor lifetime heatmap after building the memory timeline
    this->buildTensorLifetimeHeatmap();
}

void LocalMemUsageTracker::buildTensorLifetimeHeatmap() {
    // Calculate lifetime for each tensor
    std::vector<std::tuple<TensorId, Tick, Tick, uint64_t>>
        tensorLifetimes;  // tensor, start, end, size

    for (const auto& item : this->memWrites) {
        const TensorId& tensorName = item.first;
        Tick start = item.second.start;
        Tick end;

        // Find the last read time
        if (this->memReads.find(tensorName) != this->memReads.end() &&
            !this->memReads.at(tensorName).empty()) {
            end = this->memReads.at(tensorName).back().end;
        } else {
            // No reads; tensor didn't end. Use simulation's last tick if
            // available
            if (!this->memoryUsage.empty()) {
                end = this->memoryUsage.rbegin()->first;
            } else {
                end = item.second.end;
            }
        }

        uint64_t size = this->tensorSize.at(tensorName);
        tensorLifetimes.emplace_back(tensorName, start, end, size);
    }

    // Sort tensors by lifetime duration (longest first)
    std::sort(tensorLifetimes.begin(), tensorLifetimes.end(),
              [](const auto& a, const auto& b) {
                  Tick durationA = std::get<2>(a) - std::get<1>(a);
                  Tick durationB = std::get<2>(b) - std::get<1>(b);
                  return durationA > durationB;  // Longest lifetime first
              });

    // Generate heatmap events for Perfetto
    const uint64_t heatmapProcessId =
        this->sysId +
        3000000ul;  // Use a different process ID for the heatmap view

    // Add a process name metadata event to label the heatmap view in Perfetto
    json processNameEvent = {{"name", "process_name"},
                             {"ph", "M"},  // Metadata event
                             {"pid", heatmapProcessId},
                             {"args", json{{"name", "Tensor Lifetime Heap"}}}};
    this->serializedMemoryTrace.push_back(std::move(processNameEvent));

    // Add thread name metadata for the legend/scale
    json threadNameEvent = {
        {"name", "thread_name"},
        {"ph", "M"},  // Metadata event
        {"pid", heatmapProcessId},
        {"tid", 0},
        {"args", json{{"name", "Longest Lifetime → Shortest Lifetime"}}}};
    this->serializedMemoryTrace.push_back(std::move(threadNameEvent));

    // Use all tensors instead of limiting to 100
    int count = static_cast<int>(tensorLifetimes.size());
    uint64_t minLifetime = std::numeric_limits<uint64_t>::max();
    uint64_t maxLifetime = 0;
    for (int i = 0; i < count; i++) {
        uint64_t duration =
            std::get<2>(tensorLifetimes[i]) - std::get<1>(tensorLifetimes[i]);
        if (duration < minLifetime) {
            minLifetime = duration;
        }
        if (duration > maxLifetime) {
            maxLifetime = duration;
        }
    }

    // Generate color gradient for size visualization
    auto getSizeColor = [](uint64_t size, uint64_t maxSize) -> std::string {
        // Simple heat gradient: small tensors are blue, large are red
        int intensity = std::min(
            255, static_cast<int>((static_cast<double>(size) / maxSize) * 255));
        char color[8];
        sprintf(color, "#%02X%02X%02X", intensity, 100, 255 - intensity);
        return std::string(color);
    };

    // Find maximum tensor size for color scaling
    uint64_t maxTensorSize = 0;
    for (const auto& item : this->tensorSize) {
        maxTensorSize = std::max(maxTensorSize, item.second);
    }

    // Generate the heatmap events using tensor lifetime to compute heap
    // position
    for (int i = 0; i < count; i++) {
        const auto& [tensorName, start, end, size] = tensorLifetimes[i];
        uint64_t duration = end - start;
        int heapPos = 0;
        if (maxLifetime != minLifetime) {
            heapPos =
                static_cast<int>((static_cast<double>(duration - minLifetime) /
                                  (maxLifetime - minLifetime)) *
                                 (count - 1));
        }
        std::string color = getSizeColor(size, maxTensorSize);
        double sizeMB = static_cast<double>(size) / (1024.0 * 1024.0);
        std::string displayName = tensorName;
        if (displayName.length() > 20) {
            displayName = displayName.substr(0, 17) + "...";
        }
        displayName += " (" + std::to_string(sizeMB).substr(0, 5) + " MB)";

        json heatmapEvent = {{"name", displayName},
                             {"cat", "tensorHeatmap"},
                             {"ph", "X"},
                             {"ts", 1e-3 * start},
                             {"dur", 1e-3 * duration},
                             {"pid", heatmapProcessId},
                             {"tid", heapPos},
                             {"cname", color},
                             {"args", json{{"tensor_name", tensorName},
                                           {"size_bytes", size},
                                           {"size_mb", sizeMB},
                                           {"lifetime_ns", duration},
                                           {"position", heapPos}}}};
        this->serializedMemoryTrace.push_back(std::move(heatmapEvent));
    }

    // Add additional metadata to describe the view
    json heatmapInfoEvent = {
        {"name", "Tensor Lifetime Heatmap"},
        {"cat", "tensorHeatmap"},
        {"ph", "i"},  // Instant event
        {"ts", 0},    // Start of trace
        {"pid", heatmapProcessId},
        {"s", "p"},  // Process scoped
        {"args",
         json{{"description",
               "Tensors arranged by lifetime duration (longest at bottom)"},
              {"total_tensors", tensorLifetimes.size()},
              {"displayed_tensors", count}}}};
    this->serializedMemoryTrace.push_back(std::move(heatmapInfoEvent));
}

uint64_t LocalMemUsageTracker::getPeakMemUsage() const {
    uint64_t peakMemUsage = 0ul;
    for (const auto& item : this->memoryUsage) {
        peakMemUsage = std::max(peakMemUsage, item.second);
    }
    return peakMemUsage;
}

std::tuple<float, std::string> LocalMemUsageTracker::getPeakMemUsageFormatted()
    const {
    uint64_t peakMemUsage = 0ul;
    for (const auto& item : this->memoryUsage) {
        peakMemUsage = std::max(peakMemUsage, item.second);
    }

    float value = static_cast<float>(peakMemUsage);
    std::string unit = "B";

    if (peakMemUsage < 1024ull) {
        value = static_cast<float>(peakMemUsage);
        unit = "B";
    } else if (peakMemUsage < 1024ull * 1024) {
        value = static_cast<float>(peakMemUsage) / 1024.0f;
        unit = "KB";
    } else if (peakMemUsage < 1024ull * 1024 * 1024) {
        value = static_cast<float>(peakMemUsage) / (1024.0f * 1024);
        unit = "MB";
    } else if (peakMemUsage < 1024ull * 1024 * 1024 * 1024) {
        value = static_cast<float>(peakMemUsage) / (1024.0f * 1024 * 1024);
        unit = "GB";
    } else {
        value =
            static_cast<float>(peakMemUsage) / (1024.0f * 1024 * 1024 * 1024);
        unit = "TB";
    }

    return std::make_tuple(value, unit);
}
LocalMemUsageTracker::~LocalMemUsageTracker() {
    this->memReads.clear();
    this->memWrites.clear();
    this->tensorSize.clear();
    this->activityStartTime.clear();
    this->serializedMemoryTrace.clear();
    this->memoryContents.clear();
    this->memoryUsage.clear();
    this->tensorMapId.clear();
}
