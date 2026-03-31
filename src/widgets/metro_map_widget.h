#pragma once

#include <QHash>
#include <QImage>
#include <QWidget>

#include "models/metro_network_data.h"
#include "services/fare_service.h"

class QPaintEvent;
class QResizeEvent;
class QWheelEvent;
class QMouseEvent;
class QButtonGroup;
class QVariantAnimation;
class QTimer;
class QWidget;
class StationPanelWidget;

class MetroMapWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit MetroMapWidget(QWidget* parent = nullptr);
    void resetInteractiveView();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void buildLineSelector();
    void layoutSelectorPanel();
    void layoutStationPanel();
    void onLineButtonClicked(int lineIndex);
    QRectF focusedBoundsForLine(int lineIndex) const;
    QRectF focusedBoundsForStation(const QPointF& stationPosition) const;
    QRectF clampRectToBounds(const QRectF& rect, const QRectF& bounds) const;
    void startViewTransition(const QRectF& targetBounds);
    bool pickStationAt(const QPointF& widgetPos, szmetro::MetroDrawStation* outStation,
                       QStringList* outLines) const;
    QStringList collectStationLines(const QString& stationId) const;
    void drawLineStartBadges(QPainter& painter, const QRectF& drawRect);
    void drawFocusedStationMap(QPainter& painter, const QRectF& drawRect);
    void drawCompassOverlay(QPainter& painter, const QRectF& drawRect);
    QString resolveStationMapDirectory();
    QImage loadStationMap(const QString& stationId);
    void restartInactivityTimer();
    void initializeMapView();
    void initializeRandomCurrentStation();

    QRectF calculateDrawRect() const;
    QPointF mapToWidget(const QPointF& point, const QRectF& drawRect) const;
    void drawStationLabels(QPainter& painter, const QRectF& drawRect);

    szmetro::MetroNetworkData networkData_;
    QString loadError_;
    QWidget* selectorPanel_ = nullptr;
    StationPanelWidget* stationPanel_ = nullptr;
    QButtonGroup* lineButtonGroup_ = nullptr;
    QVariantAnimation* viewAnimation_ = nullptr;
    QRectF viewBounds_;
    int selectedLineIndex_ = -1;
    QString selectedStationId_;
    QString selectedStationName_;
    QPointF selectedStationPosition_;
    bool selectedStationInterchange_ = false;
    QStringList selectedStationLines_;
    QString currentStationId_;
    QString currentStationName_;
    qreal zoomScale_ = 1.0;
    QPointF panOffset_;
    bool middleDragging_ = false;
    QPoint lastDragPos_;
    QString stationMapDirectory_;
    QHash<QString, QImage> stationMapCache_;
    QTimer* inactivityTimer_ = nullptr;
    szmetro::FareService fareService_;
};
