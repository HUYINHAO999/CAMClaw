import unittest

from camclaw_agent_backend.planner import AgentPlanner, PlannerError, PlannerInput
from camclaw_agent_backend.tool_library import Tool, ToolLibrary


class FakeLlmClient:
    def __init__(self, response):
        self.response = response
        self.last_request = None

    def create_plan_json(self, **kwargs):
        self.last_request = kwargs
        return self.response


class AgentPlannerTests(unittest.TestCase):
    def test_builds_draft_from_structured_llm_response(self):
        llm = FakeLlmClient(
            "{"
            '"operation_type":"roughing",'
            '"tool_id":"tool_010",'
            '"stepover":"2.0",'
            '"stepdown":"1.0",'
            '"tolerance":"0.02"'
            "}"
        )
        planner = AgentPlanner(llm, ToolLibrary(
            default_tool_id="tool_010",
            tools=[
                Tool("tool_006", "6mm flat end mill", 6.0, "flat_end_mill"),
                Tool("tool_010", "10mm flat end mill", 10.0, "flat_end_mill"),
            ],
        ))

        draft = planner.create_draft(
            PlannerInput(
                trace_id="trace_py_001",
                user_request="给当前型腔做粗加工",
                target_object_id="feature_001",
                rejection_reason="只要粗加工，不要精加工",
            )
        )

        self.assertEqual("trace_py_001", draft["trace_id"])
        self.assertEqual("pending_review", draft["status"])
        self.assertEqual(
            "browser.createOperation",
            draft["steps"][0]["command_id"],
        )
        self.assertEqual("browser.createOperation.v1", draft["steps"][0]["schema_id"])
        self.assertEqual("feature_001", draft["steps"][0]["inputs"]["target_object_id"])
        self.assertEqual("tool_010", draft["steps"][0]["inputs"]["tool_id"])
        self.assertEqual("只要粗加工，不要精加工", llm.last_request["rejection_reason"])
        self.assertIn("operation_type", llm.last_request["response_contract"])
        self.assertIn("command_id", llm.last_request["response_contract"])
        self.assertIn("schema_id", llm.last_request["response_contract"])
        self.assertIn("tool_006", llm.last_request["response_contract"])
        self.assertIn("smaller tool", llm.last_request["response_contract"])

    def test_tool_library_context_is_data_driven(self):
        tool_library = ToolLibrary(
            default_tool_id="tool_010",
            tools=[
                Tool("tool_016", "16mm flat end mill", 16.0, "flat_end_mill"),
                Tool("tool_006", "6mm flat end mill", 6.0, "flat_end_mill"),
            ],
        )

        context = tool_library.to_planning_context()

        self.assertLess(context.index("tool_006"), context.index("tool_016"))
        self.assertIn("Default tool_id: tool_010", context)

    def test_rejects_unstructured_llm_response(self):
        planner = AgentPlanner(FakeLlmClient("roughing with tool_010"))

        with self.assertRaises(PlannerError) as caught:
            planner.create_draft(
                PlannerInput(
                    trace_id="trace_py_002",
                    user_request="给当前型腔做粗加工",
                    target_object_id="feature_001",
                )
            )

        self.assertEqual("invalid_llm_response", caught.exception.error_code)

    def test_rejects_missing_required_llm_fields(self):
        planner = AgentPlanner(FakeLlmClient('{"operation_type":"roughing"}'))

        with self.assertRaises(PlannerError) as caught:
            planner.create_draft(
                PlannerInput(
                    trace_id="trace_py_003",
                    user_request="给当前型腔做粗加工",
                    target_object_id="feature_001",
                )
            )

        self.assertEqual("missing_llm_field", caught.exception.error_code)

    def test_normalizes_numeric_fields_before_cpp_execution(self):
        llm = FakeLlmClient(
            "{"
            '"operation_type":"roughing",'
            '"tool_id":"tool_010",'
            '"stepover":"4 mm",'
            '"stepdown":"2.0mm",'
            '"tolerance":"0.050 mm"'
            "}"
        )
        planner = AgentPlanner(llm)

        draft = planner.create_draft(
            PlannerInput(
                trace_id="trace_py_004",
                user_request="给当前型腔加工",
                target_object_id="feature_001",
            )
        )

        inputs = draft["steps"][0]["inputs"]
        self.assertEqual("4", inputs["stepover"])
        self.assertEqual("2", inputs["stepdown"])
        self.assertEqual("0.05", inputs["tolerance"])

    def test_reviews_requested_tool_diameter_against_tool_library(self):
        llm = FakeLlmClient(
            "{"
            '"command_id":"browser.createOperation",'
            '"schema_id":"browser.createOperation.v1",'
            '"inputs":{'
            '"operation_type":"pocket_roughing",'
            '"tool_id":"tool_016",'
            '"stepover":"6",'
            '"stepdown":"4",'
            '"tolerance":"0.1",'
            '"batch_count":"1"'
            "}"
            "}"
        )
        planner = AgentPlanner(
            llm,
            ToolLibrary(
                default_tool_id="tool_010",
                tools=[
                    Tool("tool_006", "6mm flat end mill", 6.0, "flat_end_mill"),
                    Tool("tool_010", "10mm flat end mill", 10.0, "flat_end_mill"),
                    Tool("tool_016", "16mm flat end mill", 16.0, "flat_end_mill"),
                ],
            ),
        )

        draft = planner.create_draft(
            PlannerInput(
                trace_id="trace_py_005",
                user_request="选择合适的工序加工当前型腔，用d=15mm的刀",
                target_object_id="feature_001",
            )
        )

        self.assertEqual("tool_010", draft["steps"][0]["inputs"]["tool_id"])

    def test_builds_batch_create_operation_command(self):
        llm = FakeLlmClient(
            "{"
            '"command_id":"browser.createOperation",'
            '"schema_id":"browser.createOperation.v1",'
            '"inputs":{'
            '"operation_type":"drilling",'
            '"tool_id":"tool_006",'
            '"stepover":"0",'
            '"stepdown":"3",'
            '"tolerance":"0.02",'
            '"batch_count":"10"'
            "}"
            "}"
        )
        planner = AgentPlanner(llm, ToolLibrary(
            default_tool_id="tool_010",
            tools=[Tool("tool_006", "6mm drill", 6.0, "drill")],
        ))

        draft = planner.create_draft(
            PlannerInput(
                trace_id="trace_py_batch",
                user_request="帮我创建10个钻孔工序",
                target_object_id="feature_holes_001",
            )
        )

        self.assertEqual("browser.createOperation", draft["steps"][0]["command_id"])
        self.assertEqual("drilling", draft["steps"][0]["inputs"]["operation_type"])
        self.assertEqual("10", draft["steps"][0]["inputs"]["batch_count"])
        self.assertEqual("false", draft["steps"][0]["inputs"]["auto_generate_toolpath"])

    def test_feature_recognition_overrides_llm_operation_for_hole_plane_and_pocket(self):
        cases = [
            ("孔组 A", "feature_holes_001", "drilling"),
            ("平面 A", "feature_plane_001", "finishing"),
            ("型腔 A", "feature_001", "pocket"),
        ]
        for display_name, target_id, expected_type in cases:
            with self.subTest(target_id=target_id):
                llm = FakeLlmClient(
                    "{"
                    '"command_id":"browser.createOperation",'
                    '"schema_id":"browser.createOperation.v1",'
                    '"inputs":{'
                    '"operation_type":"pocket",'
                    '"tool_id":"bad_tool",'
                    '"stepover":"2",'
                    '"stepdown":"3",'
                    '"tolerance":"0.02",'
                    '"batch_count":"1"'
                    "}"
                    "}"
                )
                planner = AgentPlanner(llm, ToolLibrary(
                    default_tool_id="tool_010",
                    tools=[
                        Tool("tool_010", "10mm flat end mill", 10.0, "flat_end_mill"),
                        Tool("drill_006", "6mm drill", 6.0, "drill"),
                    ],
                ))

                draft = planner.create_draft(
                    PlannerInput(
                        trace_id="trace_feature_match",
                        user_request="帮我加工这个零件",
                        target_object_id=target_id,
                        target_display_name=display_name,
                    )
                )

                inputs = draft["steps"][0]["inputs"]
                self.assertEqual(expected_type, inputs["operation_type"])
                self.assertEqual("true", inputs["auto_generate_toolpath"])

    def test_create_only_does_not_auto_generate_toolpath(self):
        llm = FakeLlmClient(
            "{"
            '"command_id":"browser.createOperation",'
            '"schema_id":"browser.createOperation.v1",'
            '"inputs":{'
            '"operation_type":"drilling",'
            '"tool_id":"drill_006",'
            '"stepover":"0",'
            '"stepdown":"3",'
            '"tolerance":"0.02",'
            '"batch_count":"10",'
            '"auto_generate_toolpath":"false"'
            "}"
            "}"
        )
        planner = AgentPlanner(llm, ToolLibrary(
            default_tool_id="tool_010",
            tools=[Tool("drill_006", "6mm drill", 6.0, "drill")],
        ))

        draft = planner.create_draft(
            PlannerInput(
                trace_id="trace_create_only",
                user_request="帮我创建10个钻孔工序",
                target_object_id="feature_holes_001",
                target_display_name="孔组 A",
            )
        )

        self.assertEqual("10", draft["steps"][0]["inputs"]["batch_count"])
        self.assertEqual("false", draft["steps"][0]["inputs"]["auto_generate_toolpath"])

    def test_builds_update_operation_command_with_recompute_flag(self):
        llm = FakeLlmClient(
            "{"
            '"command_id":"browser.updateOperation",'
            '"schema_id":"browser.updateOperation.v1",'
            '"inputs":{'
            '"parameter_name":"stepover",'
            '"parameter_value":"0.8",'
            '"recompute_toolpath":"true"'
            "}"
            "}"
        )
        planner = AgentPlanner(llm)

        draft = planner.create_draft(
            PlannerInput(
                trace_id="trace_py_update",
                user_request="当前生成的刀轨我不满意，改用更小的stepover重新生成刀轨",
                target_object_id="op_roughing_feature_001",
            )
        )

        self.assertEqual("browser.updateOperation", draft["steps"][0]["command_id"])
        self.assertEqual("browser.updateOperation.v1", draft["steps"][0]["schema_id"])
        self.assertEqual("op_roughing_feature_001", draft["steps"][0]["inputs"]["target_object_id"])
        self.assertEqual("stepover", draft["steps"][0]["inputs"]["parameter_name"])
        self.assertEqual("true", draft["steps"][0]["inputs"]["recompute_toolpath"])

    def test_builds_toolpath_visibility_command(self):
        llm = FakeLlmClient(
            "{"
            '"command_id":"browser.setToolpathVisibility",'
            '"schema_id":"browser.setToolpathVisibility.v1",'
            '"inputs":{'
            '"visibility":"hide",'
            '"scope":"all",'
            '"toolpath_ids":""'
            "}"
            "}"
        )
        planner = AgentPlanner(llm)

        draft = planner.create_draft(
            PlannerInput(
                trace_id="trace_visibility",
                user_request="帮我隐藏所有刀轨",
                target_object_id="toolpath_op_roughing_feature_001",
            )
        )

        self.assertEqual("browser.setToolpathVisibility", draft["steps"][0]["command_id"])
        self.assertEqual("browser.setToolpathVisibility.v1", draft["steps"][0]["schema_id"])
        self.assertEqual("hide", draft["steps"][0]["inputs"]["visibility"])
        self.assertEqual("all", draft["steps"][0]["inputs"]["scope"])

    def test_visibility_for_operation_name_uses_operation_type_scope(self):
        llm = FakeLlmClient(
            "{"
            '"command_id":"browser.setToolpathVisibility",'
            '"schema_id":"browser.setToolpathVisibility.v1",'
            '"inputs":{'
            '"visibility":"hide",'
            '"scope":"list",'
            '"toolpath_ids":"toolpath_op_drilling"'
            "}"
            "}"
        )
        planner = AgentPlanner(llm)

        draft = planner.create_draft(
            PlannerInput(
                trace_id="trace_visibility_operation",
                user_request="帮我隐藏型腔铣的刀轨",
                target_object_id="toolpath_op_drilling",
            )
        )

        inputs = draft["steps"][0]["inputs"]
        self.assertEqual("operation_type", inputs["scope"])
        self.assertEqual("pocket", inputs["operation_type"])
        self.assertEqual("", inputs["toolpath_ids"])

    def test_builds_action_sequence_for_hide_and_show_toolpaths(self):
        llm = FakeLlmClient(
            "{"
            '"actions":['
            "{"
            '"command_id":"browser.setToolpathVisibility",'
            '"schema_id":"browser.setToolpathVisibility.v1",'
            '"inputs":{"visibility":"hide","scope":"operation_type","operation_type":"pocket"}'
            "},"
            "{"
            '"command_id":"browser.setToolpathVisibility",'
            '"schema_id":"browser.setToolpathVisibility.v1",'
            '"inputs":{"visibility":"show","scope":"operation_type","operation_type":"drilling"}'
            "}"
            "]"
            "}"
        )
        planner = AgentPlanner(llm)

        draft = planner.create_draft(
            PlannerInput(
                trace_id="trace_visibility_sequence",
                user_request="隐藏型腔铣刀轨，显示钻孔刀轨",
                target_object_id="toolpath_op_drilling_feature_holes_001",
            )
        )

        self.assertEqual(2, len(draft["steps"]))
        self.assertEqual("hide", draft["steps"][0]["inputs"]["visibility"])
        self.assertEqual("pocket", draft["steps"][0]["inputs"]["operation_type"])
        self.assertEqual("show", draft["steps"][1]["inputs"]["visibility"])
        self.assertEqual("drilling", draft["steps"][1]["inputs"]["operation_type"])

    def test_fills_missing_visibility_operation_types_from_multi_target_request(self):
        llm = FakeLlmClient(
            "{"
            '"actions":['
            "{"
            '"command_id":"browser.setToolpathVisibility",'
            '"schema_id":"browser.setToolpathVisibility.v1",'
            '"inputs":{"visibility":"hide","scope":"operation_type","operation_type":"","toolpath_ids":""}'
            "},"
            "{"
            '"command_id":"browser.setToolpathVisibility",'
            '"schema_id":"browser.setToolpathVisibility.v1",'
            '"inputs":{"visibility":"hide","scope":"operation_type","operation_type":"","toolpath_ids":""}'
            "}"
            "]"
            "}"
        )
        planner = AgentPlanner(llm)

        draft = planner.create_draft(
            PlannerInput(
                trace_id="trace_visibility_multi_empty",
                user_request="帮我隐藏所有型腔铣和钻孔刀轨",
                target_object_id="toolpath_op_pocket_feature_001_3",
            )
        )

        self.assertEqual(2, len(draft["steps"]))
        self.assertEqual("pocket", draft["steps"][0]["inputs"]["operation_type"])
        self.assertEqual("drilling", draft["steps"][1]["inputs"]["operation_type"])
        self.assertEqual("operation_type", draft["steps"][0]["inputs"]["scope"])
        self.assertEqual("operation_type", draft["steps"][1]["inputs"]["scope"])

    def test_expands_single_visibility_action_for_multi_target_request(self):
        llm = FakeLlmClient(
            "{"
            '"actions":[{'
            '"command_id":"browser.setToolpathVisibility",'
            '"schema_id":"browser.setToolpathVisibility.v1",'
            '"inputs":{"visibility":"hide","scope":"operation_type","operation_type":"","toolpath_ids":""}'
            "}]"
            "}"
        )
        planner = AgentPlanner(llm)

        draft = planner.create_draft(
            PlannerInput(
                trace_id="trace_visibility_expand",
                user_request="帮我隐藏所有钻孔和型腔铣刀轨",
                target_object_id="toolpath_op_pocket_feature_001_3",
            )
        )

        self.assertEqual(2, len(draft["steps"]))
        self.assertEqual("drilling", draft["steps"][0]["inputs"]["operation_type"])
        self.assertEqual("pocket", draft["steps"][1]["inputs"]["operation_type"])

    def test_builds_action_sequence_for_multiple_batch_creates(self):
        llm = FakeLlmClient(
            "{"
            '"actions":['
            "{"
            '"command_id":"browser.createOperation",'
            '"schema_id":"browser.createOperation.v1",'
            '"inputs":{'
            '"operation_type":"drilling","tool_id":"drill_006","stepover":"0",'
            '"stepdown":"3","tolerance":"0.02","batch_count":"3","auto_generate_toolpath":"false"'
            "}"
            "},"
            "{"
            '"command_id":"browser.createOperation",'
            '"schema_id":"browser.createOperation.v1",'
            '"inputs":{'
            '"operation_type":"pocket","tool_id":"tool_010","stepover":"2",'
            '"stepdown":"1","tolerance":"0.02","batch_count":"5","auto_generate_toolpath":"false"'
            "}"
            "}"
            "]"
            "}"
        )
        planner = AgentPlanner(llm, ToolLibrary(
            default_tool_id="tool_010",
            tools=[
                Tool("tool_010", "10mm flat end mill", 10.0, "flat_end_mill"),
                Tool("drill_006", "6mm drill", 6.0, "drill"),
            ],
        ))

        draft = planner.create_draft(
            PlannerInput(
                trace_id="trace_batch_sequence",
                user_request="创建3个钻孔工序，创建5个型腔铣工序",
                target_object_id="feature_holes_001",
            )
        )

        self.assertEqual(2, len(draft["steps"]))
        self.assertEqual("drilling", draft["steps"][0]["inputs"]["operation_type"])
        self.assertEqual("3", draft["steps"][0]["inputs"]["batch_count"])
        self.assertEqual("pocket", draft["steps"][1]["inputs"]["operation_type"])
        self.assertEqual("5", draft["steps"][1]["inputs"]["batch_count"])


if __name__ == "__main__":
    unittest.main()
