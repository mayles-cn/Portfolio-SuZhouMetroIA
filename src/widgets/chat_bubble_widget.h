#pragma once

#include <QColor>
#include <QMargins>
#include <QString>
#include <QWidget>
class QPaintEvent;

class ChatBubbleWidget final : public QWidget
{
    Q_OBJECT

public:
    enum class Speaker
    {
        User,
        Assistant
    };

    explicit ChatBubbleWidget(QWidget* parent = nullptr);

    void setSpeaker(Speaker speaker);
    Speaker speaker() const;

    void setText(const QString& text);
    QString text() const;

    void setBubbleColor(const QColor& color);
    QColor bubbleColor() const;

    void setTextColor(const QColor& color);
    QColor textColor() const;

    void setCornerRadius(int radius);
    int cornerRadius() const;

    void setTextPadding(const QMargins& padding);
    QMargins textPadding() const;

    void setMaxBubbleWidth(int width);
    int maxBubbleWidth() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QSize computeTextSize() const;

    Speaker speaker_ = Speaker::Assistant;
    QString text_;
    QColor bubbleColor_;
    QColor textColor_;
    int cornerRadius_ = 14;
    QMargins textPadding_ = QMargins(14, 10, 14, 10);
    int maxBubbleWidth_ = 320;
};
