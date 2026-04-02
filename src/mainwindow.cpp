#include "mainwindow.h"

#include "services/activity_logger.h"
#include "services/style_service.h"
#include "widgets/chat_panel_widget.h"
#include "widgets/information_bar_widget.h"
#include "widgets/metro_map_widget.h"
#include "widgets/model_settings_widget.h"
#include "widgets/route_plan_widget.h"
#include "widgets/settlement_widget.h"
#include "widgets/text_prompt_widget.h"

#include <QEasingCurve>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QScreen>
#include <QShortcut>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    buildUi();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi()
{
    setWindowTitle(QStringLiteral("苏州地铁智能助手"));
    setWindowIcon(QIcon(QStringLiteral(":/icons/suzhouMetroLogo.ico")));
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setStyleSheet(szmetro::UiStyleService::style(
        QStringLiteral("mainwindow.window"), QStringLiteral("background-color: #f7f8f8;")));

    metroMap_ = new MetroMapWidget(this);
    chatPanel_ = new ChatPanelWidget(this);
    informationBar_ = new InformationBarWidget(this);
    settlementPanel_ = new SettlementWidget(this);
    textPromptPanel_ = new TextPromptWidget(this);
    modelSettingsPanel_ = new ModelSettingsWidget(this);
    routePlanWidget_ = new RoutePlanWidget(this);

    settlementAnimation_ = new QPropertyAnimation(settlementPanel_, "geometry", this);
    settlementAnimation_->setDuration(320);
    settlementAnimation_->setEasingCurve(QEasingCurve::InOutCubic);

    settlementShortcut_ = new QShortcut(QKeySequence(Qt::Key_F8), this);
    settlementShortcut_->setContext(Qt::ApplicationShortcut);
    connect(settlementShortcut_, &QShortcut::activated, this, &MainWindow::toggleSettlementPanel);

    textPromptShortcut_ = new QShortcut(QKeySequence(Qt::Key_F7), this);
    textPromptShortcut_->setContext(Qt::ApplicationShortcut);
    connect(textPromptShortcut_, &QShortcut::activated, this, &MainWindow::toggleTextPromptPanel);

    modelSettingsShortcut_ = new QShortcut(QKeySequence(Qt::Key_F9), this);
    modelSettingsShortcut_->setContext(Qt::ApplicationShortcut);
    connect(modelSettingsShortcut_, &QShortcut::activated, this,
            &MainWindow::toggleModelSettingsPanel);

    if (textPromptPanel_ != nullptr && chatPanel_ != nullptr)
    {
        connect(textPromptPanel_, &TextPromptWidget::submitRequested, this,
                [this](const QString& text) {
                    chatPanel_->submitTextPrompt(text);
                });
    }

    if (chatPanel_ != nullptr && metroMap_ != nullptr)
    {
        connect(chatPanel_, &ChatPanelWidget::promptSubmitted, metroMap_,
                &MetroMapWidget::focusStationFromText);
        connect(metroMap_, &MetroMapWidget::stationSelected, chatPanel_,
                &ChatPanelWidget::appendMapSelectionHint);
        connect(metroMap_, &MetroMapWidget::stationSelected, routePlanWidget_,
                &RoutePlanWidget::setTargetStationName);
        connect(metroMap_, &MetroMapWidget::currentStationChanged, chatPanel_,
                &ChatPanelWidget::setCurrentStationForModel);
        connect(metroMap_, &MetroMapWidget::currentStationChanged, routePlanWidget_,
                &RoutePlanWidget::setCurrentStationName);
        connect(metroMap_, &MetroMapWidget::goToSettlementRequested, this,
                [this](const QString&) { openSettlementPanel(); });
        connect(metroMap_, &MetroMapWidget::routeSettlementRequested, this,
                [this](const QString& from, const QString& to, int unitFare) {
                    if (settlementPanel_ != nullptr)
                    {
                        settlementPanel_->prefillRouteTicket(from, to, unitFare);
                    }
                    openSettlementPanel();
                });
        connect(metroMap_, &MetroMapWidget::quickPurchaseRequested, this,
                [this](const QString& type, int unitFare, int quantity) {
                    if (settlementPanel_ != nullptr)
                    {
                        settlementPanel_->prefillQuickTicket(type, unitFare, quantity);
                    }
                    openSettlementPanel();
                });
    }

    if (chatPanel_ != nullptr)
    {
        connect(chatPanel_, &ChatPanelWidget::openSettlementRequested, this,
                &MainWindow::openSettlementPanel);
    }
    if (settlementPanel_ != nullptr)
    {
        connect(settlementPanel_, &SettlementWidget::paymentCompleted, chatPanel_,
                &ChatPanelWidget::resetConversationUi);
        connect(settlementPanel_, &SettlementWidget::closeRequested, this,
                &MainWindow::closeSettlementPanel);
    }

    if (modelSettingsPanel_ != nullptr && chatPanel_ != nullptr)
    {
        connect(modelSettingsPanel_, &ModelSettingsWidget::modelLogEnabledChanged, chatPanel_,
                &ChatPanelWidget::setModelLogEnabled);
        connect(modelSettingsPanel_, &ModelSettingsWidget::modelStatusVisibleChanged, chatPanel_,
                &ChatPanelWidget::setModelStatusVisible);
        connect(modelSettingsPanel_, &ModelSettingsWidget::voiceStatusVisibleChanged, chatPanel_,
                &ChatPanelWidget::setVoiceStatusVisible);
        connect(modelSettingsPanel_, &ModelSettingsWidget::toolCallVisibleChanged, chatPanel_,
                &ChatPanelWidget::setToolCallVisible);
        connect(modelSettingsPanel_, &ModelSettingsWidget::toolResultVisibleChanged, chatPanel_,
                &ChatPanelWidget::setToolResultVisible);

        // 默认关闭调试输出相关能力。
        chatPanel_->setModelLogEnabled(false);
        chatPanel_->setModelStatusVisible(false);
        chatPanel_->setVoiceStatusVisible(false);
        chatPanel_->setToolCallVisible(false);
        chatPanel_->setToolResultVisible(false);
    }

    if (metroMap_ != nullptr && modelSettingsPanel_ != nullptr)
    {
        connect(metroMap_, &MetroMapWidget::currentStationChanged, modelSettingsPanel_,
                &ModelSettingsWidget::setCurrentStationName);
        connect(modelSettingsPanel_, &ModelSettingsWidget::autoSetCurrentStationChanged, metroMap_,
                &MetroMapWidget::setAutoSetCurrentStationOnSelection);
        metroMap_->setAutoSetCurrentStationOnSelection(false);
    }

    chatPanel_->setFixedWidth(400);
    buildBranding();

    if (QScreen* primaryScreen = QGuiApplication::primaryScreen())
    {
        setGeometry(primaryScreen->geometry());
    }

    layoutControls();
}

void MainWindow::buildBranding()
{
    brandingWidget_ = new QWidget(this);
    brandingWidget_->setAttribute(Qt::WA_StyledBackground, false);
    brandingWidget_->setStyleSheet(
        szmetro::UiStyleService::style(QStringLiteral("mainwindow.branding_widget"),
                                       QStringLiteral("background: transparent; border: none;")));

    auto* rootLayout = new QHBoxLayout(brandingWidget_);
    rootLayout->setContentsMargins(10, 8, 12, 8);
    rootLayout->setSpacing(8);

    brandLogoLabel_ = new QLabel(brandingWidget_);
    const QPixmap logoPixmap(QStringLiteral(":/icons/suzhouMetroLogo.ico"));
    brandLogoLabel_->setPixmap(
        logoPixmap.scaled(30, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    brandLogoLabel_->setFixedSize(32, 32);
    brandLogoLabel_->setAlignment(Qt::AlignCenter);
    rootLayout->addWidget(brandLogoLabel_, 0, Qt::AlignTop);

    auto* textLayout = new QVBoxLayout();
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(1);

    brandTitleLabel_ = new QLabel(QStringLiteral("苏州地铁"), brandingWidget_);
    brandTitleLabel_->setStyleSheet(szmetro::UiStyleService::style(
        QStringLiteral("mainwindow.brand_title"),
        QStringLiteral("color: #153C7A;"
                       "font: 700 17px 'Microsoft YaHei';")));
    brandTitleLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    brandSubtitleLabel_ = new QLabel(QStringLiteral("智能出行与购票助手"), brandingWidget_);
    brandSubtitleLabel_->setStyleSheet(szmetro::UiStyleService::style(
        QStringLiteral("mainwindow.brand_subtitle"),
        QStringLiteral("color: rgba(31,58,98,185);"
                       "font: 10px 'Microsoft YaHei';")));
    brandSubtitleLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    textLayout->addWidget(brandTitleLabel_);
    textLayout->addWidget(brandSubtitleLabel_);
    rootLayout->addLayout(textLayout, 1);
}

void MainWindow::layoutControls()
{
    const int barHeight = (informationBar_ != nullptr) ? informationBar_->barHeight() : 0;
    const int contentHeight = qMax(0, height() - barHeight);
    const int chatWidth = (chatPanel_ != nullptr) ? chatPanel_->width() : 0;
    const int mapWidth = qMax(0, width() - chatWidth);

    if (metroMap_ != nullptr)
    {
        metroMap_->setGeometry(0, 0, mapWidth, contentHeight);
    }

    if (chatPanel_ != nullptr)
    {
        chatPanel_->setGeometry(mapWidth, 0, chatWidth, contentHeight);
    }

    if (informationBar_ != nullptr)
    {
        informationBar_->setGeometry(0, contentHeight, width(), barHeight);
    }

    if (brandingWidget_ != nullptr)
    {
        const int margin = 14;
        const int panelWidth = 270;
        const int panelHeight = 54;
        brandingWidget_->setGeometry(margin, margin, panelWidth, panelHeight);
        brandingWidget_->raise();
    }

    if (routePlanWidget_ != nullptr)
    {
        const int margin = 14;
        const int x = margin;
        const int y = margin + 54 + 10;
        const int width = 100;
        const int height = 330;
        routePlanWidget_->setGeometry(x, y, width, height);
        routePlanWidget_->raise();
    }

    layoutSettlementPanel();
    layoutTextPromptPanel();
    layoutModelSettingsPanel();
}

void MainWindow::layoutSettlementPanel()
{
    if (settlementPanel_ == nullptr)
    {
        return;
    }

    if (settlementAnimation_ != nullptr &&
        settlementAnimation_->state() == QAbstractAnimation::Running)
    {
        settlementAnimation_->stop();
    }

    settlementPanel_->setGeometry(
        settlementExpanded_ ? settlementShownGeometry() : settlementHiddenGeometry());
    settlementPanel_->raise();
}

void MainWindow::layoutTextPromptPanel()
{
    if (textPromptPanel_ == nullptr)
    {
        return;
    }

    const int panelWidth = qMax(360, width() - 620);
    const int panelHeight = 52;
    const int x = (width() - panelWidth) / 2;
    const int y = 16;
    textPromptPanel_->setGeometry(x, y, panelWidth, panelHeight);
    textPromptPanel_->raise();
}

void MainWindow::layoutModelSettingsPanel()
{
    if (modelSettingsPanel_ == nullptr)
    {
        return;
    }

    const int panelWidth = 700;
    const int panelHeight = 112;
    const int margin = 16;
    const int x = qMax(0, width() - panelWidth - margin);
    const int y = margin;
    modelSettingsPanel_->setGeometry(x, y, panelWidth, panelHeight);
    if (modelSettingsPanel_->isPanelVisible())
    {
        modelSettingsPanel_->raise();
    }
}

void MainWindow::toggleSettlementPanel()
{
    if (settlementPanel_ == nullptr || settlementAnimation_ == nullptr)
    {
        return;
    }

    settlementAnimation_->stop();
    settlementPanel_->setVisible(true);
    settlementPanel_->raise();

    settlementAnimation_->setStartValue(settlementPanel_->geometry());
    settlementAnimation_->setEndValue(
        settlementExpanded_ ? settlementHiddenGeometry() : settlementShownGeometry());
    settlementExpanded_ = !settlementExpanded_;
    szmetro::ActivityLogger::instance().log(
        QStringLiteral("ui.settlement"), QStringLiteral("toggle settlement panel"),
        QJsonObject{{QStringLiteral("expanded"), settlementExpanded_}});
    settlementAnimation_->start();
}

void MainWindow::openSettlementPanel()
{
    if (settlementPanel_ == nullptr || settlementAnimation_ == nullptr)
    {
        return;
    }

    settlementAnimation_->stop();
    settlementPanel_->setVisible(true);
    settlementPanel_->raise();
    if (settlementExpanded_ && settlementPanel_->geometry() == settlementShownGeometry())
    {
        return;
    }

    settlementAnimation_->setStartValue(settlementPanel_->geometry());
    settlementAnimation_->setEndValue(settlementShownGeometry());
    settlementExpanded_ = true;
    szmetro::ActivityLogger::instance().log(
        QStringLiteral("ui.settlement"), QStringLiteral("open settlement panel"));
    settlementAnimation_->start();
}

void MainWindow::closeSettlementPanel()
{
    if (settlementPanel_ == nullptr || settlementAnimation_ == nullptr)
    {
        return;
    }

    if (!settlementExpanded_ && settlementPanel_->geometry() == settlementHiddenGeometry())
    {
        return;
    }

    settlementAnimation_->stop();
    settlementPanel_->setVisible(true);
    settlementPanel_->raise();
    settlementAnimation_->setStartValue(settlementPanel_->geometry());
    settlementAnimation_->setEndValue(settlementHiddenGeometry());
    settlementExpanded_ = false;
    settlementAnimation_->start();
}

void MainWindow::toggleTextPromptPanel()
{
    if (textPromptPanel_ == nullptr)
    {
        return;
    }

    textPromptPanel_->togglePanel();
    szmetro::ActivityLogger::instance().log(
        QStringLiteral("ui.text_prompt"), QStringLiteral("toggle text prompt panel"),
        QJsonObject{{QStringLiteral("visible"), textPromptPanel_->isPanelVisible()}});
}

void MainWindow::toggleModelSettingsPanel()
{
    if (modelSettingsPanel_ == nullptr)
    {
        return;
    }

    modelSettingsPanel_->togglePanel();
    if (modelSettingsPanel_->isPanelVisible())
    {
        modelSettingsPanel_->raise();
    }
    szmetro::ActivityLogger::instance().log(
        QStringLiteral("ui.model_settings"), QStringLiteral("toggle model settings panel"),
        QJsonObject{{QStringLiteral("visible"), modelSettingsPanel_->isPanelVisible()}});
}

QRect MainWindow::settlementShownGeometry() const
{
    if (settlementPanel_ == nullptr)
    {
        return {};
    }

    const int panelWidth = qMax(0, width() - 800);
    const int x = (width() - panelWidth) / 2;
    return QRect(x, 0, panelWidth, settlementPanel_->panelHeight());
}

QRect MainWindow::settlementHiddenGeometry() const
{
    const QRect shownRect = settlementShownGeometry();
    return QRect(shownRect.x(), -shownRect.height(), shownRect.width(), shownRect.height());
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape)
    {
        if (modelSettingsPanel_ != nullptr && modelSettingsPanel_->isPanelVisible())
        {
            modelSettingsPanel_->closePanel();
            return;
        }
        if (textPromptPanel_ != nullptr && textPromptPanel_->isPanelVisible())
        {
            textPromptPanel_->closePanel();
            return;
        }
        szmetro::ActivityLogger::instance().log(QStringLiteral("ui.window"),
                                                QStringLiteral("escape pressed, closing window"));
        close();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    layoutControls();
}

