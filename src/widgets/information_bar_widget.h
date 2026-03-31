#pragma once

#include <QColor>
#include <QStringList>
#include <QWidget>

class QPaintEvent;
class QResizeEvent;
class QTimer;

class InformationBarWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit InformationBarWidget(QWidget* parent = nullptr);
    int barHeight() const;

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void loadConfigFromResource();
    void rebuildDisplayText();
    void onScrollTick();

    QColor backgroundColor_ = QColor(20, 30, 45, 200);
    QColor textColor_ = QColor(245, 250, 255, 235);
    int fontSize_ = 12;
    int configuredHeight_ = 34;
    int scrollSpeed_ = 1;
    int textGap_ = 72;
    int timerIntervalMs_ = 28;
    QStringList messages_;
    QString displayText_;
    int offsetX_ = 0;
    QTimer* scrollTimer_ = nullptr;
};
