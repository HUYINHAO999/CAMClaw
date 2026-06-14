# 预留模板和 RAG 推荐扩展接口

Suggested label: `交给人`

User stories covered: 23, 29, 30

## What to build

Reserve an extension interface for future templates, RAG-based process recommendations, or enterprise machining knowledge without implementing the recommendation logic in this issue. The extension should only influence how an AgentPlanDraft is proposed; it must not change the final review and execution boundary.

The decision to preserve is: templates or RAG may help produce better drafts, but users still review the AgentPlanDraft, Skills still execute approved structured steps, and CAM software still executes through Gateway, Adapter, and Component Console.

## Acceptance criteria

- [ ] There is a documented extension point for future template or RAG recommendation providers.
- [ ] The extension point can provide candidate draft inputs or planning context without executing CAM commands directly.
- [ ] The extension point does not bypass AgentPlanDraft review.
- [ ] The extension point does not bypass Gateway, Adapter, or Component Console execution.
- [ ] No real RAG retrieval, embedding storage, enterprise knowledge base, or recommendation algorithm is implemented in this issue.
- [ ] Documentation states that final confirmation flow remains unchanged when template or RAG support is added.

## Blocked by

- 预览并编辑粗加工 AgentPlanDraft
