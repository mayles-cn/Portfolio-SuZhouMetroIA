#pragma once

#include <QWidget>

#include "widgets/chat_bubble_widget.h"

class QScrollArea;
class QResizeEvent;
class QTimer;
class QVBoxLayout;
class VoiceAssistantWidget;

namespace szmetro
{
class SpeechRealtimeService;
class QwenAssistantService;
}

class ChatPanelWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit ChatPanelWidget(QWidget* parent = nullptr);

    void addMessage(const QString& text, ChatBubbleWidget::Speaker speaker);
    void submitTextPrompt(const QString& text);
    void appendMapSelectionHint(const QString& stationName);
    void setCurrentStationForModel(const QString& stationName);
    void resetConversationUi();
    void setModelLogEnabled(bool enabled);
    void setModelStatusVisible(bool visible);
    void setVoiceStatusVisible(bool visible);
    void setToolCallVisible(bool visible);
    void setToolResultVisible(bool visible);

signals:
    void promptSubmitted(const QString& text);
    void openSettlementRequested();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void layoutVoiceAssistant();
    ChatBubbleWidget* appendMessageBubble(const QString& text, ChatBubbleWidget::Speaker speaker);
    void updateLiveTranscript(const QString& partialText);
    void finalizeLiveTranscript(const QString& finalText);
    void onVoiceListeningStarted();
    void onVoiceListeningStopped();
    void onAsrPartialUpdated(const QString& text);
    void onAsrFinalReady(const QString& text);
    void onAssistantStreamStarted();
    void onAssistantStreamDelta(const QString& delta);
    void onAssistantStreamFinished(const QString& fullText);
    void onAssistantReplyReady(const QString& text);
    void onAssistantError(const QString& text);
    void onServiceStatus(const QString& text);
    void dispatchPendingRecognizedText();
    bool shouldIgnoreRecognizedText(const QString& text) const;
    QString formatStatusText(const QString& rawText) const;

    QScrollArea* scrollArea_ = nullptr;
    QWidget* messageContainer_ = nullptr;
    QVBoxLayout* messageLayout_ = nullptr;
    VoiceAssistantWidget* voiceAssistant_ = nullptr;
    szmetro::SpeechRealtimeService* speechService_ = nullptr;
    szmetro::QwenAssistantService* assistantService_ = nullptr;
    QWidget* liveTranscriptRow_ = nullptr;
    ChatBubbleWidget* liveTranscriptBubble_ = nullptr;
    ChatBubbleWidget* statusBubble_ = nullptr;
    ChatBubbleWidget* assistantStreamingBubble_ = nullptr;
    QTimer* finalDispatchTimer_ = nullptr;
    QString recognizedSessionText_;
    QString pendingRecognizedText_;
    QString lastSubmittedRecognizedText_;
    QString assistantStreamingText_;
    QString lastStreamedReplyText_;
    qint64 lastSubmittedAtMs_ = 0;
    bool listeningActive_ = false;
    bool assistantStreamActive_ = false;
    bool modelStatusVisible_ = false;
    bool voiceStatusVisible_ = false;
    bool toolCallVisible_ = false;
    bool toolResultVisible_ = false;
};
