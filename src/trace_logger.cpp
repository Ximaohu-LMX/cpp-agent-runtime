#include "agent/trace_logger.hpp"

#include <chrono>
#include <fstream>
#include <iostream>

namespace agent {

TraceLogger::TraceLogger(std::string log_dir)
    : log_dir_(std::move(log_dir)) {}

void TraceLogger::logTrace(const nlohmann::json& trace) const {
    const auto now = std::chrono::system_clock::now();
    const auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();

    const std::string session_id = trace.value("session_id", "unknown_session");
    const std::string file_path =
        log_dir_ + "/trace_" + session_id + "_" + std::to_string(timestamp_ms) + ".json";

    std::ofstream output(file_path);
    if (!output.is_open()) {
        std::cerr << "failed to write trace file: " << file_path << std::endl;
        return;
    }

    output << trace.dump(2) << std::endl;
}

} // namespace agent
