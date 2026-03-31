#include "widgets/chat_bubble_widget.h"

#include <QFontMetrics>
#include <QPaintEvent>
#include <QPainter>

ChatBubbleWidget::ChatBubbleWidget(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setSpeaker(Speaker::Assistant);
    setMaximumWidth(maxBubbleWidth_);
}

void ChatBubbleWidget::setSpeaker(Speaker speaker)
{
    speaker_ = speaker;
    if (speaker_ == Speaker::User)
    {
        bubbleColor_ = QColor(45, 140, 255);
        textColor_ = QColor(255, 255, 255);
    }
    else
    {
        bubbleColor_ = QColor(236, 239, 244);
        textColor_ = QColor(25, 30, 36);
    }
    update();
}

ChatBubbleWidget::Speaker ChatBubbleWidget::speaker() const
{
    return speaker_;
}

void ChatBubbleWidget::setText(const QString& text)
{
    text_ = text;
    updateGeometry();
    update();
}

QString ChatBubbleWidget::text() const
{
    return text_;
}

void ChatBubbleWidget::setBubbleColor(const QColor& color)
{
    bubbleColor_ = color;
    update();
}

QColor ChatBubbleWidget::bubbleColor() const
{
    return bubbleColor_;
}

void ChatBubbleWidget::setTextColor(const QColor& color)
{
    textColor_ = color;
    update();
}

QColor ChatBubbleWidget::textColor() const
{
    return textColor_;
}

void ChatBubbleWidget::setCornerRadius(int radius)
{
    cornerRadius_ = qMax(0, radius);
    update();
}

int ChatBubbleWidget::cornerRadius() const
{
    return cornerRadius_;
}

void ChatBubbleWidget::setTextPadding(const QMargins& padding)
{
    textPadding_ = padding;
    updateGeometry();
    update();
}

QMargins ChatBubbleWidget::textPadding() const
{
    return textPadding_;
}

void ChatBubbleWidget::setMaxBubbleWidth(int width)
{
    maxBubbleWidth_ = qMax(80, width);
    setMaximumWidth(maxBubbleWidth_);
    updateGeometry();
    update();
}

int ChatBubbleWidget::maxBubbleWidth() const
{
    return maxBubbleWidth_;
}

QSize ChatBubbleWidget::sizeHint() const
{
    const QSize textSize = computeTextSize();
    const int width = qMin(maxBubbleWidth_,
                           textSize.width() + textPadding_.left() + textPadding_.right());
    const int height = textSize.height() + textPadding_.top() + textPadding_.bottom();
    return QSize(width, height);
}

QSize ChatBubbleWidget::minimumSizeHint() const
{
    return sizeHint();
}

void ChatBubbleWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QRectF bubbleRect = rect();
    bubbleRect.adjust(0.5, 0.5, -0.5, -0.5);
    painter.setPen(Qt::NoPen);
    painter.setBrush(bubbleColor_);
    painter.drawRoundedRect(bubbleRect, cornerRadius_, cornerRadius_);

    painter.setPen(textColor_);
    const QRect textRect = rect().adjusted(textPadding_.left(), textPadding_.top(),
                                           -textPadding_.right(), -textPadding_.bottom());
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, text_);
}

QSize ChatBubbleWidget::computeTextSize() const
{
    const QFontMetrics metrics(font());
    const int wrapWidth =
        qMax(80, maxBubbleWidth_ - textPadding_.left() - textPadding_.right());
    const QString content = text_.isEmpty() ? QStringLiteral(" ") : text_;
    const QRect bounds = metrics.boundingRect(
        QRect(0, 0, wrapWidth, 10000), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
        content);
    return bounds.size();
}
