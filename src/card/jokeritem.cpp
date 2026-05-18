#include "jokeritem.h"
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QCursor>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>
#include <QGraphicsSceneHoverEvent>
#include <QLineF>
#include <QDateTime>
#include <QTimer>
#include <QPainterPath>
#include <QCoreApplication>
#include <QSet>
#include <QHash>
#include <QStringList>
#include <cmath>
#include <QtMath>
#include "../utils/shadereffects.h"

QPixmap *JokerItem::sSheet = nullptr;

static bool legendarySoulPos(JokerType t, QPoint &out);
static bool hologramSoulPos(JokerType t, QPoint &out);

namespace {
QSet<JokerItem*> sAnimatedJokers;
QTimer *sJokerShaderTimer = nullptr;

bool jokerNeedsShaderTick(const Joker &j)
{
    // 原版版本效果是 GPU shader，不应该因为闪箔/镭射/多彩/负片本身
    // 让每张小丑进入 CPU 定时刷新。这里只有真正会浮动的 soul 层需要 tick。
    QPoint unused;
    return legendarySoulPos(j.type, unused) || hologramSoulPos(j.type, unused);
}

void ensureJokerShaderTimer()
{
    if (sJokerShaderTimer) return;
    sJokerShaderTimer = new QTimer(QCoreApplication::instance());
    sJokerShaderTimer->setTimerType(Qt::CoarseTimer);
    QObject::connect(sJokerShaderTimer, &QTimer::timeout, []() {
        const auto items = sAnimatedJokers.values();
        for (JokerItem *item : items) if (item) item->update();
    });
    sJokerShaderTimer->start(67);
}

int shaderCacheFrame()
{
    return int(BalatroShaders::shaderTime() * 15.0);
}
}

static bool legendarySoulPos(JokerType t, QPoint &out)
{
    // 原版 game.lua：传奇小丑本体 pos 在第 8 行，前景 soul_pos 在第 9 行。
    // card.lua 会把 soul_pos 作为 floating_sprite 单独绘制，并持续做 dissolve shader 浮动。
    switch (t) {
    case JokerType::Caino:     out = {3, 9}; return true;
    case JokerType::Triboulet: out = {4, 9}; return true;
    case JokerType::Yorick:    out = {5, 9}; return true;
    case JokerType::Chicot:    out = {6, 9}; return true;
    case JokerType::Perkeo:    out = {7, 9}; return true;
    default: break;
    }
    return false;
}


static bool hologramSoulPos(JokerType t, QPoint &out)
{
    // 原版 game.lua: j_hologram soul_pos = {x=2, y=9}
    if (t == JokerType::Hologram) { out = {2, 9}; return true; }
    return false;
}

void JokerItem::drawFloatingSprite(QPainter *p, const QRectF &dst, JokerType type, bool animated)
{
    if (!p || !sSheet || sSheet->isNull()) return;

    QPoint soul;
    bool isLegendary = legendarySoulPos(type, soul);
    bool isHologram  = !isLegendary && hologramSoulPos(type, soul);
    if (!isLegendary && !isHologram) return;

    const qreal t = animated ? (QDateTime::currentMSecsSinceEpoch() / 1000.0) : 0.0;

    QRect src(soul.x() * JokerItem::SRC_W, soul.y() * JokerItem::SRC_H,
              JokerItem::SRC_W, JokerItem::SRC_H);

    if (isHologram) {
        const qreal scaleMod = 1.0 + 0.07 + 0.02 * std::sin(1.8 * t);
        const qreal rotateDeg = 0.05 * std::sin(1.219 * t) * 180.0 / 3.14159265358979323846;
        const qreal yBob = animated ? std::sin(t * 1.7) * 2.0 : 0.0;

        QPixmap sprite(JokerItem::SRC_W, JokerItem::SRC_H);
        sprite.fill(Qt::transparent);
        {
            QPainter sp(&sprite);
            sp.setRenderHint(QPainter::SmoothPixmapTransform, true);
            sp.drawPixmap(QRect(0, 0, JokerItem::SRC_W, JokerItem::SRC_H), *sSheet, src);
        }
        sprite = BalatroShaders::renderHologramPixmap(sprite, 1.2);

        p->save();
        p->setRenderHint(QPainter::SmoothPixmapTransform, true);
        p->translate(dst.center().x(), dst.center().y() + yBob);
        p->rotate(rotateDeg);
        p->scale(scaleMod, scaleMod);
        p->translate(-dst.width() / 2.0, -dst.height() / 2.0);
        p->drawPixmap(QRectF(0, 0, dst.width(), dst.height()), sprite, QRectF(0, 0, sprite.width(), sprite.height()));
        p->restore();
        return;
    }

    // 传奇小丑：scale_mod 较平缓，叠一层 dissolve 发光再画肖像。
    const qreal scaleMod = animated ? (1.07 + 0.02 * std::sin(1.8 * t)) : 1.0;
    const qreal rotateMod = animated ? (2.9 * std::sin(1.219 * t)) : 0.0;
    const qreal yBob = animated ? (2.6 * std::sin(1.65 * t)) : 0.0;

    auto drawLayer = [&](qreal scale, qreal opacity, QPainter::CompositionMode mode) {
        p->save();
        p->setRenderHint(QPainter::SmoothPixmapTransform, true);
        p->setCompositionMode(mode);
        p->setOpacity(opacity);
        p->translate(dst.center().x(), dst.center().y() + yBob);
        p->rotate(rotateMod);
        p->scale(scale, scale);
        p->translate(-dst.width() / 2.0, -dst.height() / 2.0);
        p->drawPixmap(QRectF(0, 0, dst.width(), dst.height()), *sSheet, src);
        p->restore();
    };

    drawLayer(scaleMod + 0.05, 0.40, QPainter::CompositionMode_Screen);
    drawLayer(scaleMod,        1.00, QPainter::CompositionMode_SourceOver);

}

static void paintHologramFloatingSprite(QPainter *p, QPixmap *sheet, JokerType type)
{
    Q_UNUSED(sheet);
    // 缓存绘制阶段使用原始图集尺寸；display 缩放在调用方完成。
    JokerItem::drawFloatingSprite(p, QRectF(0, 0, JokerItem::SRC_W, JokerItem::SRC_H), type, true);
}

static void paintLegendaryFloatingSprite(QPainter *p, QPixmap *sheet, JokerType type)
{
    Q_UNUSED(sheet);
    JokerItem::drawFloatingSprite(p, QRectF(0, 0, JokerItem::SRC_W, JokerItem::SRC_H), type, true);
}

void JokerItem::loadResources() {
    sSheet = new QPixmap(":/textures/images/Jokers.png");
    if (sSheet->isNull())
        qWarning("JokerItem: 加载 Jokers.png 失败");
}

// 坐标取自原版 game.lua 的 j_xxx pos = {x=?, y=?}
QPoint JokerItem::spritePos(JokerType t) {
    switch (t) {
    case JokerType::Joker:           return {0, 0};
    case JokerType::JollyJoker:      return {2, 0};
    case JokerType::ZanyJoker:       return {3, 0};
    case JokerType::MadJoker:        return {4, 0};
    case JokerType::CrazyJoker:      return {5, 0};
    case JokerType::DrollJoker:      return {6, 0};
    case JokerType::HalfJoker:       return {7, 0};
    case JokerType::GreedyJoker:     return {6, 1};
    case JokerType::LustyJoker:      return {7, 1};
    case JokerType::WrathfulJoker:   return {8, 1};
    case JokerType::GluttonousJoker: return {9, 1};
    case JokerType::GoldenJoker:     return {9, 2};
    case JokerType::ToDoList:        return {4, 11};
    case JokerType::SlyJoker:        return {0, 14};
    case JokerType::WilyJoker:       return {1, 14};
    case JokerType::CleverJoker:     return {2, 14};
    case JokerType::DeviousJoker:    return {3, 14};
    case JokerType::CraftyJoker:     return {4, 14};
    case JokerType::Banner:          return {1,  2};
    case JokerType::MysticSummit:    return {2,  2};
    case JokerType::Misprint:        return {6,  2};
    case JokerType::RaisedFist:      return {8,  2};
    case JokerType::Fibonacci:       return {1,  5};
    case JokerType::EvenSteven:      return {8,  3};
    case JokerType::OddTodd:         return {9,  3};
    case JokerType::Scholar:         return {0,  4};
    case JokerType::Bull:            return {7, 14};
    case JokerType::Bootstraps:      return {9,  8};
    case JokerType::AbstractJoker:   return {3,  3};
    case JokerType::Supernova:       return {4,  2};
    case JokerType::GrosMichel:      return {7,  6};
    case JokerType::Cavendish:       return {5, 11};
    case JokerType::IceCream:        return {4, 10};
    case JokerType::Stuntman:        return {8,  6};
    case JokerType::TheDuo:          return {5,  4};
    case JokerType::TheTrio:         return {6,  4};
    case JokerType::TheFamily:       return {7,  4};
    case JokerType::TheOrder:        return {8,  4};
    case JokerType::TheTribe:        return {9,  4};
    case JokerType::Blackboard:      return {2, 10};
    case JokerType::ScaryFace:       return {2,  3};
    case JokerType::SmileyFace:      return {6, 15};
    case JokerType::WalkieTalkie:    return {8, 15};
    case JokerType::Arrowhead:       return {1,  8};
    case JokerType::OnyxAgate:       return {2,  8};
    case JokerType::RoughGem:        return {9,  7};
    case JokerType::Bloodstone:      return {0,  8};
    case JokerType::ShootTheMoon:    return {2,  6};
    case JokerType::Baron:           return {6, 12};
    case JokerType::FlowerPot:       return {0,  6};
    case JokerType::Acrobat:         return {2,  1};
    case JokerType::Swashbuckler:    return {9,  5};
    case JokerType::Ramen:           return {2, 15};
    case JokerType::DriversLicense:  return {0,  7};
    case JokerType::Hiker:           return {0, 11};
    case JokerType::CardSharp:       return {6, 11};
    case JokerType::Hologram:        return {4, 12};
    case JokerType::MidasMask:       return {0, 13};
    case JokerType::Vampire:         return {2, 12};
    case JokerType::Constellation:   return {9, 10};
    case JokerType::Photograph:      return {2, 13};
    case JokerType::HangingChad:     return {9,  6};
    case JokerType::SockAndBuskin:   return {3,  1};
    case JokerType::Blueprint:       return {0,  3};
    case JokerType::Mime:            return {4,  1};
    case JokerType::DNA:             return {5, 10};
    case JokerType::Brainstorm:      return {7,  7};
    case JokerType::Caino:           return {3,  8};
    case JokerType::Triboulet:       return {4,  8};
    case JokerType::Yorick:          return {5,  8};
    case JokerType::Chicot:          return {6,  8};
    case JokerType::Perkeo:          return {7,  8};
    }
    return {0, 0};
}

JokerItem::JokerItem(const Joker &j, QGraphicsItem *parent)
    : QGraphicsObject(parent), mJoker(j)
{
    setAcceptHoverEvents(true);
    setCursor(Qt::PointingHandCursor);

    if (jokerNeedsShaderTick(mJoker)) {
        ensureJokerShaderTimer();
        sAnimatedJokers.insert(this);
        QObject::connect(this, &QObject::destroyed, [ptr = this]() { sAnimatedJokers.remove(ptr); });
    }
}

QRectF JokerItem::boundingRect() const {
    return QRectF(0, 0, WIDTH, HEIGHT);
}

void JokerItem::paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) {
    p->setRenderHint(QPainter::SmoothPixmapTransform, false);

    const bool floatingAnimated = jokerNeedsShaderTick(mJoker);
    const int frame = floatingAnimated ? shaderCacheFrame() : -1;
    const QString key = QString::number(int(mJoker.type)) + QLatin1Char('|')
                      + QString::number(int(mJoker.edition)) + QLatin1Char('|')
                      + QString::number(frame);
    static QHash<QString, QPixmap> cache;
    static QStringList order;

    QPixmap pix = cache.value(key);
    if (pix.isNull()) {
        // 缓存按图集原始 142×190 渲染，paint() 时再缩放到场景显示尺寸。
        pix = QPixmap(SRC_W, SRC_H);
        pix.fill(Qt::transparent);
        QPainter cp(&pix);
        cp.setRenderHint(QPainter::SmoothPixmapTransform, false);
        cp.setRenderHint(QPainter::Antialiasing, true);
        QPoint c = spritePos(mJoker.type);
        QRect src(c.x() * SRC_W, c.y() * SRC_H, SRC_W, SRC_H);
        QPixmap body = sSheet->copy(src);
        if (mJoker.edition != Edition::None)
            body = BalatroShaders::renderEditionPixmap(body, mJoker.edition);
        cp.drawPixmap(QRect(0, 0, SRC_W, SRC_H), body);
        paintLegendaryFloatingSprite(&cp, sSheet, mJoker.type);
        paintHologramFloatingSprite(&cp, sSheet, mJoker.type);
        cache.insert(key, pix);
        order.append(key);
        while (order.size() > 160) cache.remove(order.takeFirst());
    }
    p->setRenderHint(QPainter::SmoothPixmapTransform, true);
    p->drawPixmap(QRectF(0, 0, WIDTH, HEIGHT), pix, QRectF(0, 0, SRC_W, SRC_H));

    if (mHovered) {
        p->setPen(QPen(QColor(255, 240, 96, 200), 4));
        p->setBrush(Qt::NoBrush);
        p->drawRoundedRect(0, 0, WIDTH, HEIGHT, 8, 8);
    }
}

void JokerItem::mousePressEvent(QGraphicsSceneMouseEvent *e)
{
    if (e->button() == Qt::LeftButton || e->button() == Qt::RightButton) {
        mPressed = true;
        mDragging = false;
        mHoverTiltX = 0.0;
        mHoverTiltY = 0.0;
        applyHoverTransform();
        mPressScenePos = e->scenePos();
        mRestZ = zValue();
        e->accept();
        return;
    }
    QGraphicsObject::mousePressEvent(e);
}

void JokerItem::mouseMoveEvent(QGraphicsSceneMouseEvent *e)
{
    if (!mPressed) {
        QGraphicsObject::mouseMoveEvent(e);
        return;
    }
    if (!mDragging && QLineF(e->scenePos(), mPressScenePos).length() > 7.0) {
        mDragging = true;
        setZValue(650);
    }

    if (mDragging) {
        setPos(e->scenePos() - QPointF(WIDTH / 2.0, HEIGHT / 2.0));
        emit dragMoved(this, e->scenePos());
        e->accept();
        return;
    }
    QGraphicsObject::mouseMoveEvent(e);
}

void JokerItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *e)
{
    if ((e->button() == Qt::LeftButton || e->button() == Qt::RightButton) && mPressed) {
        mPressed = false;
        if (mDragging) {
            mDragging = false;
            emit dragReleased(this, e->scenePos());
        } else {
            emit pressed(this, e->button());
            if (e->button() == Qt::LeftButton) emit clicked(this);
        }
        e->accept();
        return;
    }
    QGraphicsObject::mouseReleaseEvent(e);
}

void JokerItem::applyHoverTransform()
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

void JokerItem::hoverEnterEvent(QGraphicsSceneHoverEvent *e)
{
    mHovered = true;
    setTransformOriginPoint(WIDTH / 2.0, HEIGHT / 2.0);
    animateScale(1.08, 100);
    emit hoverChanged(this, true);
    update();
    QGraphicsObject::hoverEnterEvent(e);
}

void JokerItem::hoverMoveEvent(QGraphicsSceneHoverEvent *e)
{
    if (!mHovered || mDragging) {
        QGraphicsObject::hoverMoveEvent(e);
        return;
    }
    const qreal nx = e->pos().x() / WIDTH - 0.5;
    const qreal ny = e->pos().y() / HEIGHT - 0.5;
    // 对齐原版 shader 顶点阶段的 hover 透视感：只是改变投影，不重绘像素贴图。
    mHoverTiltY = qBound(-10.0, nx * 20.0, 10.0);
    mHoverTiltX = qBound(-10.0, ny * 20.0, 10.0);
    applyHoverTransform();
    QGraphicsObject::hoverMoveEvent(e);
}

void JokerItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *e)
{
    mHovered = false;
    mHoverTiltX = 0.0;
    mHoverTiltY = 0.0;
    applyHoverTransform();
    animateScale(1.0, 110);
    emit hoverChanged(this, false);
    update();
    QGraphicsObject::hoverLeaveEvent(e);
}

void JokerItem::animateScale(qreal target, int durationMs)
{
    auto *anim = new QPropertyAnimation(this, "scale", this);
    anim->setDuration(durationMs);
    anim->setStartValue(scale());
    anim->setEndValue(target);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}


void JokerItem::moveTo(const QPointF &target, int durationMs)
{
    auto *anim = new QPropertyAnimation(this, "pos", this);
    anim->setDuration(durationMs);
    anim->setStartValue(pos());
    anim->setEndValue(target);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void JokerItem::juiceUp(double scaleAmount, int durationMs)
{
    setTransformOriginPoint(WIDTH / 2.0, HEIGHT / 2.0);

    auto *up = new QPropertyAnimation(this, "scale");
    up->setDuration(durationMs / 2);
    up->setStartValue(1.0);
    up->setEndValue(scaleAmount);

    auto *down = new QPropertyAnimation(this, "scale");
    down->setDuration(durationMs / 2);
    down->setStartValue(scaleAmount);
    down->setEndValue(1.0);

    auto *seq = new QSequentialAnimationGroup(this);
    seq->addAnimation(up);
    seq->addAnimation(down);
    up->setParent(seq); down->setParent(seq);
    seq->start(QAbstractAnimation::DeleteWhenStopped);
}
