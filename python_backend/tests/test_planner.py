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
    def test_returns_validated_semantic_intent_plan(self):
        llm = FakeLlmClient(
            "{"
            '"schema_version":"1",'
            '"intents":[{'
            '"id":"i1",'
            '"intent":"set_toolpath_visibility",'
            '"actions":[{'
            '"visibility":"hide",'
            '"target":{"kind":"query","object_type":"toolpath","scope":"matching","filters":{"operation_type":"pocket"}}'
            "}]"
            "}]"
            "}"
        )
        planner = AgentPlanner(llm)

        draft = planner.create_draft(
            PlannerInput(
                trace_id="trace_semantic_visibility",
                user_request="帮我隐藏型腔铣的刀轨",
                target_object_id="toolpath_op_drilling",
            )
        )

        self.assertEqual("1", draft["schema_version"])
        self.assertEqual("trace_semantic_visibility", draft["trace_id"])
        self.assertEqual("pending_review", draft["status"])
        self.assertEqual("semantic_draft_created", draft["trace_events"][0])
        self.assertEqual("set_toolpath_visibility", draft["intents"][0]["intent"])

    def test_contract_tells_llm_to_use_semantic_intents_not_commands(self):
        llm = FakeLlmClient(
            "{"
            '"schema_version":"1",'
            '"intents":[{'
            '"id":"i1",'
            '"intent":"machine_feature",'
            '"target":{"kind":"objects","object_ids":["feature_holes_001"]},'
            '"auto_generate_toolpath":true'
            "}]"
            "}"
        )
        planner = AgentPlanner(
            llm,
            ToolLibrary(
                default_tool_id="tool_010",
                tools=[Tool("tool_006", "6mm drill", 6.0, "drill")],
            ),
        )

        planner.create_draft(
            PlannerInput(
                trace_id="trace_contract",
                user_request="帮我加工这个工件",
                target_object_id="feature_holes_001",
                rejection_reason="",
            )
        )

        contract = llm.last_request["response_contract"]
        self.assertIn("semantic intent plan", contract)
        self.assertIn("machine_feature", contract)
        self.assertIn("create_operations", contract)
        self.assertIn("Do not name Qt slots", contract)
        self.assertIn("tool_006", contract)

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

    def test_rejects_legacy_command_response(self):
        planner = AgentPlanner(
            FakeLlmClient(
                "{"
                '"command_id":"browser.createOperation",'
                '"schema_id":"browser.createOperation.v1",'
                '"inputs":{"operation_type":"pocket"}'
                "}"
            )
        )

        with self.assertRaises(PlannerError) as caught:
            planner.create_draft(
                PlannerInput(
                    trace_id="trace_legacy_command",
                    user_request="帮我创建型腔铣工序",
                    target_object_id="feature_001",
                )
            )

        self.assertEqual("invalid_semantic_intent", caught.exception.error_code)

    def test_rejects_semantic_plan_with_invalid_target_shape(self):
        planner = AgentPlanner(
            FakeLlmClient(
                "{"
                '"schema_version":"1",'
                '"intents":[{'
                '"id":"i1",'
                '"intent":"machine_feature",'
                '"target":{"kind":"query","object_type":"feature","scope":"matching","filters":{}},'
                '"auto_generate_toolpath":true'
                "}]"
                "}"
            )
        )

        with self.assertRaises(PlannerError) as caught:
            planner.create_draft(
                PlannerInput(
                    trace_id="trace_bad_target",
                    user_request="帮我加工这个工件",
                    target_object_id="feature_001",
                )
            )

        self.assertEqual("invalid_semantic_intent", caught.exception.error_code)


if __name__ == "__main__":
    unittest.main()
