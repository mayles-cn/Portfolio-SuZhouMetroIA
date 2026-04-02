#pragma once

#include <QStringList>
#include <QWidget>

class QLabel;
class QPaintEvent;

class StationPanelWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit StationPanelWidget(QWidget* parent = nullptr);

    void clearSelection(const QString& currentStationName);
    void setSelectionInfo(const QString& currentStationName, const QString& targetStationName,
                          const QStringList& lineNames, int fareYuan, double distanceKm = -1.0,
                          int etaMinutes = -1);

signals:
    void goToHereClicked(const QString& targetStationName);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QLabel* titleLabel_ = nullptr;
    QLabel* currentLabel_ = nullptr;
    QLabel* targetLabel_ = nullptr;
    QLabel* lineLabel_ = nullptr;
    QLabel* fareLabel_ = nullptr;
    QLabel* distanceLabel_ = nullptr;
    QLabel* etaLabel_ = nullptr;
    class QPushButton* goButton_ = nullptr;
    QString targetStationName_;
};
