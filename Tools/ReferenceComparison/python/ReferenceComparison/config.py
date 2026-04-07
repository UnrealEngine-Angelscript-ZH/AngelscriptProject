"""Comparison dimensions, repository configs, and default parameters."""

from __future__ import annotations

import os
from dataclasses import dataclass, field
from pathlib import Path
from typing import List, Optional


@dataclass
class RepoConfig:
    key: str
    display_name: str
    language: str
    default_subdir: str
    source_hints: List[str] = field(default_factory=list)
    agent_config_key: Optional[str] = None

    def resolve_path(self, project_root: Path) -> Path:
        if self.agent_config_key:
            ini_path = project_root / "AgentConfig.ini"
            external = _read_agent_config_value(ini_path, self.agent_config_key)
            if external:
                return Path(external)
        return project_root / "Reference" / self.default_subdir


def _read_agent_config_value(ini_path: Path, dotted_key: str) -> Optional[str]:
    """Read a value from AgentConfig.ini by 'Section.Key' notation."""
    if not ini_path.is_file():
        return None
    parts = dotted_key.split(".", 1)
    if len(parts) != 2:
        return None
    target_section, target_key = parts
    try:
        current_section = ""
        for line in ini_path.read_text(encoding="utf-8").splitlines():
            stripped = line.strip()
            if stripped.startswith("[") and stripped.endswith("]"):
                current_section = stripped[1:-1]
            elif "=" in stripped and current_section == target_section:
                k, v = stripped.split("=", 1)
                if k.strip() == target_key:
                    return v.strip()
    except (OSError, UnicodeDecodeError):
        pass
    return None


REPOS: List[RepoConfig] = [
    RepoConfig(
        key="hazelight",
        display_name="Hazelight-Angelscript",
        language="AngelScript",
        default_subdir="",
        agent_config_key="References.HazelightAngelscriptEngineRoot",
        source_hints=[
            "Engine/Plugins/Angelscript/Source/AngelscriptCode",
            "Engine/Plugins/Angelscript/Source/AngelscriptEditor",
            "Engine/Plugins/Angelscript/Source/AngelscriptLoader",
            "Engine/Plugins/Angelscript/Binds",
        ],
    ),
    RepoConfig(
        key="unrealcsharp",
        display_name="UnrealCSharp",
        language="C#",
        default_subdir="UnrealCSharp",
        source_hints=[
            "Source/UnrealCSharp",
            "Source/Generator",
        ],
    ),
    RepoConfig(
        key="unlua",
        display_name="UnLua",
        language="Lua",
        default_subdir="UnLua",
        source_hints=[
            "Plugins/UnLua/Source",
            "Docs",
            "Content/Script/Tutorials",
        ],
    ),
    RepoConfig(
        key="puerts",
        display_name="puerts",
        language="TypeScript/JavaScript",
        default_subdir="puerts",
        source_hints=[
            "unreal/Puerts/Source",
            "doc/unreal",
        ],
    ),
    RepoConfig(
        key="sluaunreal",
        display_name="sluaunreal",
        language="Lua",
        default_subdir="sluaunreal",
        source_hints=[
            "Plugins/slua_unreal/Source",
            "Tools",
            "Content",
        ],
    ),
]

REPO_BY_KEY = {r.key: r for r in REPOS}


@dataclass
class Dimension:
    id: str
    name_en: str
    name_zh: str
    focus: str


DIMENSIONS: List[Dimension] = [
    Dimension("D1", "Architecture", "插件架构与模块划分",
              "模块数量与职责、Build 依赖关系、第三方库集成方式、插件与宿主工程的边界"),
    Dimension("D2", "ReflectionBinding", "反射绑定机制",
              "UClass/UStruct/UEnum/UInterface/Delegate 暴露方式、绑定代码生成 vs 手写、注册时机与生命周期"),
    Dimension("D3", "BlueprintInterop", "Blueprint 交互",
              "脚本覆写 Blueprint 事件、脚本调用 Blueprint 函数、Blueprint 调用脚本函数、混合继承链"),
    Dimension("D4", "HotReload", "热重载",
              "变更检测机制、重载粒度（全量 vs 增量）、状态保持策略、重载失败恢复"),
    Dimension("D5", "Debugging", "调试与开发体验",
              "调试协议（DAP/自定义）、断点与单步、变量查看、IDE 集成、日志与诊断"),
    Dimension("D6", "CodeGenIDE", "代码生成与 IDE 支持",
              "类型声明文件生成、智能提示、代码补全、跳转定义"),
    Dimension("D7", "EditorIntegration", "编辑器集成",
              "编辑器菜单/面板扩展、资产浏览器集成、Commandlet 支持"),
    Dimension("D8", "Performance", "性能与优化",
              "JIT/AOT 支持、调用开销基准、内存管理策略、批量绑定优化"),
    Dimension("D9", "Testing", "测试基础设施",
              "测试框架选择、测试分层与组织、CI 集成、覆盖率支持"),
    Dimension("D10", "Documentation", "文档与示例组织",
              "用户文档结构、API 参考生成、教程与示例项目、上手引导流程"),
    Dimension("D11", "Deployment", "部署与打包",
              "脚本打包方式、加密/签名、平台适配、版本兼容性"),
]

DIMENSION_BY_ID = {d.id: d for d in DIMENSIONS}


@dataclass
class RunConfig:
    project_root: Path
    output_dir: Path
    rule_path: Path
    repos: List[RepoConfig]
    dimensions: List[Dimension]
    max_iterations: int = 3
    opencode_command: str = "ralph-loop"
    opencode_model: str = "codez-gpt/gpt-5.4"
    opencode_variant: str = "xhigh"
    timeout_seconds: int = 600
    dry_run: bool = False

    @classmethod
    def create(
        cls,
        project_root: Optional[str] = None,
        date_suffix: Optional[str] = None,
        repo_keys: Optional[List[str]] = None,
        dimension_ids: Optional[List[str]] = None,
        max_iterations: int = 3,
        timeout_seconds: int = 600,
        dry_run: bool = False,
    ) -> "RunConfig":
        import datetime

        if project_root is None:
            project_root = str(
                Path(__file__).resolve().parent.parent.parent.parent.parent
            )
        root = Path(project_root)

        if date_suffix is None:
            date_suffix = datetime.date.today().isoformat()

        output_dir = root / "Documents" / "Comparisons" / date_suffix
        rule_path = root / "Documents" / "Rules" / "ReferenceComparisonRule_ZH.md"

        if repo_keys:
            repos = [REPO_BY_KEY[k] for k in repo_keys if k in REPO_BY_KEY]
        else:
            repos = list(REPOS)

        if dimension_ids:
            dims = [DIMENSION_BY_ID[d] for d in dimension_ids if d in DIMENSION_BY_ID]
        else:
            dims = list(DIMENSIONS)

        return cls(
            project_root=root,
            output_dir=output_dir,
            rule_path=rule_path,
            repos=repos,
            dimensions=dims,
            max_iterations=max_iterations,
            timeout_seconds=timeout_seconds,
            dry_run=dry_run,
        )
