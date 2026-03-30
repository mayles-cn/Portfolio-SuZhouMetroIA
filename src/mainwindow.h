#pragma once

#include <QMainWindow>
#include <QString>

#include <memory>

namespace szmetro
{
class RouteService;
}

class InfoPanel;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    void buildUi();
    QString buildWelcomeText() const;

    std::unique_ptr<szmetro::RouteService> routeService_;
    InfoPanel* infoPanel_ = nullptr;
};
