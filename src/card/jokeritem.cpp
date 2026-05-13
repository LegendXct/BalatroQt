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
#include <cmath>

QPixmap *JokerItem::sSheet = nullptr;

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

static void paintHologramFloatingSprite(QPainter *p, QPixmap *sheet, JokerType type)
{
    QPoint soul;
    if (!sheet || sheet->isNull() || !hologramSoulPos(type, soul)) return;

    const qreal t = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    const qreal scaleMod = 1.0 + 2.0 * (0.07 + 0.02 * std::sin(1.8 * t));
    const qreal rotateDeg = (2.0 * (0.05 * std::sin(1.219 * t))) * 180.0 / 3.14159265358979323846;
    const qreal yBob = std::sin(t * 1.7) * 2.0;
    QRect src(soul.x() * JokerItem::WIDTH, soul.y() * JokerItem::HEIGHT,
              JokerItem::WIDTH, JokerItem::HEIGHT);

    auto draw = [&](qreal extraScale, qreal opacity, QPainter::CompositionMode mode, qreal dx) {
        p->save();
        p->setRenderHint(QPainter::SmoothPixmapTransform, true);
        p->setCompositionMode(mode);
        p->setOpacity(opacity);
        p->translate(JokerItem::WIDTH / 2.0 + dx, JokerItem::HEIGHT / 2.0 + yBob);
        p->rotate(rotateDeg);
        p->scale(scaleMod + extraScale, scaleMod + extraScale);
        p->translate(-JokerItem::WIDTH / 2.0, -JokerItem::HEIGHT / 2.0);
        p->drawPixmap(QRect(0, 0, JokerItem::WIDTH, JokerItem::HEIGHT), *sheet, src);
        p->restore();
    };

    // 近似原版 draw_shader('hologram')：多层偏移、屏幕混合、蓝紫流光。
    draw(0.08, 0.25, QPainter::CompositionMode_Screen, -2.0);
    draw(0.05, 0.22, QPainter::CompositionMode_Screen,  2.0);
    draw(0.00, 0.92, QPainter::CompositionMode_SourceOver, 0.0);

    p->save();
    p->setCompositionMode(QPainter::CompositionMode_Screen);
    QLinearGradient holo(0, 0, JokerItem::WIDTH, JokerItem::HEIGHT);
    const qreal ph = std::fmod(t * 0.28, 1.0);
    holo.setColorAt(0.00, QColor(80, 220, 255, 55));
    holo.setColorAt(qBound(0.0, ph, 1.0), QColor(255, 255, 255, 100));
    holo.setColorAt(1.00, QColor(255, 90, 220, 55));
    p->fillRect(QRectF(0, 0, JokerItem::WIDTH, JokerItem::HEIGHT), holo);
    p->restore();
}

static void paintEditionShine(QPainter *p, const QRectF &r, Edition e)
{
    if (e == Edition::None) return;
    const qreal phase = std::fmod(QDateTime::currentMSecsSinceEpoch() / 900.0, 1.0);

    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    QPainterPath clip;
    clip.addRoundedRect(r.adjusted(2, 2, -2, -2), 9, 9);
    p->setClipPath(clip);

    if (e == Edition::Negative) {
        p->setCompositionMode(QPainter::CompositionMode_Multiply);
        p->fillRect(r, QColor(45, 0, 80, 155));
        p->setCompositionMode(QPainter::CompositionMode_Screen);
        QLinearGradient neg(r.topLeft(), r.bottomRight());
        neg.setColorAt(0.0, QColor(40, 0, 80, 120));
        neg.setColorAt(0.45, QColor(180, 95, 255, 105));
        neg.setColorAt(1.0, QColor(15, 10, 25, 150));
        p->fillRect(r, neg);
    } else {
        QLinearGradient g(r.topLeft(), r.bottomRight());
        if (e == Edition::Foil) {
            g.setColorAt(0.00, QColor(180, 235, 255, 70));
            g.setColorAt(0.50, QColor(255, 255, 255, 112));
            g.setColorAt(1.00, QColor(105, 170, 255, 58));
        } else if (e == Edition::Holographic) {
            g.setColorAt(0.00, QColor(255, 80, 185, 70));
            g.setColorAt(0.50, QColor(115, 245, 255, 90));
            g.setColorAt(1.00, QColor(255, 245, 120, 58));
        } else if (e == Edition::Polychrome) {
            g.setColorAt(0.00, QColor(255, 55, 70, 92));
            g.setColorAt(0.25, QColor(255, 230, 70, 88));
            g.setColorAt(0.50, QColor(70, 255, 150, 92));
            g.setColorAt(0.75, QColor(80, 160, 255, 88));
            g.setColorAt(1.00, QColor(210, 80, 255, 92));
        }
        p->setCompositionMode(QPainter::CompositionMode_Screen);
        p->fillRect(r, g);
    }

    p->setCompositionMode(QPainter::CompositionMode_Screen);
    qreal stripeX = r.left() - r.width() * 0.65 + phase * r.width() * 2.2;
    QLinearGradient stripe(stripeX, r.top(), stripeX + r.width() * 0.42, r.bottom());
    stripe.setColorAt(0.00, QColor(255, 255, 255, 0));
    stripe.setColorAt(0.48, QColor(255, 255, 255, e == Edition::Negative ? 70 : 120));
    stripe.setColorAt(1.00, QColor(255, 255, 255, 0));
    p->fillRect(r, stripe);

    p->setClipping(false);
    QColor border;
    switch (e) {
    case Edition::Foil:        border = QColor(170, 230, 255, 225); break;
    case Edition::Holographic: border = QColor(255, 130, 215, 225); break;
    case Edition::Polychrome:  border = QColor(235, 245, 120, 235); break;
    case Edition::Negative:    border = QColor(190, 100, 255, 235); break;
    default: break;
    }
    p->setCompositionMode(QPainter::CompositionMode_SourceOver);
    p->setBrush(Qt::NoBrush);
    p->setPen(QPen(border, e == Edition::Negative ? 5 : 4));
    p->drawRoundedRect(r.adjusted(2, 2, -2, -2), 9, 9);
    p->restore();
}

static void paintLegendaryFloatingSprite(QPainter *p, QPixmap *sheet, JokerType type)
{
    QPoint soul;
    if (!sheet || sheet->isNull() || !legendarySoulPos(type, soul)) return;

    const qreal t = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    const qreal scaleMod = 1.07 + 0.02 * std::sin(1.8 * t);
    const qreal rotateMod = 2.9 * std::sin(1.219 * t);
    const qreal yBob = 2.6 * std::sin(1.65 * t);
    QRect src(soul.x() * JokerItem::WIDTH, soul.y() * JokerItem::HEIGHT,
              JokerItem::WIDTH, JokerItem::HEIGHT);

    auto drawLayer = [&](qreal scale, qreal opacity, qreal dx, qreal dy, QPainter::CompositionMode mode) {
        p->save();
        p->setRenderHint(QPainter::SmoothPixmapTransform, true);
        p->setCompositionMode(mode);
        p->setOpacity(opacity);
        p->translate(JokerItem::WIDTH / 2.0 + dx, JokerItem::HEIGHT / 2.0 + dy + yBob);
        p->rotate(rotateMod);
        p->scale(scale, scale);
        p->translate(-JokerItem::WIDTH / 2.0, -JokerItem::HEIGHT / 2.0);
        p->drawPixmap(QRect(0, 0, JokerItem::WIDTH, JokerItem::HEIGHT), *sheet, src);
        p->restore();
    };

    // 近似原版 card.lua: floating_sprite 先用 dissolve shader 画一层发光，再画主体。
    drawLayer(scaleMod + 0.05, 0.40, 0, 0, QPainter::CompositionMode_Screen);
    drawLayer(scaleMod,        1.00, 0, 0, QPainter::CompositionMode_SourceOver);

    // 额外加一层周期性高光，让“人头像浮在背景前”的感觉更明显。
    p->save();
    p->setCompositionMode(QPainter::CompositionMode_Screen);
    p->setOpacity(0.28 + 0.12 * std::sin(t * 2.4));
    QRadialGradient g(QPointF(JokerItem::WIDTH * 0.50, JokerItem::HEIGHT * 0.43 + yBob), JokerItem::WIDTH * 0.48);
    g.setColorAt(0.0, QColor(255, 255, 255, 115));
    g.setColorAt(0.45, QColor(200, 160, 255, 50));
    g.setColorAt(1.0, QColor(255, 255, 255, 0));
    p->fillRect(QRectF(0, 0, JokerItem::WIDTH, JokerItem::HEIGHT), g);
    p->restore();
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

    auto *shineTimer = new QTimer(this);
    connect(shineTimer, &QTimer::timeout, this, [this]() {
        QPoint unused;
        if (mJoker.edition != Edition::None || legendarySoulPos(mJoker.type, unused) || hologramSoulPos(mJoker.type, unused)) update();
    });
    shineTimer->start(80);
}

QRectF JokerItem::boundingRect() const {
    return QRectF(0, 0, WIDTH, HEIGHT);
}

void JokerItem::paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) {
    p->setRenderHint(QPainter::SmoothPixmapTransform);

    QPoint c = spritePos(mJoker.type);
    QRect src(c.x() * WIDTH, c.y() * HEIGHT, WIDTH, HEIGHT);
    p->drawPixmap(QRect(0, 0, WIDTH, HEIGHT), *sSheet, src);

    paintEditionShine(p, QRectF(0, 0, WIDTH, HEIGHT), mJoker.edition);
    paintLegendaryFloatingSprite(p, sSheet, mJoker.type);
    paintHologramFloatingSprite(p, sSheet, mJoker.type);

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
        mPressScenePos = e->scenePos();
        mRestZ = zValue();
        setZValue(500);
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
    if (!mDragging && QLineF(e->scenePos(), mPressScenePos).length() > 7.0)
        mDragging = true;

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
            setZValue(mRestZ);
        }
        e->accept();
        return;
    }
    QGraphicsObject::mouseReleaseEvent(e);
}

void JokerItem::hoverEnterEvent(QGraphicsSceneHoverEvent *e)
{
    mHovered = true;
    setTransformOriginPoint(WIDTH / 2.0, HEIGHT / 2.0);
    animateScale(1.10, 120);
    emit hoverChanged(this, true);
    update();
    QGraphicsObject::hoverEnterEvent(e);
}

void JokerItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *e)
{
    mHovered = false;
    animateScale(1.0, 120);
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
