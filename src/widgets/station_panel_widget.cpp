#include "widgets/station_panel_widget.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPushButton>
#include <QVBoxLayout>

namespace
{
QIcon gotoIcon()
{
    return QIcon(QStringLiteral(":/icons/goto.ico"));
}
} // namespace

StationPanelWidget::StationPanelWidget(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, false);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAutoFillBackground(false);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(5);

    titleLabel_ = new QLabel(QStringLiteral("站点信息"), this);
    titleLabel_->setStyleSheet(
        "color: rgba(16,44,88,228);"
        "font: 700 13px 'Microsoft YaHei';"
        "background: transparent;");
    layout->addWidget(titleLabel_);

    currentLabel_ = new QLabel(this);
    targetLabel_ = new QLabel(this);
    lineLabel_ = new QLabel(this);
    fareLabel_ = new QLabel(this);
    distanceLabel_ = new QLabel(this);
    etaLabel_ = new QLabel(this);

    const QString commonStyle = QStringLiteral(
        "color: rgba(20,48,86,220);"
        "font: 11px 'Microsoft YaHei';"
        "background: transparent;");
    currentLabel_->setStyleSheet(commonStyle);
    targetLabel_->setStyleSheet(commonStyle);
    lineLabel_->setStyleSheet(commonStyle);
    fareLabel_->setStyleSheet(
        "color: rgba(24,72,142,236);"
        "font: 700 12px 'Microsoft YaHei';"
        "background: transparent;");
    distanceLabel_->setStyleSheet(commonStyle);
    etaLabel_->setStyleSheet(
        "color: rgba(20,72,126,230);"
        "font: 700 11px 'Microsoft YaHei';"
        "background: transparent;");
    lineLabel_->setWordWrap(true);

    layout->addWidget(currentLabel_);
    layout->addWidget(targetLabel_);
    layout->addWidget(lineLabel_);
    layout->addWidget(fareLabel_);
    layout->addWidget(distanceLabel_);
    layout->addWidget(etaLabel_);

    auto* actionRow = new QHBoxLayout();
    actionRow->setContentsMargins(0, 6, 0, 0);
    actionRow->setSpacing(6);

    goButton_ = new QPushButton(QStringLiteral("去这里"), this);
    goButton_->setIcon(gotoIcon());
    goButton_->setIconSize(QSize(16, 16));
    goButton_->setCursor(Qt::PointingHandCursor);
    goButton_->setEnabled(false);
    goButton_->setStyleSheet(
        "QPushButton {"
        "background-color: rgba(30,102,208,232);"
        "color: white;"
        "border: none;"
        "border-radius: 8px;"
        "font: 700 11px 'Microsoft YaHei';"
        "padding: 6px 10px;"
        "}"
        "QPushButton:hover { background-color: rgba(30,102,208,248); }"
        "QPushButton:disabled { background-color: rgba(138,152,176,164); }");
    actionRow->addWidget(goButton_, 0);
    actionRow->addStretch(1);
    layout->addLayout(actionRow);

    connect(goButton_, &QPushButton::clicked, this, [this]() {
        if (!targetStationName_.isEmpty())
        {
            emit goToHereClicked(targetStationName_);
        }
    });
    clearSelection(QStringLiteral("-"));
}

void StationPanelWidget::clearSelection(const QString& currentStationName)
{
    const QString currentText = currentStationName.isEmpty() ? QStringLiteral("-") : currentStationName;
    currentLabel_->setText(QStringLiteral("当前站点：%1").arg(currentText));
    targetLabel_->setText(QStringLiteral("目标站点：-"));
    lineLabel_->setText(QStringLiteral("线路：-"));
    fareLabel_->setText(QStringLiteral("票价：-"));
    distanceLabel_->setText(QStringLiteral("里程：-"));
    etaLabel_->setText(QStringLiteral("预计时长：-"));

    targetStationName_.clear();
    if (goButton_ != nullptr)
    {
        goButton_->setEnabled(false);
    }
}

void StationPanelWidget::setSelectionInfo(const QString& currentStationName,
                                          const QString& targetStationName,
                                          const QStringList& lineNames, int fareYuan,
                                          double distanceKm, int etaMinutes)
{
    const QString currentText = currentStationName.isEmpty() ? QStringLiteral("-") : currentStationName;
    currentLabel_->setText(QStringLiteral("当前站点：%1").arg(currentText));
    targetLabel_->setText(QStringLiteral("目标站点：%1").arg(targetStationName));
    lineLabel_->setText(QStringLiteral("线路：%1")
                            .arg(lineNames.isEmpty() ? QStringLiteral("-")
                                                     : lineNames.join(QStringLiteral(" / "))));
    if (fareYuan < 0)
    {
        fareLabel_->setText(QStringLiteral("票价：暂无"));
    }
    else
    {
        fareLabel_->setText(QStringLiteral("票价：%1 元").arg(fareYuan));
    }

    if (distanceKm < 0.0)
    {
        distanceLabel_->setText(QStringLiteral("里程：-"));
    }
    else
    {
        distanceLabel_->setText(QStringLiteral("里程：%1 km").arg(distanceKm, 0, 'f', 1));
    }

    if (etaMinutes < 0)
    {
        etaLabel_->setText(QStringLiteral("预计时长：-"));
    }
    else
    {
        etaLabel_->setText(QStringLiteral("预计时长：%1 分钟").arg(etaMinutes));
    }

    targetStationName_ = targetStationName.trimmed();
    if (goButton_ != nullptr)
    {
        goButton_->setEnabled(!targetStationName_.isEmpty());
    }
}

void StationPanelWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF panelRect = rect().adjusted(1.0, 1.0, -1.0, -1.0);
    QPainterPath panelPath;
    panelPath.addRoundedRect(panelRect, 14.0, 14.0);

    QLinearGradient bg(panelRect.topLeft(), panelRect.bottomLeft());
    bg.setColorAt(0.0, QColor(255, 255, 255, 188));
    bg.setColorAt(1.0, QColor(232, 242, 255, 124));
    painter.fillPath(panelPath, bg);
}
