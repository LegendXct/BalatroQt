#ifndef CONSUMABLEITEM_H
#define CONSUMABLEITEM_H

#include <QGraphicsObject>
#include <QPixmap>
#include <QPointF>
#include "consumable.h"

class QGraphicsSceneHoverEvent;
class QGraphicsSceneMouseEvent;
class CardShadowItem;

// consumableitem.h
class ConsumableItem : public QGraphicsObject
{
    Q_OBJECT
    Q_PROPERTY(qreal shadowLift READ shadowLift WRITE setShadowLift)
public:
    // 图集原始单元（Tarots.png / Planets.png / Spectrals.png / boosters.png 每格 142×190）。
    static constexpr int SRC_W  = 142;
    static constexpr int SRC_H  = 190;
    // 场景显示尺寸：与 JokerItem 同步——162×218（aspect 0.743 与原版 CARD_W/H 一致）。
    static constexpr int WIDTH  = 162;
    static constexpr int HEIGHT = 218;

    static void loadResources();                            // ← 新增
    static QPoint spritePos(ConsumableType t);              // ← 新增
    static QPixmap renderPixmap(ConsumableType type, bool negative = false); // 含灵魂白水晶/负片前景层

    explicit ConsumableItem(const Consumable &c, QGraphicsItem *parent = nullptr);
    ~ConsumableItem() override;
    QRectF boundingRect() const override { return {0, 0, WIDTH, HEIGHT}; }
    void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override;
    const Consumable &consumable() const { return mC; }

    qreal shadowLift() const { return mShadowLift; }
    void setShadowLift(qreal v);
    void setScoringLifted(bool lifted);

signals:
    void clicked(ConsumableItem *self, Qt::MouseButton button);
    void pressed(ConsumableItem *self, Qt::MouseButton button);
    void dragMoved(ConsumableItem *self, QPointF scenePos);
    void dragReleased(ConsumableItem *self, QPointF scenePos);
    void hoverChanged(ConsumableItem *self, bool hovered);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *e) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *e) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *e) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *e) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *e) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *e) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    Consumable mC;
    bool mHovered = false;
    bool mPressed = false;
    bool mDragging = false;
    QPointF mPressScenePos;
    qreal mRestZ = 0.0;
    double mHoverTiltX = 0.0;
    double mHoverTiltY = 0.0;
    // 拖拽水平速度倾斜——参数与 CardItem 一致，让消耗品也有甩动手感（用户反馈9）。
    double mDragTilt = 0.0;
    QPointF mLastDragScenePos;
    qint64  mLastDragTimeMs = 0;
    qreal mShadowLift = 0.0;
    bool mScoringLifted = false;
    CardShadowItem *mShadow = nullptr;
    void applyHoverTransform();
    void animateScale(qreal target, int durationMs = 120);
    void animateShadowLift(qreal target, int durationMs = 120);
    qreal currentShadowTarget() const;
    void updateShadowZ();
    void triggerHoverJitter();
public:
    void moveTo(const QPointF &target, int durationMs = 160);
    void juiceUp(double scaleAmount = 1.12, int durationMs = 180);
private:
    static QPixmap *sSheet;
};

#endif
