关于标题：
一级标题：# 一、; # 二、; 依次往后，markdown一级标题，中文数字，依次往后
二级标题：## 1.; ## 2.; markdown2级标题，依次往后，每个一级标题下都从1重新开始，阿拉伯数字，依次往后。
三级标题：### 1.1; ### 1.2; markdown3级标题，第一个阿拉伯数字取决于二级标题，第二个阿拉伯数字从每一个二级标题下从1重新开始，依次往后。
四级标题：#### (1); #### (2); markdown4级标题，从每一个三级标题下都从（1）开始，依次往后。

生成目标：
本文档不是普通交付备忘录，而是“实现完成后的工程学习总结”。生成 `doc/[feature].html` 时，既要记录做了什么，也要帮助读者学习工程、渲染、引擎和图形 API 知识。

当前架构事实（生成后续文档时保持一致）：
- Feature 020 建立 Asset -> Core 的 CPU-only identity/registry/extension 层。
- Feature 021 在 Asset 内建立 Image/Texture CPU payload、source import 和 semantic mip；Renderer 是 Asset-to-RHI texture realization 的唯一适配层；RHI 只定义 portable format/upload contract；Vulkan Backend 独占 native image、memory、staging、barrier、fence 与 readback。
- Feature 022 在 Asset 内建立 immutable KTX2 artifact、确定性 ETC1S/UASTC cook、bounded inspect/load 和 request-scoped transcode；Renderer 独占 target profile selection 与 Asset-to-RHI compressed texture realization；RHI 是 block geometry、footprint 和 per-format usage capability 的唯一 portable source of truth；Vulkan Backend 负责查询真实 format feature，并独占 compressed image、upload 与 readback。
- KTX-Software、Basis encoder 和 WAMR 都是 Asset-private implementation dependency。Authoritative encoder 是仓库内固定版本的 WebAssembly module；host 侧不得用 native encoder 输出替代 canonical bytes。独立 Khronos validator 仍启用 warnings-as-errors，只允许完整且无重复的九个 `stoner.*` metadata key 对应 diagnostic 7010，其他 warning/error 一律失败。
- Asset production code不得依赖 RHI、Renderer、Application、Backend、Tools、图形 API 或第三方 decoder public header。RGB/RG GPU format expansion 属于 Renderer，不得回流到 Asset payload。

总体写作要求：
- 使用中文叙述，允许保留必要英文术语，例如 Render Graph、RHI、pipeline barrier、descriptor set。
- 每章都要结合本次实际实现、实际源码、实际测试和实际取舍，避免空泛套话。
- 优先解释“为什么这样设计、解决了什么问题、如果不这样会出什么 bug”，不要只罗列“实现了什么”。
- 对 Core/RHI/Renderer/Backend/Vulkan/图形 API 相关主题，必须加入学习导向解释和跨层关系说明。
- 每个重要概念尽量给出对应代码位置、测试位置或调试入口。
- 不要粘贴大段源码；可以用短伪代码、流程图、状态机、表格和关键 API 名称辅助说明。
- 文档应当足够自包含：半年后只读本文，也能回忆本阶段的工程目标、设计思路、关键知识点和后续连接点。

# 一、文档与上下文信息
这个阶段用于快速对齐认知，并为未来 AI 对话提供高质量上下文。这里的一段话总结应当可以直接作为后续 prompt 的背景摘要。

## 1. 基础元数据
明确文档版本、更新日期、当前状态（设计中/开发中/已交付）、对应 feature/spec 目录、实现分支、核心代码目录、测试文件、验证命令和负责人。

建议包含：
- Feature 编号和名称
- Roadmap phase 编号和名称（如果与 feature 编号不同，必须说明原因）
- Spec/plan/tasks 路径
- 主要源码路径和测试路径
- 最终验证命令和结果摘要

## 2. 本期核心摘要
用一到两段说明本次子系统/阶段核心解决的问题、最大设计改动和学习价值。

要求：
- 第一段面向工程交付：说明“本期交付了什么能力”。
- 第二段面向学习：说明“本期最值得学习的引擎/渲染/API/工程概念是什么”。
- 避免只写功能名，要写清楚这些能力在引擎路线中的位置。

## 3. 上下文与依赖
列出前置文档、前置模块、强依赖子系统和外部组件，说明影响范围。

必须回答：
- 本期建立在之前哪些 feature 之上？
- 本期为之后哪些 feature 铺路？
- 本期新增或维持了哪些层级边界？
- 哪些依赖是编译期依赖，哪些只是概念或测试依赖？

# 二、目标与边界
这是防止系统走偏的关键。“不做什么”要和“做什么”一样明确。

## 1. 业务与功能目标
说明开发者或引擎调用方现在能完成什么工作。

建议按用户故事或工作流写：
- 调用方是谁：引擎开发者、Renderer、Backend、Application、测试替身等。
- 输入是什么：描述对象、句柄、资源、shader、pass、command、窗口等。
- 输出是什么：对象、状态、诊断、schedule、debug dump、测试结果等。
- 成功和失败如何被表达。

## 2. 架构目标与定位
说明该子系统在全局架构中的角色，并用简单图标出它与 Core、RHI、Renderer、Backend、Application 的关系。

必须说明：
- 本模块向上服务谁，向下依赖谁。
- 哪些概念属于 public contract，哪些属于 private implementation。
- 本期是否引入具体图形 API；如果没有，说明如何保持多 API 能力。
- 建议用 Mermaid、ASCII block graph 或表格呈现跨层关系。

## 3. 边界与非目标（约束）
明确本阶段暂时不推进的功能、不处理的性能问题、不引入的依赖和不暴露的 API。

要求：
- 每个非目标都要写一句理由：为什么现在不做。
- 对渲染/图形主题，要说明哪些工业级能力被推迟，例如真实内存 aliasing、async compute、shader reflection、pipeline disk cache、presentation、material system 等。
- 明确这些非目标未来可能在哪个阶段接回。

# 三、交互与产品设计
这里把技术语言翻译成“使用方式”和“规则”。即使是底层库，也要说明开发者如何使用它。

## 1. 用户使用路径
写出一条代表性 happy path，以及至少一条失败路径或调试路径。

建议包含：
- API 调用顺序
- 关键对象生命周期
- 需要调用方提供的输入
- 调用方能观察到的输出、诊断或 dump
- 对应测试用例或 quickstart 场景

## 2. 核心规则与状态机
列出关键状态、状态转换、非法操作和错误返回。

要求：
- 状态机必须来自实际实现或 spec，不要凭空添加。
- 对每个状态说明“为什么需要这个状态”。
- 对生命周期错误、资源失效、重复调用、缺失依赖等负路径给出可观察行为。

## 3. 失败模式与调试入口
记录本阶段最容易踩的错误和排查入口。

建议包含：
- 如果调用顺序错了，会返回什么结果或诊断？
- 如果资源/句柄/描述符不兼容，会在哪里失败？
- 应该看哪个 log、diagnostic、debug dump、测试文件或断言？
- 这些失败模式对应真实引擎或图形 API 中的什么常见问题？

# 四、技术架构与关键决策
核心原则是“细节但不那么细”：讲清楚关系、机制和取舍，不贴大段代码。

## 1. 技术栈与基础设施
列出主要语言、标准、构建系统、项目内基础设施、外部 SDK 或 fallback 策略。

必须说明：
- 使用的 C++ 标准和项目约定。
- 构建与测试入口。
- 是否依赖 Vulkan SDK、平台 API 或仅使用抽象 contract。
- SDK 缺失、runtime 缺失、headless 测试等环境差异如何处理。

## 2. 关键信息
根据本次实现设计小节，不要机械套用。

### 2.1 模块对应关系
用表格列出关键类/文件/模块的职责。

每行建议包含：
- 模块或类型名
- 所在文件
- 拥有什么状态
- 对外提供什么能力
- 与其他模块的关系

### 2.2 数据流、控制流或资源流
描述本期最重要的一条流。

渲染/引擎主题优先选择：
- 资源创建与失效流
- command recording/submission 流
- shader/pipeline 创建流
- render graph declare/compile/execute 流
- descriptor/binding 流
- barrier/transition 流

要求说明每一步由哪个模块负责，以及上一步的输出如何成为下一步的输入。

### 2.3 关键日志、诊断与可视化
说明核心节点的日志、diagnostic、debug dump、测试输出或可视化规则。

必须说明：
- 诊断记录什么，不记录什么。
- 诊断与 result code/assertion 的分工。
- 哪些输出要求 deterministic，为什么。

## 3. 架构决策记录 (ADR)
记录开发过程中遇到的关键技术难点，以及最终方案、放弃方案和选择理由。

每个 ADR 必须包含：
- 决策：最终采用什么。
- 背景：为什么这个问题出现。
- 理由：为什么当前方案适合本项目和本阶段。
- 被否决方案：至少一个替代方案，以及为什么放弃。
- 后果：这个决策带来的收益、限制和未来迁移成本。

优先记录能帮助学习的取舍，例如：
- 为什么抽象在 Renderer 而不是 RHI/Backend。
- 为什么先 contract/mock 后真实 backend。
- 为什么选择显式状态而不是异常。
- 为什么先 deterministic fallback 后真实 runtime。
- 为什么只做 eligibility 不做真实优化。

# 五、实施与验证（任务流）
这里负责将设计落地，和静态技术设计分开。

## 1. 任务拆解清单
将实现拆成阶段，而不是逐条复制 tasks.md。

建议包含：
- Setup/Foundation
- Contract/API
- Core implementation
- Backend integration 或 Renderer integration
- Diagnostics/debug tooling
- Tests and validation
- Documentation/roadmap update

每个阶段说明关键交付物和对应代码范围。

## 2. 验收标准
说明如何证明这个子系统开发完成。

必须包含：
- 构建命令和结果
- 测试命令和结果
- 关键成功场景
- 关键失败场景
- 边界/依赖隔离检查，例如 public header 不泄漏 backend/API 头
- 如果有性能或确定性目标，写出实际验证方式

## 3. 测试矩阵与覆盖意图
不要只写“测试通过”。要解释每类测试为什么存在。

建议表格列：
- 测试类别
- 覆盖的风险
- 对应文件或用例
- 证明了什么

# 六、设计巧思与最佳实践
本章是学习价值最高的章节之一。重点解释“为什么这样设计”，不是重复模块职责。

## 1. 当前业务场景与业务模型的最佳实践
围绕本期最重要的 3 到 7 个设计点展开。

每个设计点建议按以下格式写：
- 设计点：例如 stable topological sort、explicit result、TryInverse、pipeline layout compatibility、transient resource lifetime。
- 解决的问题：它避免什么混乱、bug、性能浪费或耦合。
- 本项目做法：具体到类、文件或测试。
- 如果不这样做：会出现什么失败模式。
- 与真实引擎的关系：类似 UE、FrameGraph、RHI、Vulkan/DX12/Metal 中的哪个概念。

## 2. 优化方案与收益
说明当前方案有哪些优化行为，在哪些场景获得收益。

要求：
- 区分“已实现优化”和“未来优化铺垫”。
- 对每个优化点说明收益指标：确定性、调试性、编译时间、运行时开销、内存峰值、API 稳定性、跨平台性等。
- 对渲染主题，要说明优化如何映射到 GPU 资源、同步、pipeline、descriptor、barrier 或 frame scheduling。

## 3. 已知限制与后续演进
记录当前方案的边界和下一步自然演进。

必须说明：
- 哪些能力是故意不做。
- 当前设计未来如何扩展。
- 哪些测试会成为未来重构的回归基线。
- 哪些概念还需要继续学习。

# 七、编程语言知识点
本章聚焦于“用了什么语言工具”。要帮助读者读懂代码中的 C++ 写法，而不是泛泛罗列术语。

## 1. 标准库接口与用法
列出本阶段用到的关键 std 接口，简述含义、典型签名和本项目中的具体用法。

每项建议包含：
- 接口名
- 它解决什么 C++ 层问题
- 本项目在哪里使用
- 使用时要注意什么

示例：`std::move`、`std::unique_ptr`、`std::shared_ptr`、`std::weak_ptr`、`std::ostringstream`、`std::chrono`、`std::atomic`、`std::filesystem`、`std::optional`、`std::numeric_limits`、`<cmath>` 函数等。

## 2. 语言特性与惯用法
列出本阶段用到的 C++ 语言特性，说明作用和应用场景。

建议覆盖：
- RAII 和生命周期所有权
- 移动语义
- `enum class`
- `constexpr`
- `[[nodiscard]]`
- 模板或类型别名
- friend class 或封装边界
- 宏与条件编译
- out 参数、Result + value 包装、null object、安全默认值

每项都要结合实际代码，不要写教材式定义。

## 3. 平台相关与编译器行为
如果涉及平台差异、编译器差异或 SDK/fallback 行为，在此记录。

建议说明：
- MSVC/Clang/GCC 行为差异
- Windows/macOS/Linux 差异
- Vulkan SDK/runtime 缺失时的行为
- 浮点、对齐、动态库、文件系统、线程、时间等跨平台注意事项
- 当前测试如何保证差异不会泄漏到 public contract

# 八、软件工程知识点
本章聚焦于跨语言通用的软件工程原则和方法论。与第六章的区别：第六章讲具体设计理由，本章讲抽象工程原则如何落地。

## 1. 设计模式与架构模式
列出本阶段体现的设计模式或架构模式，并说明在哪里使用、解决什么问题、代价是什么。

示例：
- Facade
- Builder
- Strategy
- Adapter
- Observer/Sink
- Repository/Cache
- Value Object
- State Machine
- Contract Test
- Compiler pipeline
- Dependency Inversion

每项必须对应实际模块，避免只是贴标签。

## 2. 工程原则与方法论
列出本阶段体现的工程原则，并说明如何体现。

建议覆盖：
- 分层架构
- 依赖倒置
- 接口隔离
- YAGNI
- DRY
- 防御性编程
- 契约式设计
- 确定性行为
- 可测试设计
- 稳定接口优先

每项说明“本项目中哪里体现”和“它避免了什么长期风险”。

## 3. 测试策略与质量保障
说明本阶段测试方法论，以及如何保障代码质量。

建议覆盖：
- happy path / negative path / boundary testing
- lifecycle matrix
- equivalence partitioning
- regression baseline
- deterministic output testing
- mock/contract test 与真实 backend integration test 的分工
- headless testing
- dependency boundary grep 或 include isolation

# 九、渲染、引擎与图形 API 学习笔记
本章是专门为学习图形工程加入的章节。即使本期不是直接 Vulkan 阶段，也要说明它如何服务未来渲染系统。

## 1. 本期核心概念拆解
选择 3 到 7 个最值得学习的概念。每个概念按以下格式写：
- 是什么：用通俗但准确的语言解释。
- 为什么引擎需要它：它解决什么规模化问题。
- 本项目如何表达：对应类型、函数、测试或文档。
- 真实图形 API/引擎中的对应物：例如 Vulkan image layout、pipeline barrier、descriptor set、command buffer、render pass、framebuffer、swapchain、pipeline layout、UE RDG 等。
- 常见误区：学习或实现时容易混淆什么。

## 2. 与 Vulkan / DX12 / Metal / OpenGL 的关系
如果本期涉及 RHI、Backend、Renderer、资源、同步、pipeline、shader、render pass、framebuffer、swapchain、command、barrier 或 Render Graph，必须写跨 API 对照。

建议用表格：
- 本项目概念
- Vulkan 对应概念
- DX12 对应概念
- Metal/OpenGL 对应概念
- 本期是否真实实现
- 未来阶段如何深化

如果某些 API 没有一一对应，也要说明“为什么没有直接对应”。

## 3. 源码阅读路线
给出建议阅读顺序，让读者能从入口读到核心实现再读测试。

建议包含：
- 入口头文件
- 核心 public contract
- private implementation
- diagnostics/debug dump
- test file
- build file

每个文件用一句话说明阅读重点。

## 4. 自测问题
给出 5 到 10 个问题，帮助读者确认是否真的理解本期内容。

问题应当偏推理，不要只问定义。例如：
- 为什么这个状态必须显式建模？
- 如果 public header 暴露 backend 类型，会破坏什么？
- 为什么这类优化先记录 metadata 而不立刻执行？
- 为什么测试要验证重复运行的确定性？
- 这个概念映射到 Vulkan 时会遇到什么额外约束？
