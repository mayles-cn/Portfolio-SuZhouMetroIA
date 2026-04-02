#pragma once

#include <QByteArray>
#include <QWidget>

class QMouseEvent;
class QPaintEvent;
class QTimer;
class QAudioSource;
class QIODevice;

class VoiceAssistantWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit VoiceAssistantWidget(QWidget* parent = nullptr);

signals:
    void listeningStarted();
    void listeningStopped();
    void audioChunkCaptured(const QByteArray& pcm16Chunk);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    enum class AudioSampleKind
    {
        Int16,
        Int32,
        Float,
        Unknown
    };

    void startListening();
    void stopListening();
    void handleAudioReadyRead();
    QByteArray convertToPcm16(const QByteArray& rawData) const;
    void tickAnimation();

    bool pressed_ = false;
    float level_ = 0.0f;
    float targetRadius_ = 30.0f;
    float currentRadius_ = 30.0f;
    float rotation_ = 0.0f;
    float phase_ = 0.0f;

    QTimer* animationTimer_ = nullptr;
    QAudioSource* audioSource_ = nullptr;
    QIODevice* audioDevice_ = nullptr;
    AudioSampleKind sampleKind_ = AudioSampleKind::Unknown;
    int channelCount_ = 1;
    int sampleRate_ = 16000;
};
