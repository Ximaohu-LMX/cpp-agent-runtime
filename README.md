# **C++ Agent Runtime 与交互式搜索重排系统**

面向“用户通过 AI 助手表达模糊需求、多轮反馈推荐结果”的场景，实现一个支持工具调用、会话状态、检索召回、负反馈重排和效果评测的工程化系统。


## Build

```bash
cmake -S . -B cmake-build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-debug
Run
./cmake-build-debug/agent_runtime_app configs/sample_plan.json
Test
ctest --test-dir cmake-build-debug --output-on-failure
```

## 项目思路

### 一阶段：Agent Runtime 核心闭环（已完成）

目标：先让 Agent 执行框架跑通。

已实现：

```text
PlanParser
Tool 抽象接口
ToolRegistry
ToolExecutor
SessionManager
TraceLogger
MetricsCollector
CalculatorTool
MemoryTool
EchoTool
```

能力：

```text
JSON Plan 解析
工具注册与调用
顺序执行
Session 状态保存
Trace / Metrics 输出
```

------

### 二阶段：DAG 依赖执行基础（已完成）

目标：让任务计划不再只是线性列表，而是可以表达依赖关系。

需要实现：

```text
depends_on 字段
依赖合法性校验
重复 step_id 检查
缺失依赖检查
循环依赖检查
拓扑分层调度
```

当前意义：

```text
为后续多工具组合、推荐链路、多路召回并行打基础
```

------

### 三阶段：交互式推荐场景建模

目标：从“通用 Agent Runtime”进入“AI 助手推荐系统”业务场景。

需要实现：

```text
UserQueryTool / QueryParser
PreferenceManager
FeedbackParser
SessionPreference
```

核心能力：

```text
解析用户模糊需求
保存用户偏好
识别正反馈 / 负反馈
将多轮对话状态转成推荐约束
```

示例：

```text
用户：我想看一些偏 C++ 工程的视频
系统：推荐 C++ 后端、数据库、Agent Runtime 相关内容

用户：不要课程项目，想看真实工程一点的
系统：降低“课程项目”权重，提高“工程实践 / 系统设计”权重
```

------

### 四阶段：检索召回工具链

目标：让系统真的能“找内容”。

需要实现：

```text
RetrievalTool
KeywordSearchTool
TagRecallTool
HotRecallTool
Candidate 数据结构
TopK 召回
```

推荐先做本地模拟数据，不急着接真实向量库。

内容数据可以先是：

```text
video_id
title
tags
category
description
score
```

这一阶段重点不是算法多强，而是打通：

```text
用户需求 -> 多路召回 -> 候选结果
```

------

### 五阶段：排序与负反馈重排

目标：体现推荐系统核心价值。

需要实现：

```text
RankTool
RerankTool
Feedback-aware scoring
负反馈降权
正反馈加权
TopK 截断
```

核心逻辑：

```text
用户喜欢的标签 / 类别：加权
用户否定的标签 / 类别：降权
与当前 query 更相关的内容：加权
重复或低质量内容：降权
```


------

### 六阶段：推荐链路 DAG 化

目标：把推荐系统链路放进 DAG Runtime。

典型执行图：

```text
parse_query
   ↓
vector_recall ─┐
keyword_recall ├── merge_candidates ── rank ── rerank ── response
hot_recall    ─┘
```

需要实现：

```text
MergeCandidatesTool
ResponseBuilderTool
按 step_id 保存输出
value_from_step / input_from_step
```


------

### 七阶段：同层并行执行

目标：优化多路召回延迟。

需要实现：

```text
同一 Layer 内并行执行
std::async 或线程池
并发安全的 SessionManager
并发安全的 Trace / Metrics
失败策略
超时策略
```

推荐场景里非常适合：

```text
vector_recall
keyword_recall
hot_recall
```

这三个可以并行。

价值：

```text
顺序耗时 = A + B + C
并行耗时 ≈ max(A, B, C)
```

------

### 八阶段：Trace / Metrics 完善

目标：让系统可观测、可分析。

```text
P50 / P95 / P99
每个工具耗时
每个推荐阶段耗时
错误率
fallback 次数
重排前后 TopK 对比
负反馈生效率
```

这阶段服务两个方向：

```text
Agent 工程化
推荐效果验证
```

------

### 九阶段：EvalRunner 自动化评测

目标：证明系统有效，不只是 Demo。
```text
eval_cases.json
EvalRunner
批量执行推荐任务
report.md 自动生成
```

评测指标：

```text
任务成功率
多轮状态保持率
负反馈命中率
目标类别提升率
被否定类别下降率
TopK 变化
平均延迟 / P95 / P99
```

------

### 十阶段：工程增强

可以补：
```text
线程池
真正 timeout 中断
LRU / TTL Cache
异步日志
配置文件 YAML
更完整错误码
单元测试 / 集成测试
benchmark 脚本
```

