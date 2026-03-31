#pragma once

#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

namespace szmetro
{
struct MetroDrawStation
{
    QString id;
    QString name;
    QPointF position;
    bool interchange = false;
};

struct MetroDrawLine
{
    QString id;
    QString name;
    QString displayName;
    QColor color;
    QVector<MetroDrawStation> stations;
};

class MetroNetworkData
{
public:
    bool loadFromResource(const QString& resourcePath, QString* errorMessage = nullptr);

    const QVector<MetroDrawLine>& lines() const;
    const QRectF& bounds() const;
    bool isEmpty() const;

private:
    bool loadFromBytes(const QByteArray& bytes, QString* errorMessage);

    QVector<MetroDrawLine> lines_;
    QRectF bounds_;
};
} // namespace szmetro
