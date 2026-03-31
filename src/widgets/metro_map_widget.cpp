#include "widgets/metro_map_widget.h"

#include "widgets/station_panel_widget.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCoreApplication>
#include <QDir>
#include <QFont>
#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPolygon>
#include <QPushButton>
#include <QRandomGenerator>
#include <QResizeEvent>
#include <QSet>
#include <QTimer>
#include <QVariantAnimation>
#include <QVector>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidget>
#include <cmath>
#include <unordered_map>
#include <limits>

namespace
{
constexpr int kOuterMargin = 20;
constexpr int kPanelHeight = 26;
constexpr int kPanelGap = 14;
constexpr int kStationPanelWidth = 280;
constexpr int kStationPanelHeight = 146;
constexpr int kLineDrawWidthFocused = 6;
constexpr int kLineDrawWidthOverview = 3;
constexpr int kGrayLineDrawWidth = 2;
constexpr qreal kStationRadius = 3.2;
constexpr qreal kInterchangeRadius = 4.2;
constexpr qreal kStationPickRadius = 10.0;
constexpr int kMapInactivityMs = 5000;
constexpr qreal kFocusMapMargin = 6.0;
constexpr qreal kFocusMapMinRadius = 56.0;
constexpr qreal kFocusMapRadiusRatio = 0.78;
constexpr qreal kFocusMapMaxRadius = 300.0;
constexpr qreal kFocusMapSourceCropRatio = 0.80;

QImage stylizeFocusedMapImage(const QImage& source)
{
    if (source.isNull())
    {
        return {};
    }

    QImage styled = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    // 轻微柔化，减少静态图锯齿和细碎噪点。
    const QSize halfSize(qMax(1, styled.width() / 2), qMax(1, styled.height() / 2));
    const QImage softened =
        styled.scaled(halfSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
            .scaled(styled.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    QPainter blendPainter(&styled);
    blendPainter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    blendPainter.setOpacity(0.20);
    blendPainter.drawImage(QPoint(0, 0), softened);

    return styled;
}

void drawGridBackdrop(QPainter& painter, const QRectF& drawRect)
{
    painter.save();
    painter.setClipRect(drawRect);

    painter.fillRect(drawRect, QColor(255, 255, 255, 14));

    constexpr qreal kStep = 28.0;
    constexpr int kMajorEvery = 5;
    const qreal xStart = std::floor(drawRect.left() / kStep) * kStep;
    const qreal yStart = std::floor(drawRect.top() / kStep) * kStep;

    for (qreal x = xStart; x <= drawRect.right(); x += kStep)
    {
        const int idx = static_cast<int>(std::round(x / kStep));
        const bool major = (idx % kMajorEvery) == 0;
        const QColor c = major ? QColor(100, 120, 148, 42) : QColor(100, 120, 148, 20);
        painter.setPen(QPen(c, major ? 1.0 : 0.8));
        painter.drawLine(QPointF(x, drawRect.top()), QPointF(x, drawRect.bottom()));
    }

    for (qreal y = yStart; y <= drawRect.bottom(); y += kStep)
    {
        const int idx = static_cast<int>(std::round(y / kStep));
        const bool major = (idx % kMajorEvery) == 0;
        const QColor c = major ? QColor(100, 120, 148, 42) : QColor(100, 120, 148, 20);
        painter.setPen(QPen(c, major ? 1.0 : 0.8));
        painter.drawLine(QPointF(drawRect.left(), y), QPointF(drawRect.right(), y));
    }

    painter.setPen(QPen(QColor(120, 140, 166, 54), 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(drawRect.adjusted(0.5, 0.5, -0.5, -0.5));
    painter.restore();
}
}

MetroMapWidget::MetroMapWidget(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, false);
    setAutoFillBackground(false);

    if (!networkData_.loadFromResource(":/metro_data/suzhou_metro_network_core.json", &loadError_))
    {
        if (loadError_.isEmpty())
        {
            loadError_ = QStringLiteral("Unknown metro data load error.");
        }
    }

    viewBounds_ = networkData_.bounds();

    selectorPanel_ = new QWidget(this);
    selectorPanel_->setAttribute(Qt::WA_StyledBackground, false);
    selectorPanel_->setAutoFillBackground(false);
    selectorPanel_->setStyleSheet(
        "background: transparent;"
        "border: none;"
        "border-radius: 0;");

    buildLineSelector();
    layoutSelectorPanel();

    stationPanel_ = new StationPanelWidget(this);
    stationPanel_->setFixedSize(kStationPanelWidth, kStationPanelHeight);
    layoutStationPanel();

    fareService_.rebuild(networkData_.lines());
    initializeRandomCurrentStation();

    viewAnimation_ = new QVariantAnimation(this);
    viewAnimation_->setDuration(420);
    viewAnimation_->setEasingCurve(QEasingCurve::InOutCubic);
    connect(viewAnimation_, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        viewBounds_ = value.toRectF();
        update();
    });

    inactivityTimer_ = new QTimer(this);
    inactivityTimer_->setSingleShot(true);
    inactivityTimer_->setInterval(kMapInactivityMs);
    connect(inactivityTimer_, &QTimer::timeout, this, [this]() {
        initializeMapView();
    });
    restartInactivityTimer();
}

void MetroMapWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    if (networkData_.isEmpty())
    {
        painter.setPen(QColor("#B00020"));
        painter.drawText(rect(), Qt::AlignCenter,
                         QStringLiteral("Metro map load failed\n%1").arg(loadError_));
        return;
    }

    const QRectF drawRect = calculateDrawRect();
    drawGridBackdrop(painter, drawRect);
    drawFocusedStationMap(painter, drawRect);

    const auto& lines = networkData_.lines();

    for (int i = 0; i < lines.size(); ++i)
    {
        const szmetro::MetroDrawLine& line = lines[i];
        if (line.stations.size() < 2)
        {
            continue;
        }

        const bool highlighted = (selectedLineIndex_ < 0) || (selectedLineIndex_ == i);
        QColor drawColor = line.color;
        qreal width = kLineDrawWidthFocused;
        if (selectedLineIndex_ < 0)
        {
            width = kLineDrawWidthOverview;
        }
        else if (!highlighted)
        {
            drawColor = QColor("#C7CEDA");
            drawColor.setAlpha(95);
            width = kGrayLineDrawWidth;
        }
        const QRectF allBounds = networkData_.bounds();
        const QRectF currentBounds = viewBounds_.isValid() ? viewBounds_ : allBounds;
        const qreal viewZoom =
            qMin(allBounds.width() / qMax<qreal>(1.0, currentBounds.width()),
                 allBounds.height() / qMax<qreal>(1.0, currentBounds.height()));
        const qreal viewZoomWidthScale = qBound(1.0, std::sqrt(qMax<qreal>(1.0, viewZoom)), 1.9);
        qreal modeBoost = 1.0;
        if (selectedLineIndex_ >= 0)
        {
            modeBoost = highlighted ? 1.42 : 0.95;
        }

        qreal stationBoost = 1.0;
        if (!selectedStationId_.isEmpty())
        {
            stationBoost = highlighted ? 1.22 : 1.08;
        }

        width = qBound(2.0, width * zoomScale_ * viewZoomWidthScale * modeBoost * stationBoost, 26.0);

        QPainterPath path;
        path.moveTo(mapToWidget(line.stations.front().position, drawRect));
        for (int p = 1; p < line.stations.size(); ++p)
        {
            path.lineTo(mapToWidget(line.stations[p].position, drawRect));
        }

        QPen linePen(drawColor, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(linePen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
    }

    drawLineStartBadges(painter, drawRect);

    for (int i = 0; i < lines.size(); ++i)
    {
        const szmetro::MetroDrawLine& line = lines[i];
        const bool highlighted = (selectedLineIndex_ < 0) || (selectedLineIndex_ == i);

        QColor stationOutline = highlighted ? line.color : QColor("#C7CEDA");
        QColor stationFill = highlighted ? QColor("#FFFFFF") : QColor("#F4F6FA");
        if (selectedLineIndex_ >= 0 && !highlighted)
        {
            stationOutline.setAlpha(95);
            stationFill.setAlpha(85);
        }
        painter.setPen(QPen(stationOutline, 1.4));
        painter.setBrush(stationFill);

        for (const szmetro::MetroDrawStation& station : line.stations)
        {
            const QPointF center = mapToWidget(station.position, drawRect);
            const qreal radius = station.interchange ? kInterchangeRadius : kStationRadius;
            painter.drawEllipse(center, radius, radius);
        }
    }

    if (!selectedStationId_.isEmpty())
    {
        QColor accent("#2A7FFF");
        if (selectedLineIndex_ >= 0 && selectedLineIndex_ < lines.size())
        {
            accent = lines[selectedLineIndex_].color;
        }
        accent.setAlpha(235);
        const QPointF center = mapToWidget(selectedStationPosition_, drawRect);
        const qreal baseRadius = selectedStationInterchange_ ? kInterchangeRadius : kStationRadius;

        painter.save();
        painter.setPen(QPen(accent, qMax(2.0, 2.0 * zoomScale_)));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(center, baseRadius + 3.2, baseRadius + 3.2);
        painter.setPen(QPen(QColor(255, 255, 255, 235), 1.2));
        painter.setBrush(accent);
        painter.drawEllipse(center, baseRadius + 1.2, baseRadius + 1.2);
        painter.restore();
    }

    drawStationLabels(painter, drawRect);
    drawCompassOverlay(painter, drawRect);

    // 当前选中线路提示：在被选按钮上方绘制倒三角和文案。
    if (lineButtonGroup_ != nullptr && selectorPanel_ != nullptr)
    {
        auto* checked = lineButtonGroup_->checkedButton();
        if (checked != nullptr && checked->isVisible())
        {
            QColor indicatorColor(45, 56, 72);
            const auto& linesRef = networkData_.lines();
            if (selectedLineIndex_ >= 0 && selectedLineIndex_ < linesRef.size())
            {
                indicatorColor = linesRef[selectedLineIndex_].color;
            }
            indicatorColor.setAlpha(240);

            const QPoint topCenter = checked->mapTo(this, QPoint(checked->width() / 2, 0));
            const QPoint apex(topCenter.x(), topCenter.y() - 1);
            const QPoint left(topCenter.x() - 8, topCenter.y() - 12);
            const QPoint right(topCenter.x() + 8, topCenter.y() - 12);

            painter.save();
            painter.setPen(Qt::NoPen);
            painter.setBrush(indicatorColor);
            painter.drawPolygon(QPolygon() << apex << left << right);

            QFont labelFont(QStringLiteral("Microsoft YaHei"), 9);
            labelFont.setBold(true);
            painter.setFont(labelFont);
            painter.setPen(indicatorColor);
            const QString label = QStringLiteral("当前选择");
            const QRect textRect(topCenter.x() - 44, topCenter.y() - 30, 88, 14);
            painter.drawText(textRect, Qt::AlignCenter, label);
            painter.restore();
        }
    }
}

void MetroMapWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    layoutSelectorPanel();
    layoutStationPanel();
}

void MetroMapWidget::resetInteractiveView()
{
    zoomScale_ = 1.0;
    panOffset_ = QPointF(0.0, 0.0);
    middleDragging_ = false;
    unsetCursor();
}

void MetroMapWidget::restartInactivityTimer()
{
    if (inactivityTimer_ != nullptr)
    {
        inactivityTimer_->start();
    }
}

void MetroMapWidget::initializeMapView()
{
    if (networkData_.isEmpty())
    {
        return;
    }

    selectedStationId_.clear();
    selectedStationName_.clear();
    selectedStationPosition_ = QPointF();
    selectedStationInterchange_ = false;
    selectedStationLines_.clear();
    if (stationPanel_ != nullptr)
    {
        stationPanel_->clearSelection(currentStationName_);
    }

    resetInteractiveView();
    const QRectF target =
        (selectedLineIndex_ < 0) ? networkData_.bounds() : focusedBoundsForLine(selectedLineIndex_);
    startViewTransition(target);
    update();
}

void MetroMapWidget::initializeRandomCurrentStation()
{
    currentStationId_.clear();
    currentStationName_.clear();

    QVector<szmetro::MetroDrawStation> uniqueStations;
    QSet<QString> seenIds;
    for (const szmetro::MetroDrawLine& line : networkData_.lines())
    {
        for (const szmetro::MetroDrawStation& station : line.stations)
        {
            if (station.id.isEmpty() || seenIds.contains(station.id))
            {
                continue;
            }
            seenIds.insert(station.id);
            uniqueStations.push_back(station);
        }
    }

    if (uniqueStations.isEmpty())
    {
        if (stationPanel_ != nullptr)
        {
            stationPanel_->clearSelection(QStringLiteral("-"));
        }
        return;
    }

    const int randomIndex = QRandomGenerator::global()->bounded(uniqueStations.size());
    currentStationId_ = uniqueStations[randomIndex].id;
    currentStationName_ = uniqueStations[randomIndex].name;

    if (stationPanel_ != nullptr)
    {
        stationPanel_->clearSelection(currentStationName_);
    }
}

void MetroMapWidget::wheelEvent(QWheelEvent* event)
{
    const QRectF drawRect = calculateDrawRect();
    const QPointF cursorPos = event->position();
    if (!drawRect.contains(cursorPos))
    {
        QWidget::wheelEvent(event);
        return;
    }
    restartInactivityTimer();

    const qreal deltaY = event->angleDelta().y();
    if (qFuzzyIsNull(deltaY))
    {
        event->accept();
        return;
    }

    const qreal oldScale = zoomScale_;
    const qreal factor = (deltaY > 0.0) ? 1.12 : (1.0 / 1.12);
    const qreal newScale = qBound(0.6, oldScale * factor, 3.8);
    if (qFuzzyCompare(oldScale, newScale))
    {
        event->accept();
        return;
    }

    const QPointF pivot = drawRect.center();
    const QPointF basePoint =
        pivot + (cursorPos - panOffset_ - pivot) / qMax(0.0001, oldScale);
    panOffset_ = cursorPos - pivot - (basePoint - pivot) * newScale;
    zoomScale_ = newScale;

    update();
    event->accept();
}

void MetroMapWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        restartInactivityTimer();
        szmetro::MetroDrawStation station;
        QStringList stationLines;
        if (pickStationAt(event->position(), &station, &stationLines))
        {
            selectedStationId_ = station.id;
            selectedStationName_ = station.name;
            selectedStationPosition_ = station.position;
            selectedStationInterchange_ = station.interchange;
            selectedStationLines_ = stationLines;

            if (stationPanel_ != nullptr)
            {
                const int fare = fareService_.calculateFareYuan(currentStationId_, selectedStationId_);
                stationPanel_->setSelectionInfo(currentStationName_, selectedStationName_,
                                                selectedStationLines_, fare);
            }

            resetInteractiveView();
            startViewTransition(focusedBoundsForStation(station.position));
            update();
            event->accept();
            return;
        }
    }

    if (event->button() == Qt::MiddleButton)
    {
        restartInactivityTimer();
        middleDragging_ = true;
        lastDragPos_ = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void MetroMapWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (middleDragging_)
    {
        restartInactivityTimer();
        const QPointF delta = event->pos() - lastDragPos_;
        panOffset_ += delta;
        lastDragPos_ = event->pos();
        update();
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void MetroMapWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton && middleDragging_)
    {
        restartInactivityTimer();
        middleDragging_ = false;
        unsetCursor();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void MetroMapWidget::buildLineSelector()
{
    auto* panelLayout = new QHBoxLayout(selectorPanel_);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    panelLayout->setSpacing(0);

    lineButtonGroup_ = new QButtonGroup(this);
    lineButtonGroup_->setExclusive(true);

    auto* allButton = new QPushButton(QStringLiteral("全部线路"), selectorPanel_);
    allButton->setCheckable(true);
    allButton->setChecked(true);
    allButton->setCursor(Qt::PointingHandCursor);
    allButton->setMinimumHeight(kPanelHeight);
    allButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    allButton->setStyleSheet(
        "QPushButton {"
        "background-color: rgba(66, 78, 96, 80);"
        "color: rgba(255, 255, 255, 230);"
        "border: none;"
        "border-radius: 0px;"
        "padding: 0px;"
        "font: 10px 'Microsoft YaHei';"
        "}"
        "QPushButton:hover { background-color: rgba(66, 78, 96, 130); }"
        "QPushButton:checked {"
        "background-color: rgba(45, 56, 72, 245);"
        "border: 1px solid rgba(255,255,255,220);"
        "font: 700 10px 'Microsoft YaHei';"
        "}");
    lineButtonGroup_->addButton(allButton, -1);
    panelLayout->addWidget(allButton);

    const auto& lines = networkData_.lines();
    for (int i = 0; i < lines.size(); ++i)
    {
        auto* button = new QPushButton(lines[i].displayName, selectorPanel_);
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setMinimumHeight(kPanelHeight);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        const QColor lineColor = lines[i].color;
        const QString style = QString(
                                  "QPushButton {"
                                  "background-color: rgba(%1, %2, %3, 95);"
                                  "color: rgba(255, 255, 255, 235);"
                                  "border: none;"
                                  "border-radius: 0px;"
                                  "padding: 0px;"
                                  "font: 10px 'Microsoft YaHei';"
                                  "}"
                                  "QPushButton:hover { background-color: rgba(%1, %2, %3, 150); }"
                                  "QPushButton:checked {"
                                  "background-color: rgba(%1, %2, %3, 255);"
                                  "border: 1px solid rgba(255,255,255,230);"
                                  "font: 700 10px 'Microsoft YaHei';"
                                  "}")
                                  .arg(lineColor.red())
                                  .arg(lineColor.green())
                                  .arg(lineColor.blue());
        button->setStyleSheet(style);
        lineButtonGroup_->addButton(button, i);
        panelLayout->addWidget(button);
    }

    connect(lineButtonGroup_, &QButtonGroup::idClicked, this,
            &MetroMapWidget::onLineButtonClicked);
}

void MetroMapWidget::layoutSelectorPanel()
{
    if (selectorPanel_ == nullptr)
    {
        return;
    }

    const int x = kOuterMargin;
    const int w = qMax(240, width() - kOuterMargin * 2);
    const int h = kPanelHeight;
    const int y = height() - h - kOuterMargin;
    selectorPanel_->setGeometry(x, y, w, h);
}

void MetroMapWidget::layoutStationPanel()
{
    if (stationPanel_ == nullptr)
    {
        return;
    }

    const QRectF drawRect = calculateDrawRect();
    const int x = qMax(kOuterMargin, static_cast<int>(drawRect.right()) - kStationPanelWidth);
    const int y = kOuterMargin;
    stationPanel_->setGeometry(x, y, kStationPanelWidth, kStationPanelHeight);
}

void MetroMapWidget::onLineButtonClicked(int lineIndex)
{
    restartInactivityTimer();
    selectedStationId_.clear();
    selectedStationName_.clear();
    selectedStationPosition_ = QPointF();
    selectedStationInterchange_ = false;
    selectedStationLines_.clear();
    if (stationPanel_ != nullptr)
    {
        stationPanel_->clearSelection(currentStationName_);
    }

    selectedLineIndex_ = lineIndex;
    resetInteractiveView();
    const QRectF target = (lineIndex < 0) ? networkData_.bounds() : focusedBoundsForLine(lineIndex);
    startViewTransition(target);
    update();
}

QRectF MetroMapWidget::focusedBoundsForLine(int lineIndex) const
{
    const auto& lines = networkData_.lines();
    if (lineIndex < 0 || lineIndex >= lines.size() || lines[lineIndex].stations.isEmpty())
    {
        return networkData_.bounds();
    }

    qreal minX = std::numeric_limits<qreal>::max();
    qreal minY = std::numeric_limits<qreal>::max();
    qreal maxX = std::numeric_limits<qreal>::lowest();
    qreal maxY = std::numeric_limits<qreal>::lowest();

    for (const szmetro::MetroDrawStation& station : lines[lineIndex].stations)
    {
        minX = qMin(minX, station.position.x());
        minY = qMin(minY, station.position.y());
        maxX = qMax(maxX, station.position.x());
        maxY = qMax(maxY, station.position.y());
    }

    QRectF focus(minX, minY, qMax<qreal>(1.0, maxX - minX), qMax<qreal>(1.0, maxY - minY));
    focus.adjust(-140.0, -140.0, 140.0, 140.0);

    const QRectF all = networkData_.bounds();
    const qreal minWidth = all.width() * 0.20;
    const qreal minHeight = all.height() * 0.20;
    if (focus.width() < minWidth)
    {
        const qreal delta = (minWidth - focus.width()) * 0.5;
        focus.adjust(-delta, 0.0, delta, 0.0);
    }
    if (focus.height() < minHeight)
    {
        const qreal delta = (minHeight - focus.height()) * 0.5;
        focus.adjust(0.0, -delta, 0.0, delta);
    }

    return clampRectToBounds(focus, all);
}

QRectF MetroMapWidget::focusedBoundsForStation(const QPointF& stationPosition) const
{
    const QRectF all = networkData_.bounds();
    const qreal focusWidth = qMax<qreal>(all.width() * 0.12, 420.0);
    const qreal focusHeight = qMax<qreal>(all.height() * 0.12, 420.0);

    QRectF focus(stationPosition.x() - focusWidth * 0.5, stationPosition.y() - focusHeight * 0.5,
                 focusWidth, focusHeight);
    return clampRectToBounds(focus, all);
}

QRectF MetroMapWidget::clampRectToBounds(const QRectF& rect, const QRectF& bounds) const
{
    if (rect.width() >= bounds.width() || rect.height() >= bounds.height())
    {
        return bounds;
    }

    qreal left = rect.left();
    qreal top = rect.top();
    if (left < bounds.left())
    {
        left = bounds.left();
    }
    if (top < bounds.top())
    {
        top = bounds.top();
    }
    if (left + rect.width() > bounds.right())
    {
        left = bounds.right() - rect.width();
    }
    if (top + rect.height() > bounds.bottom())
    {
        top = bounds.bottom() - rect.height();
    }
    return QRectF(left, top, rect.width(), rect.height());
}

void MetroMapWidget::startViewTransition(const QRectF& targetBounds)
{
    if (viewAnimation_ == nullptr)
    {
        viewBounds_ = targetBounds;
        update();
        return;
    }

    viewAnimation_->stop();
    viewAnimation_->setStartValue(viewBounds_);
    viewAnimation_->setEndValue(targetBounds);
    viewAnimation_->start();
}

QStringList MetroMapWidget::collectStationLines(const QString& stationId) const
{
    if (stationId.isEmpty())
    {
        return {};
    }

    QStringList result;
    const auto& lines = networkData_.lines();
    for (const szmetro::MetroDrawLine& line : lines)
    {
        for (const szmetro::MetroDrawStation& station : line.stations)
        {
            if (station.id == stationId)
            {
                if (!result.contains(line.displayName))
                {
                    result.push_back(line.displayName);
                }
                break;
            }
        }
    }
    return result;
}

bool MetroMapWidget::pickStationAt(const QPointF& widgetPos, szmetro::MetroDrawStation* outStation,
                                   QStringList* outLines) const
{
    const QRectF drawRect = calculateDrawRect();
    if (!drawRect.adjusted(-6.0, -6.0, 6.0, 6.0).contains(widgetPos))
    {
        return false;
    }

    const auto& lines = networkData_.lines();
    qreal bestDistance = std::numeric_limits<qreal>::max();
    bool found = false;
    szmetro::MetroDrawStation bestStation;

    const qreal pickRadius = qMax<qreal>(kStationPickRadius, 8.0 * zoomScale_);
    for (int i = 0; i < lines.size(); ++i)
    {
        const bool visibleLine = (selectedLineIndex_ < 0) || (selectedLineIndex_ == i);
        if (!visibleLine)
        {
            continue;
        }

        for (const szmetro::MetroDrawStation& station : lines[i].stations)
        {
            const QPointF center = mapToWidget(station.position, drawRect);
            const qreal dx = center.x() - widgetPos.x();
            const qreal dy = center.y() - widgetPos.y();
            const qreal dist2 = dx * dx + dy * dy;
            if (dist2 > pickRadius * pickRadius)
            {
                continue;
            }
            if (dist2 < bestDistance)
            {
                bestDistance = dist2;
                bestStation = station;
                found = true;
            }
        }
    }

    if (!found)
    {
        return false;
    }

    if (outStation != nullptr)
    {
        *outStation = bestStation;
    }
    if (outLines != nullptr)
    {
        *outLines = collectStationLines(bestStation.id);
    }
    return true;
}

void MetroMapWidget::drawLineStartBadges(QPainter& painter, const QRectF& drawRect)
{
    const auto& lines = networkData_.lines();
    QFont badgeFont(QStringLiteral("Microsoft YaHei"));
    badgeFont.setBold(true);
    badgeFont.setPointSizeF(qBound(6.0, 7.0 * zoomScale_, 10.5));
    painter.setFont(badgeFont);
    const QFontMetricsF fm(badgeFont);
    std::unordered_map<quint64, int> startBadgeStackCount;

    for (int i = 0; i < lines.size(); ++i)
    {
        const szmetro::MetroDrawLine& line = lines[i];
        if (line.stations.isEmpty())
        {
            continue;
        }

        QString numberText;
        for (const QChar c : line.displayName)
        {
            if (c.isDigit())
            {
                numberText.push_back(c);
            }
        }
        if (numberText.isEmpty())
        {
            numberText = QString::number(i + 1);
        }

        const QPointF startPoint = mapToWidget(line.stations.front().position, drawRect);
        const QSizeF textSize = fm.size(Qt::TextSingleLine, numberText);
        const qreal rectW = qMax<qreal>(textSize.width() + 8.0, 16.0);
        const qreal rectH = qMax<qreal>(textSize.height() + 4.0, 12.0);

        qreal x = startPoint.x() + 6.0;
        qreal y = startPoint.y() - rectH - 6.0;

        // 同起点线路的标号自动错位，避免重叠（如 3/6 号线）。
        const int keyX = static_cast<int>(std::round(startPoint.x() / 16.0));
        const int keyY = static_cast<int>(std::round(startPoint.y() / 16.0));
        const quint64 key =
            (static_cast<quint64>(static_cast<quint32>(keyX)) << 32) |
            static_cast<quint32>(keyY);
        const int overlapIndex = startBadgeStackCount[key]++;
        if (overlapIndex > 0)
        {
            const int row = overlapIndex % 2;
            const int col = overlapIndex / 2;
            x += col * (rectW + 3.0);
            y += (row == 0 ? 1.0 : -1.0) * (rectH + 3.0);
        }

        if (x + rectW > drawRect.right() - 2.0)
        {
            x = drawRect.right() - rectW - 2.0;
        }
        if (y < drawRect.top() + 2.0)
        {
            y = startPoint.y() + 8.0;
        }

        const QRectF badgeRect(x, y, rectW, rectH);
        QColor badgeColor = line.color;
        if (selectedLineIndex_ >= 0 && selectedLineIndex_ != i)
        {
            badgeColor = QColor("#C7CEDA");
            badgeColor.setAlpha(120);
        }
        else
        {
            badgeColor.setAlpha(235);
        }

        painter.save();
        painter.setPen(Qt::NoPen);
        painter.setBrush(badgeColor);
        painter.drawRoundedRect(badgeRect, qMin<qreal>(3.0, rectH * 0.20),
                                qMin<qreal>(3.0, rectH * 0.20));
        painter.setPen(QColor(255, 255, 255, 245));
        painter.drawText(badgeRect, Qt::AlignCenter, numberText);
        painter.restore();
    }
}

void MetroMapWidget::drawFocusedStationMap(QPainter& painter, const QRectF& drawRect)
{
    if (selectedStationId_.isEmpty())
    {
        return;
    }

    const QImage mapImage = loadStationMap(selectedStationId_);
    if (mapImage.isNull())
    {
        return;
    }

    const QPointF center = mapToWidget(selectedStationPosition_, drawRect);
    const qreal maxRadius = qMin(
        qMin(center.x() - drawRect.left(), drawRect.right() - center.x()),
        qMin(center.y() - drawRect.top(), drawRect.bottom() - center.y()));
    const qreal boundedMax = qMin(maxRadius - kFocusMapMargin, kFocusMapMaxRadius);
    if (boundedMax < kFocusMapMinRadius)
    {
        return;
    }
    const qreal radius = qBound(kFocusMapMinRadius, boundedMax * kFocusMapRadiusRatio, boundedMax);

    const int diameter = static_cast<int>(std::floor(radius * 2.0));
    if (diameter < 2)
    {
        return;
    }

    QImage layer(diameter, diameter, QImage::Format_ARGB32_Premultiplied);
    layer.fill(Qt::transparent);

    const QRect layerRect(0, 0, diameter, diameter);
    const QSize srcSize = mapImage.size();
    const int srcSide = qMin(srcSize.width(), srcSize.height());
    const int cropSide = qMax(2, static_cast<int>(std::round(srcSide * kFocusMapSourceCropRatio)));
    QRect srcRect((srcSize.width() - cropSide) / 2, (srcSize.height() - cropSide) / 2, cropSide,
                  cropSide);

    {
        QPainter layerPainter(&layer);
        layerPainter.setRenderHint(QPainter::Antialiasing, true);
        layerPainter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        layerPainter.drawImage(layerRect, mapImage, srcRect);

        // 通过 alpha 蒙版做圆形边缘羽化：中心清晰，边缘柔和收口。
        layerPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        const qreal r = static_cast<qreal>(diameter) * 0.5;
        QRadialGradient alphaMask(QPointF(r, r), r);
        alphaMask.setColorAt(0.0, QColor(0, 0, 0, 244));
        alphaMask.setColorAt(0.40, QColor(0, 0, 0, 234));
        alphaMask.setColorAt(0.70, QColor(0, 0, 0, 168));
        alphaMask.setColorAt(0.90, QColor(0, 0, 0, 56));
        alphaMask.setColorAt(1.0, QColor(0, 0, 0, 0));
        layerPainter.fillRect(layerRect, alphaMask);

        // 外圈渐进降低饱和度，中心保持原彩色。
        layerPainter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
        QRadialGradient edgeDesat(QPointF(r, r), r);
        edgeDesat.setColorAt(0.0, QColor(160, 160, 160, 0));
        edgeDesat.setColorAt(0.62, QColor(160, 160, 160, 0));
        edgeDesat.setColorAt(0.86, QColor(160, 160, 160, 46));
        edgeDesat.setColorAt(1.0, QColor(160, 160, 160, 92));
        layerPainter.fillRect(layerRect, edgeDesat);

        // 轻微暗角，收敛画面边界观感（仅在圆内生效，避免方形边）。
        QRadialGradient vignette(QPointF(r, r), r);
        vignette.setColorAt(0.0, QColor(12, 18, 28, 0));
        vignette.setColorAt(0.72, QColor(12, 18, 28, 8));
        vignette.setColorAt(1.0, QColor(12, 18, 28, 22));
        layerPainter.fillRect(layerRect, vignette);
    }

    const QRectF targetRect(center.x() - radius, center.y() - radius, radius * 2.0, radius * 2.0);
    painter.save();
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    QPainterPath circleClip;
    circleClip.addEllipse(targetRect);
    painter.setClipPath(circleClip, Qt::IntersectClip);
    painter.drawImage(targetRect, layer);
    painter.setClipping(false);

    QColor ringColor = QColor(255, 255, 255, 165);
    if (selectedLineIndex_ >= 0 && selectedLineIndex_ < networkData_.lines().size())
    {
        ringColor = networkData_.lines()[selectedLineIndex_].color;
        ringColor.setAlpha(170);
    }
    painter.setPen(QPen(ringColor, qBound(1.2, 1.45 * zoomScale_, 2.4)));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(center, radius - 1.0, radius - 1.0);

    painter.setPen(QPen(QColor(255, 255, 255, 70), 0.9));
    painter.drawEllipse(center, radius - 3.0, radius - 3.0);
    painter.restore();
}

void MetroMapWidget::drawCompassOverlay(QPainter& painter, const QRectF& drawRect)
{
    const int margin = 10;
    const int panelSize = 40;
    const QRectF panelRect(drawRect.right() - panelSize - margin,
                           drawRect.bottom() - panelSize - margin, panelSize, panelSize);
    const QPointF c = panelRect.center();
    const qreal r = panelRect.width() * 0.40;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(255, 255, 255, 205), 1.0));
    painter.setBrush(QColor(20, 34, 52, 105));
    painter.drawRoundedRect(panelRect, 8.0, 8.0);

    painter.setPen(QPen(QColor(230, 236, 248, 220), 1.2));
    painter.setBrush(QColor(40, 56, 80, 88));
    painter.drawEllipse(c, r, r);

    QPolygonF northArrow;
    northArrow << QPointF(c.x(), c.y() - r + 4.0) << QPointF(c.x() - 4.2, c.y() + 2.0)
               << QPointF(c.x() + 4.2, c.y() + 2.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(224, 76, 76, 230));
    painter.drawPolygon(northArrow);

    QPolygonF southArrow;
    southArrow << QPointF(c.x(), c.y() + r - 5.0) << QPointF(c.x() - 3.3, c.y() - 1.0)
               << QPointF(c.x() + 3.3, c.y() - 1.0);
    painter.setBrush(QColor(210, 220, 235, 195));
    painter.drawPolygon(southArrow);

    QFont nFont(QStringLiteral("Microsoft YaHei"));
    nFont.setBold(true);
    nFont.setPointSize(8);
    painter.setFont(nFont);
    painter.setPen(QColor(246, 248, 255, 240));
    painter.drawText(QRectF(c.x() - 6.0, panelRect.top() + 1.0, 12.0, 10.0), Qt::AlignCenter,
                     QStringLiteral("N"));
    painter.restore();
}

QString MetroMapWidget::resolveStationMapDirectory()
{
    if (!stationMapDirectory_.isEmpty())
    {
        return stationMapDirectory_;
    }

    const QDir currentDir = QDir::current();
    QDir appDir(QCoreApplication::applicationDirPath());
    const QStringList candidates = {
        currentDir.filePath(QStringLiteral("src/models/station_maps")),
        currentDir.filePath(QStringLiteral("models/station_maps")),
        appDir.filePath(QStringLiteral("../../src/models/station_maps")),
        appDir.filePath(QStringLiteral("../src/models/station_maps")),
        appDir.filePath(QStringLiteral("src/models/station_maps")),
    };

    for (const QString& candidate : candidates)
    {
        QDir dir(QDir::cleanPath(candidate));
        if (dir.exists())
        {
            stationMapDirectory_ = dir.absolutePath();
            break;
        }
    }

    return stationMapDirectory_;
}

QImage MetroMapWidget::loadStationMap(const QString& stationId)
{
    if (stationId.isEmpty())
    {
        return {};
    }

    const auto cacheIt = stationMapCache_.constFind(stationId);
    if (cacheIt != stationMapCache_.cend())
    {
        return cacheIt.value();
    }

    const QString mapDirPath = resolveStationMapDirectory();
    if (mapDirPath.isEmpty())
    {
        stationMapCache_.insert(stationId, QImage());
        return {};
    }

    const QString mapPath = QDir(mapDirPath).filePath(stationId + QStringLiteral(".png"));
    QImage rawImage;
    rawImage.load(mapPath);
    const QImage styledImage = stylizeFocusedMapImage(rawImage);
    stationMapCache_.insert(stationId, styledImage);
    return styledImage;
}

QRectF MetroMapWidget::calculateDrawRect() const
{
    QRectF drawRect = rect();
    drawRect.adjust(kOuterMargin, kOuterMargin, -kOuterMargin, -kOuterMargin);

    if (selectorPanel_ != nullptr)
    {
        drawRect.setBottom(selectorPanel_->y() - kPanelGap);
    }

    if (drawRect.width() < 100.0)
    {
        drawRect.setWidth(100.0);
    }
    if (drawRect.height() < 100.0)
    {
        drawRect.setHeight(100.0);
    }
    return drawRect;
}

QPointF MetroMapWidget::mapToWidget(const QPointF& point, const QRectF& drawRect) const
{
    const QRectF bounds = viewBounds_.isValid() ? viewBounds_ : networkData_.bounds();
    const qreal scaleX = drawRect.width() / qMax(1.0, bounds.width());
    const qreal scaleY = drawRect.height() / qMax(1.0, bounds.height());
    const qreal scale = qMin(scaleX, scaleY);

    const qreal contentWidth = bounds.width() * scale;
    const qreal contentHeight = bounds.height() * scale;
    const qreal offsetX = drawRect.left() + (drawRect.width() - contentWidth) * 0.5;
    const qreal offsetY = drawRect.top() + (drawRect.height() - contentHeight) * 0.5;

    const qreal x = offsetX + (point.x() - bounds.left()) * scale;
    const qreal y = offsetY + (point.y() - bounds.top()) * scale;
    const QPointF mapped(x, y);
    const QPointF pivot = drawRect.center();
    return pivot + (mapped - pivot) * zoomScale_ + panOffset_;
}

void MetroMapWidget::drawStationLabels(QPainter& painter, const QRectF& drawRect)
{
    const auto& lines = networkData_.lines();
    if (selectedLineIndex_ >= 0 && selectedLineIndex_ < lines.size())
    {
        const qreal fontSize = qBound(4.0, 6.0 * zoomScale_, 16.0);
        QFont font(QStringLiteral("Microsoft YaHei"));
        font.setPointSizeF(fontSize);
        painter.setFont(font);
        painter.setPen(QColor("#1F2A3A"));

        const QFontMetricsF metrics(painter.font());
        const szmetro::MetroDrawLine& line = lines[selectedLineIndex_];
        QSet<QString> drawnStationIds;
        QVector<QRectF> occupiedRects;
        occupiedRects.reserve(line.stations.size());

        for (int index = 0; index < line.stations.size(); ++index)
        {
            const szmetro::MetroDrawStation& station = line.stations[index];
            if (station.id.isEmpty() || drawnStationIds.contains(station.id))
            {
                continue;
            }
            drawnStationIds.insert(station.id);

            const QPointF center = mapToWidget(station.position, drawRect);
            const QSizeF textSize = metrics.size(Qt::TextSingleLine, station.name);

            // 标签固定在线路同一侧（右侧）并贴近站点，轻微上下错位避免重叠。
            QVector<QRectF> candidates;
            candidates.reserve(4);
            const qreal w = textSize.width();
            const qreal h = textSize.height();
            const qreal baseX = center.x() + 2.0;
            const qreal yBase = center.y() - 1.5;
            candidates.push_back(QRectF(baseX, yBase - h, w, h));
            candidates.push_back(QRectF(baseX, yBase - h + 4.0, w, h));
            candidates.push_back(QRectF(baseX, yBase - h - 4.0, w, h));
            candidates.push_back(QRectF(baseX + 1.5, yBase - h + 7.0, w, h));

            bool placed = false;
            for (const QRectF& rect : candidates)
            {
                if (!drawRect.adjusted(2.0, 2.0, -2.0, -2.0).contains(rect))
                {
                    continue;
                }

                bool intersects = false;
                for (const QRectF& occupied : occupiedRects)
                {
                    if (rect.adjusted(-1.0, -1.0, 1.0, 1.0).intersects(occupied))
                    {
                        intersects = true;
                        break;
                    }
                }
                if (intersects)
                {
                    continue;
                }

                painter.drawText(rect.topLeft(), station.name);
                occupiedRects.push_back(rect);
                placed = true;
                break;
            }

            // 极端密集区域允许跳过少量标签，避免明显重叠。
            if (!placed)
            {
                continue;
            }
        }
        return;
    }

    const qreal fontSize = qBound(3.5, 5.0 * zoomScale_, 14.0);
    QFont font(QStringLiteral("Microsoft YaHei"));
    font.setPointSizeF(fontSize);
    painter.setFont(font);
    painter.setPen(QColor("#4E5969"));

    QSet<QString> drawnStationIds;
    for (const szmetro::MetroDrawLine& line : lines)
    {
        for (const szmetro::MetroDrawStation& station : line.stations)
        {
            if (station.id.isEmpty() || drawnStationIds.contains(station.id))
            {
                continue;
            }
            drawnStationIds.insert(station.id);

            const QPointF center = mapToWidget(station.position, drawRect);
            const QPointF textPos(center.x() + 2.0, center.y() - 1.5);
            painter.drawText(textPos, station.name);
        }
    }
}

