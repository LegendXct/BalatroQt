#include "carditem.h"
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QPropertyAnimation>
#include <QCursor>
#include <QSequentialAnimationGroup>
#include <QLineF>
#include <QDateTime>
#include <QTimer>
#include <QPainterPath>
#include <cmath>

QPixmap *CardItem::sDeckSheet = nullptr;
QPixmap *CardItem::sEnhSheet = nullptr;
QPixmap *CardItem::sJokerSheet = nullptr;


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

    // 移动高光带：对应原版 shader 在卡面上持续流动的感觉。
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

void CardItem::loadResources() {
    sDeckSheet = new QPixmap(":/textures/images/8BitDeck.png");
    sEnhSheet = new QPixmap(":/textures/images/Enhancers.png");
    sJokerSheet = new QPixmap(":/textures/images/Jokers.png");

    if (sDeckSheet->isNull())
        qWarning("CardItem: Fail to Load 8BitDeck.png");
    if (sEnhSheet->isNull())
        qWarning("CardItem: Fail to Load Enhancers.png");
    if (sJokerSheet->isNull())
        qWarning("CardItem: Fail to Load Jokers.png");
}

CardItem::CardItem(const CardData &data, QGraphicsItem *parent)
    : QGraphicsObject(parent)
    , mData(data)
{
    setAcceptHoverEvents(true);
    setCursor(Qt::PointingHandCursor);

    auto *shineTimer = new QTimer(this);
    connect(shineTimer, &QTimer::timeout, this, [this]() {
        if (mData.edition != Edition::None) update();
    });
    shineTimer->start(80);
}

QRectF CardItem::boundingRect() const {
    return QRectF(0, 0, WIDTH, HEIGHT);
}

void CardItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    if (mData.faceUp) paintFront(painter);
    else paintBack(painter);
}

QRect CardItem::whiteBaseSrcRect() const {
    return QRect(1 * WIDTH, 0 * HEIGHT, WIDTH, HEIGHT);
}

QRect CardItem::deckSrcRect() const {
    int col = static_cast<int>(mData.rank) - 2;
    int row = 0;
    switch (mData.suit) {
    case Suit::Hearts: row = 0; break;
    case Suit::Clubs: row = 1; break;
    case Suit::Diamonds: row = 2; break;
    case Suit::Spades: row = 3; break;
    }
    return QRect(col * WIDTH, row * HEIGHT, WIDTH, HEIGHT);
}

QRect CardItem::enhanceSrcRect() const {
    switch (mData.enhancement) {
    case Enhancement::Bonus: return QRect(1 * WIDTH, 1 * HEIGHT, WIDTH, HEIGHT);
    case Enhancement::Mult: return QRect(2 * WIDTH, 1 * HEIGHT, WIDTH, HEIGHT);
    case Enhancement::Wild: return QRect(3 * WIDTH, 1 * HEIGHT, WIDTH, HEIGHT);
    case Enhancement::Lucky: return QRect(4 * WIDTH, 1 * HEIGHT, WIDTH, HEIGHT);
    case Enhancement::Glass: return QRect(5 * WIDTH, 1 * HEIGHT, WIDTH, HEIGHT);
    case Enhancement::Steel: return QRect(6 * WIDTH, 1 * HEIGHT, WIDTH, HEIGHT);
    case Enhancement::Stone: return QRect(5 * WIDTH, 0 * HEIGHT, WIDTH, HEIGHT);
    case Enhancement::Gold: return QRect(6 * WIDTH, 0 * HEIGHT, WIDTH, HEIGHT);
    default: return whiteBaseSrcRect();
    }
}

QRect CardItem::sealSrcRect() const {
    switch (mData.seal) {
    case Seal::Gold: return QRect(2 * WIDTH, 0 * HEIGHT, WIDTH, HEIGHT);
    case Seal::Purple: return QRect(4 * WIDTH, 4 * HEIGHT, WIDTH, HEIGHT);
    case Seal::Red: return QRect(5 * WIDTH, 4 * HEIGHT, WIDTH, HEIGHT);
    case Seal::Blue: return QRect(6 * WIDTH, 4 * HEIGHT, WIDTH, HEIGHT);
    default: return QRect();
    }
}

void CardItem::paintFront(QPainter *painter)
{
    QRect dst(0, 0, WIDTH, HEIGHT);

    QRect enh = enhanceSrcRect();
    if (!enh.isNull()) painter->drawPixmap(dst, *sEnhSheet, enh);

    if (mData.enhancement != Enhancement::Stone)
        painter->drawPixmap(dst, *sDeckSheet, deckSrcRect());

    QRect seal = sealSrcRect();
    if (!seal.isNull()) painter->drawPixmap(dst, *sEnhSheet, seal);

    paintEditionShine(painter, QRectF(0, 0, WIDTH, HEIGHT), mData.edition);

    if (mData.isDebuffed) {
        painter->fillRect(0, 0, WIDTH, HEIGHT, QColor(0, 0, 0, 130));
        painter->setPen(QPen(QColor(255, 80, 80), 3));
        painter->drawLine(8, 8, WIDTH - 8, HEIGHT - 8);
        painter->drawLine(WIDTH - 8, 8, 8, HEIGHT - 8);
    }
}

void CardItem::paintBack(QPainter *painter) {
    QRect dst(0, 0, WIDTH, HEIGHT);

    QRect backSrc(0 * WIDTH, 0 * HEIGHT, WIDTH, HEIGHT);
    painter->drawPixmap(dst, *sEnhSheet, backSrc);
}

void CardItem::setCardData(const CardData &data) {
    mData = data;
    update();
}

void CardItem::setCardSelected(bool selected) {
    mSelected = selected;
    update();
}

void CardItem::moveTo(const QPointF &target, int durationMs) {
    auto *anim = new QPropertyAnimation(this, "pos", this);
    anim->setDuration(durationMs);
    anim->setStartValue(pos());
    anim->setEndValue(target);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void CardItem::flip() {
    auto *shrink = new QPropertyAnimation(this, "scale", this);
    shrink->setDuration(120);
    shrink->setStartValue(1.0);
    shrink->setEndValue(0.0);
    shrink->setEasingCurve(QEasingCurve::InQuad);

    connect(shrink, &QPropertyAnimation::finished, this, [this]() {
        mData.faceUp = !mData.faceUp;
        update();
        auto *expand = new QPropertyAnimation(this, "scale", this);
        expand->setDuration(120);
        expand->setStartValue(0.0);
        expand->setEndValue(1.0);
        expand->setEasingCurve(QEasingCurve::OutQuad);
        expand->start(QAbstractAnimation::DeleteWhenStopped);
    });

    shrink->start(QAbstractAnimation::DeleteWhenStopped);
}

void CardItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        mPressed = true;
        mDragging = false;
        mPressScenePos = event->scenePos();
        setZValue(100);
        event->accept();
    }
    else QGraphicsObject::mousePressEvent(event);
}

void CardItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (!mPressed) {
        QGraphicsObject::mouseMoveEvent(event);
        return;
    }
    if (!mDragging && QLineF(event->scenePos(), mPressScenePos).length() > 8.0)
        mDragging = true;

    if (mDragging) {
        setPos(event->scenePos() - QPointF(WIDTH / 2.0, HEIGHT / 2.0));
        emit dragMoved(this, event->scenePos());
        event->accept();
        return;
    }
    QGraphicsObject::mouseMoveEvent(event);
}

void CardItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && mPressed) {
        mPressed = false;
        if (mDragging) {
            mDragging = false;
            emit dragReleased(this, event->scenePos());
        } else {
            emit clicked(this);
        }
        event->accept();
        return;
    }
    QGraphicsObject::mouseReleaseEvent(event);
}

void CardItem::juiceUp(double scaleAmount, int durationMs)
{
    // 设变换原点为中心,避免缩放时位置漂移
    setTransformOriginPoint(WIDTH / 2.0, HEIGHT / 2.0);

    auto *up = new QPropertyAnimation(this, "scale");
    up->setDuration(durationMs / 2);
    up->setStartValue(1.0);
    up->setEndValue(scaleAmount);
    up->setEasingCurve(QEasingCurve::OutQuad);

    auto *down = new QPropertyAnimation(this, "scale");
    down->setDuration(durationMs / 2);
    down->setStartValue(scaleAmount);
    down->setEndValue(1.0);
    down->setEasingCurve(QEasingCurve::InQuad);

    auto *seq = new QSequentialAnimationGroup(this);
    seq->addAnimation(up);
    seq->addAnimation(down);
    up->setParent(seq);
    down->setParent(seq);
    seq->start(QAbstractAnimation::DeleteWhenStopped);
}

void CardItem::applyTransform()
{
    // 中心点
    qreal cx = WIDTH  / 2.0;
    qreal cy = HEIGHT / 2.0;

    // 透视参数:鼠标越往边缘,倾斜越大,深度感越明显
    qreal tiltX = qDegreesToRadians(mHoverTiltX);
    qreal tiltY = qDegreesToRadians(mHoverTiltY);
    qreal zRot  = qDegreesToRadians(mBaseRotation);

    // 步骤:平移到中心 → 绕 Y 倾斜 → 绕 X 倾斜 → 绕 Z 扇形 → 平移回去
    QTransform t;
    t.translate(cx, cy);

    // 模拟绕 Y 轴旋转:左右两边远近不同 → 水平缩放 + 透视投影
    // 简化:用 m11/m13 实现(perspective)
    // y轴倾斜 = 水平倾斜效果(左右翻),用 shear + scale 近似
    qreal cosY = std::cos(tiltY);
    qreal cosX = std::cos(tiltX);
    qreal sinY = std::sin(tiltY);
    qreal sinX = std::sin(tiltX);

    QTransform persp;
    persp.setMatrix(
        cosY,           sinY * sinX,    0.005 * sinY,
        0,              cosX,           0.005 * sinX,
        0,              0,              1
        );
    t = persp * t;

    // 应用 Z 轴扇形旋转
    t.rotateRadians(zRot);

    t.translate(-cx, -cy);
    setTransform(t);
}

void CardItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
    mHovered = true;
    emit hoverChanged(this, true);
    QGraphicsObject::hoverEnterEvent(event);
}

void CardItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    if (!mHovered) return;
    qreal lx = event->pos().x();
    qreal ly = event->pos().y();
    qreal nx = (lx / WIDTH)  - 0.5;     // [-0.5, 0.5]
    qreal ny = (ly / HEIGHT) - 0.5;
    // 鼠标在右半 → 卡片向右后倾(Y 轴倾斜)
    mHoverTiltY = nx * 20.0;            // 最大 ±10°
    // 鼠标在上半 → 卡片向上后倾(X 轴倾斜),负号让方向自然
    mHoverTiltX = ny * 20.0;
    applyTransform();
    QGraphicsObject::hoverMoveEvent(event);
}

void CardItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
    mHovered = false;
    emit hoverChanged(this, false);
    mHoverTiltX = 0;
    mHoverTiltY = 0;
    applyTransform();
    QGraphicsObject::hoverLeaveEvent(event);
}
