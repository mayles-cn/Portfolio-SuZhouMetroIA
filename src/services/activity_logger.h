#pragma once

#include <QJsonObject>
#include <QString>

class QFile;
class QMutex;

namespace szmetro
{
class ActivityLogger
{
public:
    static ActivityLogger& instance();

    bool initialize();
    bool isInitialized() const;
    QString logFilePath() const;

    void log(const QString& category, const QString& message,
             const QJsonObject& context = QJsonObject());

private:
    ActivityLogger() = default;
    ~ActivityLogger();

    ActivityLogger(const ActivityLogger&) = delete;
    ActivityLogger& operator=(const ActivityLogger&) = delete;

    QString resolveLogPath() const;
    QString formatLine(const QString& category, const QString& message,
                       const QJsonObject& context) const;

    QFile* file_ = nullptr;
    QMutex* mutex_ = nullptr;
    QString logPath_;
    bool initialized_ = false;
};

void installQtMessageLogging();
} // namespace szmetro
