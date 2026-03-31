#include "mainwindow.h"

#include "widgets/chat_panel_widget.h"
#include "widgets/information_bar_widget.h"
#include "widgets/metro_map_widget.h"

#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QPixmap>
#include <QScreen>
#include <QResizeEvent>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    buildUi();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi()
{
    setWindowTitle(QStringLiteral("SuZhou Metro Smart Intelligent Assistant"));
    setWindowIcon(QIcon(":/icons/suzhouMetroLogo.ico"));
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setStyleSheet(QStringLiteral("background-color: #f7f8f8;"));

    metroMap_ = new MetroMapWidget(this);
    chatPanel_ = new ChatPanelWidget(this);
    informationBar_ = new InformationBarWidget(this);
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
    brandingWidget_->setStyleSheet("background: transparent; border: none;");

    auto* rootLayout = new QHBoxLayout(brandingWidget_);
    rootLayout->setContentsMargins(10, 8, 12, 8);
    rootLayout->setSpacing(8);

    brandLogoLabel_ = new QLabel(brandingWidget_);
    const QPixmap logoPixmap(":/icons/suzhouMetroLogo.ico");
    brandLogoLabel_->setPixmap(
        logoPixmap.scaled(30, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    brandLogoLabel_->setFixedSize(32, 32);
    brandLogoLabel_->setAlignment(Qt::AlignCenter);
    rootLayout->addWidget(brandLogoLabel_, 0, Qt::AlignTop);

    auto* textLayout = new QVBoxLayout();
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(1);

    brandTitleLabel_ = new QLabel(QStringLiteral("苏州轨道交通"), brandingWidget_);
    brandTitleLabel_->setStyleSheet(
        "color: #153C7A;"
        "font: 700 17px 'Microsoft YaHei';");
    brandTitleLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    brandSubtitleLabel_ = new QLabel(QStringLiteral("苏州地铁智能购票助理"), brandingWidget_);
    brandSubtitleLabel_->setStyleSheet(
        "color: rgba(31,58,98,185);"
        "font: 10px 'Microsoft YaHei';");
    brandSubtitleLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    textLayout->addWidget(brandTitleLabel_);
    textLayout->addWidget(brandSubtitleLabel_);
    rootLayout->addLayout(textLayout, 1);
}

void MainWindow::layoutControls()
{
    const int barHeight =
        (informationBar_ != nullptr) ? informationBar_->barHeight() : 0;
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
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape)
    {
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
