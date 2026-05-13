#ifndef JOKERITEM_H
#define JOKERITEM_H

#include <QGraphicsObject>
#include <QPixmap>
#include <QPointF>
#include "joker.h"

class JokerItem : public QGraphicsObject
{
    Q_OBJECT
public:
    static constexpr int WIDTH = 142;
    static constexpr int HEIGHT = 190;

    static void loadResources();                  // main.cpp 里调一次
    static QPoint spritePos(JokerType type);      // 在 Jokers.png 里的(列,行)

    explicit JokerItem(const Joker &joker, QGraphicsItem *parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override;

    const Joker &joker() const { return mJoker; }
    void juiceUp(double scaleAmount = 1.15, int durationMs = 200);
    void moveTo(const QPointF &target, int durationMs = 180);

signals:
    void clicked(JokerItem *self);
    void pressed(JokerItem *self, Qt::MouseButton button);
    void dragMoved(JokerItem *self, QPointF scenePos);
    void dragReleased(JokerItem *self, QPointF scenePos);
    void hoverChanged(JokerItem *self, bool hovered);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *e) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *e) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *e) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *e) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *e) override;

private:
    Joker mJoker;
    bool mHovered = false;
    bool mPressed = false;
    bool mDragging = false;
    QPointF mPressScenePos;
    qreal mRestZ = 0;
    void animateScale(qreal target, int durationMs = 120);
    static QPixmap *sSheet;
};

#endif
