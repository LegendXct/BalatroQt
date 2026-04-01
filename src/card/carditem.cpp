#include "carditem.h"
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QPropertyAnimation>
#include <QCursor>

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
    default: return QRect();
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

void CardItem::paintFront(QPainter *painter) {
    QRect dst(0, 0, WIDTH, HEIGHT);

    painter->drawPixmap(dst, *sEnhSheet, whiteBaseSrcRect());
    painter->drawPixmap(dst, *sDeckSheet, deckSrcRect());
    paintOverlays(painter);
    paintHover(painter);
    paintSelect(painter);
}

void CardItem::paintBack(QPainter *painter) {
    QRect dst(0, 0, WIDTH, HEIGHT);

    QRect backSrc(0 * WIDTH, 0 * HEIGHT, WIDTH, HEIGHT);
    painter->drawPixmap(dst, *sEnhSheet, backSrc);
    paintSelect(painter);
}

void CardItem::paintOverlays(QPainter *painter) {
    QRect dst(0, 0, WIDTH, HEIGHT);
    QRect seal = sealSrcRect();
    if (!seal.isNull()) painter->drawPixmap(dst, *sEnhSheet, seal);
    QRect enh = enhanceSrcRect();
    if (!enh.isNull()) painter->drawPixmap(dst, *sEnhSheet, enh);
}

void CardItem::paintHover(QPainter *painter) {
    if (!mHovered) return;
}

void CardItem::paintSelect(QPainter *painter) {
    if (!mSelected) return;
}

void CardItem::setCardData(const CardData &data) {
    mData = data;
    update();
}

void CardItem::setCardSelected(bool selected) {
    mSelected = selected;
    setY(selected ? -30 : 0);
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
    if (event->button() == Qt::LeftButton)
        emit clicked(this);
    QGraphicsObject::mousePressEvent(event);
}

void CardItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
    mHovered = true;
    update();
    QGraphicsObject::hoverEnterEvent(event);
}

void CardItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
    mHovered = false;
    update();
    QGraphicsObject::hoverLeaveEvent(event);
}
