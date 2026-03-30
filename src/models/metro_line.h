#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace szmetro
{
struct Station
{
    QString name;
    int order = 0;
};

class MetroLine
{
public:
    explicit MetroLine(QString name = {});

    void addStation(QString stationName);
    const QString& name() const;
    QStringList stationNames() const;

private:
    QString name_;
    QVector<Station> stations_;
};
} // namespace szmetro
