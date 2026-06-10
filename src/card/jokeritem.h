#ifndef JOKERITEM_H
#define JOKERITEM_H

#include <QGraphicsObject>
#include <QPixmap>
#include <QPointF>
#include "joker.h"

class CardShadowItem;

class JokerItem : public QGraphicsObject
{
    Q_OBJECT
    Q_PROPERTY(qreal shadowLift READ shadowLift WRITE setShadowLift)
public:
    // 图集采样单元（Jokers.png 每格固定 142×190）。
    static constexpr int SRC_W = 142;
    static constexpr int SRC_H = 190;
    // 场景显示尺寸：商店去掉金额栏后，把槽位卡牌略微放大与商店保持一致。
    // 162×218 ≈ 图集 142×190 × 1.14，aspect 0.743 与原版 CARD_W/H 完全一致。
    static constexpr int WIDTH = 162;
    static constexpr int HEIGHT = 218;

    static void loadResources();                  // main.cpp 里调一次
    static QPoint spritePos(JokerType type);      // 在 Jokers.png 里的(列,行)
    // 程设扩展小丑（运算符重载/类模板）的专属整卡贴图（142×190，已按图集小丑
    // 轮廓裁圆角）；非扩展类型返回空 pixmap，调用方回退到 Jokers.png 图集采样。
    static QPixmap customCardPixmap(JokerType type);

    // 在给定的 painter 上、给定的目标矩形里，画出小丑的“浮动 soul 层”（Hologram 上方的小丑、
    // 五张传奇牌中央的肖像）。商店里 offerPixmap 也需要这一步，否则全息投影的卡只有空相框。
    static void drawFloatingSprite(QPainter *p, const QRectF &dst, JokerType type,
                                   bool animated = true);

    explicit JokerItem(const Joker &joker, QGraphicsItem *parent = nullptr);
    ~JokerItem() override;

    QRectF boundingRect() const override;
    void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override;

    const Joker &joker() const { return mJoker; }
    void juiceUp(double scaleAmount = 1.15, int durationMs = 200);
    void moveTo(const QPointF &target, int durationMs = 180);

    qreal shadowLift() const { return mShadowLift; }
    void setShadowLift(qreal v);
    // 计分阶段抬升：让 OnPlayedHand / OnScoringCard 触发时阴影最大化。
    void setScoringLifted(bool lifted);

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
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    Joker mJoker;
    bool mHovered = false;
    bool mPressed = false;
    bool mDragging = false;
    QPointF mPressScenePos;
    qreal mRestZ = 0;
    double mHoverTiltX = 0.0;
    double mHoverTiltY = 0.0;
    // 拖拽水平速度倾斜，与 CardItem 用同一套折算公式，让小丑/塔罗/星球 也具备甩动手感。
    double mDragTilt = 0.0;
    // 重排移动倾斜：被其它牌挤动而 moveTo 滑向新位置时，朝运动方向倾斜（对齐原版 move_r）。
    double mMoveTilt = 0.0;
    // 上次喂给阴影的剪影 key（type|edition|debuff），变了才重算阴影剪影。
    QString mShadowSilKey;
    // 异形小丑按实际内容垂直居中的下移量（场景像素），随 key 变化时重算。
    qreal mContentDyScreen = 0.0;
    QPointF mLastDragScenePos;
    qint64  mLastDragTimeMs = 0;
    qreal mShadowLift = 0.0;
    bool mScoringLifted = false;
    CardShadowItem *mShadow = nullptr;
    void applyHoverTransform();
    void animateScale(qreal target, int durationMs = 120);
    void animateShadowLift(qreal target, int durationMs = 120);
    void triggerHoverJitter();
    qreal currentShadowTarget() const;
    void updateShadowZ();
    static QPixmap *sSheet;
};

#endif
