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
    // 图集采样单元（Jokers.png 每格固定 142×190）。
    static constexpr int SRC_W = 142;
    static constexpr int SRC_H = 190;
    // 场景显示尺寸：与 CardItem 同步放大约 20%，让小窗口下也能看清。
    static constexpr int WIDTH = 170;
    static constexpr int HEIGHT = 228;

    static void loadResources();                  // main.cpp 里调一次
    static QPoint spritePos(JokerType type);      // 在 Jokers.png 里的(列,行)

    // 在给定的 painter 上、给定的目标矩形里，画出小丑的“浮动 soul 层”（Hologram 上方的小丑、
    // 五张传奇牌中央的肖像）。商店里 offerPixmap 也需要这一步，否则全息投影的卡只有空相框。
    static void drawFloatingSprite(QPainter *p, const QRectF &dst, JokerType type,
                                   bool animated = true);

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
    void hoverMoveEvent(QGraphicsSceneHoverEvent *e) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *e) override;

private:
    Joker mJoker;
    bool mHovered = false;
    bool mPressed = false;
    bool mDragging = false;
    QPointF mPressScenePos;
    qreal mRestZ = 0;
    double mHoverTiltX = 0.0;
    double mHoverTiltY = 0.0;
    void applyHoverTransform();
    void animateScale(qreal target, int durationMs = 120);
    static QPixmap *sSheet;
};

#endif
