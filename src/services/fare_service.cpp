#include "services/fare_service.h"

#include <QLineF>
#include <cmath>
#include <queue>
#include <utility>

namespace szmetro
{
namespace
{
constexpr double kConfiguredNetworkMaxFareYuan = 11.0;
constexpr double kDistanceAtFare6Km = 30.0;
constexpr double kDistancePerYuanAfter30Km = 9.0;
constexpr double kFallbackKmPerUnit = 0.02;
constexpr double kFallbackInterStationKm = 1.6;
} // namespace

void FareService::rebuild(const QVector<MetroDrawLine>& lines)
{
    stationIndexById_.clear();
    stationIds_.clear();
    graph_.clear();
    kmPerUnit_ = 0.0;

    auto ensureStationIndex = [this](const QString& stationId) -> int {
        const auto it = stationIndexById_.constFind(stationId);
        if (it != stationIndexById_.cend())
        {
            return it.value();
        }
        const int index = stationIds_.size();
        stationIndexById_.insert(stationId, index);
        stationIds_.push_back(stationId);
        graph_.push_back({});
        return index;
    };

    for (const MetroDrawLine& line : lines)
    {
        if (line.stations.size() < 2)
        {
            continue;
        }

        for (int i = 1; i < line.stations.size(); ++i)
        {
            const MetroDrawStation& prev = line.stations[i - 1];
            const MetroDrawStation& curr = line.stations[i];
            if (prev.id.isEmpty() || curr.id.isEmpty())
            {
                continue;
            }

            const int u = ensureStationIndex(prev.id);
            const int v = ensureStationIndex(curr.id);
            const double units = qMax(1.0, QLineF(prev.position, curr.position).length());

            graph_[u].push_back({v, units});
            graph_[v].push_back({u, units});
        }
    }

    if (stationIds_.isEmpty())
    {
        return;
    }

    double maxUnits = 0.0;
    for (int i = 0; i < stationIds_.size(); ++i)
    {
        const QVector<double> distances = dijkstraFrom(i);
        for (double d : distances)
        {
            if (d >= 0.0)
            {
                maxUnits = qMax(maxUnits, d);
            }
        }
    }

    if (maxUnits > 1e-6)
    {
        const double networkMaxKm =
            kDistanceAtFare6Km + (kConfiguredNetworkMaxFareYuan - 6.0) * kDistancePerYuanAfter30Km;
        kmPerUnit_ = networkMaxKm / maxUnits;
    }
    else
    {
        kmPerUnit_ = kFallbackKmPerUnit;
    }
}

bool FareService::isReady() const
{
    return !stationIds_.isEmpty() && !graph_.isEmpty();
}

int FareService::calculateFareYuan(const QString& fromStationId, const QString& toStationId) const
{
    if (fromStationId.isEmpty() || toStationId.isEmpty())
    {
        return -1;
    }
    if (fromStationId == toStationId)
    {
        return 0;
    }

    const double km = estimateDistanceKm(fromStationId, toStationId);
    if (km < 0.0)
    {
        return -1;
    }
    return fareByDistanceKm(km);
}

double FareService::estimateDistanceKm(const QString& fromStationId, const QString& toStationId) const
{
    const double units = shortestUnits(fromStationId, toStationId);
    if (units < 0.0)
    {
        return -1.0;
    }

    if (kmPerUnit_ > 1e-8)
    {
        return units * kmPerUnit_;
    }
    return units * kFallbackInterStationKm;
}

QVector<double> FareService::dijkstraFrom(int sourceIndex) const
{
    const int n = graph_.size();
    QVector<double> dist(n, -1.0);
    if (sourceIndex < 0 || sourceIndex >= n)
    {
        return dist;
    }

    using QueueItem = std::pair<double, int>;
    std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> pq;
    dist[sourceIndex] = 0.0;
    pq.push({0.0, sourceIndex});

    while (!pq.empty())
    {
        const auto [curDist, u] = pq.top();
        pq.pop();
        if (dist[u] >= 0.0 && curDist > dist[u] + 1e-9)
        {
            continue;
        }

        for (const Edge& e : graph_[u])
        {
            const double nd = curDist + e.units;
            if (dist[e.to] < 0.0 || nd < dist[e.to] - 1e-9)
            {
                dist[e.to] = nd;
                pq.push({nd, e.to});
            }
        }
    }
    return dist;
}

double FareService::shortestUnits(const QString& fromStationId, const QString& toStationId) const
{
    const auto fromIt = stationIndexById_.constFind(fromStationId);
    const auto toIt = stationIndexById_.constFind(toStationId);
    if (fromIt == stationIndexById_.cend() || toIt == stationIndexById_.cend())
    {
        return -1.0;
    }

    const QVector<double> distances = dijkstraFrom(fromIt.value());
    const double d = distances[toIt.value()];
    return (d >= 0.0 ? d : -1.0);
}

int FareService::fareByDistanceKm(double distanceKm)
{
    if (distanceKm <= 0.0)
    {
        return 0;
    }
    if (distanceKm <= 6.0)
    {
        return 2;
    }
    if (distanceKm <= 11.0)
    {
        return 3;
    }
    if (distanceKm <= 16.0)
    {
        return 4;
    }
    if (distanceKm <= 23.0)
    {
        return 5;
    }
    if (distanceKm <= 30.0)
    {
        return 6;
    }
    return 6 + static_cast<int>(std::ceil((distanceKm - 30.0) / 9.0));
}
} // namespace szmetro
