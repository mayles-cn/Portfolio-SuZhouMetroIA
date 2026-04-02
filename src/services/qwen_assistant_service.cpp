#include "services/qwen_assistant_service.h"

#include "services/activity_logger.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QtGlobal>
#include <QUrl>

namespace szmetro
{
namespace
{
constexpr int kMaxToolCallRounds = 2;
constexpr int kMaxConversationMessages = 6;
constexpr int kNetworkTimeoutMs = 25000;
constexpr int kMaxNetworkRetries = 1;
constexpr int kStreamMinDurationMs = 850;
constexpr int kStreamMaxDurationMs = 3600;
const char* kQwenCompatibleEndpoint =
    "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions";

QString compactJson(const QJsonObject& object)
{
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}
} // namespace

QwenAssistantService::QwenAssistantService(QObject* parent)
    : QObject(parent)
    , config_(AppConfigService::load())
{
    network_ = new QNetworkAccessManager(this);
    stationNamesRule_ = toolService_.allStationNames().join(QStringLiteral("、"));

    logModelEvent(
        QStringLiteral("llm"),
        QStringLiteral("assistant service initialized"),
        QJsonObject{{QStringLiteral("model"), config_.llmModel},
                    {QStringLiteral("station_count"), toolService_.allStationNames().size()}});

    if (!toolService_.isReady() && !toolService_.lastError().isEmpty())
    {
        emit statusChanged(QStringLiteral("Tool service warning: %1").arg(toolService_.lastError()));
    }
}

void QwenAssistantService::setCurrentStationName(const QString& stationName)
{
    const QString trimmed = stationName.trimmed();
    if (currentStationName_ == trimmed)
    {
        return;
    }
    currentStationName_ = trimmed;
    logModelEvent(QStringLiteral("llm.context"), QStringLiteral("current station updated"),
                  QJsonObject{{QStringLiteral("station"), currentStationName_}});
}

void QwenAssistantService::setModelLogEnabled(bool enabled)
{
    modelLogEnabled_ = enabled;
    ActivityLogger::instance().log(
        QStringLiteral("llm.config"), QStringLiteral("model log switch updated"),
        QJsonObject{{QStringLiteral("enabled"), modelLogEnabled_}});
}

bool QwenAssistantService::isModelLogEnabled() const
{
    return modelLogEnabled_;
}

void QwenAssistantService::requestReply(const QString& userText)
{
    const QString text = userText.trimmed();
    if (text.isEmpty())
    {
        return;
    }

    const QString lowered = text.toLower();
    const bool clearContextCommand =
        text == QStringLiteral("清空上下文") ||
        text == QStringLiteral("清除上下文") ||
        text == QStringLiteral("重置上下文") ||
        text == QStringLiteral("重置对话") ||
        lowered == QStringLiteral("clean out") ||
        lowered == QStringLiteral("clear context") ||
        lowered == QStringLiteral("reset context");
    if (clearContextCommand)
    {
        clearConversation();
        logModelEvent(
            QStringLiteral("llm.request"),
            QStringLiteral("conversation cleared by user command"),
            QJsonObject{{QStringLiteral("input"), text.left(120)}});
        streamReplyText(QStringLiteral("已清空上下文。请提出新的苏州地铁问题。"));
        return;
    }

    logModelEvent(
        QStringLiteral("llm.request"),
        QStringLiteral("request reply"),
        QJsonObject{{QStringLiteral("input"), text.left(200)}});

    if (config_.qwenApiKey.trimmed().isEmpty())
    {
        emit errorOccurred(QStringLiteral("No usable model key found. Check config/api_keys.json."));
        return;
    }

    if (requestInFlight_ || streamActive_)
    {
        queuedUserText_ = text;
        logModelEvent(
            QStringLiteral("llm.request"),
            QStringLiteral("queued while busy"),
            QJsonObject{{QStringLiteral("input"), text.left(200)}});
        emit statusChanged(QStringLiteral("Model is busy. Latest request queued."));
        return;
    }

    beginTurnRequest(text);
}

void QwenAssistantService::beginTurnRequest(const QString& userText)
{
    requestInFlight_ = true;

    conversationHistory_.push_back(
        QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                    {QStringLiteral("content"), userText}});
    trimConversationHistory();

    PendingRequest request;
    request.depth = 0;
    request.retry_count = 0;
    request.messages.push_back(
        QJsonObject{{QStringLiteral("role"), QStringLiteral("system")},
                    {QStringLiteral("content"), buildSystemPrompt()}});

    for (const QJsonValue& message : conversationHistory_)
    {
        request.messages.push_back(message);
    }

    postCompletion(request);
}

void QwenAssistantService::clearConversation()
{
    conversationHistory_ = QJsonArray();
    queuedUserText_.clear();
}

void QwenAssistantService::trimConversationHistory()
{
    while (conversationHistory_.size() > kMaxConversationMessages)
    {
        conversationHistory_.removeAt(0);
    }
}

void QwenAssistantService::continueWithQueuedRequest()
{
    if (requestInFlight_ || streamActive_)
    {
        return;
    }

    const QString queued = queuedUserText_.trimmed();
    if (queued.isEmpty())
    {
        return;
    }

    queuedUserText_.clear();
    beginTurnRequest(queued);
}

QStringList QwenAssistantService::functionCatalog() const
{
    return {
        QStringLiteral("list_lines: 列出全部地铁线路。"),
        QStringLiteral("query_station_lines(station_name): 查询站点所属线路。"),
        QStringLiteral("estimate_fare(from_station_name,to_station_name): 估算票价与里程。"),
        QStringLiteral("open_settlement_panel(): 打开结算界面。")};
}

QString QwenAssistantService::effectiveModel() const
{
    return config_.llmModel;
}

QString QwenAssistantService::buildSystemPrompt() const
{
    QStringList rules = config_.assistantRules;
    if (rules.isEmpty())
    {
        rules = {
            QStringLiteral("默认使用中文，语气专业、简洁、可执行。"),
            QStringLiteral("优先围绕苏州地铁场景回答：线路、站点、票价、换乘。"),
            QStringLiteral("用户问票价、线路归属、线路列表、站点信息时，先调用工具再回答。")};
    }

    QString prompt =
        QStringLiteral("你是苏州地铁智能助手，请严格执行以下规则：\n");
    for (int i = 0; i < rules.size(); ++i)
    {
        prompt += QStringLiteral("%1. %2\n").arg(i + 1).arg(rules[i]);
    }

    prompt += QStringLiteral(
        "硬性要求：\n"
        "- 始终使用中文回复。\n"
        "- 回复尽量控制在 1-3 句，直接给结论。\n"
        "- 使用纯文本，不要使用 Markdown 标题、列表、加粗。\n"
        "- 票价、距离、线路、站点问题必须先调用工具，再给结果。\n"
        "- 只回答当前最新一条用户输入，不要继续回答更早的问题。\n"
        "- 对苏州地铁无关问题，用 1 句话收束并引导回地铁问题。\n"
        "- 需要进入结算或支付界面时，优先调用 open_settlement_panel 工具。\n"
        "- 禁止声称“不支持流式输出”或暴露内部实现细节。\n");

    if (!stationNamesRule_.isEmpty())
    {
        prompt += QStringLiteral("已知站点全量名单（用于纠错和模糊匹配）：\n");
        prompt += stationNamesRule_;
        prompt += QStringLiteral("\n");
    }

    if (!currentStationName_.isEmpty())
    {
        prompt += QStringLiteral("当前站点：");
        prompt += currentStationName_;
        prompt += QStringLiteral("\n");
    }

    return prompt;
}

QJsonArray QwenAssistantService::buildToolSchemas() const
{
    QJsonObject listLinesParams{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject()},
    };
    QJsonObject listLinesFunc{
        {QStringLiteral("name"), QStringLiteral("list_lines")},
        {QStringLiteral("description"), QStringLiteral("List all metro lines")},
        {QStringLiteral("parameters"), listLinesParams},
    };

    QJsonObject stationNameProp{
        {QStringLiteral("type"), QStringLiteral("string")},
        {QStringLiteral("description"), QStringLiteral("Station name")},
    };
    QJsonObject queryStationProps{
        {QStringLiteral("station_name"), stationNameProp},
    };
    QJsonObject queryStationParams{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), queryStationProps},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("station_name")}},
    };
    QJsonObject queryStationFunc{
        {QStringLiteral("name"), QStringLiteral("query_station_lines")},
        {QStringLiteral("description"), QStringLiteral("Find lines of a station")},
        {QStringLiteral("parameters"), queryStationParams},
    };

    QJsonObject fromProp{{QStringLiteral("type"), QStringLiteral("string")}};
    QJsonObject toProp{{QStringLiteral("type"), QStringLiteral("string")}};
    QJsonObject fareProps{
        {QStringLiteral("from_station_name"), fromProp},
        {QStringLiteral("to_station_name"), toProp},
    };
    QJsonObject fareParams{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), fareProps},
        {QStringLiteral("required"),
         QJsonArray{QStringLiteral("from_station_name"), QStringLiteral("to_station_name")}},
    };
    QJsonObject fareFunc{
        {QStringLiteral("name"), QStringLiteral("estimate_fare")},
        {QStringLiteral("description"), QStringLiteral("Estimate fare and distance")},
        {QStringLiteral("parameters"), fareParams},
    };

    QJsonObject settlementParams{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject()},
    };
    QJsonObject settlementFunc{
        {QStringLiteral("name"), QStringLiteral("open_settlement_panel")},
        {QStringLiteral("description"), QStringLiteral("Open settlement panel in UI")},
        {QStringLiteral("parameters"), settlementParams},
    };

    return QJsonArray{
        QJsonObject{{QStringLiteral("type"), QStringLiteral("function")},
                    {QStringLiteral("function"), listLinesFunc}},
        QJsonObject{{QStringLiteral("type"), QStringLiteral("function")},
                    {QStringLiteral("function"), queryStationFunc}},
        QJsonObject{{QStringLiteral("type"), QStringLiteral("function")},
                    {QStringLiteral("function"), fareFunc}},
        QJsonObject{{QStringLiteral("type"), QStringLiteral("function")},
                    {QStringLiteral("function"), settlementFunc}},
    };
}

QJsonObject QwenAssistantService::executeToolCall(const QJsonObject& toolCall)
{
    const QJsonObject functionObject = toolCall.value(QStringLiteral("function")).toObject();
    const QString functionName = functionObject.value(QStringLiteral("name")).toString();
    const QString argumentsText = functionObject.value(QStringLiteral("arguments")).toString();

    QJsonObject argumentsObject;
    if (!argumentsText.trimmed().isEmpty())
    {
        QJsonParseError error;
        const QJsonDocument argDoc = QJsonDocument::fromJson(argumentsText.toUtf8(), &error);
        if (error.error == QJsonParseError::NoError && argDoc.isObject())
        {
            argumentsObject = argDoc.object();
        }
    }

    if (functionName == QStringLiteral("list_lines"))
    {
        return toolService_.toolListLines();
    }
    if (functionName == QStringLiteral("query_station_lines"))
    {
        return toolService_.toolQueryStationLines(
            argumentsObject.value(QStringLiteral("station_name")).toString());
    }
    if (functionName == QStringLiteral("estimate_fare"))
    {
        return toolService_.toolEstimateFare(
            argumentsObject.value(QStringLiteral("from_station_name")).toString(),
            argumentsObject.value(QStringLiteral("to_station_name")).toString());
    }
    if (functionName == QStringLiteral("open_settlement_panel"))
    {
        emit settlementRequested();
        return QJsonObject{
            {QStringLiteral("ok"), true},
            {QStringLiteral("action"), QStringLiteral("open_settlement_panel")},
        };
    }

    return QJsonObject{{QStringLiteral("ok"), false},
                       {QStringLiteral("error"), QStringLiteral("unknown_function")},
                       {QStringLiteral("function_name"), functionName}};
}

QString QwenAssistantService::messageContentToText(const QJsonValue& content) const
{
    if (content.isString())
    {
        return content.toString().trimmed();
    }
    if (!content.isArray())
    {
        return {};
    }

    QStringList fragments;
    for (const QJsonValue& value : content.toArray())
    {
        if (!value.isObject())
        {
            continue;
        }
        const QJsonObject obj = value.toObject();
        if (obj.value(QStringLiteral("type")).toString() == QStringLiteral("text"))
        {
            const QString text = obj.value(QStringLiteral("text")).toString().trimmed();
            if (!text.isEmpty())
            {
                fragments.push_back(text);
            }
        }
    }
    return fragments.join(QStringLiteral("\n"));
}

void QwenAssistantService::postCompletion(const PendingRequest& request)
{
    QNetworkRequest httpRequest(QUrl(QString::fromUtf8(kQwenCompatibleEndpoint)));
    httpRequest.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    httpRequest.setTransferTimeout(kNetworkTimeoutMs);
    httpRequest.setRawHeader("Authorization",
                             QByteArray("Bearer ") + config_.qwenApiKey.trimmed().toUtf8());

    const QJsonObject payload{{QStringLiteral("model"), config_.llmModel},
                              {QStringLiteral("messages"), request.messages},
                              {QStringLiteral("temperature"), 0.3},
                              {QStringLiteral("tools"), buildToolSchemas()},
                              {QStringLiteral("tool_choice"), QStringLiteral("auto")}};

    logModelEvent(
        QStringLiteral("llm.http"),
        QStringLiteral("post completion"),
        QJsonObject{{QStringLiteral("model"), config_.llmModel},
                    {QStringLiteral("message_count"), request.messages.size()},
                    {QStringLiteral("depth"), request.depth},
                    {QStringLiteral("retry_count"), request.retry_count}});

    QNetworkReply* reply = network_->post(httpRequest, QJsonDocument(payload).toJson());
    pendingRequests_.insert(reply, request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleCompletionReply(reply);
    });
}

void QwenAssistantService::handleCompletionReply(QNetworkReply* reply)
{
    const auto requestIt = pendingRequests_.find(reply);
    if (requestIt == pendingRequests_.end())
    {
        reply->deleteLater();
        return;
    }

    const PendingRequest request = requestIt.value();
    pendingRequests_.erase(requestIt);

    const QByteArray body = reply->readAll();
    if (reply->error() != QNetworkReply::NoError)
    {
        const auto errorCode = reply->error();
        const QString errorText = reply->errorString();

        logModelEvent(
            QStringLiteral("llm.http.error"),
            QStringLiteral("network error"),
            QJsonObject{{QStringLiteral("error"), errorText},
                        {QStringLiteral("code"), static_cast<int>(errorCode)},
                        {QStringLiteral("retry_count"), request.retry_count}});

        if ((errorCode == QNetworkReply::TimeoutError ||
             errorCode == QNetworkReply::TemporaryNetworkFailureError) &&
            request.retry_count < kMaxNetworkRetries)
        {
            PendingRequest retryRequest = request;
            retryRequest.retry_count += 1;
            logModelEvent(
                QStringLiteral("llm.http.retry"),
                QStringLiteral("retry after network error"),
                QJsonObject{{QStringLiteral("error"), errorText},
                            {QStringLiteral("next_retry_count"), retryRequest.retry_count}});
            emit statusChanged(QStringLiteral("Model request timed out, retrying once..."));
            reply->deleteLater();
            postCompletion(retryRequest);
            return;
        }

        requestInFlight_ = false;
        emit errorOccurred(QStringLiteral("Model request failed: %1").arg(errorText));
        reply->deleteLater();
        continueWithQueuedRequest();
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
    {
        logModelEvent(
            QStringLiteral("llm.http.error"),
            QStringLiteral("parse error"),
            QJsonObject{{QStringLiteral("error"), parseError.errorString()}});
        requestInFlight_ = false;
        emit errorOccurred(QStringLiteral("Failed to parse model response."));
        reply->deleteLater();
        continueWithQueuedRequest();
        return;
    }

    const QJsonObject root = doc.object();
    const QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty() || !choices.first().isObject())
    {
        logModelEvent(QStringLiteral("llm.http.error"), QStringLiteral("empty choices"));
        requestInFlight_ = false;
        emit errorOccurred(QStringLiteral("Model response is empty."));
        reply->deleteLater();
        continueWithQueuedRequest();
        return;
    }

    const QJsonObject message =
        choices.first().toObject().value(QStringLiteral("message")).toObject();
    if (!queuedUserText_.trimmed().isEmpty())
    {
        logModelEvent(
            QStringLiteral("llm.response"),
            QStringLiteral("drop superseded response"),
            QJsonObject{{QStringLiteral("queued_input"), queuedUserText_.left(200)},
                        {QStringLiteral("depth"), request.depth}});
        requestInFlight_ = false;
        reply->deleteLater();
        continueWithQueuedRequest();
        return;
    }

    const QJsonArray toolCalls = message.value(QStringLiteral("tool_calls")).toArray();
    if (!toolCalls.isEmpty() && request.depth < kMaxToolCallRounds)
    {
        emit statusChanged(QStringLiteral("Model is calling tools..."));

        PendingRequest nextRequest;
        nextRequest.depth = request.depth + 1;
        nextRequest.retry_count = 0;
        nextRequest.messages = request.messages;
        nextRequest.messages.push_back(message);

        for (const QJsonValue& value : toolCalls)
        {
            if (!value.isObject())
            {
                continue;
            }

            const QJsonObject toolCall = value.toObject();
            const QJsonObject result = executeToolCall(toolCall);
            const QString toolName =
                toolCall.value(QStringLiteral("function")).toObject().value(
                    QStringLiteral("name")).toString();
            const QString argumentsText =
                toolCall.value(QStringLiteral("function")).toObject().value(
                    QStringLiteral("arguments")).toString().trimmed();

            emit statusChanged(QStringLiteral("Tool call -> %1 args=%2")
                                   .arg(toolName,
                                        argumentsText.isEmpty() ? QStringLiteral("{}")
                                                                : argumentsText));
            emit statusChanged(
                QStringLiteral("Tool result -> %1 %2").arg(toolName, compactJson(result)));

            logModelEvent(
                QStringLiteral("llm.tool"),
                QStringLiteral("tool call handled"),
                QJsonObject{{QStringLiteral("tool"), toolName},
                            {QStringLiteral("args"),
                             argumentsText.isEmpty() ? QStringLiteral("{}") : argumentsText},
                            {QStringLiteral("result"), compactJson(result)}});

            nextRequest.messages.push_back(
                QJsonObject{{QStringLiteral("role"), QStringLiteral("tool")},
                            {QStringLiteral("tool_call_id"),
                             toolCall.value(QStringLiteral("id")).toString()},
                            {QStringLiteral("name"), toolName},
                            {QStringLiteral("content"), compactJson(result)}});
        }

        postCompletion(nextRequest);
        reply->deleteLater();
        return;
    }

    QString text = messageContentToText(message.value(QStringLiteral("content")));
    if (text.isEmpty())
    {
        text = QStringLiteral("Request completed, but no text content was returned.");
    }

    logModelEvent(
        QStringLiteral("llm.response"),
        QStringLiteral("response ready"),
        QJsonObject{{QStringLiteral("text"), text.left(160)},
                    {QStringLiteral("chars"), text.size()}});

    conversationHistory_.push_back(
        QJsonObject{{QStringLiteral("role"), QStringLiteral("assistant")},
                    {QStringLiteral("content"), text}});
    trimConversationHistory();

    requestInFlight_ = false;
    streamReplyText(text);
    reply->deleteLater();
}

void QwenAssistantService::streamReplyText(const QString& fullText)
{
    const QString text = fullText.trimmed();

    streamBuiltText_.clear();
    streamChunks_.clear();
    streamChunkIndex_ = 0;
    streamDeltaCount_ = 0;
    streamEmittedChars_ = 0;

    if (text.isEmpty())
    {
        emit replyStreamStarted();
        emit replyStreamFinished(QString());
        emit replyReady(QString());
        continueWithQueuedRequest();
        return;
    }

    streamChunks_ = splitIntoStreamChunks(text);
    if (streamChunks_.isEmpty())
    {
        streamChunks_.push_back(text);
    }

    const int targetDurationMs =
        qBound(kStreamMinDurationMs, text.size() * 36, kStreamMaxDurationMs);
    streamBaseDelayMs_ = qBound(22, targetDurationMs / qMax(1, streamChunks_.size()), 140);

    streamActive_ = true;
    streamStartedAtMs_ = QDateTime::currentMSecsSinceEpoch();

    logModelEvent(
        QStringLiteral("llm.stream"),
        QStringLiteral("stream started"),
        QJsonObject{{QStringLiteral("chars"), text.size()},
                    {QStringLiteral("chunks"), streamChunks_.size()},
                    {QStringLiteral("base_delay_ms"), streamBaseDelayMs_}});

    emit replyStreamStarted();
    emitNextStreamChunk();
}

void QwenAssistantService::emitNextStreamChunk()
{
    if (!streamActive_)
    {
        return;
    }

    if (streamChunkIndex_ >= streamChunks_.size())
    {
        streamActive_ = false;
        const qint64 durationMs =
            qMax<qint64>(0, QDateTime::currentMSecsSinceEpoch() - streamStartedAtMs_);

        logModelEvent(
            QStringLiteral("llm.stream"),
            QStringLiteral("stream finished"),
            QJsonObject{{QStringLiteral("duration_ms"), durationMs},
                        {QStringLiteral("deltas"), streamDeltaCount_},
                        {QStringLiteral("chars"), streamEmittedChars_}});

        emit replyStreamFinished(streamBuiltText_);
        emit replyReady(streamBuiltText_);
        continueWithQueuedRequest();
        return;
    }

    const QString delta = streamChunks_.at(streamChunkIndex_);
    ++streamChunkIndex_;

    if (!delta.isEmpty())
    {
        streamBuiltText_ += delta;
        ++streamDeltaCount_;
        streamEmittedChars_ += delta.size();

        logModelEvent(
            QStringLiteral("llm.stream.delta"),
            QStringLiteral("stream delta"),
            QJsonObject{{QStringLiteral("index"), streamChunkIndex_},
                        {QStringLiteral("total"), streamChunks_.size()},
                        {QStringLiteral("len"), delta.size()}});

        emit replyStreamDelta(delta);
    }

    const QChar tail = delta.isEmpty() ? QChar() : delta.back();
    const bool pauseOnBoundary = isSentenceBoundary(tail);
    int delayMs = streamBaseDelayMs_ + (pauseOnBoundary ? 56 : 0);
    if (streamChunkIndex_ == 1)
    {
        delayMs += 36;
    }

    QTimer::singleShot(delayMs, this, [this]() {
        emitNextStreamChunk();
    });
}

QStringList QwenAssistantService::splitIntoStreamChunks(const QString& text)
{
    QStringList chunks;
    if (text.isEmpty())
    {
        return chunks;
    }

    constexpr int kMinChunk = 3;
    constexpr int kMaxChunk = 14;
    int pos = 0;

    while (pos < text.size())
    {
        const int remain = text.size() - pos;
        int chunkLen = qMin(kMaxChunk, remain);

        if (remain > kMinChunk)
        {
            bool foundBoundary = false;
            for (int i = kMinChunk - 1; i < chunkLen; ++i)
            {
                const QChar ch = text.at(pos + i);
                if (isSentenceBoundary(ch))
                {
                    chunkLen = i + 1;
                    foundBoundary = true;
                    break;
                }
            }

            if (!foundBoundary && remain <= (kMaxChunk + 4))
            {
                chunkLen = remain;
            }
        }

        const QString part = text.mid(pos, chunkLen);
        if (!part.isEmpty())
        {
            chunks.push_back(part);
        }
        pos += chunkLen;
    }

    return chunks;
}

void QwenAssistantService::logModelEvent(const QString& category, const QString& message,
                                         const QJsonObject& context) const
{
    if (!modelLogEnabled_)
    {
        return;
    }
    ActivityLogger::instance().log(category, message, context);
}

bool QwenAssistantService::isSentenceBoundary(QChar ch)
{
    return ch == QChar(0xFF0C) || ch == QChar(0x3002) || ch == QChar(0xFF01) ||
           ch == QChar(0xFF1F) || ch == QChar(0xFF1B) || ch == QChar(0xFF1A) ||
           ch == QChar(u',') || ch == QChar(u'.') || ch == QChar(u'!') ||
           ch == QChar(u'?') || ch == QChar(u';') || ch == QChar(u':') ||
           ch == QChar(0x3001) || ch == QChar(u'\n');
}
} // namespace szmetro
