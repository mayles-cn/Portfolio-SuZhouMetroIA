#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include "models/metro_network_data.h"
#include "services/fare_service.h"

namespace szmetro
{
class MetroToolService
{
public:
    MetroToolService();

    bool isReady() const;
    QString lastError() const;

    QJsonObject toolListLines() const;
    QJsonObject toolQueryStationLines(const QString& stationName) const;
    QJsonObject toolEstimateFare(const QString& fromStationName, const QString& toStationName) const;
    QStringList allStationNames() const;

private:
    struct StationInfo
    {
        QString id;
        QString name;
        QPointF position;
        QStringList lineNames;
    };

    static QString normalizeStationKey(const QString& name);
    static QStringList buildStationAliasKeys(const QString& name);
    static int levenshteinDistance(const QString& a, const QString& b);
    static int fuzzyScore(const QString& inputKey, const QString& candidateKey);
    QString resolveStationId(const QString& stationName, QString* resolvedName,
                            QJsonArray* suggestions) const;
    QJsonArray topStationSuggestions(const QString& stationName, int maxCount) const;

    MetroNetworkData networkData_;
    FareService fareService_;
    QString lastError_;
    QHash<QString, StationInfo> stationsById_;
    QHash<QString, QString> stationIdByNormalizedName_;
    QVector<QString> lineDisplayNames_;
    QStringList stationNameList_;
};
} // namespace szmetro
