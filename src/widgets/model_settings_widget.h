#pragma once

#include <QString>
#include <QWidget>

class QCheckBox;

class ModelSettingsWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit ModelSettingsWidget(QWidget* parent = nullptr);
    void setCurrentStationName(const QString& stationName);

    void togglePanel();
    void openPanel();
    void closePanel();
    bool isPanelVisible() const;

signals:
    void autoSetCurrentStationChanged(bool enabled);
    void modelLogEnabledChanged(bool enabled);
    void modelStatusVisibleChanged(bool visible);
    void voiceStatusVisibleChanged(bool visible);
    void toolCallVisibleChanged(bool visible);
    void toolResultVisibleChanged(bool visible);

private:
    class QLabel* stationHintLabel_ = nullptr;
    QCheckBox* autoSetCurrentStationCheckbox_ = nullptr;
    QCheckBox* debugInfoCheckbox_ = nullptr;
    QCheckBox* modelLogCheckbox_ = nullptr;
    QCheckBox* modelStatusCheckbox_ = nullptr;
    QCheckBox* voiceStatusCheckbox_ = nullptr;
    QCheckBox* toolCallCheckbox_ = nullptr;
    QCheckBox* toolResultCheckbox_ = nullptr;
};
