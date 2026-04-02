#include "widgets/route_plan_widget.h"

#include "services/style_service.h"

#include <cmath>
#include <QFontMetricsF>
#include <QPainter>
#include <QPaintEvent>

namespace
{
QColor blendColor(const QColor& a, const QColor& b)
{
    return QColor((a.red() + b.red()) / 2, (a.green() + b.green()) / 2,
                  (a.blue() + b.blue()) / 2, 235);
}

QString compactStationName(QString name)
{
    name = name.trimmed();
    if (name.size() <= 6)
    {
        return name;
    }
    return name.left(6);
}

void drawVerticalCenteredText(QPainter* painter, const QRectF& anchorRect, const QString& text,
                              const QFont& font, const QColor& color)
{
    if (painter == nullptr || text.trimmed().isEmpty() || anchorRect.width() < 2.0 ||
        anchorRect.height() < 2.0)
    {
        return;
    }

    painter->save();
    painter->setFont(font);
    painter->setPen(color);

    const QPointF c = anchorRect.center();
    painter->translate(c);
    painter->rotate(-90.0);
    const QRectF rotatedRect(-anchorRect.height() * 0.5, -anchorRect.width() * 0.5,
                             anchorRect.height(), anchorRect.width());
    painter->drawText(rotatedRect, Qt::AlignCenter, text);
    painter->restore();
}
} // namespace

RoutePlanWidget::RoutePlanWidget(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, false);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAutoFillBackground(false);
    setStyleSheet(szmetro::UiStyleService::style(
        QStringLiteral("route_plan.root"), QStringLiteral("background: transparent; border: none;")));

    if (!networkData_.loadFromResource(QStringLiteral(":/metro_data/suzhou_metro_network_core.json"),
                                       &loadError_))
    {
        if (loadError_.isEmpty())
        {
            loadError_ = QStringLiteral("地铁网络数据加载失败");
        }
    }

    rebuildGraph();
    refreshView();
}

void RoutePlanWidget::setCurrentStationName(const QString& stationName)
{
    currentStationName_ = stationName.trimmed();
    refreshView();
}

void RoutePlanWidget::setTargetStationName(const QString& stationName)
{
    targetStationName_ = stationName.trimmed();
    refreshView();
}

void RoutePlanWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF canvas = rect().adjusted(1.0, 1.0, -1.0, -1.0);
    if (canvas.width() < 20.0 || canvas.height() < 20.0)
    {
        return;
    }

    QFont titleFont(QStringLiteral("Microsoft YaHei"));
    titleFont.setBold(true);
    titleFont.setPointSize(10);
    painter.setFont(titleFont);
    painter.setPen(QColor(42, 66, 104, 210));
    painter.drawText(QRectF(8.0, 4.0, width() - 16.0, 18.0), Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("路线规划"));

    if (!routeReady_)
    {
        QFont hintFont(QStringLiteral("Microsoft YaHei"));
        hintFont.setPointSize(7);
        painter.setFont(hintFont);
        painter.setPen(QColor(98, 116, 138, 188));
        const QFontMetricsF hintMetrics(hintFont);
        const QString hint = hintMetrics.elidedText(hintText_, Qt::ElideRight, qMax(20, width() - 16));
        painter.drawText(QRectF(8.0, 22.0, width() - 16.0, 12.0), Qt::AlignLeft | Qt::AlignVCenter,
                         hint);
        return;
    }

    if (routeLineIndexes_.isEmpty())
    {
        return;
    }

    const QRectF graphRect(8.0, 24.0, width() - 16.0, height() - 30.0);
    if (graphRect.width() < 30.0 || graphRect.height() < 50.0)
    {
        return;
    }

    const int segmentCount = routeLineIndexes_.size();
    const qreal laneMargin = qMin<qreal>(14.0, qMax<qreal>(5.0, graphRect.width() * 0.16));
    const qreal leftX = graphRect.left() + laneMargin;
    const qreal rightX = graphRect.right() - laneMargin;
    const qreal xStep = (segmentCount <= 1) ? 0.0 : (rightX - leftX) / (segmentCount - 1);
    const qreal topY = graphRect.top() + 2.0;
    const qreal bottomY = graphRect.bottom() - 2.0;

    int maxStops = 1;
    for (const int c : routeLineStopCounts_)
    {
        if (c > maxStops)
        {
            maxStops = c;
        }
    }

    const qreal minLength = qMax<qreal>(70.0, graphRect.height() * 0.52);
    const qreal maxLength = graphRect.height();
    const qreal lineWidth = 11.0;
    const qreal transferWidth = 6.0;

    QFont lineNameFont(QStringLiteral("Microsoft YaHei"));
    lineNameFont.setPointSize(8);
    lineNameFont.setBold(true);

    QFont stationFont(QStringLiteral("Microsoft YaHei"));
    stationFont.setPointSize(7);
    stationFont.setBold(true);

    for (int i = 0; i < segmentCount; ++i)
    {
        const qreal x = leftX + xStep * i;
        const bool down = (i % 2 == 0);
        const int stopCount =
            (i < routeLineStopCounts_.size() && routeLineStopCounts_[i] > 0) ? routeLineStopCounts_[i] : 1;
        const qreal ratio = static_cast<qreal>(stopCount) / qMax(1, maxStops);
        const qreal symbolicLength = qBound(minLength, minLength + (maxLength - minLength) * std::sqrt(ratio), maxLength);

        const qreal anchorStart = down ? topY : bottomY;
        const qreal anchorEnd = down ? qMin(bottomY, topY + symbolicLength) : qMax(topY, bottomY - symbolicLength);
        const qreal startY = anchorStart;
        const qreal endY = anchorEnd;
        const qreal segTop = qMin(startY, endY);
        const qreal segBottom = qMax(startY, endY);
        const QRectF segmentRect(x - lineWidth * 0.5, segTop, lineWidth, segBottom - segTop);
        const QColor lineColor = lineColorByIndex(routeLineIndexes_[i]);

        painter.setPen(Qt::NoPen);
        painter.setBrush(lineColor);
        painter.drawRoundedRect(segmentRect, 4.0, 4.0);

        const QRectF lineNameRect = segmentRect.adjusted(0.5, 10.0, -0.5, -10.0);
        drawVerticalCenteredText(&painter, lineNameRect, lineNameByIndex(routeLineIndexes_[i]), lineNameFont,
                                 QColor(255, 255, 255, 238));

        if (i == 0 && !routeFromName_.isEmpty())
        {
            const qreal stationH = qMin<qreal>(54.0, qMax<qreal>(32.0, segmentRect.height() * 0.38));
            const QRectF stationRect(x - lineWidth * 0.5, startY, lineWidth, stationH);
            drawVerticalCenteredText(&painter, stationRect, compactStationName(routeFromName_), stationFont,
                                     QColor(255, 255, 255, 245));
        }
        if (i == segmentCount - 1 && !routeToName_.isEmpty())
        {
            const qreal stationH = qMin<qreal>(54.0, qMax<qreal>(32.0, segmentRect.height() * 0.38));
            const qreal stationY = down ? (endY - stationH) : endY;
            const QRectF stationRect(x - lineWidth * 0.5,
                                     qBound(segTop, stationY, segBottom - stationH), lineWidth,
                                     stationH);
            drawVerticalCenteredText(&painter, stationRect, compactStationName(routeToName_), stationFont,
                                     QColor(255, 255, 255, 245));
        }

        if (i >= segmentCount - 1)
        {
            continue;
        }

        const qreal nextX = leftX + xStep * (i + 1);
        const QColor nextColor = lineColorByIndex(routeLineIndexes_[i + 1]);
        const bool nextDown = ((i + 1) % 2 == 0);
        const qreal nextEntryY = nextDown ? topY : bottomY;
        const qreal connectorStartY = endY;
        const qreal midX = (x + nextX) * 0.5;

        painter.setPen(Qt::NoPen);
        painter.setBrush(lineColor);
        painter.drawRect(QRectF(qMin(x, midX), connectorStartY - transferWidth * 0.5, qAbs(midX - x),
                                transferWidth));

        const qreal vTop = qMin(connectorStartY, nextEntryY);
        const qreal vBottom = qMax(connectorStartY, nextEntryY);
        painter.setBrush(blendColor(lineColor, nextColor));
        painter.drawRect(
            QRectF(midX - transferWidth * 0.5, vTop, transferWidth, qMax<qreal>(1.0, vBottom - vTop)));

        painter.setBrush(nextColor);
        painter.drawRect(QRectF(qMin(midX, nextX), nextEntryY - transferWidth * 0.5,
                                qAbs(nextX - midX), transferWidth));
    }
}

QString RoutePlanWidget::normalizeStationName(QString stationName) const
{
    stationName = stationName.trimmed();
    stationName.replace(QStringLiteral("地铁站"), QStringLiteral(""));
    stationName.replace(QStringLiteral("站点"), QStringLiteral(""));
    stationName.replace(QStringLiteral("地铁"), QStringLiteral(""));
    if (stationName.endsWith(QStringLiteral("站")) && stationName.size() > 1)
    {
        stationName.chop(1);
    }
    return stationName;
}

void RoutePlanWidget::rebuildGraph()
{
    stations_.clear();
    stationIndexById_.clear();
    graph_.clear();

    auto ensureStationIndex = [this](const szmetro::MetroDrawStation& station) {
        const auto it = stationIndexById_.constFind(station.id);
        if (it != stationIndexById_.cend())
        {
            return it.value();
        }

        const int index = stations_.size();
        stations_.push_back(station);
        stationIndexById_.insert(station.id, index);
        graph_.push_back({});
        return index;
    };

    auto appendEdge = [this](int from, int to, int lineIndex) {
        for (GraphEdge& edge : graph_[from])
        {
            if (edge.to != to)
            {
                continue;
            }
            if (!edge.lineIndexes.contains(lineIndex))
            {
                edge.lineIndexes.push_back(lineIndex);
            }
            return;
        }

        GraphEdge edge;
        edge.to = to;
        edge.lineIndexes.push_back(lineIndex);
        graph_[from].push_back(edge);
    };

    const auto& lines = networkData_.lines();
    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex)
    {
        const auto& line = lines[lineIndex];
        if (line.stations.size() < 2)
        {
            continue;
        }

        for (int i = 1; i < line.stations.size(); ++i)
        {
            const auto& prev = line.stations[i - 1];
            const auto& curr = line.stations[i];
            if (prev.id.isEmpty() || curr.id.isEmpty())
            {
                continue;
            }

            const int u = ensureStationIndex(prev);
            const int v = ensureStationIndex(curr);
            appendEdge(u, v, lineIndex);
            appendEdge(v, u, lineIndex);
        }
    }
}

int RoutePlanWidget::findStationIndexByName(const QString& stationName) const
{
    const QString raw = stationName.trimmed();
    if (raw.isEmpty())
    {
        return -1;
    }

    for (int i = 0; i < stations_.size(); ++i)
    {
        if (stations_[i].name == raw)
        {
            return i;
        }
    }

    const QString normalized = normalizeStationName(raw);
    for (int i = 0; i < stations_.size(); ++i)
    {
        const QString stationRaw = stations_[i].name;
        if (stationRaw.contains(raw) || raw.contains(stationRaw))
        {
            return i;
        }

        const QString stationNormalized = normalizeStationName(stationRaw);
        if (!normalized.isEmpty() &&
            (stationNormalized == normalized || stationNormalized.contains(normalized) ||
             normalized.contains(stationNormalized)))
        {
            return i;
        }
    }

    return -1;
}

bool RoutePlanWidget::buildShortestPath(int fromIndex, int toIndex, QVector<int>* outPath) const
{
    if (outPath == nullptr || fromIndex < 0 || toIndex < 0 || fromIndex >= graph_.size() ||
        toIndex >= graph_.size())
    {
        return false;
    }

    QVector<int> prev(graph_.size(), -1);
    QVector<int> queue;
    queue.reserve(graph_.size());
    queue.push_back(fromIndex);
    prev[fromIndex] = fromIndex;

    int head = 0;
    while (head < queue.size())
    {
        const int u = queue[head++];
        if (u == toIndex)
        {
            break;
        }

        for (const GraphEdge& edge : graph_[u])
        {
            if (edge.to < 0 || edge.to >= prev.size())
            {
                continue;
            }
            if (prev[edge.to] != -1)
            {
                continue;
            }
            prev[edge.to] = u;
            queue.push_back(edge.to);
        }
    }

    if (prev[toIndex] == -1)
    {
        return false;
    }

    QVector<int> reversePath;
    for (int cur = toIndex; cur != fromIndex; cur = prev[cur])
    {
        reversePath.push_back(cur);
    }
    reversePath.push_back(fromIndex);

    outPath->clear();
    outPath->reserve(reversePath.size());
    for (int i = reversePath.size() - 1; i >= 0; --i)
    {
        outPath->push_back(reversePath[i]);
    }
    return outPath->size() >= 2;
}

QVector<int> RoutePlanWidget::lineIndexesForHop(int fromIndex, int toIndex) const
{
    if (fromIndex < 0 || fromIndex >= graph_.size())
    {
        return {};
    }

    for (const GraphEdge& edge : graph_[fromIndex])
    {
        if (edge.to == toIndex)
        {
            return edge.lineIndexes;
        }
    }
    return {};
}

int RoutePlanWidget::chooseLineForHop(int fromIndex, int toIndex, int preferredLineIndex) const
{
    const QVector<int> candidates = lineIndexesForHop(fromIndex, toIndex);
    if (candidates.isEmpty())
    {
        return -1;
    }
    if (preferredLineIndex >= 0 && candidates.contains(preferredLineIndex))
    {
        return preferredLineIndex;
    }
    return candidates.front();
}

QColor RoutePlanWidget::lineColorByIndex(int lineIndex) const
{
    const auto& lines = networkData_.lines();
    if (lineIndex < 0 || lineIndex >= lines.size())
    {
        return QColor(122, 132, 148, 235);
    }
    QColor color = lines[lineIndex].color;
    color.setAlpha(235);
    return color;
}

QString RoutePlanWidget::lineNameByIndex(int lineIndex) const
{
    const auto& lines = networkData_.lines();
    if (lineIndex < 0 || lineIndex >= lines.size())
    {
        return QStringLiteral("未知线");
    }

    QString name = lines[lineIndex].displayName.trimmed();
    if (name.isEmpty())
    {
        name = lines[lineIndex].name.trimmed();
    }
    if (name.isEmpty())
    {
        name = lines[lineIndex].id.trimmed();
    }
    if (name.isEmpty())
    {
        return QStringLiteral("线路");
    }

    if (name.contains(QStringLiteral("线")))
    {
        return name;
    }

    QString digits;
    digits.reserve(name.size());
    for (const QChar c : name)
    {
        if (c.isDigit())
        {
            digits.push_back(c);
        }
    }
    if (!digits.isEmpty())
    {
        return digits + QStringLiteral("号线");
    }
    return name;
}

void RoutePlanWidget::refreshView()
{
    routeReady_ = false;
    routeLineIndexes_.clear();
    routeLineStopCounts_.clear();
    routeFromName_.clear();
    routeToName_.clear();
    summaryText_.clear();

    if (networkData_.isEmpty())
    {
        hintText_ = loadError_.isEmpty() ? QStringLiteral("地铁网络加载失败") : loadError_;
        update();
        return;
    }

    if (targetStationName_.isEmpty())
    {
        hintText_ = QStringLiteral("请点击站点后规划线路");
        update();
        return;
    }

    const int fromIndex = findStationIndexByName(currentStationName_);
    const int toIndex = findStationIndexByName(targetStationName_);
    if (fromIndex < 0 || toIndex < 0)
    {
        hintText_ = QStringLiteral("站点识别失败，请重新选择站点");
        update();
        return;
    }

    if (fromIndex == toIndex)
    {
        hintText_ = QStringLiteral("当前已在目标站点");
        update();
        return;
    }

    QVector<int> path;
    if (!buildShortestPath(fromIndex, toIndex, &path))
    {
        hintText_ = QStringLiteral("当前无法规划到目标站点的路线");
        update();
        return;
    }

    int lineIndex = chooseLineForHop(path[0], path[1], -1);
    if (lineIndex < 0)
    {
        hintText_ = QStringLiteral("路线数据异常，请重试");
        update();
        return;
    }
    int segmentStopCount = 1;

    for (int i = 1; i < path.size() - 1; ++i)
    {
        const int nextLineIndex = chooseLineForHop(path[i], path[i + 1], lineIndex);
        if (nextLineIndex < 0)
        {
            hintText_ = QStringLiteral("路线数据异常，请重试");
            routeLineIndexes_.clear();
            update();
            return;
        }
        if (nextLineIndex != lineIndex)
        {
            routeLineIndexes_.push_back(lineIndex);
            routeLineStopCounts_.push_back(segmentStopCount);
            lineIndex = nextLineIndex;
            segmentStopCount = 1;
        }
        else
        {
            ++segmentStopCount;
        }
    }
    routeLineIndexes_.push_back(lineIndex);
    routeLineStopCounts_.push_back(segmentStopCount);

    routeReady_ = true;
    routeFromName_ = stations_[fromIndex].name;
    routeToName_ = stations_[toIndex].name;
    const int totalStops = path.size() - 1;
    const int transferCount = qMax(0, routeLineIndexes_.size() - 1);
    summaryText_ = QStringLiteral("%1 → %2 · %3站 · %4次换乘")
                       .arg(stations_[fromIndex].name, stations_[toIndex].name)
                       .arg(totalStops)
                       .arg(transferCount);
    update();
}




