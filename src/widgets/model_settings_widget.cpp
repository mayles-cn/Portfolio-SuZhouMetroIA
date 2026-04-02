#include "widgets/model_settings_widget.h"

#include "services/style_service.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

ModelSettingsWidget::ModelSettingsWidget(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(szmetro::UiStyleService::style(
        QStringLiteral("model_settings.root"),
        QStringLiteral("background-color: rgba(255,255,255,242);"
                       "border: 1px solid rgba(39,66,112,120);"
                       "border-radius: 10px;")));

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(10, 8, 10, 8);
    rootLayout->setSpacing(6);

    auto* topRow = new QHBoxLayout();
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->setSpacing(8);
    auto* title = new QLabel(QStringLiteral("\u6a21\u578b\u8bbe\u7f6e"), this);
    title->setStyleSheet(szmetro::UiStyleService::style(
        QStringLiteral("model_settings.title"),
        QStringLiteral("color: rgba(34,60,106,230);"
                       "font: 700 12px 'Microsoft YaHei';"
                       "border: none;")));
    topRow->addWidget(title, 0);

    stationHintLabel_ = new QLabel(QStringLiteral("\u5f53\u524d\u7ad9\u70b9\uff1a-\uff08\u5df2\u540c\u6b65\u7ed9\u6a21\u578b\uff09"),
                                   this);
    stationHintLabel_->setStyleSheet(szmetro::UiStyleService::style(
        QStringLiteral("model_settings.station_hint"),
        QStringLiteral("color: rgba(34,60,106,185);"
                       "font: 11px 'Microsoft YaHei';"
                       "border: none;")));
    topRow->addWidget(stationHintLabel_, 1);
    rootLayout->addLayout(topRow);

    auto* controlsRow = new QHBoxLayout();
    controlsRow->setContentsMargins(0, 0, 0, 0);
    controlsRow->setSpacing(10);

    const QString checkboxStyle = szmetro::UiStyleService::style(
        QStringLiteral("model_settings.checkbox"),
        QStringLiteral("QCheckBox {"
                       "color: rgba(34,60,106,220);"
                       "font: 11px 'Microsoft YaHei';"
                       "border: none;"
                       "}"));

    autoSetCurrentStationCheckbox_ = new QCheckBox(QStringLiteral("\u9009\u4e2d\u7ad9\u70b9\u540e\u8bbe\u4e3a\u5f53\u524d\u7ad9"), this);
    autoSetCurrentStationCheckbox_->setChecked(false);
    autoSetCurrentStationCheckbox_->setStyleSheet(checkboxStyle);
    controlsRow->addWidget(autoSetCurrentStationCheckbox_, 0);

    debugInfoCheckbox_ = new QCheckBox(QStringLiteral("\u663e\u793a\u8c03\u8bd5\u4fe1\u606f"), this);
    debugInfoCheckbox_->setChecked(false);
    debugInfoCheckbox_->setStyleSheet(checkboxStyle);
    controlsRow->addWidget(debugInfoCheckbox_, 0);

    modelLogCheckbox_ = new QCheckBox(QStringLiteral("\u6a21\u578b\u65e5\u5fd7"), this);
    modelLogCheckbox_->setChecked(false);
    modelLogCheckbox_->setStyleSheet(checkboxStyle);
    controlsRow->addWidget(modelLogCheckbox_, 0);

    modelStatusCheckbox_ = new QCheckBox(QStringLiteral("\u6a21\u578b\u72b6\u6001"), this);
    modelStatusCheckbox_->setChecked(false);
    modelStatusCheckbox_->setStyleSheet(checkboxStyle);
    controlsRow->addWidget(modelStatusCheckbox_, 0);

    voiceStatusCheckbox_ = new QCheckBox(QStringLiteral("\u8bed\u97f3\u72b6\u6001"), this);
    voiceStatusCheckbox_->setChecked(false);
    voiceStatusCheckbox_->setStyleSheet(checkboxStyle);
    controlsRow->addWidget(voiceStatusCheckbox_, 0);

    toolCallCheckbox_ = new QCheckBox(QStringLiteral("\u51fd\u6570\u8c03\u7528"), this);
    toolCallCheckbox_->setChecked(false);
    toolCallCheckbox_->setStyleSheet(checkboxStyle);
    controlsRow->addWidget(toolCallCheckbox_, 0);

    toolResultCheckbox_ = new QCheckBox(QStringLiteral("\u51fd\u6570\u7ed3\u679c"), this);
    toolResultCheckbox_->setChecked(false);
    toolResultCheckbox_->setStyleSheet(checkboxStyle);
    controlsRow->addWidget(toolResultCheckbox_, 0);
    controlsRow->addStretch(1);
    rootLayout->addLayout(controlsRow);

    connect(autoSetCurrentStationCheckbox_, &QCheckBox::toggled, this,
            &ModelSettingsWidget::autoSetCurrentStationChanged);
    connect(debugInfoCheckbox_, &QCheckBox::toggled, this, [this](bool enabled) {
        for (QCheckBox* checkbox :
             {modelLogCheckbox_, modelStatusCheckbox_, voiceStatusCheckbox_, toolCallCheckbox_,
              toolResultCheckbox_})
        {
            if (checkbox == nullptr)
            {
                continue;
            }
            checkbox->setEnabled(enabled);
            if (!enabled)
            {
                checkbox->setChecked(false);
            }
        }
    });
    connect(modelLogCheckbox_, &QCheckBox::toggled, this,
            &ModelSettingsWidget::modelLogEnabledChanged);
    connect(modelStatusCheckbox_, &QCheckBox::toggled, this,
            &ModelSettingsWidget::modelStatusVisibleChanged);
    connect(voiceStatusCheckbox_, &QCheckBox::toggled, this,
            &ModelSettingsWidget::voiceStatusVisibleChanged);
    connect(toolCallCheckbox_, &QCheckBox::toggled, this,
            &ModelSettingsWidget::toolCallVisibleChanged);
    connect(toolResultCheckbox_, &QCheckBox::toggled, this,
            &ModelSettingsWidget::toolResultVisibleChanged);
    if (debugInfoCheckbox_ != nullptr)
    {
        const bool debugEnabled = debugInfoCheckbox_->isChecked();
        for (QCheckBox* checkbox :
             {modelLogCheckbox_, modelStatusCheckbox_, voiceStatusCheckbox_, toolCallCheckbox_,
              toolResultCheckbox_})
        {
            if (checkbox == nullptr)
            {
                continue;
            }
            checkbox->setEnabled(debugEnabled);
            if (!debugEnabled)
            {
                checkbox->setChecked(false);
            }
        }
    }

    hide();
}

void ModelSettingsWidget::setCurrentStationName(const QString& stationName)
{
    if (stationHintLabel_ == nullptr)
    {
        return;
    }
    const QString station = stationName.trimmed().isEmpty() ? QStringLiteral("-")
                                                            : stationName.trimmed();
    stationHintLabel_->setText(
        QStringLiteral("\u5f53\u524d\u7ad9\u70b9\uff1a%1\uff08\u5df2\u540c\u6b65\u7ed9\u6a21\u578b\uff09")
            .arg(station));
}

void ModelSettingsWidget::togglePanel()
{
    if (isVisible())
    {
        closePanel();
    }
    else
    {
        openPanel();
    }
}

void ModelSettingsWidget::openPanel()
{
    show();
    raise();
}

void ModelSettingsWidget::closePanel()
{
    hide();
}

bool ModelSettingsWidget::isPanelVisible() const
{
    return isVisible();
}

