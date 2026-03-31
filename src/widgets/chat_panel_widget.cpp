#include "widgets/chat_panel_widget.h"

#include "widgets/voice_assistant_widget.h"

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
    voiceAssistant_->raise();

    addMessage(QStringLiteral("Hello, I am your assistant."),
               ChatBubbleWidget::Speaker::Assistant);
    addMessage(QStringLiteral("Please plan a route from Station A to Station B."),
               ChatBubbleWidget::Speaker::User);
    addMessage(QStringLiteral("Sure, share your travel time and transfer preference."),
               ChatBubbleWidget::Speaker::Assistant);

    layoutVoiceAssistant();
}

void ChatPanelWidget::addMessage(const QString& text, ChatBubbleWidget::Speaker speaker)
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
