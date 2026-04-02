#pragma once

#include <QHash>
#include <QVector>
#include <QWidget>

#include "models/metro_network_data.h"

class QPaintEvent;

class RoutePlanWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit RoutePlanWidget(QWidget* parent = nullptr);

    void setCurrentStationName(const QString& stationName);
    void setTargetStationName(const QString& stationName);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    struct GraphEdge
    {
        int to = -1;
        QVector<int> lineIndexes;
    };

    QString normalizeStationName(QString stationName) const;
    void rebuildGraph();
    void refreshView();
    int findStationIndexByName(const QString& stationName) const;
    bool buildShortestPath(int fromIndex, int toIndex, QVector<int>* outPath) const;
    QVector<int> lineIndexesForHop(int fromIndex, int toIndex) const;
    int chooseLineForHop(int fromIndex, int toIndex, int preferredLineIndex) const;
    QColor lineColorByIndex(int lineIndex) const;
    QString lineNameByIndex(int lineIndex) const;

    szmetro::MetroNetworkData networkData_;
    QString loadError_;
    QVector<szmetro::MetroDrawStation> stations_;
    QHash<QString, int> stationIndexById_;
    QVector<QVector<GraphEdge>> graph_;

    QString currentStationName_;
    QString targetStationName_;
    QString hintText_ = QStringLiteral("请点击站点后规划线路");
    QString summaryText_;
    bool routeReady_ = false;
    QVector<int> routeLineIndexes_;
    QVector<int> routeLineStopCounts_;
    QString routeFromName_;
    QString routeToName_;
};
