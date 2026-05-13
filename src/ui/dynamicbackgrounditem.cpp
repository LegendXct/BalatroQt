#include "dynamicbackgrounditem.h"
#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <QStyleOptionGraphicsItem>
#include <cmath>

DynamicBackgroundItem::DynamicBackgroundItem(QGraphicsItem *parent)
    : QGraphicsObject(parent)
{
    setZValue(-1000);
    connect(&mTimer, &QTimer::timeout, this, [this]() {
        mTime += 0.016;
        update();
    });
    mTimer.start(16);
}

QRectF DynamicBackgroundItem::boundingRect() const
{
    return QRectF(0, 0, mW, mH);
}

void DynamicBackgroundItem::setSceneSize(qreal w, qreal h)
{
    prepareGeometryChange();
    mW = qMax<qreal>(1, w);
    mH = qMax<qreal>(1, h);
    update();
}

void DynamicBackgroundItem::setMood(Mood mood)
{
    if (mMood == mood) return;
    mMood = mood;
    update();
}

QColor DynamicBackgroundItem::baseA() const
{
    switch (mMood) {
    case Mood::Tarot:       return QColor(74, 35, 108);
    case Mood::Spectral:    return QColor(31, 35, 92);
    case Mood::Celestial:   return QColor(23, 58, 104);
    case Mood::Buffoon:     return QColor(96, 59, 20);
    case Mood::Standard:    return QColor(28, 82, 61);
    case Mood::Shop:        return QColor(37, 54, 62);
    case Mood::BlindSelect: return QColor(38, 46, 62);
    case Mood::Default:     return QColor(29, 87, 64);
    }
    return QColor(29, 87, 64);
}

QColor DynamicBackgroundItem::baseB() const
{
    switch (mMood) {
    case Mood::Tarot:       return QColor(129, 59, 151);
    case Mood::Spectral:    return QColor(86, 41, 128);
    case Mood::Celestial:   return QColor(30, 91, 145);
    case Mood::Buffoon:     return QColor(145, 89, 33);
    case Mood::Standard:    return QColor(42, 114, 78);
    case Mood::Shop:        return QColor(59, 76, 82);
    case Mood::BlindSelect: return QColor(52, 63, 86);
    case Mood::Default:     return QColor(41, 114, 79);
    }
    return QColor(41, 114, 79);
}

QColor DynamicBackgroundItem::accent() const
{
    switch (mMood) {
    case Mood::Tarot:       return QColor(205, 111, 255, 95);
    case Mood::Spectral:    return QColor(124, 104, 255, 95);
    case Mood::Celestial:   return QColor(78, 180, 255, 90);
    case Mood::Buffoon:     return QColor(255, 174, 68, 80);
    case Mood::Standard:    return QColor(95, 220, 145, 80);
    case Mood::Shop:        return QColor(255, 190, 80, 70);
    case Mood::BlindSelect: return QColor(125, 170, 255, 65);
    case Mood::Default:     return QColor(92, 217, 145, 75);
    }
    return QColor(92, 217, 145, 75);
}

void DynamicBackgroundItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setRenderHint(QPainter::Antialiasing);

    QLinearGradient bg(0, 0, mW, mH);
    bg.setColorAt(0.0, baseA().darker(120));
    bg.setColorAt(0.48, baseB());
    bg.setColorAt(1.0, baseA().darker(145));
    painter->fillRect(QRectF(0, 0, mW, mH), bg);

    // 原版背景不是贴图，而是持续流动的颜色/纹理感。这里用 Qt 画出同样方向的代码背景：
    // 大面积渐变 + 斜向流动线 + 柔和粒子，包界面按类型换色。
    QColor acc = accent();
    painter->setPen(QPen(acc, 1.25));
    const qreal spacing = 54.0;
    const qreal drift = std::fmod(mTime * 46.0, spacing);
    for (qreal x = -mH; x < mW + mH; x += spacing) {
        QPainterPath path;
        path.moveTo(x + drift, mH + 20);
        path.lineTo(x + mH * 0.72 + drift, -20);
        painter->drawPath(path);
    }

    painter->setPen(Qt::NoPen);
    for (int i = 0; i < 38; ++i) {
        qreal seed = i * 17.37;
        qreal px = std::fmod(seed * 97.0 + mTime * (12.0 + (i % 5) * 4.0), mW + 120.0) - 60.0;
        qreal py = std::fmod(seed * 53.0 + std::sin(mTime * 0.6 + i) * 45.0, mH + 80.0) - 40.0;
        qreal r = 2.0 + (i % 7) * 0.9;
        QColor c = acc;
        c.setAlpha(26 + (i % 4) * 10);
        painter->setBrush(c);
        painter->drawEllipse(QPointF(px, py), r, r);
    }

    QRadialGradient glow(QPointF(mW * (0.55 + 0.08 * std::sin(mTime * 0.35)),
                                mH * (0.38 + 0.04 * std::cos(mTime * 0.41))),
                         qMax(mW, mH) * 0.55);
    QColor g = accent();
    g.setAlpha(54);
    glow.setColorAt(0, g);
    g.setAlpha(0);
    glow.setColorAt(1, g);
    painter->fillRect(QRectF(0, 0, mW, mH), glow);
}
