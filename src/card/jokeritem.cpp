#include "jokeritem.h"
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QCursor>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>

QPixmap *JokerItem::sSheet = nullptr;

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
    }
    return {0, 0};
}

JokerItem::JokerItem(const Joker &j, QGraphicsItem *parent)
    : QGraphicsObject(parent), mJoker(j)
{
    setAcceptHoverEvents(true);
    setCursor(Qt::PointingHandCursor);
}

QRectF JokerItem::boundingRect() const {
    return QRectF(0, 0, WIDTH, HEIGHT);
}

void JokerItem::paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) {
    p->setRenderHint(QPainter::SmoothPixmapTransform);

    QPoint c = spritePos(mJoker.type);
    QRect src(c.x() * WIDTH, c.y() * HEIGHT, WIDTH, HEIGHT);
    p->drawPixmap(QRect(0, 0, WIDTH, HEIGHT), *sSheet, src);

    if (mHovered) {
        p->setPen(QPen(QColor(255, 240, 96, 200), 4));
        p->setBrush(Qt::NoBrush);
        p->drawRoundedRect(0, 0, WIDTH, HEIGHT, 8, 8);
    }
}

void JokerItem::mousePressEvent(QGraphicsSceneMouseEvent *e) {
    if (e->button() == Qt::LeftButton) {
        emit clicked(this);
        e->accept();
    } else {
        QGraphicsObject::mousePressEvent(e);
    }
}

void juiceUp(double scaleAmount = 1.15, int durationMs = 200);

// jokeritem.cpp
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
