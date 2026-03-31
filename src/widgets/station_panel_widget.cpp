#include "widgets/station_panel_widget.h"

#include <QLabel>
#include <QPainter>
#include <QPaintEvent>
#include <QVBoxLayout>

StationPanelWidget::StationPanelWidget(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, false);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAutoFillBackground(false);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(4);

    titleLabel_ = new QLabel(QStringLiteral("站点信息"), this);
    titleLabel_->setStyleSheet(
        "color: rgba(16,44,88,220);"
        "font: 700 13px 'Microsoft YaHei';");
    layout->addWidget(titleLabel_);

    currentLabel_ = new QLabel(this);
    targetLabel_ = new QLabel(this);
    lineLabel_ = new QLabel(this);
    fareLabel_ = new QLabel(this);

    const QString commonStyle =
        QStringLiteral("color: rgba(20,48,86,218); font: 11px 'Microsoft YaHei';");
    currentLabel_->setStyleSheet(commonStyle);
    targetLabel_->setStyleSheet(commonStyle);
    lineLabel_->setStyleSheet(commonStyle);
    fareLabel_->setStyleSheet(
        "color: rgba(24,72,142,232);"
        "font: 700 12px 'Microsoft YaHei';");

    lineLabel_->setWordWrap(true);

    layout->addWidget(currentLabel_);
    layout->addWidget(targetLabel_);
    layout->addWidget(lineLabel_);
    layout->addWidget(fareLabel_);

    clearSelection(QStringLiteral("-"));
}

void StationPanelWidget::clearSelection(const QString& currentStationName)
{
    const QString currentText = currentStationName.isEmpty() ? QStringLiteral("-") : currentStationName;
    currentLabel_->setText(QStringLiteral("当前站点：%1（当前站点）").arg(currentText));
    targetLabel_->setText(QStringLiteral("目标站点：未选择"));
    lineLabel_->setText(QStringLiteral("目标线路：-"));
    fareLabel_->setText(QStringLiteral("票价：-"));
}

void StationPanelWidget::setSelectionInfo(const QString& currentStationName, const QString& targetStationName,
                                          const QStringList& lineNames, int fareYuan)
{
    const QString currentText = currentStationName.isEmpty() ? QStringLiteral("-") : currentStationName;
    currentLabel_->setText(QStringLiteral("当前站点：%1（当前站点）").arg(currentText));
    targetLabel_->setText(QStringLiteral("目标站点：%1").arg(targetStationName));
    lineLabel_->setText(
        QStringLiteral("目标线路：%1")
            .arg(lineNames.isEmpty() ? QStringLiteral("-")
                                     : lineNames.join(QStringLiteral(" / "))));
    if (fareYuan < 0)
    {
        fareLabel_->setText(QStringLiteral("票价：暂无法计算"));
    }
    else
    {
        fareLabel_->setText(QStringLiteral("票价：%1 元").arg(fareYuan));
    }
}

void StationPanelWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF panelRect = rect().adjusted(0.5, 0.5, -0.5, -0.5);
    QLinearGradient bg(panelRect.topLeft(), panelRect.bottomRight());
    bg.setColorAt(0.0, QColor(255, 255, 255, 132));
    bg.setColorAt(1.0, QColor(236, 244, 255, 108));

    painter.setPen(QPen(QColor(255, 255, 255, 185), 1.0));
    painter.setBrush(bg);
    painter.drawRoundedRect(panelRect, 12.0, 12.0);

    QLinearGradient sheen(panelRect.left(), panelRect.top(), panelRect.left(), panelRect.bottom());
    sheen.setColorAt(0.0, QColor(255, 255, 255, 62));
    sheen.setColorAt(1.0, QColor(255, 255, 255, 10));
    painter.setPen(Qt::NoPen);
    painter.setBrush(sheen);
    painter.drawRoundedRect(panelRect.adjusted(1.5, 1.5, -1.5, -1.5), 10.0, 10.0);
}
