#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QString>
#include <QStringList>

#include "services/app_config_service.h"
#include "services/metro_tool_service.h"

class QNetworkAccessManager;
class QNetworkReply;

namespace szmetro
{
class QwenAssistantService final : public QObject
{
    Q_OBJECT

public:
    explicit QwenAssistantService(QObject* parent = nullptr);

    void requestReply(const QString& userText);
    void clearConversation();
    void setCurrentStationName(const QString& stationName);
    void setModelLogEnabled(bool enabled);
    bool isModelLogEnabled() const;

    QStringList functionCatalog() const;
    QString effectiveModel() const;

signals:
    void replyReady(const QString& text);
    void replyStreamStarted();
    void replyStreamDelta(const QString& delta);
    void replyStreamFinished(const QString& fullText);
    void settlementRequested();
    void errorOccurred(const QString& errorText);
    void statusChanged(const QString& statusText);

private:
    struct PendingRequest
    {
        QJsonArray messages;
        int depth = 0;
        int retry_count = 0;
    };

    void beginTurnRequest(const QString& userText);
    void trimConversationHistory();
    void continueWithQueuedRequest();
    QString buildSystemPrompt() const;
    QJsonArray buildToolSchemas() const;
    QJsonObject executeToolCall(const QJsonObject& toolCall);
    QString messageContentToText(const QJsonValue& content) const;
    void postCompletion(const PendingRequest& request);
    void handleCompletionReply(QNetworkReply* reply);
    void logModelEvent(const QString& category, const QString& message,
                       const QJsonObject& context = QJsonObject()) const;

    void streamReplyText(const QString& fullText);
    void emitNextStreamChunk();
    static QStringList splitIntoStreamChunks(const QString& text);
    static bool isSentenceBoundary(QChar ch);

    AppConfig config_;
    MetroToolService toolService_;
    QNetworkAccessManager* network_ = nullptr;
    QJsonArray conversationHistory_;
    QHash<QNetworkReply*, PendingRequest> pendingRequests_;
    bool requestInFlight_ = false;
    QString queuedUserText_;

    QString stationNamesRule_;
    QString currentStationName_;
    QString streamBuiltText_;
    QStringList streamChunks_;
    int streamChunkIndex_ = 0;
    int streamBaseDelayMs_ = 40;
    int streamDeltaCount_ = 0;
    int streamEmittedChars_ = 0;
    qint64 streamStartedAtMs_ = 0;
    bool streamActive_ = false;
    bool modelLogEnabled_ = true;
};
} // namespace szmetro
