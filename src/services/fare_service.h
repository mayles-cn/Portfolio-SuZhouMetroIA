#pragma once

#include <QHash>
#include <QString>
#include <QVector>

#include "models/metro_network_data.h"

namespace szmetro
{
class FareService
{
public:
    FareService() = default;

    void rebuild(const QVector<MetroDrawLine>& lines);
    bool isReady() const;

    int calculateFareYuan(const QString& fromStationId, const QString& toStationId) const;
    double estimateDistanceKm(const QString& fromStationId, const QString& toStationId) const;

private:
    struct Edge
    {
        int to = -1;
        double units = 0.0;
    };

    QVector<double> dijkstraFrom(int sourceIndex) const;
    double shortestUnits(const QString& fromStationId, const QString& toStationId) const;
    static int fareByDistanceKm(double distanceKm);

    QHash<QString, int> stationIndexById_;
    QVector<QString> stationIds_;
    QVector<QVector<Edge>> graph_;
    double kmPerUnit_ = 0.0;
};
} // namespace szmetro
