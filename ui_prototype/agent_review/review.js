const initialDraft = {
  trace_id: "trace_ui_001",
  status: "pending_review",
  target: {
    object_id: "feature_001",
    object_type: "Feature",
    display_name: "型腔 A",
  },
  steps: [
    {
      skill_id: "browser.create_roughing_operation_and_generate_toolpath",
      inputs: {
        target_object_id: "feature_001",
        operation_type: "roughing",
        tool_id: "tool_010",
        stepover: "2.0",
        stepdown: "1.0",
        tolerance: "0.02",
      },
    },
  ],
  trace_events: ["draft_created"],
};

const parameterLabels = {
  operation_type: "工序类型",
  tool_id: "刀具",
  stepover: "步距",
  stepdown: "切深",
  tolerance: "容差",
};

const state = {
  draft: initialDraft,
  executionResult: null,
};

function stepInputs() {
  return state.draft.steps[0].inputs;
}

function normalizeBackendDraft(backendDraft) {
  return {
    ...backendDraft,
    target: state.draft.target,
    trace_events: backendDraft.trace_events || ["draft_created"],
  };
}

function addTrace(eventName) {
  state.draft.trace_events.push(eventName);
}

function render() {
  document.getElementById("traceId").textContent = state.draft.trace_id;
  document.getElementById("targetName").textContent = state.draft.target.display_name;
  document.getElementById("targetMeta").textContent = `${state.draft.target.object_id} · ${state.draft.target.object_type}`;

  const statusEl = document.getElementById("draftStatus");
  statusEl.textContent = statusText(state.draft.status);
  statusEl.className = `status-pill ${statusClass(state.draft.status)}`;

  renderSteps();
  renderParameters();
  renderResult();
  renderTrace();
  renderControls();
}

function statusText(status) {
  if (status === "confirmed") return "Confirmed";
  if (status === "rejected") return "已拒绝";
  if (status === "failed") return "失败";
  return "待审核";
}

function statusClass(status) {
  if (status === "confirmed") return "done";
  if (status === "rejected") return "rejected";
  if (status === "failed") return "failed";
  return "";
}

function renderSteps() {
  const stepList = document.getElementById("stepList");
  stepList.innerHTML = "";
  state.draft.steps.forEach((step, index) => {
    const node = document.createElement("div");
    node.className = "step";
    node.innerHTML = `
      <strong>${index + 1}. 创建粗加工工序并生成刀轨</strong>
      <code>${step.skill_id}</code>
    `;
    stepList.appendChild(node);
  });
}

function renderParameters() {
  const form = document.getElementById("parameterForm");
  form.innerHTML = "";
  const editable = state.draft.status === "pending_review";
  document.getElementById("editMode").textContent = editable ? "可编辑" : "已锁定";

  Object.keys(parameterLabels).forEach((key) => {
    const wrapper = document.createElement("div");
    wrapper.className = "field";

    const label = document.createElement("label");
    label.htmlFor = `field-${key}`;
    label.textContent = parameterLabels[key];

    const input = document.createElement("input");
    input.id = `field-${key}`;
    input.name = key;
    input.value = stepInputs()[key];
    input.disabled = !editable;
    input.addEventListener("input", () => {
      stepInputs()[key] = input.value;
      addTrace("draft_input_edited");
      renderTrace();
    });

    const hint = document.createElement("span");
    hint.textContent = key;

    wrapper.appendChild(label);
    wrapper.appendChild(input);
    wrapper.appendChild(hint);
    form.appendChild(wrapper);
  });
}

function renderResult() {
  const resultStatus = document.getElementById("resultStatus");
  const resultBody = document.getElementById("resultBody");
  if (!state.executionResult) {
    resultStatus.textContent = "未执行";
    resultBody.textContent = "确认审核后的草案后，将通过 C++ Core 执行。";
    return;
  }

  resultStatus.textContent = state.executionResult.ok ? "执行成功" : "执行失败";
  resultBody.textContent = JSON.stringify(state.executionResult, null, 2);
}

function renderTrace() {
  const traceList = document.getElementById("traceList");
  traceList.innerHTML = "";
  state.draft.trace_events.forEach((eventName) => {
    const item = document.createElement("li");
    item.textContent = eventName;
    traceList.appendChild(item);
  });
  document.getElementById("traceCount").textContent = `${state.draft.trace_events.length} 条事件`;
}

function renderControls() {
  const editable = state.draft.status === "pending_review";
  document.getElementById("confirmButton").disabled = !editable;
  document.getElementById("rejectButton").disabled = !editable;
  document.getElementById("simulateFailureButton").disabled = !editable;
  document.getElementById("rejectReason").disabled = !editable;
  document.getElementById("generateDraftButton").disabled = state.draft.status === "confirmed";
}

async function generateDraft() {
  const userRequest = document.getElementById("userRequest").value.trim();
  if (!userRequest) return;

  state.executionResult = {
    ok: false,
    error_code: "planning",
    message: "正在请求 Python backend 生成草案...",
    primary_object_id: "",
    object_ids: [],
    trace_events: [...state.draft.trace_events],
  };
  renderResult();

  try {
    const response = await fetch("/v1/agent/plan", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        trace_id: `trace_ui_${Date.now()}`,
        user_request: userRequest,
        target_object_id: state.draft.target.object_id,
        rejection_reason: state.draft.rejection_reason || "",
      }),
    });
    const payload = await response.json();
    if (!response.ok) {
      throw new Error(`${payload.error_code || "plan_failed"}: ${payload.message || ""}`);
    }

    state.draft = normalizeBackendDraft(payload);
    state.executionResult = null;
    render();
  } catch (error) {
    state.executionResult = {
      ok: false,
      error_code: "plan_request_failed",
      message: error.message,
      primary_object_id: "",
      object_ids: [],
      trace_events: [...state.draft.trace_events],
    };
    render();
  }
}

function confirmDraft() {
  if (state.draft.status !== "pending_review") return;
  state.draft.status = "confirmed";
  addTrace("draft_confirmed_final");
  state.executionResult = executeDraft(false);
  render();
}

function rejectDraft() {
  if (state.draft.status !== "pending_review") return;
  const reason = document.getElementById("rejectReason").value.trim();
  state.draft.status = "rejected";
  state.draft.rejection_reason = reason;
  addTrace("draft_rejected");
  state.executionResult = {
    ok: false,
    error_code: "draft_rejected",
    message: reason,
    primary_object_id: "",
    object_ids: [],
    trace_events: [...state.draft.trace_events],
  };
  render();
}

function simulateFailure() {
  if (state.draft.status !== "pending_review") return;
  state.draft.status = "failed";
  addTrace("draft_confirmed_final");
  addTrace("gateway_rejected");
  state.executionResult = {
    ok: false,
    error_code: "invalid_argument",
    message: "stepover must be a positive number.",
    primary_object_id: stepInputs().target_object_id,
    object_ids: [],
    trace_events: [...state.draft.trace_events],
  };
  render();
}

function executeDraft(shouldFail) {
  addTrace("draft_execution_started");
  addTrace("skill_started");
  addTrace("gateway_validated");
  addTrace("skill_step_completed");
  addTrace("gateway_validated");
  addTrace("skill_completed");
  addTrace("draft_execution_completed");

  if (shouldFail) {
    return {
      ok: false,
      error_code: "execution_failed",
      message: "Execution failed.",
      primary_object_id: stepInputs().target_object_id,
      object_ids: [],
      trace_events: [...state.draft.trace_events],
    };
  }

  return {
    ok: true,
    error_code: "",
    message: "",
    primary_object_id: "op_roughing_feature_001",
    object_ids: ["op_roughing_feature_001", "toolpath_op_roughing_feature_001"],
    trace_events: [...state.draft.trace_events],
  };
}

document.getElementById("confirmButton").addEventListener("click", confirmDraft);
document.getElementById("rejectButton").addEventListener("click", rejectDraft);
document.getElementById("simulateFailureButton").addEventListener("click", simulateFailure);
document.getElementById("generateDraftButton").addEventListener("click", generateDraft);
document.getElementById("useSelectionButton").addEventListener("click", () => {
  addTrace("target_confirmation_requested");
  addTrace("target_confirmed");
  renderTrace();
});

render();
