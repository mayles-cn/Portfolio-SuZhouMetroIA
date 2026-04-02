#include "widgets/settlement_widget.h"

#include <QAbstractButton>
#include <QAbstractSpinBox>
#include <QButtonGroup>
#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

SettlementWidget::SettlementWidget(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet("background: transparent;");
    setFixedHeight(kPanelHeight);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto* panel = new QFrame(this);
    panel->setObjectName("settlementPanel");
    panel->setStyleSheet(
        "#settlementPanel {"
        "background-color: rgba(94, 101, 112, 232);"
        "border-bottom-left-radius: 16px;"
        "border-bottom-right-radius: 16px;"
        "}"
        "#titleLabel {"
        "color: rgba(242, 246, 252, 246);"
        "font: 700 21px 'Microsoft YaHei';"
        "}"
        "#subtitleLabel {"
        "color: rgba(219, 226, 238, 232);"
        "font: 12px 'Microsoft YaHei';"
        "}"
        "#summaryLabel {"
        "color: rgba(235, 241, 251, 244);"
        "font: 700 14px 'Microsoft YaHei';"
        "}"
        "#infoLabel {"
        "color: rgba(213, 222, 236, 236);"
        "font: 12px 'Microsoft YaHei';"
        "}"
        "#readonlyValue {"
        "color: rgba(233, 239, 249, 246);"
        "background-color: rgba(118, 126, 140, 186);"
        "font: 700 12px 'Microsoft YaHei';"
        "border-radius: 8px;"
        "padding: 6px 10px;"
        "}"
        "#ticketInfoSuccess {"
        "color: rgba(20, 79, 45, 238);"
        "background-color: rgba(233, 247, 238, 228);"
        "font: 12px 'Microsoft YaHei';"
        "border-radius: 10px;"
        "padding: 12px;"
        "}");

    auto* panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(24, 14, 24, 14);
    panelLayout->setSpacing(9);

    auto* titleLabel = new QLabel(QStringLiteral("车票结算"), panel);
    titleLabel->setObjectName("titleLabel");
    panelLayout->addWidget(titleLabel, 0, Qt::AlignLeft);

    auto* subtitle =
        new QLabel(QStringLiteral("票种与票价已根据进入方式自动确定，请确认张数并支付"), panel);
    subtitle->setObjectName("subtitleLabel");
    panelLayout->addWidget(subtitle, 0, Qt::AlignLeft);

    summaryLabel_ = new QLabel(QStringLiteral("待支付订单"), panel);
    summaryLabel_->setObjectName("summaryLabel");
    panelLayout->addWidget(summaryLabel_, 0, Qt::AlignLeft);

    routeLabel_ = new QLabel(panel);
    routeLabel_->setObjectName("infoLabel");
    routeLabel_->setWordWrap(true);
    panelLayout->addWidget(routeLabel_, 0, Qt::AlignLeft);

    auto* formGrid = new QGridLayout();
    formGrid->setContentsMargins(0, 6, 0, 0);
    formGrid->setHorizontalSpacing(8);
    formGrid->setVerticalSpacing(8);

    auto* ticketTypeTitle = new QLabel(QStringLiteral("票种"), panel);
    ticketTypeTitle->setObjectName("infoLabel");
    ticketTypeValueLabel_ = new QLabel(panel);
    ticketTypeValueLabel_->setObjectName("readonlyValue");
    formGrid->addWidget(ticketTypeTitle, 0, 0);
    formGrid->addWidget(ticketTypeValueLabel_, 0, 1);

    auto* unitPriceTitle = new QLabel(QStringLiteral("票价"), panel);
    unitPriceTitle->setObjectName("infoLabel");
    unitPriceValueLabel_ = new QLabel(panel);
    unitPriceValueLabel_->setObjectName("readonlyValue");
    formGrid->addWidget(unitPriceTitle, 1, 0);
    formGrid->addWidget(unitPriceValueLabel_, 1, 1);

    auto* quantityTitle = new QLabel(QStringLiteral("张数"), panel);
    quantityTitle->setObjectName("infoLabel");
    auto* quantityRow = new QHBoxLayout();
    quantityRow->setContentsMargins(0, 0, 0, 0);
    quantityRow->setSpacing(6);

    auto* minusButton = new QPushButton(QStringLiteral("-"), panel);
    minusButton->setCursor(Qt::PointingHandCursor);
    minusButton->setFixedSize(36, 32);

    quantitySpin_ = new QSpinBox(panel);
    quantitySpin_->setRange(1, 20);
    quantitySpin_->setValue(1);
    quantitySpin_->setButtonSymbols(QAbstractSpinBox::NoButtons);
    quantitySpin_->setAlignment(Qt::AlignCenter);
    quantitySpin_->setReadOnly(true);
    quantitySpin_->setFixedHeight(32);

    auto* plusButton = new QPushButton(QStringLiteral("+"), panel);
    plusButton->setCursor(Qt::PointingHandCursor);
    plusButton->setFixedSize(36, 32);

    const QString quantityBtnStyle = QStringLiteral(
        "QPushButton {"
        "background-color: rgba(236, 241, 249, 244);"
        "color: rgba(30, 61, 101, 240);"
        "border: none;"
        "border-radius: 8px;"
        "font: 700 18px 'Microsoft YaHei';"
        "}"
        "QPushButton:pressed { background-color: rgba(222, 232, 245, 250); }");
    minusButton->setStyleSheet(quantityBtnStyle);
    plusButton->setStyleSheet(quantityBtnStyle);
    quantitySpin_->setStyleSheet(
        "QSpinBox {"
        "background-color: rgba(246, 248, 252, 230);"
        "color: rgba(25, 52, 88, 245);"
        "border: none;"
        "border-radius: 8px;"
        "font: 700 13px 'Microsoft YaHei';"
        "padding: 4px 8px;"
        "}");

    quantityRow->addWidget(minusButton);
    quantityRow->addWidget(quantitySpin_, 1);
    quantityRow->addWidget(plusButton);

    auto* quantityRowContainer = new QWidget(panel);
    quantityRowContainer->setLayout(quantityRow);
    formGrid->addWidget(quantityTitle, 2, 0);
    formGrid->addWidget(quantityRowContainer, 2, 1);

    panelLayout->addLayout(formGrid);

    unitPriceLabel_ = new QLabel(panel);
    unitPriceLabel_->setObjectName("infoLabel");
    panelLayout->addWidget(unitPriceLabel_, 0, Qt::AlignLeft);

    totalPriceLabel_ = new QLabel(panel);
    totalPriceLabel_->setObjectName("summaryLabel");
    panelLayout->addWidget(totalPriceLabel_, 0, Qt::AlignLeft);

    auto* payMethodTitle = new QLabel(QStringLiteral("支付方式"), panel);
    payMethodTitle->setObjectName("infoLabel");
    panelLayout->addWidget(payMethodTitle, 0, Qt::AlignLeft);

    auto* payMethodRow = new QHBoxLayout();
    payMethodRow->setContentsMargins(0, 0, 0, 0);
    payMethodRow->setSpacing(8);

    wechatButton_ = new QPushButton(QStringLiteral("微信"), panel);
    alipayButton_ = new QPushButton(QStringLiteral("支付宝"), panel);
    unionpayButton_ = new QPushButton(QStringLiteral("云闪付"), panel);
    wechatButton_->setIcon(QIcon(QStringLiteral(":/images/payWeixin.png")));
    alipayButton_->setIcon(QIcon(QStringLiteral(":/images/payZhifubao.png")));
    unionpayButton_->setIcon(QIcon(QStringLiteral(":/images/payYinlian.png")));

    for (QPushButton* btn : {wechatButton_, alipayButton_, unionpayButton_})
    {
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setIconSize(QSize(18, 18));
        btn->setStyleSheet(
            "QPushButton {"
            "background-color: rgba(235,242,252,238);"
            "color: #2e4f80;"
            "border: none;"
            "border-radius: 8px;"
            "font: 700 11px 'Microsoft YaHei';"
            "padding: 7px 10px;"
            "}"
            "QPushButton:checked {"
            "background-color: rgba(218,232,252,248);"
            "}");
        payMethodRow->addWidget(btn, 1);
    }
    wechatButton_->setChecked(true);
    panelLayout->addLayout(payMethodRow);

    auto* actionRow = new QHBoxLayout();
    actionRow->setContentsMargins(0, 0, 0, 0);
    actionRow->setSpacing(8);

    auto* payButton = new QPushButton(QStringLiteral("模拟支付"), panel);
    payButton->setCursor(Qt::PointingHandCursor);
    payButton->setMinimumHeight(36);
    payButton->setStyleSheet(
        "QPushButton {"
        "background-color: rgba(34, 101, 202, 240);"
        "color: white;"
        "border: none;"
        "border-radius: 10px;"
        "font: 700 14px 'Microsoft YaHei';"
        "padding: 8px 16px;"
        "}"
        "QPushButton:hover { background-color: rgba(30, 92, 186, 248); }"
        "QPushButton:pressed { background-color: rgba(24, 77, 160, 250); }");
    actionRow->addWidget(payButton, 1);

    auto* closeButton = new QPushButton(QStringLiteral("关闭"), panel);
    closeButton->setCursor(Qt::PointingHandCursor);
    closeButton->setMinimumHeight(36);
    closeButton->setStyleSheet(
        "QPushButton {"
        "background-color: rgba(143, 152, 167, 225);"
        "color: rgba(244, 247, 252, 245);"
        "border: none;"
        "border-radius: 10px;"
        "font: 700 13px 'Microsoft YaHei';"
        "padding: 8px 14px;"
        "}"
        "QPushButton:hover { background-color: rgba(132, 143, 160, 236); }"
        "QPushButton:pressed { background-color: rgba(116, 128, 146, 246); }");
    actionRow->addWidget(closeButton, 0);
    panelLayout->addLayout(actionRow);

    paidTicketInfoLabel_ = new QLabel(panel);
    paidTicketInfoLabel_->setObjectName("ticketInfoSuccess");
    paidTicketInfoLabel_->setWordWrap(true);
    paidTicketInfoLabel_->setMinimumHeight(112);
    paidTicketInfoLabel_->setVisible(false);
    panelLayout->addWidget(paidTicketInfoLabel_);

    rootLayout->addWidget(panel);

    auto* payMethodGroup = new QButtonGroup(this);
    payMethodGroup->setExclusive(true);
    payMethodGroup->addButton(wechatButton_);
    payMethodGroup->addButton(alipayButton_);
    payMethodGroup->addButton(unionpayButton_);

    connect(payMethodGroup, &QButtonGroup::buttonClicked, this, [this](QAbstractButton* button) {
        if (button == wechatButton_)
        {
            selectedPaymentMethod_ = QStringLiteral("微信");
        }
        else if (button == alipayButton_)
        {
            selectedPaymentMethod_ = QStringLiteral("支付宝");
        }
        else
        {
            selectedPaymentMethod_ = QStringLiteral("云闪付");
        }
    });
    connect(minusButton, &QPushButton::clicked, this, [this]() {
        if (quantitySpin_ != nullptr)
        {
            quantitySpin_->setValue(qMax(quantitySpin_->minimum(), quantitySpin_->value() - 1));
        }
    });
    connect(plusButton, &QPushButton::clicked, this, [this]() {
        if (quantitySpin_ != nullptr)
        {
            quantitySpin_->setValue(qMin(quantitySpin_->maximum(), quantitySpin_->value() + 1));
        }
    });
    connect(quantitySpin_, &QSpinBox::valueChanged, this, [this](int) { refreshSummary(); });

    connect(closeButton, &QPushButton::clicked, this, [this]() { emit closeRequested(); });
    connect(payButton, &QPushButton::clicked, this, [this]() {
        const int quantity = quantitySpin_->value();
        const int totalYuan = unitPriceYuan_ * quantity;
        const QString paidAt =
            QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));

        const QString tripInfo = (!fromStationName_.isEmpty() && !toStationName_.isEmpty())
                                     ? QStringLiteral("行程：%1 -> %2\n").arg(fromStationName_, toStationName_)
                                     : QString();
        paidTicketInfoLabel_->setText(
            QStringLiteral("出票成功\n"
                           "订单号：%1\n"
                           "%2"
                           "票种：%3\n"
                           "票价：%4 元/张\n"
                           "数量：%5 张\n"
                           "支付方式：%6\n"
                           "实付：%7 元\n"
                           "出票时间：%8")
                .arg(generateTicketCode())
                .arg(tripInfo)
                .arg(ticketType_)
                .arg(unitPriceYuan_)
                .arg(quantity)
                .arg(selectedPaymentMethod_)
                .arg(totalYuan)
                .arg(paidAt));
        paidTicketInfoLabel_->setVisible(true);
        emit paymentCompleted();
        QTimer::singleShot(3000, this, [this]() { emit closeRequested(); });
    });

    refreshSummary();
}

int SettlementWidget::panelHeight() const
{
    return kPanelHeight;
}

void SettlementWidget::prefillRouteTicket(const QString& fromStationName, const QString& toStationName,
                                          int unitFareYuan)
{
    fromStationName_ = fromStationName.trimmed();
    toStationName_ = toStationName.trimmed();
    ticketType_ = QStringLiteral("单程票");
    unitPriceYuan_ = qBound(2, unitFareYuan, 13);
    quantitySpin_->setValue(1);
    paidTicketInfoLabel_->setVisible(false);
    refreshSummary();
}

void SettlementWidget::prefillQuickTicket(const QString& ticketType, int unitFareYuan, int quantity)
{
    fromStationName_.clear();
    toStationName_.clear();
    applyTicketType(ticketType);
    if (ticketType_ == QStringLiteral("单日畅行旅游票"))
    {
        unitPriceYuan_ = 25;
    }
    else
    {
        unitPriceYuan_ = qBound(2, unitFareYuan, 13);
    }
    quantitySpin_->setValue(qBound(1, quantity, 20));
    paidTicketInfoLabel_->setVisible(false);
    refreshSummary();
}

void SettlementWidget::refreshSummary()
{
    if (ticketTypeValueLabel_ != nullptr)
    {
        ticketTypeValueLabel_->setText(ticketType_);
    }
    if (unitPriceValueLabel_ != nullptr)
    {
        unitPriceValueLabel_->setText(QStringLiteral("%1 元").arg(unitPriceYuan_));
    }

    if (!fromStationName_.isEmpty() && !toStationName_.isEmpty())
    {
        summaryLabel_->setText(QStringLiteral("路线购票订单"));
        routeLabel_->setText(QStringLiteral("从 %1 到 %2").arg(fromStationName_, toStationName_));
    }
    else
    {
        summaryLabel_->setText(QStringLiteral("快捷购票订单"));
        routeLabel_->setText(QStringLiteral("票种：%1").arg(ticketType_));
    }

    const int quantity = quantitySpin_->value();
    const int total = unitPriceYuan_ * quantity;
    unitPriceLabel_->setText(QStringLiteral("票价 %1 元 × %2 张").arg(unitPriceYuan_).arg(quantity));
    totalPriceLabel_->setText(QStringLiteral("应付总额：%1 元").arg(total));
}

void SettlementWidget::applyTicketType(const QString& typeName)
{
    const bool dayPass = (typeName == QStringLiteral("单日畅行旅游票")) ||
                         (typeName == QStringLiteral("One-Day Pass"));
    ticketType_ = dayPass ? QStringLiteral("单日畅行旅游票") : QStringLiteral("单程票");
    if (ticketType_ == QStringLiteral("单日畅行旅游票"))
    {
        unitPriceYuan_ = 25;
    }
    else
    {
        unitPriceYuan_ = qBound(2, unitPriceYuan_, 13);
    }
}

QString SettlementWidget::generateTicketCode() const
{
    const QString timestamp =
        QDateTime::currentDateTime().toString(QStringLiteral("yyMMddhhmmss"));
    const int randSuffix = QRandomGenerator::global()->bounded(1000, 9999);
    return QStringLiteral("SZM%1%2").arg(timestamp).arg(randSuffix);
}
