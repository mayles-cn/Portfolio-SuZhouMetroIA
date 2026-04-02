#pragma once

#include <QMainWindow>
class ChatPanelWidget;
class MetroMapWidget;
class InformationBarWidget;
class SettlementWidget;
class TextPromptWidget;
class ModelSettingsWidget;
class RoutePlanWidget;
class QLabel;
class QWidget;
class QKeyEvent;
class QResizeEvent;
class QShortcut;
class QPropertyAnimation;
class QRect;

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
    void layoutSettlementPanel();
    void layoutTextPromptPanel();
    void layoutModelSettingsPanel();
    void toggleSettlementPanel();
    void openSettlementPanel();
    void closeSettlementPanel();
    void toggleTextPromptPanel();
    void toggleModelSettingsPanel();
    QRect settlementShownGeometry() const;
    QRect settlementHiddenGeometry() const;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

    MetroMapWidget* metroMap_ = nullptr;
    ChatPanelWidget* chatPanel_ = nullptr;
    InformationBarWidget* informationBar_ = nullptr;
    SettlementWidget* settlementPanel_ = nullptr;
    TextPromptWidget* textPromptPanel_ = nullptr;
    ModelSettingsWidget* modelSettingsPanel_ = nullptr;
    RoutePlanWidget* routePlanWidget_ = nullptr;
    QWidget* brandingWidget_ = nullptr;
    QLabel* brandLogoLabel_ = nullptr;
    QLabel* brandTitleLabel_ = nullptr;
    QLabel* brandSubtitleLabel_ = nullptr;
    QShortcut* settlementShortcut_ = nullptr;
    QShortcut* textPromptShortcut_ = nullptr;
    QShortcut* modelSettingsShortcut_ = nullptr;
    QPropertyAnimation* settlementAnimation_ = nullptr;
    bool settlementExpanded_ = false;
};
