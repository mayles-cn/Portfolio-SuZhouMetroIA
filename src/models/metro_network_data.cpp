#include "models/metro_network_data.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace szmetro
{
namespace
{
QRectF parseBounds(const QJsonObject& object)
{
    const double minX = object.value("min_x").toDouble(0.0);
    const double maxX = object.value("max_x").toDouble(1.0);
    const double minY = object.value("min_y").toDouble(0.0);
    const double maxY = object.value("max_y").toDouble(1.0);
    const double width = qMax(1.0, maxX - minX);
    const double height = qMax(1.0, maxY - minY);
    return QRectF(minX, minY, width, height);
}
} // namespace

bool MetroNetworkData::loadFromResource(const QString& resourcePath, QString* errorMessage)
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = QString("Failed to open resource: %1").arg(resourcePath);
        }
        return false;
    }
    return loadFromBytes(file.readAll(), errorMessage);
}

const QVector<MetroDrawLine>& MetroNetworkData::lines() const
{
    return lines_;
}

const QRectF& MetroNetworkData::bounds() const
{
    return bounds_;
}

bool MetroNetworkData::isEmpty() const
{
    return lines_.isEmpty();
}

bool MetroNetworkData::loadFromBytes(const QByteArray& bytes, QString* errorMessage)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = QString("Invalid metro json: %1").arg(parseError.errorString());
        }
        return false;
    }

    const QJsonObject root = document.object();
    const QJsonObject layout = root.value("layout").toObject();
    bounds_ = parseBounds(layout.value("bounds").toObject());

    lines_.clear();
    const QJsonArray lineArray = root.value("lines").toArray();
    lines_.reserve(lineArray.size());

    for (const QJsonValue& lineValue : lineArray)
    {
        const QJsonObject lineObject = lineValue.toObject();
        MetroDrawLine line;
        line.id = lineObject.value("id").toString();
        line.name = lineObject.value("name").toString();
        line.displayName = lineObject.value("display_name").toString(line.name);
        line.color = QColor(lineObject.value("color").toString("#808080"));
        if (!line.color.isValid())
        {
            line.color = QColor("#808080");
        }

        const QJsonArray stationArray = lineObject.value("stations").toArray();
        line.stations.reserve(stationArray.size());
        for (const QJsonValue& stationValue : stationArray)
        {
            const QJsonObject stationObject = stationValue.toObject();
            MetroDrawStation station;
            station.id = stationObject.value("id").toString();
            station.name = stationObject.value("name").toString();
            station.position =
                QPointF(stationObject.value("x").toDouble(0.0),
                        stationObject.value("y").toDouble(0.0));
            station.interchange = stationObject.value("interchange").toBool(false);
            line.stations.push_back(station);
        }
        if (!line.stations.isEmpty())
        {
            lines_.push_back(std::move(line));
        }
    }

    if (lines_.isEmpty())
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = QStringLiteral("Metro data has no drawable lines.");
        }
        return false;
    }
    return true;
}
} // namespace szmetro
