#include "widgets/chat_panel_widget.h"

#include "services/activity_logger.h"
#include "services/qwen_assistant_service.h"
#include "services/speech_realtime_service.h"
#include "widgets/chat_bubble_widget.h"
#include "widgets/voice_assistant_widget.h"

#include <QColor>
#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace
{
constexpr int kPanelMargin = 12;
constexpr int kMessageSpacing = 8;
constexpr int kRowSpacing = 6;
constexpr int kBubbleMaxWidth = 260;
constexpr int kVoiceBottomMargin = 20;
constexpr int kVoiceOverlayReserve = 112;

QString mergeRecognizedText(const QString& base, const QString& delta)
{
    const QString left = base.trimmed();
    const QString right = delta.trimmed();
    if (left.isEmpty())
    {
        return right;
    }
    if (right.isEmpty())
    {
        return left;
    }

    const QChar lastChar = left.back();
    const QChar firstChar = right.front();
    const bool needSpace = !lastChar.isPunct() && !firstChar.isPunct() && lastChar != QChar(' ') &&
                           firstChar != QChar(' ');
    return needSpace ? (left + QStringLiteral(" ") + right) : (left + right);
}
}

ChatPanelWidget::ChatPanelWidget(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet("background-color: rgba(230, 230, 230, 230);");

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(kPanelMargin, kPanelMargin, kPanelMargin, kPanelMargin);
    rootLayout->setSpacing(0);

    scrollArea_ = new QScrollArea(this);
    scrollArea_->setFrameShape(QFrame::NoFrame);
    scrollArea_->setWidgetResizable(true);
    scrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea_->setStyleSheet("background: transparent;");

    messageContainer_ = new QWidget(scrollArea_);
    messageContainer_->setAttribute(Qt::WA_StyledBackground, true);
    messageContainer_->setStyleSheet("background: transparent;");

    messageLayout_ = new QVBoxLayout(messageContainer_);
    messageLayout_->setContentsMargins(0, 0, 0, kVoiceOverlayReserve);
    messageLayout_->setSpacing(kMessageSpacing);
    messageLayout_->addStretch(1);

    scrollArea_->setWidget(messageContainer_);
    rootLayout->addWidget(scrollArea_, 1);

    voiceAssistant_ = new VoiceAssistantWidget(this);
    speechService_ = new szmetro::SpeechRealtimeService(this);
    assistantService_ = new szmetro::QwenAssistantService(this);
    finalDispatchTimer_ = new QTimer(this);
    finalDispatchTimer_->setSingleShot(true);
    finalDispatchTimer_->setInterval(180);
    connect(finalDispatchTimer_, &QTimer::timeout, this,
            &ChatPanelWidget::dispatchPendingRecognizedText);
    szmetro::ActivityLogger::instance().log(
        QStringLiteral("chat"),
        QStringLiteral("chat panel initialized"),
        QJsonObject{{QStringLiteral("speech_service"), speechService_ != nullptr},
                    {QStringLiteral("assistant_service"), assistantService_ != nullptr}});
    voiceAssistant_->raise();

    connect(voiceAssistant_, &VoiceAssistantWidget::listeningStarted, this,
            &ChatPanelWidget::onVoiceListeningStarted);
    connect(voiceAssistant_, &VoiceAssistantWidget::listeningStopped, this,
            &ChatPanelWidget::onVoiceListeningStopped);
    connect(voiceAssistant_, &VoiceAssistantWidget::audioChunkCaptured, speechService_,
            &szmetro::SpeechRealtimeService::appendAudioPcm16);

    connect(speechService_, &szmetro::SpeechRealtimeService::partialTranscriptUpdated, this,
            &ChatPanelWidget::onAsrPartialUpdated);
    connect(speechService_, &szmetro::SpeechRealtimeService::finalTranscriptReady, this,
            &ChatPanelWidget::onAsrFinalReady);
    connect(speechService_, &szmetro::SpeechRealtimeService::errorOccurred, this,
            &ChatPanelWidget::onAssistantError);
    connect(speechService_, &szmetro::SpeechRealtimeService::statusChanged, this,
            &ChatPanelWidget::onServiceStatus);

    connect(assistantService_, &szmetro::QwenAssistantService::replyReady, this,
            &ChatPanelWidget::onAssistantReplyReady);
    connect(assistantService_, &szmetro::QwenAssistantService::replyStreamStarted, this,
            &ChatPanelWidget::onAssistantStreamStarted);
    connect(assistantService_, &szmetro::QwenAssistantService::replyStreamDelta, this,
            &ChatPanelWidget::onAssistantStreamDelta);
    connect(assistantService_, &szmetro::QwenAssistantService::replyStreamFinished, this,
            &ChatPanelWidget::onAssistantStreamFinished);
    connect(assistantService_, &szmetro::QwenAssistantService::errorOccurred, this,
            &ChatPanelWidget::onAssistantError);
    connect(assistantService_, &szmetro::QwenAssistantService::statusChanged, this,
            &ChatPanelWidget::onServiceStatus);
    connect(assistantService_, &szmetro::QwenAssistantService::settlementRequested, this,
            [this]() {
                szmetro::ActivityLogger::instance().log(
                    QStringLiteral("ui.settlement"),
                    QStringLiteral("settlement requested by model tool"));
                emit openSettlementRequested();
            });

    addMessage(
        QStringLiteral(
            "乘客您好，我是苏州地铁智能购票助手，有什么可以帮您？"),
        ChatBubbleWidget::Speaker::Assistant);
    addMessage(
        QStringLiteral(
            "您可以对我说：我想去富强地铁站，帮我买张去东方之门的票，我想去虎丘塔买哪里的票等等。"),
        ChatBubbleWidget::Speaker::Assistant);

    layoutVoiceAssistant();
}

void ChatPanelWidget::addMessage(const QString& text, ChatBubbleWidget::Speaker speaker)
{
    szmetro::ActivityLogger::instance().log(
        QStringLiteral("chat.message"),
        QStringLiteral("add message"),
        QJsonObject{{QStringLiteral("speaker"),
                     speaker == ChatBubbleWidget::Speaker::User ? QStringLiteral("user")
                                                                : QStringLiteral("assistant")},
                    {QStringLiteral("text"), text.left(180)}});
    appendMessageBubble(text, speaker);
}

void ChatPanelWidget::submitTextPrompt(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
    {
        return;
    }

    addMessage(trimmed, ChatBubbleWidget::Speaker::User);
    lastSubmittedRecognizedText_ = trimmed;
    lastSubmittedAtMs_ = QDateTime::currentMSecsSinceEpoch();
    emit promptSubmitted(trimmed);

    if (assistantService_ != nullptr)
    {
        assistantService_->requestReply(trimmed);
    }
}

void ChatPanelWidget::appendMapSelectionHint(const QString& stationName)
{
    const QString name = stationName.trimmed();
    if (name.isEmpty())
    {
        return;
    }

    addMessage(QStringLiteral("地图已选中“%1”站，可继续询问票价、线路或换乘。").arg(name),
               ChatBubbleWidget::Speaker::Assistant);
}

void ChatPanelWidget::setCurrentStationForModel(const QString& stationName)
{
    if (assistantService_ == nullptr)
    {
        return;
    }
    assistantService_->setCurrentStationName(stationName);
}

void ChatPanelWidget::resetConversationUi()
{
    if (assistantService_ != nullptr)
    {
        assistantService_->clearConversation();
    }
    if (finalDispatchTimer_ != nullptr)
    {
        finalDispatchTimer_->stop();
    }

    pendingRecognizedText_.clear();
    recognizedSessionText_.clear();
    lastSubmittedRecognizedText_.clear();
    assistantStreamingText_.clear();
    lastStreamedReplyText_.clear();
    lastSubmittedAtMs_ = 0;
    listeningActive_ = false;
    assistantStreamActive_ = false;
    liveTranscriptRow_ = nullptr;
    liveTranscriptBubble_ = nullptr;
    statusBubble_ = nullptr;
    assistantStreamingBubble_ = nullptr;

    if (scrollArea_ != nullptr)
    {
        if (QWidget* oldWidget = scrollArea_->takeWidget())
        {
            oldWidget->deleteLater();
        }

        messageContainer_ = new QWidget(scrollArea_);
        messageContainer_->setAttribute(Qt::WA_StyledBackground, true);
        messageContainer_->setStyleSheet("background: transparent;");

        messageLayout_ = new QVBoxLayout(messageContainer_);
        messageLayout_->setContentsMargins(0, 0, 0, kVoiceOverlayReserve);
        messageLayout_->setSpacing(kMessageSpacing);
        messageLayout_->addStretch(1);
        scrollArea_->setWidget(messageContainer_);
    }

    addMessage(QStringLiteral("您好，我是苏州地铁智能购票助手，请告诉我您的目的站点。"),
               ChatBubbleWidget::Speaker::Assistant);
    addMessage(QStringLiteral("例如：我要去东方之门，帮我买2张票。"),
               ChatBubbleWidget::Speaker::Assistant);
}

void ChatPanelWidget::setModelLogEnabled(bool enabled)
{
    if (assistantService_ == nullptr)
    {
        return;
    }
    assistantService_->setModelLogEnabled(enabled);
}

void ChatPanelWidget::setModelStatusVisible(bool visible)
{
    modelStatusVisible_ = visible;
}

void ChatPanelWidget::setVoiceStatusVisible(bool visible)
{
    voiceStatusVisible_ = visible;
}

void ChatPanelWidget::setToolCallVisible(bool visible)
{
    toolCallVisible_ = visible;
}

void ChatPanelWidget::setToolResultVisible(bool visible)
{
    toolResultVisible_ = visible;
}

ChatBubbleWidget* ChatPanelWidget::appendMessageBubble(const QString& text,
                                                       ChatBubbleWidget::Speaker speaker)
{
    auto* rowWidget = new QWidget(messageContainer_);
    rowWidget->setAttribute(Qt::WA_StyledBackground, true);
    rowWidget->setStyleSheet("background: transparent;");

    auto* rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(2, 0, 2, 0);
    rowLayout->setSpacing(kRowSpacing);

    auto* bubble = new ChatBubbleWidget(rowWidget);
    bubble->setMaxBubbleWidth(kBubbleMaxWidth);
    bubble->setSpeaker(speaker);
    bubble->setText(text);

    if (speaker == ChatBubbleWidget::Speaker::User)
    {
        rowLayout->addStretch(1);
        rowLayout->addWidget(bubble, 0, Qt::AlignRight);
    }
    else
    {
        rowLayout->addWidget(bubble, 0, Qt::AlignLeft);
        rowLayout->addStretch(1);
    }

    const int insertIndex = qMax(0, messageLayout_->count() - 1);
    messageLayout_->insertWidget(insertIndex, rowWidget);

    QTimer::singleShot(0, this, [this]() {
        if (scrollArea_ != nullptr && scrollArea_->verticalScrollBar() != nullptr)
        {
            scrollArea_->verticalScrollBar()->setValue(
                scrollArea_->verticalScrollBar()->maximum());
        }
    });
    return bubble;
}

void ChatPanelWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    layoutVoiceAssistant();
}

void ChatPanelWidget::layoutVoiceAssistant()
{
    if (voiceAssistant_ == nullptr)
    {
        return;
    }

    const int x = (width() - voiceAssistant_->width()) / 2;
    const int y = height() - voiceAssistant_->height() - kVoiceBottomMargin;
    voiceAssistant_->move(qMax(0, x), qMax(0, y));
    voiceAssistant_->raise();
}

void ChatPanelWidget::updateLiveTranscript(const QString& partialText)
{
    const QString partial = partialText.trimmed();
    if (partial.isEmpty())
    {
        return;
    }
    const QString text = mergeRecognizedText(recognizedSessionText_, partial);

    if (liveTranscriptBubble_ == nullptr)
    {
        liveTranscriptBubble_ = appendMessageBubble(text, ChatBubbleWidget::Speaker::User);
        liveTranscriptRow_ = liveTranscriptBubble_->parentWidget();
    }
    else
    {
        liveTranscriptBubble_->setText(text);
    }
}

void ChatPanelWidget::finalizeLiveTranscript(const QString& finalText)
{
    const QString text = finalText.trimmed();
    if (text.isEmpty())
    {
        return;
    }

    if (liveTranscriptBubble_ != nullptr)
    {
        liveTranscriptBubble_->setText(text);
    }
    else
    {
        appendMessageBubble(text, ChatBubbleWidget::Speaker::User);
    }

}

void ChatPanelWidget::onVoiceListeningStarted()
{
    szmetro::ActivityLogger::instance().log(QStringLiteral("voice"),
                                            QStringLiteral("listening started"));
    listeningActive_ = true;
    recognizedSessionText_.clear();
    pendingRecognizedText_.clear();
    liveTranscriptBubble_ = nullptr;
    liveTranscriptRow_ = nullptr;
    if (finalDispatchTimer_ != nullptr)
    {
        finalDispatchTimer_->stop();
    }
    if (speechService_ != nullptr)
    {
        speechService_->startCapture();
    }
}

void ChatPanelWidget::onVoiceListeningStopped()
{
    szmetro::ActivityLogger::instance().log(QStringLiteral("voice"),
                                            QStringLiteral("listening stopped"));
    listeningActive_ = false;
    if (speechService_ != nullptr)
    {
        speechService_->stopCapture();
    }

    if (!recognizedSessionText_.isEmpty())
    {
        finalizeLiveTranscript(recognizedSessionText_);
    }

    if (finalDispatchTimer_ != nullptr && !pendingRecognizedText_.isEmpty())
    {
        finalDispatchTimer_->start();
    }
}

void ChatPanelWidget::onAsrPartialUpdated(const QString& text)
{
    updateLiveTranscript(text);
}

void ChatPanelWidget::onAsrFinalReady(const QString& text)
{
    const QString recognized = text.trimmed();
    if (recognized.isEmpty())
    {
        return;
    }

    recognizedSessionText_ = mergeRecognizedText(recognizedSessionText_, recognized);
    pendingRecognizedText_ = recognizedSessionText_;
    finalizeLiveTranscript(recognizedSessionText_);
    if (listeningActive_)
    {
        return;
    }
    if (finalDispatchTimer_ != nullptr)
    {
        finalDispatchTimer_->start();
    }
    else
    {
        dispatchPendingRecognizedText();
    }
}

void ChatPanelWidget::onAssistantReplyReady(const QString& text)
{
    const QString answer = text.trimmed();
    if (!answer.isEmpty())
    {
        szmetro::ActivityLogger::instance().log(
            QStringLiteral("llm.reply"), QStringLiteral("assistant reply ready"),
            QJsonObject{{QStringLiteral("text"), answer.left(240)}});
        if (answer != lastStreamedReplyText_)
        {
            addMessage(answer, ChatBubbleWidget::Speaker::Assistant);
        }
        else
        {
            szmetro::ActivityLogger::instance().log(
                QStringLiteral("chat.stream"),
                QStringLiteral("skip duplicate final reply"),
                QJsonObject{{QStringLiteral("chars"), answer.size()}});
        }
    }
    statusBubble_ = nullptr;
}

void ChatPanelWidget::onAssistantStreamStarted()
{
    szmetro::ActivityLogger::instance().log(QStringLiteral("chat.stream"),
                                            QStringLiteral("assistant stream started"));
    assistantStreamActive_ = true;
    assistantStreamingText_.clear();
    assistantStreamingBubble_ =
        appendMessageBubble(QString(), ChatBubbleWidget::Speaker::Assistant);
    statusBubble_ = nullptr;
}

void ChatPanelWidget::onAssistantStreamDelta(const QString& delta)
{
    if (!assistantStreamActive_ || delta.isEmpty())
    {
        return;
    }
    if (assistantStreamingBubble_ == nullptr)
    {
        assistantStreamingBubble_ =
            appendMessageBubble(QString(), ChatBubbleWidget::Speaker::Assistant);
    }

    assistantStreamingText_ += delta;
    assistantStreamingBubble_->setText(assistantStreamingText_);
    if (scrollArea_ != nullptr && scrollArea_->verticalScrollBar() != nullptr)
    {
        scrollArea_->verticalScrollBar()->setValue(scrollArea_->verticalScrollBar()->maximum());
    }
}

void ChatPanelWidget::onAssistantStreamFinished(const QString& fullText)
{
    const QString answer = fullText.trimmed();
    szmetro::ActivityLogger::instance().log(
        QStringLiteral("chat.stream"), QStringLiteral("assistant stream finished"),
        QJsonObject{{QStringLiteral("chars"), answer.size()}});
    if (assistantStreamingBubble_ != nullptr)
    {
        assistantStreamingBubble_->setText(answer);
    }
    assistantStreamActive_ = false;
    assistantStreamingBubble_ = nullptr;
    assistantStreamingText_.clear();
    lastStreamedReplyText_ = answer;
}

void ChatPanelWidget::onAssistantError(const QString& text)
{
    assistantStreamActive_ = false;
    assistantStreamingBubble_ = nullptr;
    assistantStreamingText_.clear();

    QString errorText = text.trimmed();
    szmetro::ActivityLogger::instance().log(
        QStringLiteral("llm.error"), QStringLiteral("assistant error"),
        QJsonObject{{QStringLiteral("error"), errorText}});
    if (errorText == QStringLiteral("No usable model key found. Check config/api_keys.json."))
    {
        errorText = QStringLiteral("未找到可用模型 Key，请检查 config/api_keys.json。");
    }
    else if (errorText == QStringLiteral("No API key found for realtime ASR."))
    {
        errorText = QStringLiteral("未找到实时语音 Key，请检查 config/api_keys.json。");
    }
    else if (errorText.startsWith(QStringLiteral("Realtime ASR socket error: ")))
    {
        errorText.replace(QStringLiteral("Realtime ASR socket error: "),
                          QStringLiteral("实时语音通道错误："));
    }
    else if (errorText.contains(QStringLiteral("Audio is not vaild 'pcm16'"),
                                Qt::CaseInsensitive) ||
             errorText.contains(QStringLiteral("Audio is not valid 'pcm16'"),
                                Qt::CaseInsensitive))
    {
        errorText =
            QStringLiteral("语音格式不匹配：服务端拒绝了 pcm16 音频流，已切换为 pcm 协议，请重试。");
    }
    else if (errorText.contains(QStringLiteral("Error committing input audio buffer"),
                                Qt::CaseInsensitive))
    {
        errorText = QStringLiteral("语音提交失败：当前没有有效音频流，请重试。");
    }
    else if (errorText.startsWith(QStringLiteral("Model request failed: ")))
    {
        errorText.replace(QStringLiteral("Model request failed: "),
                          QStringLiteral("模型请求失败："));
    }
    else if (errorText == QStringLiteral("Failed to parse model response."))
    {
        errorText = QStringLiteral("模型返回解析失败。");
    }
    else if (errorText == QStringLiteral("Model response is empty."))
    {
        errorText = QStringLiteral("模型返回为空。");
    }

    addMessage(errorText.isEmpty() ? QStringLiteral("请求失败，请稍后重试。") : errorText,
               ChatBubbleWidget::Speaker::Assistant);
    statusBubble_ = nullptr;
}

void ChatPanelWidget::onServiceStatus(const QString& text)
{
    const QString statusText = text.trimmed();
    if (statusText.isEmpty())
    {
        return;
    }
    szmetro::ActivityLogger::instance().log(
        QStringLiteral("service.status"), QStringLiteral("service status"),
        QJsonObject{{QStringLiteral("status"), statusText}});

    const bool isToolCall = statusText.startsWith(QStringLiteral("Tool call -> "));
    if (isToolCall && !toolCallVisible_)
    {
        return;
    }

    const bool isToolResult = statusText.startsWith(QStringLiteral("Tool result -> "));
    if (isToolResult && !toolResultVisible_)
    {
        return;
    }

    const bool isModelStatus = statusText.startsWith(QStringLiteral("Model "));
    if (isModelStatus && !modelStatusVisible_)
    {
        return;
    }

    const bool isVoiceStatus =
        statusText == QStringLiteral("Speech started.") ||
        statusText == QStringLiteral("Speech stopped.") ||
        statusText == QStringLiteral("Realtime ASR channel connected.") ||
        statusText == QStringLiteral("Realtime ASR channel disconnected.") ||
        statusText == QStringLiteral("Realtime ASR channel disconnected, reconnecting.") ||
        statusText == QStringLiteral("Realtime ASR channel dropped, reconnecting.");
    if (isVoiceStatus && !voiceStatusVisible_)
    {
        return;
    }

    const QString formatted = formatStatusText(statusText);
    if (formatted.isEmpty())
    {
        return;
    }

    const bool isToolTrace = isToolCall || isToolResult;
    if (isToolTrace)
    {
        ChatBubbleWidget* bubble =
            appendMessageBubble(formatted, ChatBubbleWidget::Speaker::Assistant);
        bubble->setBubbleColor(QColor(241, 245, 252));
        bubble->setTextColor(QColor(64, 77, 96));
        return;
    }

    if (statusBubble_ == nullptr)
    {
        statusBubble_ = appendMessageBubble(formatted, ChatBubbleWidget::Speaker::Assistant);
        statusBubble_->setBubbleColor(QColor(241, 245, 252));
        statusBubble_->setTextColor(QColor(64, 77, 96));
    }
    else
    {
        statusBubble_->setText(formatted);
    }
}

QString ChatPanelWidget::formatStatusText(const QString& rawText) const
{
    if (rawText.startsWith(QStringLiteral("Tool call -> ")))
    {
        const QString payload = rawText.mid(QStringLiteral("Tool call -> ").size()).trimmed();
        return QStringLiteral("【函数调用】\n%1").arg(payload);
    }
    if (rawText.startsWith(QStringLiteral("Tool result -> ")))
    {
        const QString payload = rawText.mid(QStringLiteral("Tool result -> ").size()).trimmed();
        return QStringLiteral("【函数结果】\n%1").arg(payload);
    }
    if (rawText == QStringLiteral("Model is calling tools..."))
    {
        return QStringLiteral("【模型状态】正在调用函数...");
    }
    if (rawText == QStringLiteral("Speech started."))
    {
        return QStringLiteral("【语音状态】检测到语音开始");
    }
    if (rawText == QStringLiteral("Speech stopped."))
    {
        return QStringLiteral("【语音状态】检测到语音结束");
    }
    if (rawText == QStringLiteral("Realtime ASR channel connected."))
    {
        return QStringLiteral("【语音状态】实时语音通道已连接");
    }
    if (rawText == QStringLiteral("Realtime ASR channel disconnected."))
    {
        return QStringLiteral("【语音状态】实时语音通道已断开");
    }
    if (rawText == QStringLiteral("Realtime ASR channel disconnected, reconnecting."))
    {
        return QStringLiteral("【语音状态】实时语音通道已断开，正在重连");
    }
    if (rawText == QStringLiteral("Realtime ASR channel dropped, reconnecting."))
    {
        return QStringLiteral("【语音状态】实时语音通道中断，正在重连");
    }
    if (rawText == QStringLiteral("Model is busy. Latest request queued."))
    {
        return QStringLiteral("【模型状态】模型忙碌中，已排队最新请求");
    }
    if (rawText == QStringLiteral("Model request timed out, retrying once..."))
    {
        return QStringLiteral("【模型状态】请求超时，正在自动重试");
    }
    return QStringLiteral("【状态】%1").arg(rawText);
}

void ChatPanelWidget::dispatchPendingRecognizedText()
{
    const QString recognized = pendingRecognizedText_.trimmed();
    if (recognized.isEmpty())
    {
        return;
    }
    pendingRecognizedText_.clear();

    if (shouldIgnoreRecognizedText(recognized))
    {
        szmetro::ActivityLogger::instance().log(
            QStringLiteral("asr.final"), QStringLiteral("ignored final text"),
            QJsonObject{{QStringLiteral("text"), recognized.left(120)}});
        return;
    }

    if (assistantService_ != nullptr)
    {
        lastSubmittedRecognizedText_ = recognized;
        lastSubmittedAtMs_ = QDateTime::currentMSecsSinceEpoch();
        emit promptSubmitted(recognized);
        assistantService_->requestReply(recognized);
    }

    recognizedSessionText_.clear();
    liveTranscriptBubble_ = nullptr;
    liveTranscriptRow_ = nullptr;
}

bool ChatPanelWidget::shouldIgnoreRecognizedText(const QString& text) const
{
    const QString normalized = text.trimmed();
    if (normalized.isEmpty())
    {
        return true;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (!lastSubmittedRecognizedText_.isEmpty() &&
        normalized == lastSubmittedRecognizedText_ &&
        nowMs - lastSubmittedAtMs_ < 2500)
    {
        return true;
    }

    return false;
}
