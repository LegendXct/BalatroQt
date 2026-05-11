#include "carditem.h"
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QPropertyAnimation>
#include <QCursor>
#include <QSequentialAnimationGroup>

QPixmap *CardItem::sDeckSheet = nullptr;
QPixmap *CardItem::sEnhSheet = nullptr;
QPixmap *CardItem::sJokerSheet = nullptr;

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

    // ── edition 视觉 ──
    painter->setBrush(Qt::NoBrush);
    switch (mData.edition) {
    case Edition::Foil:
        painter->setPen(QPen(QColor(120, 200, 255, 200), 4));
        painter->drawRoundedRect(2, 2, WIDTH - 4, HEIGHT - 4, 8, 8);
        break;
    case Edition::Holographic:
        painter->setPen(QPen(QColor(255, 100, 200, 200), 4));
        painter->drawRoundedRect(2, 2, WIDTH - 4, HEIGHT - 4, 8, 8);
        break;
    case Edition::Polychrome: {
        QLinearGradient g(0, 0, WIDTH, HEIGHT);
        g.setColorAt(0,    QColor(255, 100, 100, 220));
        g.setColorAt(0.5,  QColor(100, 255, 100, 220));
        g.setColorAt(1,    QColor(100, 100, 255, 220));
        painter->setPen(QPen(QBrush(g), 4));
        painter->drawRoundedRect(2, 2, WIDTH - 4, HEIGHT - 4, 8, 8);
        break;
    }
    case Edition::Negative:
        painter->fillRect(0, 0, WIDTH, HEIGHT, QColor(40, 0, 60, 120));
        painter->setPen(QPen(QColor(180, 100, 255, 200), 4));
        painter->drawRoundedRect(2, 2, WIDTH - 4, HEIGHT - 4, 8, 8);
        break;
    default: break;
    }

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
        emit clicked(this);
        event->accept();
    }
    else QGraphicsObject::mousePressEvent(event);
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
    mHoverTiltX = 0;
    mHoverTiltY = 0;
    applyTransform();
    QGraphicsObject::hoverLeaveEvent(event);
}
