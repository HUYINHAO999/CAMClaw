#include "camclaw/qt/AgentDraftService.h"

#include <QByteArray>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace camclaw {

namespace {

std::string toStd(const QString& value)
{
    return value.toUtf8().constData();
}

QString fromStd(const std::string& value)
{
    return QString::fromUtf8(value.c_str());
}

QString jsonStringValue(const QString& json, const QString& key)
{
    const QString needle = "\"" + key + "\"";
    const int key_pos = json.indexOf(needle);
    if (key_pos < 0) {
        return QString();
    }

    const int colon_pos = json.indexOf(":", key_pos + needle.size());
    if (colon_pos < 0) {
        return QString();
    }

    int quote_pos = json.indexOf("\"", colon_pos + 1);
    if (quote_pos < 0) {
        return QString();
    }
    ++quote_pos;

    QString value;
    while (quote_pos < json.size()) {
        const QChar ch = json.at(quote_pos++);
        if (ch == '"') {
            return value;
        }
        if (ch == '\\' && quote_pos < json.size()) {
            value += json.at(quote_pos++);
        } else {
            value += ch;
        }
    }
    return QString();
}

} // namespace

AgentDraftServiceResult::AgentDraftServiceResult()
    : ok(false),
      draft("")
{
}

HttpAgentDraftService::HttpAgentDraftService(const QUrl& endpoint)
    : endpoint_(endpoint)
{
}

AgentDraftServiceResult HttpAgentDraftService::createDraft(const AgentDraftRequest& request)
{
    AgentDraftServiceResult result;

    QNetworkAccessManager manager;
    QNetworkRequest network_request(endpoint_);
    network_request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json; charset=utf-8");
    network_request.setRawHeader("Accept", "application/json");

    QString body;
    body += "{";
    body += "\"trace_id\":" + quoteJson(request.trace_id) + ",";
    body += "\"user_request\":" + quoteJson(request.user_request) + ",";
    body += "\"target_object_id\":" + quoteJson(request.target_object_id) + ",";
    body += "\"target_display_name\":" + quoteJson(request.target_display_name) + ",";
    body += "\"rejection_reason\":" + quoteJson(request.rejection_reason);
    body += "}";

    QNetworkReply* reply = manager.post(network_request, body.toUtf8());
    QEventLoop loop;
    QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    loop.exec();

    const QByteArray response_body = reply->readAll();
    const int status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError) {
        const QString body_text = QString::fromUtf8(response_body);
        const QString backend_error = jsonStringValue(body_text, "error_code");
        const QString backend_message = jsonStringValue(body_text, "message");
        result.error_code = backend_error.isEmpty() ? "backend_unavailable" : backend_error;
        result.message = backend_message.isEmpty() ? reply->errorString() : backend_message;
        reply->deleteLater();
        return result;
    }
    reply->deleteLater();

    if (status_code < 200 || status_code >= 300) {
        const QString body_text = QString::fromUtf8(response_body);
        const QString backend_error = jsonStringValue(body_text, "error_code");
        const QString backend_message = jsonStringValue(body_text, "message");
        result.error_code = backend_error.isEmpty() ? "backend_rejected" : backend_error;
        result.message = backend_message.isEmpty() ? body_text : backend_message;
        return result;
    }

    const AgentPlanDraftDecodeResult decoded = codec_.decodeBackendDraft(std::string(response_body.constData(), response_body.size()));
    if (!decoded.ok) {
        result.error_code = fromStd(decoded.error_code);
        result.message = fromStd(decoded.message);
        return result;
    }

    result.ok = true;
    result.draft = decoded.draft;
    return result;
}

QString HttpAgentDraftService::quoteJson(const QString& value) const
{
    QString escaped = value;
    escaped.replace("\\", "\\\\");
    escaped.replace("\"", "\\\"");
    escaped.replace("\n", "\\n");
    escaped.replace("\r", "\\r");
    return "\"" + escaped + "\"";
}

} // namespace camclaw
