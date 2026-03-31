#pragma once

#include <QStringList>
#include <QWidget>

class QLabel;
class QPaintEvent;

class StationPanelWidget final : public QWidget
{
public:
    explicit StationPanelWidget(QWidget* parent = nullptr);

    void clearSelection(const QString& currentStationName);
    void setSelectionInfo(const QString& currentStationName, const QString& targetStationName,
                          const QStringList& lineNames, int fareYuan);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QLabel* titleLabel_ = nullptr;
    QLabel* currentLabel_ = nullptr;
    QLabel* targetLabel_ = nullptr;
    QLabel* lineLabel_ = nullptr;
    QLabel* fareLabel_ = nullptr;
};
