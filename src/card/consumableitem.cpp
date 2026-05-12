#include "consumableitem.h"
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QCursor>

QPixmap *ConsumableItem::sSheet = nullptr;

void ConsumableItem::loadResources() {
    sSheet = new QPixmap(":/textures/images/Tarots.png");
    if (sSheet->isNull()) qWarning("ConsumableItem: 加载 Tarots.png 失败");
}

// 坐标取自原版 game.lua c_xxx pos
QPoint ConsumableItem::spritePos(ConsumableType t) {
    switch (t) {
    // 塔罗（前 3 行）
    case ConsumableType::Tarot_Empress:    return {3, 0};
    case ConsumableType::Tarot_Hierophant: return {5, 0};
    case ConsumableType::Tarot_Lovers:     return {6, 0};
    case ConsumableType::Tarot_Chariot:    return {7, 0};
    case ConsumableType::Tarot_Hermit:     return {9, 0};
    case ConsumableType::Tarot_Tower:      return {6, 1};

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

    // 幻灵（原版 Tarots.png 第 4、5 行）
    case ConsumableType::Spectral_Talisman: return {3, 4};
    case ConsumableType::Spectral_Aura:     return {4, 4};
    case ConsumableType::Spectral_Immolate: return {9, 4};
    case ConsumableType::Spectral_DejaVu:   return {1, 5};
    case ConsumableType::Spectral_Trance:   return {3, 5};
    case ConsumableType::Spectral_Medium:   return {4, 5};
    }
    return {0, 0};
}

ConsumableItem::ConsumableItem(const Consumable &c, QGraphicsItem *parent)
    : QGraphicsObject(parent), mC(c)
{
    setAcceptHoverEvents(true);
    setCursor(Qt::PointingHandCursor);
    setToolTip(QString("%1\n%2\n左键: 使用    右键: 卖出 (+$%3)")
                   .arg(mC.name, mC.description).arg(mC.sellValue));
}

void ConsumableItem::paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) {
    p->setRenderHint(QPainter::SmoothPixmapTransform);

    if (sSheet && !sSheet->isNull()) {
        QPoint c = spritePos(mC.type);
        QRect src(c.x() * WIDTH, c.y() * HEIGHT, WIDTH, HEIGHT);
        p->drawPixmap(QRect(0, 0, WIDTH, HEIGHT), *sSheet, src);
    }

    if (mHovered) {
        p->setPen(QPen(QColor(255, 240, 96, 220), 4));
        p->setBrush(Qt::NoBrush);
        p->drawRoundedRect(2, 2, WIDTH - 4, HEIGHT - 4, 10, 10);
    }
}

void ConsumableItem::mousePressEvent(QGraphicsSceneMouseEvent *e) {
    emit clicked(this, e->button());
    e->accept();
}
