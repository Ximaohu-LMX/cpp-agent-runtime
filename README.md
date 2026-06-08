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

### 三阶段：交互式推荐场景建模(已完成)

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

### 四阶段：推荐主链路（召回 → 合并 → 排序 → 重排 → 响应）(已完成)

目标：把「用户结构化偏好 → 多路召回 → 候选合并 → 排序 → 负反馈重排 → 响应组装」完整跑通，
所有工具直接以 DAG 节点的形式接入 Runtime，工具间通过 step 输出 / `*_from_step` 解耦。

> 设计取舍：原计划中的「四（召回）/ 五（排序重排）/ 六（推荐链路 DAG 化）」三个阶段
> 在工程实现上强耦合 —— 召回需要先有候选数据结构，排序需要先有候选，DAG 化需要前两者都存在 ——
> 因此合并为同一个大阶段，按子步骤推进。

典型执行图：

```text
structured_preference ── update_preference
                              │
                ┌─────────────┼─────────────┐
                ▼             ▼             ▼
          vector_recall  keyword_recall  hot_recall
                └─────────────┼─────────────┘
                              ▼
                       merge_candidates
                              ▼
                            rank
                              ▼
                           rerank
                              ▼
                       response_builder
```

参考 plan：`configs/sample_plan_4.json`。

#### 4.1 物料库与候选数据结构

- `agent::CatalogItem`（`include/agent/item_catalog.hpp`）：`id / title / tags / category / popularity`
- `getItemCatalog()` 返回进程内单例，作为三路召回共享的物料源（当前是 10 条硬编码内容，后续可替换为按需加载实现）
- 候选在召回环节产出，统一约定为：
  ```json
  {"id": "...", "title": "...", "tags": [...], "category": "...", "score": 0.x, "sources": ["vector"]}
  ```
- 标签 / 类别使用 `TagNormalizer` 归一化后的英文 ID，保证与 `PreferenceManager` 中的偏好可直接精确匹配；title 保留中文，供展示与关键词召回的子串匹配使用

#### 4.2 三路召回

| 工具 | 主要信号 | 打分逻辑 |
|---|---|---|
| `VectorRecallTool` | 偏好语义（伪向量相似度） | `tag_score * 0.7 + category_score * 0.3`，`score <= 0` 不进结果 |
| `KeywordRecallTool` | `last_query` + 偏好标签 | 从 query 抽取 ASCII token（如 "C++"、"Redis"）→ title 子串命中 + 标签精确命中，每命中 +0.3，封顶 1.0 |
| `HotRecallTool` | popularity + 偏好类别 | `popularity * (in_preferred_category ? 1.0 : 0.5)`，按 `top_n` 截断 |

三路均只依赖 `PreferenceManager`（单一事实源）与 `getItemCatalog()`，无横向依赖，可天然并行。

#### 4.3 候选合并 —— `MergeCandidatesTool`

- 通过 `input.candidates_from_steps`（数组）从 `SessionManager` 读取每路召回的输出
- 按 `id` 去重：分数取 `max(score)`，`sources` 取并集
- 输出按 score 降序的统一候选数组，并附 `merged_count`

#### 4.4 排序 —— `RankTool`

在召回基础分上叠加偏好特征：

```text
rank_score = base_score
           + 0.3 * (positive_tag 命中数)
           + 0.2 * (preferred_category 命中)
           + 0.1 * (sources 数 - 1)   // 多路共同召回的奖励
```

#### 4.5 重排 —— `RerankTool`（负反馈）

当前实现采用**硬过滤**：

- 候选 `category` 命中 `rejected_categories` → 直接丢弃
- 候选任一 `tag` 命中 `negative_tags` → 直接丢弃
- 输出 `candidates`（保留集）+ `dropped`（含丢弃原因），方便 trace 与评测

> 选择硬过滤的原因：与三阶段「负反馈覆盖正反馈」的语义保持一致，
> 也契合「不要 X」这种明确否定的用户指令。
> 若后续要支持「少看一些 X 但仍可出现」，可在此基础上扩展为权重衰减模式。

#### 4.6 响应组装 —— `ResponseBuilderTool`

- 按 `top_n` 截断
- 产出 `items`（id / title / score / sources）+ 中文 `summary` 文案
- 推荐链路的最后一跳，结果可直接交给上层 LLM / UI

#### 4.7 端到端 DAG 串联

- 全链路通过 `depends_on` + `*_from_step` 字段在 DAG 中显式连接，工具间不依赖隐式调用顺序
- `vector_recall / keyword_recall / hot_recall` 在依赖图上同层，已具备并行执行的拓扑条件（实际并行执行留到五阶段）
- `configs/sample_plan_4.json` 是完整可跑的 9-step plan，覆盖从结构化偏好到最终响应

------

### 五阶段：同层并行执行

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

### 六阶段：Trace / Metrics 完善

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

### 七阶段：EvalRunner 自动化评测

目标：基于推荐主链路构建 EvalRunner，补全用例和报告生成。
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

### 八阶段：工程增强

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

