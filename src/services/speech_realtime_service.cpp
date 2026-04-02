#include "services/speech_realtime_service.h"
#include "services/activity_logger.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>

#if defined(SZMETRO_ENABLE_WEBSOCKET_ASR)
#include <QAbstractSocket>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QWebSocket>
#include <QUuid>
#endif

namespace szmetro
{
#if defined(SZMETRO_ENABLE_WEBSOCKET_ASR)
namespace
{
const char* kRealtimeEndpoint = "wss://dashscope.aliyuncs.com/api-ws/v1/realtime";

QString asrTextField(const QJsonObject& eventObject, const QString& key)
{
    const QJsonValue value = eventObject.value(key);
    if (value.isString())
    {
        return value.toString().trimmed();
    }
    if (value.isObject())
    {
        const QJsonObject obj = value.toObject();
        const QString text = obj.value(QStringLiteral("text")).toString().trimmed();
        if (!text.isEmpty())
        {
            return text;
        }
    }
    return {};
}
}
#endif

SpeechRealtimeService::SpeechRealtimeService(QObject* parent)
    : QObject(parent)
    , config_(AppConfigService::load())
{
#if defined(SZMETRO_ENABLE_WEBSOCKET_ASR)
    ActivityLogger::instance().log(
        QStringLiteral("asr"),
        QStringLiteral("speech realtime service initialized"),
        QJsonObject{{QStringLiteral("model"), config_.asrRealtimeModel}});
    socket_ = new QWebSocket();

    connect(socket_, &QWebSocket::connected, this, [this]() {
        sessionReady_ = false;
        sendSessionUpdate();
        ActivityLogger::instance().log(QStringLiteral("asr.socket"), QStringLiteral("connected"));
        emit statusChanged(QStringLiteral("Realtime ASR channel connected."));
    });

    connect(socket_, &QWebSocket::disconnected, this, [this]() {
        sessionReady_ = false;
        ActivityLogger::instance().log(QStringLiteral("asr.socket"), QStringLiteral("disconnected"));
        if (captureRequested_)
        {
            emit statusChanged(QStringLiteral("Realtime ASR channel disconnected, reconnecting."));
            QTimer::singleShot(200, this, [this]() {
                ensureConnected();
            });
        }
    });

    connect(socket_, &QWebSocket::textMessageReceived, this, [this](const QString& text) {
        QJsonParseError error;
        const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject())
        {
            return;
        }
        handleIncomingEvent(doc.object());
    });

    connect(socket_,
            QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::errorOccurred), this,
            [this](QAbstractSocket::SocketError error) {
                const QString socketError = socket_ == nullptr ? QString() : socket_->errorString();
                const bool remoteHostClosed =
                    error == QAbstractSocket::RemoteHostClosedError ||
                    socketError.contains(QStringLiteral("Remote host closed"),
                                         Qt::CaseInsensitive);
                if (remoteHostClosed)
                {
                    ActivityLogger::instance().log(
                        QStringLiteral("asr.socket.info"), QStringLiteral("remote host closed"),
                        QJsonObject{{QStringLiteral("capture_requested"), captureRequested_}});
                    if (captureRequested_)
                    {
                        emit statusChanged(
                            QStringLiteral("Realtime ASR channel dropped, reconnecting."));
                    }
                    return;
                }
                ActivityLogger::instance().log(
                    QStringLiteral("asr.socket.error"), QStringLiteral("socket error"),
                    QJsonObject{{QStringLiteral("error"), socketError}});
                emit errorOccurred(
                    QStringLiteral("Realtime ASR socket error: %1").arg(socketError));
            });
    if (!config_.qwenApiKey.trimmed().isEmpty())
    {
        QTimer::singleShot(0, this, [this]() {
            ensureConnected();
        });
    }
#else
    ActivityLogger::instance().log(
        QStringLiteral("asr"), QStringLiteral("fallback mode"),
        QJsonObject{{QStringLiteral("reason"), QStringLiteral("Qt WebSockets missing")}});
    emit statusChanged(
        QStringLiteral("Realtime ASR fallback mode: Qt WebSockets component is unavailable."));
#endif
}

SpeechRealtimeService::~SpeechRealtimeService()
{
#if defined(SZMETRO_ENABLE_WEBSOCKET_ASR)
    if (socket_ != nullptr)
    {
        socket_->close();
        socket_->deleteLater();
        socket_ = nullptr;
    }
#endif
}

void SpeechRealtimeService::startCapture()
{
    if (config_.qwenApiKey.trimmed().isEmpty())
    {
        ActivityLogger::instance().log(QStringLiteral("asr.error"),
                                       QStringLiteral("missing api key"));
        emit errorOccurred(QStringLiteral("No API key found for realtime ASR."));
        return;
    }

#if defined(SZMETRO_ENABLE_WEBSOCKET_ASR)
    captureRequested_ = true;
    hasAudioInCurrentCapture_ = false;
    serverCommittedInCurrentCapture_ = false;
    lastServerSpeechStoppedAtMs_ = 0;
    ++captureGeneration_;
    ActivityLogger::instance().log(QStringLiteral("asr.capture"), QStringLiteral("start capture"));
    ensureConnected();
#else
    ActivityLogger::instance().log(QStringLiteral("asr.error"), QStringLiteral("websocket disabled"));
    emit errorOccurred(
        QStringLiteral("Realtime ASR is disabled because Qt WebSockets is not installed."));
#endif
}

void SpeechRealtimeService::stopCapture()
{
#if defined(SZMETRO_ENABLE_WEBSOCKET_ASR)
    captureRequested_ = false;
    ActivityLogger::instance().log(QStringLiteral("asr.capture"), QStringLiteral("stop capture"));
    if (!hasAudioInCurrentCapture_)
    {
        return;
    }
    if (socket_ == nullptr || socket_->state() != QAbstractSocket::ConnectedState)
    {
        hasAudioInCurrentCapture_ = false;
        return;
    }
    if (!sessionReady_)
    {
        hasAudioInCurrentCapture_ = false;
        return;
    }
    if (serverCommittedInCurrentCapture_)
    {
        hasAudioInCurrentCapture_ = false;
        return;
    }
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (lastServerSpeechStoppedAtMs_ > 0 &&
        nowMs - lastServerSpeechStoppedAtMs_ < 1200)
    {
        ActivityLogger::instance().log(
            QStringLiteral("asr.capture"), QStringLiteral("skip commit after server speech stop"),
            QJsonObject{{QStringLiteral("delta_ms"),
                         nowMs - lastServerSpeechStoppedAtMs_}});
        hasAudioInCurrentCapture_ = false;
        return;
    }
    const quint64 generation = captureGeneration_;
    QTimer::singleShot(300, this, [this, generation]() {
        if (!hasAudioInCurrentCapture_)
        {
            return;
        }
        if (generation != captureGeneration_ || serverCommittedInCurrentCapture_)
        {
            hasAudioInCurrentCapture_ = false;
            return;
        }
        if (socket_ == nullptr || socket_->state() != QAbstractSocket::ConnectedState ||
            !sessionReady_)
        {
            hasAudioInCurrentCapture_ = false;
            return;
        }
        sendEvent(QJsonObject{{QStringLiteral("type"), QStringLiteral("input_audio_buffer.commit")}});
        ActivityLogger::instance().log(
            QStringLiteral("asr.capture"), QStringLiteral("commit sent (delayed)"));
        hasAudioInCurrentCapture_ = false;
    });
#endif
}

void SpeechRealtimeService::appendAudioPcm16(const QByteArray& pcm16Chunk)
{
#if defined(SZMETRO_ENABLE_WEBSOCKET_ASR)
    if (!captureRequested_ || pcm16Chunk.isEmpty())
    {
        return;
    }
    hasAudioInCurrentCapture_ = true;

    if (sessionReady_ && socket_ != nullptr &&
        socket_->state() == QAbstractSocket::ConnectedState)
    {
        sendEvent(QJsonObject{
            {QStringLiteral("type"), QStringLiteral("input_audio_buffer.append")},
            {QStringLiteral("audio"), QString::fromLatin1(pcm16Chunk.toBase64())},
        });
        return;
    }

    if (pendingAudioChunks_.size() < 80)
    {
        pendingAudioChunks_.push_back(pcm16Chunk);
    }
#else
    Q_UNUSED(pcm16Chunk);
#endif
}

void SpeechRealtimeService::ensureConnected()
{
#if defined(SZMETRO_ENABLE_WEBSOCKET_ASR)
    if (socket_ == nullptr)
    {
        return;
    }
    if (socket_->state() == QAbstractSocket::ConnectedState ||
        socket_->state() == QAbstractSocket::ConnectingState)
    {
        return;
    }

    QUrl url(QString::fromUtf8(kRealtimeEndpoint));
    QUrlQuery query(url);
    query.addQueryItem(QStringLiteral("model"), config_.asrRealtimeModel);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("Authorization",
                         QByteArray("Bearer ") + config_.qwenApiKey.trimmed().toUtf8());
    ActivityLogger::instance().log(
        QStringLiteral("asr.socket"), QStringLiteral("open websocket"),
        QJsonObject{{QStringLiteral("url"), url.toString()}});
    socket_->open(request);
#endif
}

void SpeechRealtimeService::sendSessionUpdate()
{
#if defined(SZMETRO_ENABLE_WEBSOCKET_ASR)
    sendEvent(QJsonObject{
        {QStringLiteral("event_id"), QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {QStringLiteral("type"), QStringLiteral("session.update")},
        {QStringLiteral("session"),
         QJsonObject{
             {QStringLiteral("output_modalities"), QJsonArray{QStringLiteral("text")}},
             {QStringLiteral("enable_input_audio_transcription"), true},
             {QStringLiteral("transcription_params"),
              QJsonObject{
                  {QStringLiteral("model"), config_.asrRealtimeModel},
                  {QStringLiteral("language"), QStringLiteral("zh")},
                  {QStringLiteral("sample_rate"), 16000},
                  {QStringLiteral("input_audio_format"), QStringLiteral("pcm")},
              }},
         }},
    });
#endif
}

void SpeechRealtimeService::sendEvent(const QJsonObject& eventObject)
{
#if defined(SZMETRO_ENABLE_WEBSOCKET_ASR)
    QJsonObject payload = eventObject;
    if (!payload.contains(QStringLiteral("event_id")))
    {
        payload.insert(QStringLiteral("event_id"),
                       QUuid::createUuid().toString(QUuid::WithoutBraces));
    }
    if (socket_ == nullptr || socket_->state() != QAbstractSocket::ConnectedState)
    {
        return;
    }
    socket_->sendTextMessage(
        QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact)));
#else
    Q_UNUSED(eventObject);
#endif
}

void SpeechRealtimeService::flushBufferedAudio()
{
#if defined(SZMETRO_ENABLE_WEBSOCKET_ASR)
    while (!pendingAudioChunks_.isEmpty())
    {
        const QByteArray chunk = pendingAudioChunks_.front();
        pendingAudioChunks_.pop_front();
        appendAudioPcm16(chunk);
    }
#endif
}

void SpeechRealtimeService::handleIncomingEvent(const QJsonObject& eventObject)
{
#if defined(SZMETRO_ENABLE_WEBSOCKET_ASR)
    const QString type = eventObject.value(QStringLiteral("type")).toString();
    if (type != QStringLiteral("conversation.item.input_audio_transcription.text"))
    {
        ActivityLogger::instance().log(
            QStringLiteral("asr.event"), QStringLiteral("incoming event"),
            QJsonObject{{QStringLiteral("type"), type}});
    }
    if (type == QStringLiteral("session.created") || type == QStringLiteral("session.updated"))
    {
        sessionReady_ = true;
        flushBufferedAudio();
        return;
    }

    if (type == QStringLiteral("conversation.item.input_audio_transcription.text"))
    {
        const QString stash = asrTextField(eventObject, QStringLiteral("stash"));
        if (!stash.isEmpty())
        {
            emit partialTranscriptUpdated(stash);
        }
        return;
    }

    if (type == QStringLiteral("conversation.item.input_audio_transcription.completed"))
    {
        const QString transcript = asrTextField(eventObject, QStringLiteral("transcript"));
        if (!transcript.isEmpty())
        {
            hasAudioInCurrentCapture_ = false;
            ActivityLogger::instance().log(
                QStringLiteral("asr.final"), QStringLiteral("final text"),
                QJsonObject{{QStringLiteral("text"), transcript}});
            emit finalTranscriptReady(transcript);
        }
        return;
    }

    if (type == QStringLiteral("input_audio_buffer.speech_started"))
    {
        emit statusChanged(QStringLiteral("Speech started."));
        return;
    }

    if (type == QStringLiteral("input_audio_buffer.committed"))
    {
        serverCommittedInCurrentCapture_ = true;
        hasAudioInCurrentCapture_ = false;
        return;
    }

    if (type == QStringLiteral("input_audio_buffer.speech_stopped"))
    {
        lastServerSpeechStoppedAtMs_ = QDateTime::currentMSecsSinceEpoch();
        emit statusChanged(QStringLiteral("Speech stopped."));
        return;
    }

    if (type == QStringLiteral("session.finished"))
    {
        sessionReady_ = false;
        emit statusChanged(QStringLiteral("Realtime ASR session finished."));
        return;
    }

    if (type == QStringLiteral("error"))
    {
        const QJsonObject errorObj = eventObject.value(QStringLiteral("error")).toObject();
        const QString message = errorObj.value(QStringLiteral("message")).toString().trimmed();
        ActivityLogger::instance().log(
            QStringLiteral("asr.error"), QStringLiteral("api error"),
            QJsonObject{{QStringLiteral("message"), message},
                        {QStringLiteral("payload"),
                         QString::fromUtf8(
                             QJsonDocument(eventObject).toJson(QJsonDocument::Compact))}});
        emit errorOccurred(
            message.isEmpty() ? QStringLiteral("Realtime ASR API returned an error.") : message);
    }
#else
    Q_UNUSED(eventObject);
#endif
}
} // namespace szmetro
