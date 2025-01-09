#include <iostream>
#include <string>

#include "rocksdb/db.h"
#include "rocksdb/stats_collector.h"

#include <fstream>


#define LOG(msg) \
std::cout << __FILE__ << "(" << __LINE__ << "): " << msg << std::endl

namespace
ROCKSDB_NAMESPACE {
    std::string to_string(const StatsCollector::OpType op) {
        using OpType = StatsCollector::OpType;
        switch (op) {
            case OpType::kInsert: return "Insert";
            case OpType::kUpdate: return "Update";
            case OpType::kDeletePoint: return "PointDelete";
            case OpType::kDeleteRange: return "RangeDelete";
            case OpType::kQueryPoint: return "PointQuery";
            case OpType::kQueryRange: return "RangeQuery";
        }
        return {};
    }

    StatsCollector::OpType from_string(const std::string &op) {
        using OpType = StatsCollector::OpType;
        if (op == "Insert") return OpType::kInsert;
        if (op == "Update") return OpType::kUpdate;
        if (op == "PointDelete") return OpType::kDeletePoint;
        if (op == "RangeDelete") return OpType::kDeleteRange;
        if (op == "PointQuery") return OpType::kQueryPoint;
        if (op == "RangeQuery") return OpType::kQueryRange;
        throw std::invalid_argument("Invalid DBOperation string");
    }

    void to_json(json &j, const StatsCollector::OpResult &result) {
        j = json{
            {"operation", to_string(result.operation)},
            {"duration", result.duration}
        };
    }

    void from_json(const json &j, StatsCollector::OpResult &result) {
        result.operation = from_string(j.at("operation").get<std::string>());
        result.duration = j.at("duration").get<long>();
    }

    void to_json(json &j, const StatsCollector::MemtableSwitch &switchEvent) {
        j = json{{"memtable", switchEvent.memtable}};
    }

    void from_json(const json &j, StatsCollector::MemtableSwitch &switchEvent) {
        switchEvent.memtable = j.at("memtable").get<std::string>();
    }

    void to_json(json &j, const StatsCollector::BenchmarkEvent &event) {
        using OpResult = StatsCollector::OpResult;
        using MemtableSwitch = StatsCollector::MemtableSwitch;
        if (std::holds_alternative<OpResult>(event)) {
            j = json{{"type", "OpResult"}, {"data", std::get<OpResult>(event)}};
        } else if (std::holds_alternative<MemtableSwitch>(event)) {
            j = json{{"type", "DBMemtableSwitch"}, {"data", std::get<MemtableSwitch>(event)}};
        }
    }

    void from_json(const json &j, StatsCollector::BenchmarkEvent &event) {
        using OpResult = StatsCollector::OpResult;
        using MemtableSwitch = StatsCollector::MemtableSwitch;
        const auto &type = j.at("type").get<std::string>();
        if (type == "OpResult") {
            event = j.at("data").get<OpResult>();
        } else if (type == "MemtableSwitch") {
            event = j.at("data").get<MemtableSwitch>();
        } else {
            throw std::invalid_argument("Invalid BenchmarkEvent type");
        }
    }

    // void join_vector(std::stringstream &ss, const std::vector<long> &vec, const std::string &delimiter) {
    //     for (size_t i = 0; i < vec.size(); ++i) {
    //         if (i != 0) {
    //             ss << delimiter;
    //         }
    //         ss << vec[i];
    //     }
    // }

    double average(const std::vector<long> &vec) {
        if (vec.empty()) return 0.0;

        const long long sum = std::accumulate(std::begin(vec), std::end(vec), 0LL);

        return static_cast<double>(sum) / static_cast<double>(vec.size());
    }

    [[nodiscard]] size_t StatsCollector::size() {
        std::lock_guard guard(mutex_);
        return inserts_.size() + updates_.size() + point_deletes_.size() + range_deletes_.size() +
               point_queries_.size() + range_queries_.size();
    }

    std::string StatsCollector::stats_string() {
        const size_t total_items = size();

        std::stringstream ss;
        ss << std::fixed << std::setprecision(4);

        mutex_.lock();
        ss << static_cast<long double>(inserts_.size()) / total_items * 100.0 << ",";
        ss << static_cast<long double>(updates_.size()) / total_items * 100.0 << ",";
        ss << static_cast<long double>(point_deletes_.size()) / total_items * 100.0 << ",";
        ss << static_cast<long double>(range_deletes_.size()) / total_items * 100.0 << ",";
        ss << static_cast<long double>(point_queries_.size()) / total_items * 100.0 << ",";
        ss << static_cast<long double>(range_queries_.size()) / total_items * 100.0 << ";";

        LOG("sending avg latencies");

        ss << average(inserts_) << ":";
        ss << average(updates_) << ":";
        ss << average(point_deletes_) << ":";
        ss << average(range_deletes_) << ":";
        ss << average(point_queries_) << ":";
        ss << average(range_queries_);

        inserts_.clear();
        updates_.clear();
        point_deletes_.clear();
        range_deletes_.clear();
        point_queries_.clear();
        range_queries_.clear();

        mutex_.unlock();
        return ss.str();
    }


    void StatsCollector::start() {
        start_ = std::chrono::high_resolution_clock::now();
    }

    void StatsCollector::end(const OpType op_type) {
        const auto now = std::chrono::high_resolution_clock::now();
        const auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(now - start_).count();

        // Make sure to lock the mutex after measuring e2e time, so that lock acquisition isn't considered in the
        // duration.
        std::lock_guard guard(mutex_);

        benchmark_events_.emplace_back(OpResult{op_type, duration});

        switch (op_type) {
            case kInsert: {
                inserts_.push_back(duration);
                break;
            }
            case kUpdate: {
                updates_.push_back(duration);
                break;
            }
            case kDeletePoint: {
                point_deletes_.push_back(duration);
                break;
            }
            case kDeleteRange: {
                range_deletes_.push_back(duration);
                break;
            }
            case kQueryPoint: {
                point_queries_.push_back(duration);
                break;
            }
            case kQueryRange: {
                range_queries_.push_back(duration);
                break;
            }
            default: {
                LOG("ERROR: unhandled db operation in StatsCollector::end: " << op_type);
            }
        }
    }

    void StatsCollector::write_to_file(const std::filesystem::path &filepath) {
        std::ofstream output(filepath);
        if (!output) {
            std::cerr << "Error: Could not open file " << filepath << " for writing.\n";
        }
        json j = benchmark_events_;
        output << j.dump(4);
        output.close();
    }

    void StatsCollector::log_memtable_switch(const std::string &memtable) {
        mutex_.lock();
        benchmark_events_.emplace_back(MemtableSwitch{memtable});
        mutex_.unlock();
    }
} // namespace ROCKSDB_NAMESPACE
