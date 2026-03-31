#include "widgets/information_bar_widget.h"

#include <QFile>
#include <QFont>
#include <QFontMetrics>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPaintEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QTimer>

namespace
{
QColor parseColor(const QJsonValue& value, const QColor& fallback)
{
    const QString colorText = value.toString();
    if (colorText.isEmpty())
    {
        return fallback;
    }
    const QColor parsed(colorText);
    return parsed.isValid() ? parsed : fallback;
}
} // namespace

InformationBarWidget::InformationBarWidget(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, false);
    setAutoFillBackground(false);

    loadConfigFromResource();
    rebuildDisplayText();

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(configuredHeight_);

    scrollTimer_ = new QTimer(this);
    scrollTimer_->setInterval(timerIntervalMs_);
    connect(scrollTimer_, &QTimer::timeout, this, &InformationBarWidget::onScrollTick);
    scrollTimer_->start();
}

int InformationBarWidget::barHeight() const
{
    return configuredHeight_;
}

void InformationBarWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.fillRect(rect(), backgroundColor_);

    QFont font(QStringLiteral("Microsoft YaHei"));
    font.setPointSize(fontSize_);
    painter.setFont(font);
    painter.setPen(textColor_);

    const QFontMetrics metrics(font);
    const int textWidth = metrics.horizontalAdvance(displayText_);
    if (displayText_.isEmpty() || textWidth <= 0)
    {
        return;
    }

    const int span = textWidth + textGap_;
    const int baseline = (height() + metrics.ascent() - metrics.descent()) / 2;

    int x = offsetX_;
    while (x > -span)
    {
        x -= span;
    }
    while (x < width())
    {
        painter.drawText(x, baseline, displayText_);
        x += span;
    }
}

void InformationBarWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    update();
}

void InformationBarWidget::loadConfigFromResource()
{
    QFile file(QStringLiteral(":/metro_data/informationBar.json"));
    if (!file.open(QIODevice::ReadOnly))
    {
        messages_ = {
            QStringLiteral("信息公开：苏州地铁线网服务信息实时发布"),
            QStringLiteral("寻物启事：如有遗失物品请联系车站服务台"),
            QStringLiteral("失物招领：可通过官方渠道查询认领"),
        };
        return;
    }

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
    {
        messages_ = {
            QStringLiteral("信息公开：苏州地铁线网服务信息实时发布"),
            QStringLiteral("寻物启事：如有遗失物品请联系车站服务台"),
            QStringLiteral("失物招领：可通过官方渠道查询认领"),
        };
        return;
    }

    const QJsonObject root = doc.object();
    backgroundColor_ = parseColor(root.value("background_color"), backgroundColor_);
    textColor_ = parseColor(root.value("font_color"), textColor_);
    fontSize_ = qBound(9, root.value("font_size").toInt(fontSize_), 28);
    configuredHeight_ = qBound(22, root.value("height").toInt(configuredHeight_), 80);
    scrollSpeed_ = qBound(1, root.value("scroll_speed").toInt(scrollSpeed_), 12);
    textGap_ = qBound(20, root.value("text_gap").toInt(textGap_), 300);
    timerIntervalMs_ = qBound(12, root.value("timer_interval_ms").toInt(timerIntervalMs_), 100);

    messages_.clear();
    const QJsonArray messageArray = root.value("messages").toArray();
    for (const QJsonValue& value : messageArray)
    {
        if (value.isString())
        {
            const QString text = value.toString().trimmed();
            if (!text.isEmpty())
            {
                messages_.push_back(text);
            }
            continue;
        }

        if (value.isObject())
        {
            const QJsonObject obj = value.toObject();
            const QString type = obj.value("type").toString().trimmed();
            const QString content = obj.value("content").toString().trimmed();
            if (type.isEmpty() && content.isEmpty())
            {
                continue;
            }
            if (type.isEmpty())
            {
                messages_.push_back(content);
            }
            else if (content.isEmpty())
            {
                messages_.push_back(type);
            }
            else
            {
                messages_.push_back(QStringLiteral("%1：%2").arg(type, content));
            }
        }
    }

    if (messages_.isEmpty())
    {
        messages_ = {
            QStringLiteral("信息公开：苏州地铁线网服务信息实时发布"),
            QStringLiteral("寻物启事：如有遗失物品请联系车站服务台"),
            QStringLiteral("失物招领：可通过官方渠道查询认领"),
        };
    }
}

void InformationBarWidget::rebuildDisplayText()
{
    if (messages_.isEmpty())
    {
        displayText_ = QStringLiteral("信息更新中");
    }
    else
    {
        displayText_ = messages_.join(QStringLiteral("   |   "));
    }
}

void InformationBarWidget::onScrollTick()
{
    QFont font(QStringLiteral("Microsoft YaHei"));
    font.setPointSize(fontSize_);
    const QFontMetrics metrics(font);
    const int span = metrics.horizontalAdvance(displayText_) + textGap_;
    if (span <= 0)
    {
        return;
    }

    offsetX_ -= scrollSpeed_;
    if (offsetX_ <= -span)
    {
        offsetX_ += span;
    }
    update();
}
