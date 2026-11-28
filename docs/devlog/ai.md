# AI 对话与 Provider 故障边界

每条经验都链接当前源码、测试和修复提交。先按症状查标题，再检查对应边界。配置说明见 [SETTINGS](../SETTINGS.md)。

## 工具调用后模型不续答，或工具轮次被过早截断

模型返回工具调用后，如果只收到工具结果而没有后续答复，或长链路中途结束，先检查 Agent 的工具循环。根因是控制流把一次工具批次或响应结束当作整轮完成。每次 provider 请求都应获得配置的输出额度，工具轮数则单独控制。检查 `Agent.cpp` 中 provider response → tool dispatch → 下一次 request 的流程，以及测试中的请求次数和 `maxOutputTokens`。修复应完整处理 provider continuation 和工具往返，并为每次请求应用额度。回归见 [`tests/completion/agent.cpp`](../../tests/completion/agent.cpp) 中 “agent completes long tool sequences…” 和 “agent grants the configured output allowance…”。修复提交：[90d0c9c9](https://github.com/water2004/Folia/commit/90d0c9c9)、[944db7ea](https://github.com/water2004/Folia/commit/944db7ea)。

## 搜索词含反斜杠时被解释成控制字符，工具摘录显示转义文本

搜索反斜杠、代码片段或 Windows 路径时，如果模型看到的是 `\\` 等 JSON 转义文本，先检查 `SearchQuery` 和 `ToolOutput` 的 JSON 到文本转换，以及各 adapter 收到的结果。根因是把结构化 JSON 直接用作模型可读的工具回执。协议编码应保留调用 ID 和结构，展示给模型的摘录则要逐字符保留原文。回归见 [`tests/completion/agent.cpp`](../../tests/completion/agent.cpp) 中 “search distinguishes a literal backslash…” 和 “all model protocols receive literal excerpts…”。修复提交：[66f30658](https://github.com/water2004/Folia/commit/66f30658)。

## 长目录或历史检索只返回第一页，模型看不到后续结果

目录或历史搜索结果较多时，如果模型漏掉匹配项或无法继续阅读，先检查工具页大小、返回对象和 offset，再确认 `Agent.cpp` 是否把完整页面交给 provider。根因是 agent 将分页结果裁成首屏，或回执丢了 continuation offset。分页游标必须和命中内容一起返回。发生截断时应明确提示并允许继续读取。回归见 [`tests/completion/agent.cpp`](../../tests/completion/agent.cpp) 中 “agent forwards a complete directory page…” 和 “trimming an earlier literal tool excerpt…”。修复提交：[944db7ea](https://github.com/water2004/Folia/commit/944db7ea)。

## 允许网页读取后，本地文档意外进入请求；或读取权限被误当作搜索权限

用户只允许读取网页时，如果请求仍携带本地上下文，或执行了未授权的网络操作，先检查 `HostedTools` 构造的 capability 和 `Agent.cpp` 组装的隔离请求及其 message/tool 列表。根因是 hosted capability 选路把网页读取、搜索授权和本地工具上下文混在一起。每项能力应单独授权。隔离请求只携带当前查询，不带本地私有历史，未授权工具不得 dispatch。回归见 [`tests/completion/agent.cpp`](../../tests/completion/agent.cpp) 中 “agent can read a URL without granting search or sharing local context…” 和 “agent correlates a completed batch and rejects tools outside the current grants”。修复提交：[d35ccb5f](https://github.com/water2004/Folia/commit/d35ccb5f)，后续协议映射见 [38c47e50](https://github.com/water2004/Folia/commit/38c47e50)。

## 对话变长后请求超出上下文，重试仍然失败或工具调用对断

老对话触发 context-length rejection 后，如果缩短内容仍失败，或历史裁剪后 tool call/result 不再配对，先检查 `Context.cpp` 的预算、裁剪顺序和拒绝后的重试分支。根因是只按消息字符数预算，没有为系统提示、工具定义、历史和输出预留空间；裁剪时也可能拆开当前工具事务。预算应优先回收旧历史和可重新读取的工具回执，同时保留最后问题及完整工具事务。压缩不能改写 transcript。回归见 [`tests/completion/agent.cpp`](../../tests/completion/agent.cpp) 中 “agent compacts prior history…”, “an upstream context rejection reduces history and preserves the pending tool pair” 和 “manual compaction keeps the transcript…”。修复提交：[da555af7](https://github.com/water2004/Folia/commit/da555af7)。

## Prompt cache 没命中，或缓存边界变化后 developer 指令被漏掉

同一对话重复请求却未复用稳定前缀，或切换 hosted search 后 prompt cache 边界改变、developer 指令丢失时，先检查 `PromptCache.cpp` 的 provider dialect 和 `chat_completions/Request.cpp` 的消息序列化边界。不同 provider 的 cache 参数和边界编码不同。易变工具状态不能混入稳定前缀，developer 指令也必须留在 OpenAI 规定的边界内。回归见 [`tests/completion/agent.cpp`](../../tests/completion/agent.cpp) 中 “short turns keep their cache prefix when web access changes”，以及检查 request 编码的 [`tests/completion/protocol.cpp`](../../tests/completion/protocol.cpp)。修复提交：[da555af7](https://github.com/water2004/Folia/commit/da555af7) 和 [77406393](https://github.com/water2004/Folia/commit/77406393)。

## 历史压缩后旧消息无法找回，或重启恢复错位

压缩后模型无法引用原话，或重启后 checkpoint 与消息顺序错位时，先检查 `ChatJournal.cpp` 的正反向读取、checkpoint 保留数量和 history tool 的搜索/读取逻辑。根因是把摘要当成完整 transcript，或消息与压缩 checkpoint 使用了不一致的持久化格式和游标方向。摘要只缩短模型上下文。原始消息应保存在可检索的 journal 中，checkpoint 只记录保留边界，恢复时顺序可往返。相关逻辑见 [`ChatJournal.cpp`](../../src/app-winui/agent/ChatJournal.cpp) 中的 `HistoryPage`、`AppendCompaction` 和 `conversation_history`。重构提交：[1d455d49](https://github.com/water2004/Folia/commit/1d455d49)、[c564128a](https://github.com/water2004/Folia/commit/c564128a)。
