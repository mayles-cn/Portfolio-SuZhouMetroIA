#include <QApplication>
#include <QIcon>
#include <QJsonObject>
#include <QObject>

#include "mainwindow.h"
#include "services/activity_logger.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/icons/suzhouMetroLogo.ico"));
    szmetro::ActivityLogger::instance().initialize();
    szmetro::installQtMessageLogging();
    szmetro::ActivityLogger::instance().log(
        QStringLiteral("app"),
        QStringLiteral("application startup"),
        QJsonObject{{QStringLiteral("argv_count"), argc},
                    {QStringLiteral("log_file"),
                     szmetro::ActivityLogger::instance().logFilePath()}});
    QObject::connect(&app, &QCoreApplication::aboutToQuit, []() {
        szmetro::ActivityLogger::instance().log(QStringLiteral("app"),
                                                QStringLiteral("application shutdown"));
    });

    MainWindow window;
    window.show();

    return app.exec();
}
