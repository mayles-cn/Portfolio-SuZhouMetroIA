#include "services/route_service.h"

namespace szmetro
{
RouteService::RouteService()
    : line1_("Line 1")
{
    line1_.addStation("Mudu");
    line1_.addStation("Shilu");
    line1_.addStation("Leqiao");
    line1_.addStation("Xinghai Square");
}

QStringList RouteService::demoRoute() const
{
    return line1_.stationNames();
}

QString RouteService::routeSummary() const
{
    return QString("%1 has %2 stations in the demo set.")
        .arg(line1_.name())
        .arg(line1_.stationNames().size());
}
} // namespace szmetro
