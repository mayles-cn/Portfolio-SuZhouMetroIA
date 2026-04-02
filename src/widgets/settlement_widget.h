#pragma once

#include <QString>
#include <QWidget>

class SettlementWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit SettlementWidget(QWidget* parent = nullptr);

    int panelHeight() const;
    void prefillRouteTicket(const QString& fromStationName, const QString& toStationName,
                            int unitFareYuan);
    void prefillQuickTicket(const QString& ticketType, int unitFareYuan, int quantity);

signals:
    void paymentCompleted();
    void closeRequested();

private:
    void refreshSummary();
    void applyTicketType(const QString& typeName);
    QString generateTicketCode() const;

    static constexpr int kPanelHeight = 600;
    class QLabel* summaryLabel_ = nullptr;
    class QLabel* routeLabel_ = nullptr;
    class QLabel* ticketTypeValueLabel_ = nullptr;
    class QLabel* unitPriceValueLabel_ = nullptr;
    class QLabel* unitPriceLabel_ = nullptr;
    class QLabel* totalPriceLabel_ = nullptr;
    class QLabel* paidTicketInfoLabel_ = nullptr;
    class QSpinBox* quantitySpin_ = nullptr;
    class QPushButton* wechatButton_ = nullptr;
    class QPushButton* alipayButton_ = nullptr;
    class QPushButton* unionpayButton_ = nullptr;
    QString selectedPaymentMethod_ = QStringLiteral("微信");
    QString fromStationName_;
    QString toStationName_;
    QString ticketType_ = QStringLiteral("单程票");
    int unitPriceYuan_ = 2;
};
