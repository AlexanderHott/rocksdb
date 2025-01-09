#pragma once

#include <mutex>
#include <string>
#include <variant>
#include <vector>

#include "rocksdb/rocksdb_namespace.h"
#include "rocksdb/utilities/json.hpp"

using json = nlohmann::json;

namespace
ROCKSDB_NAMESPACE {
    class StatsCollector {
    public:
        enum OpType {
            kInsert = 0,
            kUpdate,
            kDeletePoint,
            kDeleteRange,
            kQueryPoint,
            kQueryRange,
        };

        struct OpResult {
            OpType operation;
            long duration;
        };

        struct MemtableSwitch {
            std::string memtable;
        };

        using BenchmarkEvent = std::variant<OpResult, MemtableSwitch>;

        [[nodiscard]] size_t size();

        [[nodiscard]] std::string stats_string();

        void start();

        void end(OpType op_type);

        void log_memtable_switch(const std::string &memtable);

        void write_to_file(const std::filesystem::path& filepath);

    private:
        std::mutex mutex_;
        std::chrono::time_point<std::chrono::high_resolution_clock> start_;
        std::vector<long> inserts_;
        std::vector<long> updates_;
        std::vector<long> point_deletes_;
        std::vector<long> range_deletes_;
        std::vector<long> point_queries_;
        std::vector<long> range_queries_;
        std::vector<BenchmarkEvent> benchmark_events_;
    };
} // namespace ROCKSDB_NAMESPACE
