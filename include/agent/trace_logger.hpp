#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace agent {

class TraceLogger {
public:
    explicit TraceLogger(std::string log_dir);

    void logTrace(const nlohmann::json& trace) const;

private:
    std::string log_dir_;
};

} // namespace agent
