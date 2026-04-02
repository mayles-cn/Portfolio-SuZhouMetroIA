#include "services/metro_tool_service.h"

#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>

namespace szmetro
{
namespace
{
constexpr int kStrongMatchScore = 78;
constexpr int kWeakMatchScore = 66;
constexpr int kUniqueDeltaScore = 8;
}

MetroToolService::MetroToolService()
{
    if (!networkData_.loadFromResource(QStringLiteral(":/metro_data/suzhou_metro_network_core.json"),
                                       &lastError_))
    {
        if (lastError_.isEmpty())
        {
            lastError_ = QStringLiteral("Failed to load metro map resource.");
        }
        return;
    }

    fareService_.rebuild(networkData_.lines());

    QSet<QString> seenLines;
    for (const MetroDrawLine& line : networkData_.lines())
    {
        if (!line.displayName.isEmpty() && !seenLines.contains(line.displayName))
        {
            seenLines.insert(line.displayName);
            lineDisplayNames_.push_back(line.displayName);
        }

        for (const MetroDrawStation& station : line.stations)
        {
            if (station.id.isEmpty())
            {
                continue;
            }

            StationInfo& info = stationsById_[station.id];
            info.id = station.id;
            info.name = station.name;
            info.position = station.position;
            if (!line.displayName.isEmpty() && !info.lineNames.contains(line.displayName))
            {
                info.lineNames.push_back(line.displayName);
            }
        }
    }

    for (auto it = stationsById_.cbegin(); it != stationsById_.cend(); ++it)
    {
        if (!it->name.isEmpty() && !stationNameList_.contains(it->name))
        {
            stationNameList_.push_back(it->name);
        }

        const QStringList aliasKeys = buildStationAliasKeys(it->name);
        for (const QString& alias : aliasKeys)
        {
            if (!alias.isEmpty() && !stationIdByNormalizedName_.contains(alias))
            {
                stationIdByNormalizedName_.insert(alias, it->id);
            }
        }
    }

    std::sort(stationNameList_.begin(), stationNameList_.end());
}

bool MetroToolService::isReady() const
{
    return !stationsById_.isEmpty();
}

QString MetroToolService::lastError() const
{
    return lastError_;
}

QJsonObject MetroToolService::toolListLines() const
{
    QJsonArray lines;
    for (const QString& name : lineDisplayNames_)
    {
        lines.push_back(name);
    }

    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("line_count"), lineDisplayNames_.size()},
        {QStringLiteral("lines"), lines},
    };
}

QJsonObject MetroToolService::toolQueryStationLines(const QString& stationName) const
{
    QString resolvedName;
    QJsonArray suggestions;
    const QString stationId = resolveStationId(stationName, &resolvedName, &suggestions);
    if (stationId.isEmpty())
    {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("station_not_found")},
            {QStringLiteral("station_name"), stationName},
            {QStringLiteral("suggestions"), suggestions},
        };
    }

    const auto stationIt = stationsById_.constFind(stationId);
    if (stationIt == stationsById_.cend())
    {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("station_not_found")},
            {QStringLiteral("station_name"), stationName},
            {QStringLiteral("suggestions"), suggestions},
        };
    }

    QJsonArray lines;
    for (const QString& line : stationIt->lineNames)
    {
        lines.push_back(line);
    }

    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("station_id"), stationIt->id},
        {QStringLiteral("station_name"), stationIt->name},
        {QStringLiteral("fuzzy_matched"),
         normalizeStationKey(stationName) != normalizeStationKey(stationIt->name)},
        {QStringLiteral("lines"), lines},
    };
}

QJsonObject MetroToolService::toolEstimateFare(const QString& fromStationName,
                                               const QString& toStationName) const
{
    QString resolvedFromName;
    QString resolvedToName;
    QJsonArray fromSuggestions;
    QJsonArray toSuggestions;

    const QString fromStationId =
        resolveStationId(fromStationName, &resolvedFromName, &fromSuggestions);
    const QString toStationId =
        resolveStationId(toStationName, &resolvedToName, &toSuggestions);
    if (fromStationId.isEmpty() || toStationId.isEmpty())
    {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("station_not_found")},
            {QStringLiteral("from_station_name"), fromStationName},
            {QStringLiteral("to_station_name"), toStationName},
            {QStringLiteral("from_suggestions"), fromSuggestions},
            {QStringLiteral("to_suggestions"), toSuggestions},
        };
    }

    const int fare = fareService_.calculateFareYuan(fromStationId, toStationId);
    const double km = fareService_.estimateDistanceKm(fromStationId, toStationId);
    if (fare < 0 || km < 0.0)
    {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("error"), QStringLiteral("fare_calculation_failed")},
            {QStringLiteral("from_station_name"), fromStationName},
            {QStringLiteral("to_station_name"), toStationName},
        };
    }

    const auto fromStation = stationsById_.constFind(fromStationId);
    const auto toStation = stationsById_.constFind(toStationId);
    const QString fromCanonicalName =
        fromStation != stationsById_.cend() ? fromStation->name : fromStationName;
    const QString toCanonicalName =
        toStation != stationsById_.cend() ? toStation->name : toStationName;

    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("from_station_name"), fromCanonicalName},
        {QStringLiteral("to_station_name"), toCanonicalName},
        {QStringLiteral("from_fuzzy_matched"),
         normalizeStationKey(fromStationName) != normalizeStationKey(fromCanonicalName)},
        {QStringLiteral("to_fuzzy_matched"),
         normalizeStationKey(toStationName) != normalizeStationKey(toCanonicalName)},
        {QStringLiteral("estimated_distance_km"), km},
        {QStringLiteral("estimated_fare_yuan"), fare},
    };
}

QStringList MetroToolService::allStationNames() const
{
    return stationNameList_;
}

QString MetroToolService::normalizeStationKey(const QString& name)
{
    QString key = name.trimmed().toLower();
    if (key.isEmpty())
    {
        return {};
    }

    const QStringList removeTokens = {
        QStringLiteral("地铁站"),
        QStringLiteral("站点"),
        QStringLiteral("火车站"),
        QStringLiteral("高铁站"),
        QStringLiteral("地铁"),
    };
    for (const QString& token : removeTokens)
    {
        key.replace(token, QStringLiteral(""));
    }

    if (key.endsWith(QStringLiteral("站")) && key.size() > 1)
    {
        key.chop(1);
    }

    key.remove(QRegularExpression(QStringLiteral("[\\s\\-()（）,，.。!！?？:：、/\\\\]+")));
    return key.trimmed();
}

QStringList MetroToolService::buildStationAliasKeys(const QString& name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
    {
        return {};
    }

    QStringList variants = {
        trimmed,
        trimmed + QStringLiteral("站"),
        trimmed + QStringLiteral("地铁站"),
        trimmed + QStringLiteral("站点"),
    };

    if (trimmed.endsWith(QStringLiteral("站")) && trimmed.size() > 1)
    {
        variants.push_back(trimmed.left(trimmed.size() - 1));
    }

    if (trimmed.contains(QStringLiteral("火车站")))
    {
        QString replaced = trimmed;
        replaced.replace(QStringLiteral("火车站"), QStringLiteral("站"));
        variants.push_back(replaced);
    }

    if (trimmed.startsWith(QStringLiteral("高铁")) && trimmed.size() > 2)
    {
        variants.push_back(trimmed.mid(2));
    }

    QSet<QString> seen;
    QStringList keys;
    for (const QString& variant : variants)
    {
        const QString key = normalizeStationKey(variant);
        if (!key.isEmpty() && !seen.contains(key))
        {
            seen.insert(key);
            keys.push_back(key);
        }
    }
    return keys;
}

int MetroToolService::levenshteinDistance(const QString& a, const QString& b)
{
    const int n = a.size();
    const int m = b.size();
    if (n == 0)
    {
        return m;
    }
    if (m == 0)
    {
        return n;
    }

    QVector<int> prev(m + 1, 0);
    QVector<int> cur(m + 1, 0);
    for (int j = 0; j <= m; ++j)
    {
        prev[j] = j;
    }

    for (int i = 1; i <= n; ++i)
    {
        cur[0] = i;
        for (int j = 1; j <= m; ++j)
        {
            const int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + cost});
        }
        prev.swap(cur);
    }

    return prev[m];
}

int MetroToolService::fuzzyScore(const QString& inputKey, const QString& candidateKey)
{
    if (inputKey.isEmpty() || candidateKey.isEmpty())
    {
        return 0;
    }
    if (inputKey == candidateKey)
    {
        return 100;
    }

    if (candidateKey.contains(inputKey) || inputKey.contains(candidateKey))
    {
        const int delta = std::abs(inputKey.size() - candidateKey.size());
        return std::max(60, 88 - delta * 3);
    }

    const int maxLen = std::max(inputKey.size(), candidateKey.size());
    if (maxLen <= 0)
    {
        return 0;
    }

    const int dist = levenshteinDistance(inputKey, candidateKey);
    int score = static_cast<int>((1.0 - static_cast<double>(dist) / static_cast<double>(maxLen)) * 70.0);
    if (!inputKey.isEmpty() && !candidateKey.isEmpty() && inputKey.front() == candidateKey.front())
    {
        score += 5;
    }

    return std::max(0, score);
}

QString MetroToolService::resolveStationId(const QString& stationName, QString* resolvedName,
                                           QJsonArray* suggestions) const
{
    if (resolvedName != nullptr)
    {
        resolvedName->clear();
    }

    if (suggestions != nullptr)
    {
        *suggestions = topStationSuggestions(stationName, 3);
    }

    const QString key = normalizeStationKey(stationName);
    if (key.isEmpty())
    {
        return {};
    }

    const auto directIt = stationIdByNormalizedName_.constFind(key);
    if (directIt != stationIdByNormalizedName_.cend())
    {
        const auto stationIt = stationsById_.constFind(directIt.value());
        if (resolvedName != nullptr && stationIt != stationsById_.cend())
        {
            *resolvedName = stationIt->name;
        }
        return directIt.value();
    }

    QString bestStationId;
    int bestScore = -1;
    int secondBestScore = -1;

    for (auto it = stationIdByNormalizedName_.cbegin(); it != stationIdByNormalizedName_.cend();
         ++it)
    {
        const int score = fuzzyScore(key, it.key());
        if (score > bestScore)
        {
            secondBestScore = bestScore;
            bestScore = score;
            bestStationId = it.value();
        }
        else if (score > secondBestScore)
        {
            secondBestScore = score;
        }
    }

    const bool accepted =
        bestScore >= kStrongMatchScore ||
        (bestScore >= kWeakMatchScore && (bestScore - secondBestScore) >= kUniqueDeltaScore);
    if (!accepted || bestStationId.isEmpty())
    {
        return {};
    }

    const auto stationIt = stationsById_.constFind(bestStationId);
    if (resolvedName != nullptr && stationIt != stationsById_.cend())
    {
        *resolvedName = stationIt->name;
    }
    return bestStationId;
}

QJsonArray MetroToolService::topStationSuggestions(const QString& stationName, int maxCount) const
{
    QJsonArray result;
    const QString key = normalizeStationKey(stationName);
    if (key.isEmpty() || maxCount <= 0)
    {
        return result;
    }

    struct Suggestion
    {
        QString name;
        int score = 0;
    };

    QVector<Suggestion> scored;
    scored.reserve(stationNameList_.size());

    for (const QString& name : stationNameList_)
    {
        const int score = fuzzyScore(key, normalizeStationKey(name));
        if (score > 35)
        {
            scored.push_back({name, score});
        }
    }

    std::sort(scored.begin(), scored.end(), [](const Suggestion& a, const Suggestion& b) {
        if (a.score != b.score)
        {
            return a.score > b.score;
        }
        return a.name < b.name;
    });

    QSet<QString> seen;
    for (const Suggestion& item : scored)
    {
        if (seen.contains(item.name))
        {
            continue;
        }

        seen.insert(item.name);
        result.push_back(item.name);
        if (result.size() >= maxCount)
        {
            break;
        }
    }

    return result;
}
} // namespace szmetro
