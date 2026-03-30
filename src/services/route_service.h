#pragma once

#include <QString>
#include <QStringList>

#include "models/metro_line.h"

namespace szmetro
{
class RouteService
{
public:
    RouteService();

    QStringList demoRoute() const;
    QString routeSummary() const;

private:
    MetroLine line1_;
};
} // namespace szmetro
