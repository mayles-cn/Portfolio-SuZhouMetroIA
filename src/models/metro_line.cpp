#include "models/metro_line.h"

#include <utility>

namespace szmetro
{
MetroLine::MetroLine(QString name)
    : name_(std::move(name))
{
}

void MetroLine::addStation(QString stationName)
{
    Station station;
    station.order = stations_.size();
    station.name = std::move(stationName);
    stations_.push_back(std::move(station));
}

const QString& MetroLine::name() const
{
    return name_;
}

QStringList MetroLine::stationNames() const
{
    QStringList names;
    names.reserve(stations_.size());
    for (const Station& station : stations_)
    {
        names.push_back(station.name);
    }

    return names;
}
} // namespace szmetro
