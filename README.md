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
```

## 项目演进过程

### 一阶段：核心闭环版本（已完成）
实现顺序执行的 Agent Runtime，支持 Plan 解析、工具注册与调用、Session 状态管理、Trace 记录与 Metrics 统计，完成从任务输入到结果输出的完整执行闭环。

### 二阶段：DAG 执行与分层调度（已完成）
引入 depends_on 显式依赖建模，实现依赖校验（重复 ID、缺失依赖、循环依赖），基于依赖关系进行拓扑排序，将任务划分为多层（Layer），从线性执行升级为"按层执行"的 DAG 调度模型，为并行执行提供结构基础。

### 三阶段：并行执行（规划中）
支持同一 Layer 内任务并行执行，引入线程池或异步执行机制，适配多路召回等推荐系统场景，优化整体延迟（降低 P95 / P99）。

### 四阶段：执行语义优化（规划中）
重构数据传递方式（如从 _last_output 升级为按 step_id 存储），明确并发语义，避免状态冲突，提升系统一致性与可扩展性。

### 五阶段：性能与稳定性增强（规划中）
引入超时控制（强制中断）、重试与 fallback 策略优化，增加缓存（LRU / TTL）、异步日志，完善 Metrics（P50/P95/P99）与压测能力。

### 六阶段：推荐系统场景扩展（规划中）
扩展 RetrievalTool、RankTool 等组件，构建“多路召回 + 排序 + 重排”的完整推荐链路，支持结果合并与降级策略。