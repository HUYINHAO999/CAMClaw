import tempfile
import unittest
from pathlib import Path

from camclaw_agent_backend.tool_library import ToolLibrary


class ToolLibraryTests(unittest.TestCase):
    def test_reads_tool_library_from_json_file(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            config_path = Path(temp_dir) / "tool_library.json"
            config_path.write_text(
                """
                {
                  "default_tool_id": "tool_010",
                  "tools": [
                    {
                      "tool_id": "tool_016",
                      "display_name": "16mm flat end mill",
                      "diameter_mm": 16.0,
                      "tool_type": "flat_end_mill"
                    },
                    {
                      "tool_id": "tool_006",
                      "display_name": "6mm flat end mill",
                      "diameter_mm": 6.0,
                      "tool_type": "flat_end_mill"
                    }
                  ]
                }
                """,
                encoding="utf-8",
            )

            library = ToolLibrary.from_file(config_path)

        self.assertEqual("tool_010", library.default_tool_id)
        self.assertEqual("tool_006", library.tools[0].tool_id)
        self.assertEqual("tool_016", library.tools[1].tool_id)

    def test_selects_largest_available_tool_not_exceeding_requested_diameter(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            config_path = Path(temp_dir) / "tool_library.json"
            config_path.write_text(
                """
                {
                  "default_tool_id": "tool_010",
                  "tools": [
                    {"tool_id": "tool_006", "display_name": "6mm", "diameter_mm": 6.0, "tool_type": "flat_end_mill"},
                    {"tool_id": "tool_010", "display_name": "10mm", "diameter_mm": 10.0, "tool_type": "flat_end_mill"},
                    {"tool_id": "tool_016", "display_name": "16mm", "diameter_mm": 16.0, "tool_type": "flat_end_mill"}
                  ]
                }
                """,
                encoding="utf-8",
            )
            library = ToolLibrary.from_file(config_path)

        self.assertEqual("tool_010", library.best_tool_id_at_or_below(15.0))


if __name__ == "__main__":
    unittest.main()
