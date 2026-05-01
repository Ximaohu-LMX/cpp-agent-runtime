# C++ Agent Runtime

MVP version of a lightweight C++ Agent Runtime.

## Features

- JSON plan parsing
- Tool abstraction
- Tool registry
- Sequential tool execution
- Session state management
- Trace logging
- Basic metrics collection
- CalculatorTool
- MemoryTool

## Build

```bash
cmake -S . -B cmake-build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-debug
Run
./cmake-build-debug/agent_runtime_app configs/sample_plan.json
Test
ctest --test-dir cmake-build-debug --output-on-failure

