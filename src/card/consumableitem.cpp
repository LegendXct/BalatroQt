#include "consumableitem.h"
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QtMath>
#include <QPropertyAnimation>
#include <QGraphicsSceneHoverEvent>
#include "../utils/shadereffects.h"
#include <QCursor>
#include <QTimer>
#include <QDateTime>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QCoreApplication>
#include <QSet>
#include <QHash>
#include <QStringList>
#include <cmath>

QPixmap *ConsumableItem::sSheet = nullptr;

namespace {
QSet<ConsumableItem*> sAnimatedConsumables;
QTimer *sConsumableShaderTimer = nullptr;

bool consumableNeedsShaderTick(const Consumable &c)
{
    return c.type == ConsumableType::Spectral_Soul;
}

void ensureConsumableShaderTimer()
{
    if (sConsumableShaderTimer) return;
    sConsumableShaderTimer = new QTimer(QCoreApplication::instance());
    sConsumableShaderTimer->setTimerType(Qt::CoarseTimer);
    QObject::connect(sConsumableShaderTimer, &QTimer::timeout, []() {
        const auto items = sAnimatedConsumables.values();
        for (ConsumableItem *item : items) if (item) item->update();
    });
    sConsumableShaderTimer->start(67);
}
}

void ConsumableItem::loadResources() {
    sSheet = new QPixmap(":/textures/images/Tarots.png");
    if (sSheet->isNull()) qWarning("ConsumableItem: 加载 Tarots.png 失败");
}

// 坐标取自原版 game.lua c_xxx pos
QPoint ConsumableItem::spritePos(ConsumableType t) {
    switch (t) {
    // 塔罗（原版 game.lua c_xxx pos）
    case ConsumableType::Tarot_Fool:          return {0, 0};
    case ConsumableType::Tarot_Magician:      return {1, 0};
    case ConsumableType::Tarot_HighPriestess: return {2, 0};
    case ConsumableType::Tarot_Empress:       return {3, 0};
    case ConsumableType::Tarot_Emperor:       return {4, 0};
    case ConsumableType::Tarot_Hierophant:    return {5, 0};
    case ConsumableType::Tarot_Lovers:        return {6, 0};
    case ConsumableType::Tarot_Chariot:       return {7, 0};
    case ConsumableType::Tarot_Justice:       return {8, 0};
    case ConsumableType::Tarot_Hermit:        return {9, 0};
    case ConsumableType::Tarot_Wheel:         return {0, 1};
    case ConsumableType::Tarot_Strength:      return {1, 1};
    case ConsumableType::Tarot_HangedMan:     return {2, 1};
    case ConsumableType::Tarot_Death:         return {3, 1};
    case ConsumableType::Tarot_Temperance:    return {4, 1};
    case ConsumableType::Tarot_Devil:         return {5, 1};
    case ConsumableType::Tarot_Tower:         return {6, 1};
    case ConsumableType::Tarot_Star:          return {7, 1};
    case ConsumableType::Tarot_Moon:          return {8, 1};
    case ConsumableType::Tarot_Sun:           return {9, 1};
    case ConsumableType::Tarot_Judgement:     return {0, 2};
    case ConsumableType::Tarot_World:         return {1, 2};

    // 行星
    case ConsumableType::Planet_Mercury:   return {0, 3};
    case ConsumableType::Planet_Venus:     return {1, 3};
    case ConsumableType::Planet_Earth:     return {2, 3};
    case ConsumableType::Planet_Mars:      return {3, 3};
    case ConsumableType::Planet_Jupiter:   return {4, 3};
    case ConsumableType::Planet_Saturn:    return {5, 3};
    case ConsumableType::Planet_Uranus:    return {6, 3};
    case ConsumableType::Planet_Neptune:   return {7, 3};
    case ConsumableType::Planet_Pluto:     return {8, 3};
    case ConsumableType::Planet_PlanetX:   return {9, 2};
    case ConsumableType::Planet_Ceres:     return {8, 2};
    case ConsumableType::Planet_Eris:      return {3, 2};

    // 幻灵（原版 Tarots.png 第 4、5 行；The Soul 本体背景在 {2,2}）
    case ConsumableType::Spectral_Familiar: return {0, 4};
    case ConsumableType::Spectral_Grim:     return {1, 4};
    case ConsumableType::Spectral_Incantation:return {2, 4};
    case ConsumableType::Spectral_Talisman: return {3, 4};
    case ConsumableType::Spectral_Aura:     return {4, 4};
    case ConsumableType::Spectral_Wraith:   return {5, 4};
    case ConsumableType::Spectral_Sigil:    return {6, 4};
    case ConsumableType::Spectral_Ouija:    return {7, 4};
    case ConsumableType::Spectral_Ectoplasm:return {8, 4};
    case ConsumableType::Spectral_Immolate: return {9, 4};
    case ConsumableType::Spectral_Ankh:     return {0, 5};
    case ConsumableType::Spectral_DejaVu:   return {1, 5};
    case ConsumableType::Spectral_Hex:      return {2, 5};
    case ConsumableType::Spectral_Trance:   return {3, 5};
    case ConsumableType::Spectral_Medium:   return {4, 5};
    case ConsumableType::Spectral_Cryptid:  return {5, 5};
    case ConsumableType::Spectral_Soul:     return {2, 2};
    case ConsumableType::Spectral_BlackHole:return {9, 3};
    }
    return {0, 0};
}

QPixmap ConsumableItem::renderPixmap(ConsumableType type, bool negative)
{
    if (!sSheet || sSheet->isNull()) {
        loadResources();
    }

    const bool animated = (type == ConsumableType::Spectral_Soul) || negative;
    const int frame = animated ? int(BalatroShaders::shaderTime() * 15.0) : -1;
    const QString key = QString::number(int(type)) + QLatin1Char('|')
                      + QString::number(negative ? 1 : 0) + QLatin1Char('|')
                      + QString::number(frame);

    static QHash<QString, QPixmap> cache;
    static QStringList order;
    QPixmap pix = cache.value(key);
    if (!pix.isNull()) return pix;

    pix = QPixmap(WIDTH, HEIGHT);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.setRenderHint(QPainter::Antialiasing, true);

    if (sSheet && !sSheet->isNull()) {
        QPoint c = spritePos(type);
        QRect src(c.x() * WIDTH, c.y() * HEIGHT, WIDTH, HEIGHT);
        p.drawPixmap(QRect(0, 0, WIDTH, HEIGHT), *sSheet, src);
    }

    p.end();

    if (negative) {
        pix = BalatroShaders::renderEditionPixmap(pix, Edition::Negative);
    }

    // 原版 The Soul 不是单张平面图：背景牌面之外，额外绘制 G.shared_soul。
    // 这里放在 negative 之后，保证你要的白水晶不会被反相污染。
    if (type == ConsumableType::Spectral_Soul) {
        QPainter soulPainter(&pix);
        soulPainter.setRenderHint(QPainter::SmoothPixmapTransform, false);
        soulPainter.setRenderHint(QPainter::Antialiasing, true);
        static QPixmap enhSheet(QStringLiteral(":/textures/images/Enhancers.png"));
        BalatroShaders::paintSoulCrystal(&soulPainter, QRectF(0, 0, WIDTH, HEIGHT), enhSheet);
    }

    cache.insert(key, pix);
    order.append(key);
    while (order.size() > 96) cache.remove(order.takeFirst());
    return pix;
}

ConsumableItem::ConsumableItem(const Consumable &c, QGraphicsItem *parent)
    : QGraphicsObject(parent), mC(c)
{
    setAcceptHoverEvents(true);
    setCursor(Qt::PointingHandCursor);
    setToolTip(QString("%1\n%2\n左键: 使用    右键: 卖出 (+$%3)")
                   .arg(mC.name, mC.description).arg(mC.sellValue));

    if (consumableNeedsShaderTick(mC)) {
        ensureConsumableShaderTimer();
        sAnimatedConsumables.insert(this);
        QObject::connect(this, &QObject::destroyed, [ptr = this]() { sAnimatedConsumables.remove(ptr); });
    }
}

void ConsumableItem::paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) {
    p->setRenderHint(QPainter::SmoothPixmapTransform, false);
    p->drawPixmap(QRect(0, 0, WIDTH, HEIGHT), renderPixmap(mC.type, mC.negative));

}

void ConsumableItem::mousePressEvent(QGraphicsSceneMouseEvent *e) {
    emit clicked(this, e->button());
    e->accept();
}

void ConsumableItem::applyHoverTransform()
{
    const qreal cx = WIDTH / 2.0;
    const qreal cy = HEIGHT / 2.0;
    const qreal tiltX = qDegreesToRadians(mHoverTiltX);
    const qreal tiltY = qDegreesToRadians(mHoverTiltY);

    const qreal cosY = std::cos(tiltY);
    const qreal cosX = std::cos(tiltX);
    const qreal sinY = std::sin(tiltY);
    const qreal sinX = std::sin(tiltX);

    QTransform t;
    t.translate(cx, cy);
    QTransform persp;
    persp.setMatrix(
        cosY,           sinY * sinX,    0.0048 * sinY,
        0,              cosX,           0.0048 * sinX,
        0,              0,              1
    );
    t = persp * t;
    t.translate(-cx, -cy);
    setTransform(t);
}

void ConsumableItem::hoverEnterEvent(QGraphicsSceneHoverEvent *e)
{
    mHovered = true;
    setTransformOriginPoint(WIDTH / 2.0, HEIGHT / 2.0);
    setZValue(qMax<qreal>(zValue(), 120));
    animateScale(1.08, 100);
    update();
    QGraphicsObject::hoverEnterEvent(e);
}

void ConsumableItem::hoverMoveEvent(QGraphicsSceneHoverEvent *e)
{
    if (!mHovered) {
        QGraphicsObject::hoverMoveEvent(e);
        return;
    }
    const qreal nx = e->pos().x() / WIDTH - 0.5;
    const qreal ny = e->pos().y() / HEIGHT - 0.5;
    mHoverTiltY = qBound(-10.0, nx * 20.0, 10.0);
    mHoverTiltX = qBound(-10.0, ny * 20.0, 10.0);
    applyHoverTransform();
    QGraphicsObject::hoverMoveEvent(e);
}

void ConsumableItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *e)
{
    mHovered = false;
    mHoverTiltX = 0.0;
    mHoverTiltY = 0.0;
    applyHoverTransform();
    animateScale(1.0, 110);
    update();
    QGraphicsObject::hoverLeaveEvent(e);
}

void ConsumableItem::animateScale(qreal target, int durationMs)
{
    auto *anim = new QPropertyAnimation(this, "scale", this);
    anim->setDuration(durationMs);
    anim->setStartValue(scale());
    anim->setEndValue(target);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}
