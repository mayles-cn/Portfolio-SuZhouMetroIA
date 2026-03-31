#pragma once

#include <QWidget>

#include "widgets/chat_bubble_widget.h"

class QScrollArea;
class QResizeEvent;
class QVBoxLayout;
class VoiceAssistantWidget;

class ChatPanelWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit ChatPanelWidget(QWidget* parent = nullptr);

    void addMessage(const QString& text, ChatBubbleWidget::Speaker speaker);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void layoutVoiceAssistant();

    QScrollArea* scrollArea_ = nullptr;
    QWidget* messageContainer_ = nullptr;
    QVBoxLayout* messageLayout_ = nullptr;
    VoiceAssistantWidget* voiceAssistant_ = nullptr;
};
