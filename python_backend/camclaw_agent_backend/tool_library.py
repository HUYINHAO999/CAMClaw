from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import List


@dataclass(frozen=True)
class Tool:
    tool_id: str
    display_name: str
    diameter_mm: float
    tool_type: str


@dataclass(frozen=True)
class ToolLibrary:
    default_tool_id: str
    tools: List[Tool]

    @staticmethod
    def from_file(config_path: Path | None = None) -> "ToolLibrary":
        path = config_path or Path(__file__).resolve().parents[1] / "config" / "tool_library.json"
        with path.open("r", encoding="utf-8") as config_file:
            config = json.load(config_file)

        tools = [
            Tool(
                tool_id=str(item["tool_id"]),
                display_name=str(item["display_name"]),
                diameter_mm=float(item["diameter_mm"]),
                tool_type=str(item["tool_type"]),
            )
            for item in config.get("tools", [])
        ]
        return ToolLibrary(
            default_tool_id=str(config.get("default_tool_id", "")),
            tools=sorted(tools, key=lambda tool: tool.diameter_mm),
        )

    def to_planning_context(self) -> str:
        if not self.tools:
            return "No tool catalog is available."

        lines = [
            "Available tool catalog, sorted from smaller to larger diameter:",
        ]
        for tool in sorted(self.tools, key=lambda item: item.diameter_mm):
            lines.append(
                "- {tool_id}: {diameter:g}mm {tool_type} ({display_name})".format(
                    tool_id=tool.tool_id,
                    diameter=tool.diameter_mm,
                    tool_type=tool.tool_type,
                    display_name=tool.display_name,
                )
            )
        if self.default_tool_id:
            lines.append("Default tool_id: " + self.default_tool_id)
        lines.append(
            "When feedback says the selected tool is too large or asks for a smaller tool, choose a smaller available tool_id from this catalog."
        )
        return "\n".join(lines)

    def best_tool_id_at_or_below(self, requested_diameter_mm: float) -> str:
        candidates = [
            tool for tool in self.tools
            if tool.diameter_mm <= requested_diameter_mm
        ]
        if candidates:
            return max(candidates, key=lambda tool: tool.diameter_mm).tool_id
        if self.tools:
            return min(self.tools, key=lambda tool: tool.diameter_mm).tool_id
        return self.default_tool_id

    def has_tool_id(self, tool_id: str) -> bool:
        return any(tool.tool_id == tool_id for tool in self.tools)

    def default_tool_id_for_operation(self, operation_type: str) -> str:
        preferred_type = "drill" if operation_type == "drilling" else "flat_end_mill"
        candidates = [tool for tool in self.tools if tool.tool_type == preferred_type]
        if candidates:
            return candidates[0].tool_id
        return self.default_tool_id
