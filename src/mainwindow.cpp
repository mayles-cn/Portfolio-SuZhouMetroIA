#include "mainwindow.h"

#include "services/route_service.h"
#include "utils/time_utils.h"
#include "widgets/info_panel.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      routeService_(std::make_unique<szmetro::RouteService>())
{
    buildUi();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi()
{
    setWindowTitle("SuZhouMetroIA");

    infoPanel_ = new InfoPanel(this);
    infoPanel_->setTitle("Project Bootstrap");
    infoPanel_->setContent(buildWelcomeText());
    setCentralWidget(infoPanel_);
}

QString MainWindow::buildWelcomeText() const
{
    const QStringList route = routeService_->demoRoute();
    return QString("Welcome to SuZhouMetroIA.\n\n"
                   "%1\n"
                   "Demo route: %2\n"
                   "%3")
        .arg(routeService_->routeSummary(), route.join(" -> "),
             szmetro::util::buildInfo("SuZhouMetroIA"));
}
