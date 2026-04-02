#include "services/activity_logger.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QMutex>
#include <QMutexLocker>
#include <QStringConverter>
#include <QTextStream>

#include <cstdio>

namespace szmetro
{
namespace
{
QtMessageHandler g_previousHandler = nullptr;

const char* messageTypeText(QtMsgType type)
{
    switch (type)
    {
    case QtDebugMsg:
        return "debug";
    case QtInfoMsg:
        return "info";
    case QtWarningMsg:
        return "warning";
    case QtCriticalMsg:
        return "critical";
    case QtFatalMsg:
        return "fatal";
    }
    return "unknown";
}

void activityQtMessageHandler(QtMsgType type, const QMessageLogContext& context,
                              const QString& msg)
{
    QJsonObject details{
        {QStringLiteral("file"), QString::fromUtf8(context.file ? context.file : "")},
        {QStringLiteral("line"), context.line},
        {QStringLiteral("function"), QString::fromUtf8(context.function ? context.function : "")},
    };
    ActivityLogger::instance().log(QStringLiteral("qt.%1").arg(messageTypeText(type)), msg,
                                   details);

    if (g_previousHandler != nullptr)
    {
        g_previousHandler(type, context, msg);
        return;
    }

    const QByteArray line =
        QStringLiteral("[%1] %2\n").arg(messageTypeText(type), msg).toLocal8Bit();
    std::fwrite(line.constData(), 1, static_cast<size_t>(line.size()), stderr);
    std::fflush(stderr);
}
} // namespace

ActivityLogger& ActivityLogger::instance()
{
    static ActivityLogger logger;
    return logger;
}

ActivityLogger::~ActivityLogger()
{
    if (file_ != nullptr)
    {
        file_->close();
        delete file_;
        file_ = nullptr;
    }
    delete mutex_;
    mutex_ = nullptr;
}

bool ActivityLogger::initialize()
{
    if (mutex_ == nullptr)
    {
        mutex_ = new QMutex();
    }

    QMutexLocker locker(mutex_);
    if (initialized_)
    {
        return true;
    }

    logPath_ = resolveLogPath();
    if (logPath_.isEmpty())
    {
        return false;
    }

    QFile* out = new QFile(logPath_);
    if (!out->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
    {
        delete out;
        return false;
    }

    QTextStream stream(out);
    stream.setEncoding(QStringConverter::Utf8);
    if (out->size() == 0)
    {
        stream.setGenerateByteOrderMark(true);
    }
    file_ = out;
    initialized_ = true;
    const QString line = formatLine(QStringLiteral("app"), QStringLiteral("logger initialized"),
                                    QJsonObject{{QStringLiteral("path"), logPath_}});
    stream << line << '\n';
    stream.flush();
    return true;
}

bool ActivityLogger::isInitialized() const
{
    return initialized_;
}

QString ActivityLogger::logFilePath() const
{
    return logPath_;
}

void ActivityLogger::log(const QString& category, const QString& message, const QJsonObject& context)
{
    if (!initialized_ && !initialize())
    {
        return;
    }

    if (mutex_ == nullptr || file_ == nullptr)
    {
        return;
    }

    QMutexLocker locker(mutex_);
    if (!initialized_ || file_ == nullptr)
    {
        return;
    }

    QTextStream stream(file_);
    stream.setEncoding(QStringConverter::Utf8);
    stream << formatLine(category, message, context) << '\n';
    stream.flush();
}

QString ActivityLogger::resolveLogPath() const
{
    const QDir currentDir = QDir::current();
    const QDir appDir = QDir(QCoreApplication::applicationDirPath());
    const QStringList roots = {
        currentDir.absolutePath(),
        appDir.absolutePath(),
    };

    for (const QString& root : roots)
    {
        QDir dir(root);
        if (!dir.exists(QStringLiteral("logs")))
        {
            if (!dir.mkpath(QStringLiteral("logs")))
            {
                continue;
            }
        }
        const QString filePath = dir.filePath(QStringLiteral("logs/activity.log"));
        return QDir::cleanPath(filePath);
    }
    return {};
}

QString ActivityLogger::formatLine(const QString& category, const QString& message,
                                   const QJsonObject& context) const
{
    const QString timeText =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
    QString line =
        QStringLiteral("[%1] [%2] %3").arg(timeText, category.trimmed(), message.trimmed());
    if (!context.isEmpty())
    {
        line += QStringLiteral(" | ");
        line += QString::fromUtf8(QJsonDocument(context).toJson(QJsonDocument::Compact));
    }
    return line;
}

void installQtMessageLogging()
{
    if (g_previousHandler == nullptr)
    {
        g_previousHandler = qInstallMessageHandler(activityQtMessageHandler);
    }
}
} // namespace szmetro
