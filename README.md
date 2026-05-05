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
（思路更新：不再重点做自然语言关键词解析，默认LLM传回结构化信息，然后将其转成推荐系统可用的标准化偏好。）

去掉：
```text
QueryParserTool
FeedbackParserTool 的自然语言关键词解析
```
需要实现：

```text
StructuredPreferenceTool
PreferenceUpdateTool
PreferenceManager
标签标准化 / 校验
```
模块职责：
```text
StructuredPreferenceTool：接收 LLM 输出的结构化偏好
PreferenceUpdateTool：将偏好增量写入 PreferenceManager
PreferenceManager：负责合并、去重、负反馈覆盖正反馈
TagNormalizer：负责中文 / 别名标签到标准标签的映射
```

核心能力：

```text
接收结构化用户需求 / 反馈
标准化标签与类别
保存多轮用户偏好
处理正反馈 / 负反馈
将多轮对话状态转成推荐约束
```

示例：

```text
用户：我想看一些偏 C++ 工程的视频
系统：推荐 C++ 后端、数据库、Agent Runtime 相关内容

用户：不要课程项目，想看真实工程一点的
系统：降低“课程项目”权重，提高“工程实践 / 系统设计”权重
```
对应plan：
```json
{
  "id": "update_preference",
  "tool": "preference_update",
  "input": {
    "preference_delta": {
      "positive_tags": ["工程实践", "C++后端"],
      "negative_tags": ["课程"]
    }
  }
}
```
------
### 原六阶段提前：DAG 数据传递与推荐链路骨架
目标：在进入召回之前，先把工具间数据流打通，让后续每个工具天然跑在 DAG 里，避免返工。

需要实现：
```text
按 step_id 保存输出
value_from_step / input_from_step
MergeCandidatesTool（空壳，先定义接口）
ResponseBuilderTool（空壳，先定义接口）
```

典型执行图（此时工具内部可以是 mock）：
```text
structured_preference
      ↓
vector_recall ─┐
keyword_recall ├── merge_candidates ── rank ── rerank ── response
hot_recall    ─┘
```
后续四、五阶段的工具直接在 DAG 中开发，不需要临时串联方案
MergeCandidatesTool / ResponseBuilderTool 先占位，后续填实现即可

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

### 六阶段：推荐链路 DAG 化（已提前）

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
PreferenceManager 锁粒度优化
失败策略
超时策略
```

（补充）PreferenceManager 锁优化说明：
```text
现状：单个 std::mutex 保护整个 preferences_ 哈希表，所有读写串行。
观察：同一 session 不会并发（DAG 中同 session 的步骤不会同 layer 写偏好），
      真实竞争只发生在不同 session 同时操作 preferences_ 哈希表。

优化方向：
  1. 存储改为 unordered_map<string, unique_ptr<Preference>>，
     使 Preference 指针在 rehash 时保持稳定
  2. 外层用 std::shared_mutex 仅保护哈希表本身：
       - find 走 shared lock
       - 新 session 的 insert 走 exclusive lock
  3. 拿到 Preference* 后释放外层锁，直接读写；
     因同 session 不并发，Preference 内部无需再加锁

效果：写路径除首次 insert 外几乎无锁，跨 session 读写完全并行。
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

目标：基于五阶段搭建的 EvalRunner 骨架，补全用例和报告生成。
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

