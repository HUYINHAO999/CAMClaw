const assert = require("assert");
const fs = require("fs");
const path = require("path");
const vm = require("vm");

function createElement(tag) {
  return {
    tagName: tag,
    children: [],
    className: "",
    textContent: "",
    innerHTML: "",
    value: "",
    disabled: false,
    rows: 0,
    appendChild(child) {
      this.children.push(child);
      return child;
    },
    addEventListener(eventName, callback) {
      this[`on${eventName}`] = callback;
    },
  };
}

function createDocument() {
  const elements = {};
  const ids = [
    "traceId",
    "targetName",
    "targetMeta",
    "draftStatus",
    "stepList",
    "parameterForm",
    "editMode",
    "resultStatus",
    "resultBody",
    "traceList",
    "traceCount",
    "confirmButton",
    "rejectButton",
    "simulateFailureButton",
    "rejectReason",
    "useSelectionButton",
    "userRequest",
    "generateDraftButton",
  ];
  ids.forEach((id) => {
    elements[id] = createElement("div");
  });
  elements.rejectReason.value = "只要粗加工，不要精加工";
  elements.userRequest.value = "给当前型腔做粗加工";

  return {
    elements,
    getElementById(id) {
      return elements[id];
    },
    createElement,
  };
}

const document = createDocument();
const context = {
  document,
  console,
  fetch() {
    throw new Error("fetch should not be called by this test");
  },
  Date,
};
vm.createContext(context);

const source = fs.readFileSync(path.join(__dirname, "review.js"), "utf8");
vm.runInContext(source, context);

assert.strictEqual(document.elements.draftStatus.textContent, "待审核");
assert.strictEqual(document.elements.parameterForm.children.length, 5);

context.confirmDraft();

assert.strictEqual(document.elements.draftStatus.textContent, "Confirmed");
assert.strictEqual(document.elements.confirmButton.disabled, true);
assert.ok(document.elements.resultBody.textContent.includes("op_roughing_feature_001"));
assert.ok(document.elements.traceList.children.length >= 8);

console.log("review ui prototype tests passed");
