#ifndef CONSUMABLEITEM_H
#define CONSUMABLEITEM_H

#include <QGraphicsObject>
#include <QPixmap>
#include "consumable.h"

// consumableitem.h
class ConsumableItem : public QGraphicsObject
{
    Q_OBJECT
public:
    static constexpr int WIDTH  = 142;
    static constexpr int HEIGHT = 190;

    static void loadResources();                            // ← 新增
    static QPoint spritePos(ConsumableType t);              // ← 新增
    static QPixmap renderPixmap(ConsumableType type, bool negative = false); // 含灵魂白水晶/负片前景层

    explicit ConsumableItem(const Consumable &c, QGraphicsItem *parent = nullptr);
    QRectF boundingRect() const override { return {0, 0, WIDTH, HEIGHT}; }
    void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override;
    const Consumable &consumable() const { return mC; }

signals:
    void clicked(ConsumableItem *self, Qt::MouseButton button);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *e) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *) override { mHovered = true;  update(); }
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *) override { mHovered = false; update(); }

private:
    Consumable mC;
    bool mHovered = false;
    static QPixmap *sSheet;
};

#endif
