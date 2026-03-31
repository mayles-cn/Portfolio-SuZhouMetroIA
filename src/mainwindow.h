#pragma once

#include <QMainWindow>
class ChatPanelWidget;
class MetroMapWidget;
class InformationBarWidget;
class QLabel;
class QWidget;
class QKeyEvent;
class QResizeEvent;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    void buildUi();
    void layoutControls();
    void buildBranding();
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

    MetroMapWidget* metroMap_ = nullptr;
    ChatPanelWidget* chatPanel_ = nullptr;
    InformationBarWidget* informationBar_ = nullptr;
    QWidget* brandingWidget_ = nullptr;
    QLabel* brandLogoLabel_ = nullptr;
    QLabel* brandTitleLabel_ = nullptr;
    QLabel* brandSubtitleLabel_ = nullptr;
};
