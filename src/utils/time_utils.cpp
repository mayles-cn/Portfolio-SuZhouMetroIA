#include "utils/time_utils.h"

#include <QDateTime>

namespace szmetro::util
{
QString buildInfo(const QString& projectName)
{
    const QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    return QString("%1 bootstrap loaded at %2").arg(projectName, timestamp);
}
} // namespace szmetro::util
