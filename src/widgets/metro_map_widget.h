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
class QSpinBox;
class QPushButton;
class QWidget;
class StationPanelWidget;

class MetroMapWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit MetroMapWidget(QWidget* parent = nullptr);
    void resetInteractiveView();
    void focusStationFromText(const QString& text);
    void setAutoSetCurrentStationOnSelection(bool enabled);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

signals:
    void stationSelected(const QString& stationName);
    void currentStationChanged(const QString& stationName);
    void goToSettlementRequested(const QString& targetStationName);
    void routeSettlementRequested(const QString& fromStationName, const QString& toStationName,
                                  int unitFareYuan);
    void quickPurchaseRequested(const QString& ticketType, int unitFareYuan, int quantity);

private:
    void buildLineSelector();
    void buildQuickBuyPanel();
    void layoutSelectorPanel();
    void layoutStationPanel();
    void layoutQuickBuyPanel();
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
    void drawSelectedGoToMarker(QPainter& painter, const QRectF& drawRect);
    void drawCurrentStationMarker(QPainter& painter, const QRectF& drawRect);
    void drawCompassOverlay(QPainter& painter, const QRectF& drawRect);
    void updateStationPanelSelection();
    bool locateStationByName(const QString& stationName, szmetro::MetroDrawStation* outStation) const;
    int estimateTravelMinutes(double distanceKm, const QStringList& lineNames) const;
    QString resolveStationMapDirectory();
    QImage loadStationMap(const QString& stationId);
    void restartInactivityTimer();
    void initializeMapView();
    void initializeRandomCurrentStation();
    QRectF selectedGoToBadgeRect(const QRectF& drawRect) const;
    void emitRouteSettlementForSelected();

    QRectF calculateDrawRect() const;
    QPointF mapToWidget(const QPointF& point, const QRectF& drawRect) const;
    void drawStationLabels(QPainter& painter, const QRectF& drawRect);

    szmetro::MetroNetworkData networkData_;
    QString loadError_;
    QWidget* selectorPanel_ = nullptr;
    QWidget* quickBuyPanel_ = nullptr;
    StationPanelWidget* stationPanel_ = nullptr;
    QSpinBox* customFareSpin_ = nullptr;
    QPushButton* customFareBuyButton_ = nullptr;
    QPushButton* dayPassBuyButton_ = nullptr;
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
    QPointF currentStationPosition_;
    bool autoSetCurrentStationOnSelection_ = false;
    qreal zoomScale_ = 1.0;
    QPointF panOffset_;
    bool middleDragging_ = false;
    QPoint lastDragPos_;
    QString stationMapDirectory_;
    QHash<QString, QImage> stationMapCache_;
    QTimer* inactivityTimer_ = nullptr;
    szmetro::FareService fareService_;
};
