cmake_minimum_required(VERSION 3.20)

project(cpp_agent_runtime LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_TESTS "Enable unit tests" ON)

include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.17.0
)
FetchContent_MakeAvailable(spdlog)

add_library(agent_runtime
    src/plan_parser.cpp
    src/tool_registry.cpp
    src/tool_executor.cpp
    src/session_manager.cpp
    src/trace_logger.cpp
    src/metrics_collector.cpp
    src/tools/calculator_tool.cpp
    src/tools/memory_tool.cpp
)

target_include_directories(agent_runtime
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(agent_runtime
    PUBLIC
        nlohmann_json::nlohmann_json
        spdlog::spdlog
)

add_executable(agent_runtime_app
    src/main.cpp
)

target_link_libraries(agent_runtime_app
    PRIVATE
        agent_runtime
)

if (ENABLE_TESTS)
    enable_testing()

    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.16.0.zip
    )

    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(googletest)

    add_executable(agent_runtime_tests
        tests/test_runtime.cpp
    )

    target_link_libraries(agent_runtime_tests
        PRIVATE
            agent_runtime
            GTest::gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(agent_runtime_tests)
endif()
