#ifndef CAMCLAW_QT_AGENT_DRAFT_SERVICE_H
#define CAMCLAW_QT_AGENT_DRAFT_SERVICE_H

#include "camclaw/AgentPlanDraft.h"
#include "camclaw/AgentPlanDraftJsonCodec.h"

#include <QString>
#include <QUrl>

namespace camclaw {

struct AgentDraftRequest {
    QString trace_id;
    QString user_request;
    QString target_object_id;
    QString target_display_name;
    QString rejection_reason;
};

struct AgentDraftServiceResult {
    AgentDraftServiceResult();

    bool ok;
    AgentPlanDraft draft;
    QString error_code;
    QString message;
};

class AgentDraftService {
public:
    virtual ~AgentDraftService()
    {
    }

    virtual AgentDraftServiceResult createDraft(const AgentDraftRequest& request) = 0;
};

class HttpAgentDraftService : public AgentDraftService {
public:
    explicit HttpAgentDraftService(const QUrl& endpoint);

    AgentDraftServiceResult createDraft(const AgentDraftRequest& request);

private:
    QString quoteJson(const QString& value) const;

    QUrl endpoint_;
    AgentPlanDraftJsonCodec codec_;
};

} // namespace camclaw

#endif // CAMCLAW_QT_AGENT_DRAFT_SERVICE_H
