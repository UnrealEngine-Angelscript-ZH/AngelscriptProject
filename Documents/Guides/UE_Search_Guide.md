# Knot 搜索

遇到不确定的 UE API、引擎机制、类继承关系等问题时，**优先使用 Knot MCP 工具（`knowledgebase_search`）查询 UE5 知识库**，而不是凭记忆猜测。

可查询的知识库：`UE5-main`（knowledge_uuid: `d890d83194b04c8aad24d0e904cdb762`）

可用数据类型：

- `git`：源代码片段检索（函数实现、类定义）
- `git_iwiki`：代码库总结信息（架构、模块交互）
- `document`：官方文档（教程、API 参考、最佳实践）
- `git_commit`：代码变更历史
- `git_merge_request`：合并请求历史

典型使用场景：

- 不确定某个 UClass 的继承关系或接口实现方式
- 需要查找某个 UE API 的正确用法或签名
- 想了解 UE 某个子系统的内部实现原理
- 需要参考 UE 源码中类似功能的实现模式

# 源码搜索

如果Knot 搜索不全面，通过Knot 中提示到的相关文件路径，去本地引擎中进行查找