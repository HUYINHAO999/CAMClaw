# 拒绝 AgentPlanDraft 并带原因重新规划

Suggested label: `交给Agent`

User stories covered: 6, 7, 8, 26

## What to build

Build the user path for rejecting an entire AgentPlanDraft with a reason and using that reason as context for the next planning attempt. For example, if the draft includes roughing and finishing but the user says "只要粗加工，不要精加工", the system should reject the whole draft and regenerate a simpler one instead of asking the user to manually delete steps.

The rejected draft should remain traceable, but it should not become a Project or Repository object.

## Acceptance criteria

- [ ] The user can reject an entire AgentPlanDraft before execution.
- [ ] Rejection requires or captures a user-readable reason.
- [ ] The rejection reason is recorded in Trace.
- [ ] The next planning attempt can receive the rejection reason as context.
- [ ] V1 does not support deleting, disabling, or reordering individual draft steps as an alternative to rejection.
- [ ] Tests verify that rejected drafts are not executed.

## Blocked by

- 预览并编辑粗加工 AgentPlanDraft
