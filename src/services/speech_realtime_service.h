#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <QtGlobal>

#include "services/app_config_service.h"

class QWebSocket;

namespace szmetro
{
class SpeechRealtimeService final : public QObject
{
    Q_OBJECT

public:
    explicit SpeechRealtimeService(QObject* parent = nullptr);
    ~SpeechRealtimeService() override;

    void startCapture();
    void stopCapture();
    void appendAudioPcm16(const QByteArray& pcm16Chunk);

signals:
    void partialTranscriptUpdated(const QString& text);
    void finalTranscriptReady(const QString& text);
    void errorOccurred(const QString& errorText);
    void statusChanged(const QString& statusText);

private:
    void ensureConnected();
    void sendSessionUpdate();
    void sendEvent(const QJsonObject& eventObject);
    void flushBufferedAudio();
    void handleIncomingEvent(const QJsonObject& eventObject);

    AppConfig config_;
    QWebSocket* socket_ = nullptr;
    QList<QByteArray> pendingAudioChunks_;
    bool captureRequested_ = false;
    bool sessionReady_ = false;
    bool hasAudioInCurrentCapture_ = false;
    bool serverCommittedInCurrentCapture_ = false;
    qint64 lastServerSpeechStoppedAtMs_ = 0;
    quint64 captureGeneration_ = 0;
};
} // namespace szmetro
