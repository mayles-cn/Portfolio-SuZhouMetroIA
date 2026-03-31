#include "widgets/voice_assistant_widget.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSource>
#include <QIODevice>
#include <QMediaDevices>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QTimer>

#include <algorithm>
#include <array>
#include <cmath>

VoiceAssistantWidget::VoiceAssistantWidget(QWidget* parent)
    : QWidget(parent)
{
    setFixedSize(110, 110);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setWindowFlags(Qt::Widget | Qt::FramelessWindowHint);
    setCursor(Qt::PointingHandCursor);

    // 统一由定时器驱动绘制刷新，避免手动触发过多重绘。
    animationTimer_ = new QTimer(this);
    animationTimer_->setInterval(16);
    connect(animationTimer_, &QTimer::timeout, this, &VoiceAssistantWidget::tickAnimation);
    animationTimer_->start();

    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    const QAudioDevice inputDevice = QMediaDevices::defaultAudioInput();
    if (!inputDevice.isNull())
    {
        if (!inputDevice.isFormatSupported(format))
        {
            format = inputDevice.preferredFormat();
        }

        switch (format.sampleFormat())
        {
        case QAudioFormat::Int16:
            sampleKind_ = AudioSampleKind::Int16;
            break;
        case QAudioFormat::Int32:
            sampleKind_ = AudioSampleKind::Int32;
            break;
        case QAudioFormat::Float:
            sampleKind_ = AudioSampleKind::Float;
            break;
        default:
            sampleKind_ = AudioSampleKind::Unknown;
            break;
        }

        audioSource_ = new QAudioSource(inputDevice, format, this);
    }
}

void VoiceAssistantWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QPointF center = rect().center();
    const qreal radius = currentRadius_;

    // 外层呼吸光圈（点击后会保持静止）。
    for (int i = 0; i < 4; ++i)
    {
        const qreal ringRadius =
            radius + 6.0 + i * 5.3 + std::sin(phase_ * 1.2 + i * 0.85) * 2.8;
        const int alpha = pressed_ ? (92 - i * 18) : (48 - i * 10);
        QPen ringPen(QColor(130, 223, 255, std::max(0, alpha)));
        ringPen.setWidthF(1.85 + i * 0.32);
        painter.setPen(ringPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(center, ringRadius, ringRadius);
    }

    const QRectF orbRect(center.x() - radius, center.y() - radius, radius * 2.0, radius * 2.0);
    QPainterPath orbPath;
    orbPath.addEllipse(orbRect);

    // 毛玻璃球体基底。
    QLinearGradient glassGradient(orbRect.topLeft(), orbRect.bottomRight());
    glassGradient.setColorAt(0.0, QColor(255, 255, 255, 165));
    glassGradient.setColorAt(0.26, QColor(208, 228, 255, 128));
    glassGradient.setColorAt(0.62, QColor(101, 150, 255, 94));
    glassGradient.setColorAt(1.0, QColor(27, 43, 92, 88));
    painter.setPen(Qt::NoPen);
    painter.setBrush(glassGradient);
    painter.drawPath(orbPath);

    // 球体描边。
    QPen rimPen(QColor(255, 255, 255, pressed_ ? 168 : 128));
    rimPen.setWidthF(1.5);
    painter.setPen(rimPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(orbRect);

    // 球体内部彩色羽片。
    painter.save();
    painter.setClipPath(orbPath);
    painter.setCompositionMode(QPainter::CompositionMode_Screen);
    const std::array<QColor, 6> colors = {
        QColor(68, 226, 255, 210), QColor(78, 121, 255, 205),
        QColor(175, 98, 255, 205), QColor(255, 102, 196, 200),
        QColor(96, 255, 186, 198), QColor(255, 174, 72, 190)};
    for (int i = 0; i < static_cast<int>(colors.size()); ++i)
    {
        painter.save();
        const qreal angle = rotation_ + i * 60.0 + std::sin(phase_ * 2.4 + i) * 14.0;
        painter.translate(center);
        painter.rotate(angle);
        painter.scale(1.0, 0.68 + std::sin(phase_ + i * 0.8) * 0.05);

        QRadialGradient featherGradient(QPointF(0.0, -radius * 0.46), radius * 1.02);
        QColor main = colors[i];
        featherGradient.setColorAt(0.0, QColor(main.red(), main.green(), main.blue(), 240));
        featherGradient.setColorAt(0.58, QColor(main.red(), main.green(), main.blue(), 128));
        featherGradient.setColorAt(1.0, QColor(main.red(), main.green(), main.blue(), 0));

        painter.setPen(Qt::NoPen);
        painter.setBrush(featherGradient);
        painter.drawEllipse(QRectF(-radius * 0.28, -radius * 1.04, radius * 0.56,
                                   radius * 1.76));
        painter.restore();
    }
    painter.restore();

    // 中心高亮，增强层次感。
    QRadialGradient coreGlow(center + QPointF(0.0, radius * 0.10), radius * 0.65);
    const int coreAlpha = pressed_ ? 135 : 92;
    coreGlow.setColorAt(0.0, QColor(255, 255, 255, coreAlpha));
    coreGlow.setColorAt(0.45, QColor(170, 210, 255, coreAlpha / 2));
    coreGlow.setColorAt(1.0, QColor(170, 210, 255, 0));
    painter.setBrush(coreGlow);
    painter.drawEllipse(orbRect.adjusted(radius * 0.08, radius * 0.08,
                                         -radius * 0.08, -radius * 0.08));

    // 顶部高光，模拟玻璃反射。
    QRadialGradient highlight(center + QPointF(-radius * 0.30, -radius * 0.46), radius * 0.80);
    highlight.setColorAt(0.0, QColor(255, 255, 255, 170));
    highlight.setColorAt(0.45, QColor(255, 255, 255, 56));
    highlight.setColorAt(1.0, QColor(255, 255, 255, 0));
    painter.setPen(Qt::NoPen);
    painter.setBrush(highlight);
    painter.drawEllipse(orbRect);
}

void VoiceAssistantWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        startListening();
    }
    QWidget::mousePressEvent(event);
}

void VoiceAssistantWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        stopListening();
    }
    QWidget::mouseReleaseEvent(event);
}

void VoiceAssistantWidget::startListening()
{
    pressed_ = true;
    level_ = 0.0f;

    if (audioSource_ != nullptr)
    {
        audioDevice_ = audioSource_->start();
        if (audioDevice_ != nullptr)
        {
            connect(audioDevice_, &QIODevice::readyRead, this,
                    &VoiceAssistantWidget::handleAudioReadyRead, Qt::UniqueConnection);
        }
    }
}

void VoiceAssistantWidget::stopListening()
{
    pressed_ = false;
    level_ = 0.0f;
    targetRadius_ = 30.0f;
    update();

    if (audioDevice_ != nullptr)
    {
        disconnect(audioDevice_, &QIODevice::readyRead, this,
                   &VoiceAssistantWidget::handleAudioReadyRead);
        audioDevice_ = nullptr;
    }
    if (audioSource_ != nullptr)
    {
        audioSource_->stop();
    }
}

void VoiceAssistantWidget::handleAudioReadyRead()
{
    if (audioDevice_ == nullptr)
    {
        return;
    }

    const QByteArray data = audioDevice_->readAll();
    if (data.size() < static_cast<int>(sizeof(qint16)))
    {
        return;
    }

    double sum = 0.0;
    int sampleCount = 0;
    if (sampleKind_ == AudioSampleKind::Int16)
    {
        const int count = data.size() / static_cast<int>(sizeof(qint16));
        const qint16* samples = reinterpret_cast<const qint16*>(data.constData());
        for (int i = 0; i < count; ++i)
        {
            const double normalized = samples[i] / 32768.0;
            sum += normalized * normalized;
        }
        sampleCount = count;
    }
    else if (sampleKind_ == AudioSampleKind::Int32)
    {
        const int count = data.size() / static_cast<int>(sizeof(qint32));
        const qint32* samples = reinterpret_cast<const qint32*>(data.constData());
        for (int i = 0; i < count; ++i)
        {
            const double normalized = samples[i] / 2147483648.0;
            sum += normalized * normalized;
        }
        sampleCount = count;
    }
    else if (sampleKind_ == AudioSampleKind::Float)
    {
        const int count = data.size() / static_cast<int>(sizeof(float));
        const float* samples = reinterpret_cast<const float*>(data.constData());
        for (int i = 0; i < count; ++i)
        {
            const double normalized = std::clamp(static_cast<double>(samples[i]), -1.0, 1.0);
            sum += normalized * normalized;
        }
        sampleCount = count;
    }

    if (sampleCount <= 0)
    {
        return;
    }

    const double rms = std::sqrt(sum / sampleCount);
    const float amplified = static_cast<float>(std::clamp(rms * 4.4, 0.0, 1.0));
    level_ = level_ * 0.58f + amplified * 0.42f;
}

void VoiceAssistantWidget::tickAnimation()
{
    // 响应态保持动态，空闲态仅降低速度。
    phase_ += pressed_ ? 0.16f : 0.045f;
    if (phase_ > 1000.0f)
    {
        phase_ = 0.0f;
    }

    if (pressed_)
    {
        if (audioSource_ != nullptr)
        {
            targetRadius_ = 28.0f + level_ * (40.0f - 28.0f);
        }
        else
        {
            const float pseudo = 0.5f + 0.5f * std::sin(phase_ * 3.2f);
            targetRadius_ = 28.0f + pseudo * (38.0f - 28.0f);
        }
        rotation_ += 3.4f + level_ * 9.5f;
    }
    else
    {
        targetRadius_ = 30.0f + std::sin(phase_ * 0.72f) * 1.2f;
        rotation_ += 0.38f;
    }

    if (rotation_ > 360.0f)
    {
        rotation_ -= 360.0f;
    }

    const float blend = pressed_ ? 0.24f : 0.10f;
    currentRadius_ += (targetRadius_ - currentRadius_) * blend;
    currentRadius_ = std::clamp(currentRadius_, 28.0f, 40.0f);
    update();
}
