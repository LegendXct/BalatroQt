#ifndef CARDITEM_H
#define CARDITEM_H

#include <QGraphicsObject>
#include <QPixmap>
#include <QPropertyAnimation>
#include "carddata.h"

class CardShadowItem;

class CardItem : public QGraphicsObject
{
    Q_OBJECT
    Q_PROPERTY(QPointF pos READ pos WRITE setPos)
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity)
    // 翻牌动画专用：仅 X 方向缩放，对应原版 card.lua 的 pinch.x（绕 Y 轴翻牌效果）。
    Q_PROPERTY(qreal flipXScale READ flipXScale WRITE setFlipXScale)
    // 阴影抬升量(0..1)。原版 G.SHADOW + shadow_parallax：卡片悬停 / 按下 / 计分时
    // Z 高度增加 → 阴影 offset.y 与软化范围同步增大，给出"悬浮起来"的立体感。
    Q_PROPERTY(qreal shadowLift READ shadowLift WRITE setShadowLift)
public:
    // 图集采样单元：8BitDeck.png/Enhancers.png 每格固定 142×190，不能改。
    static constexpr int SRC_W = 142;
    static constexpr int SRC_H = 190;
    // 场景显示尺寸：原版 1920×1080 下卡牌约 150×201，这里再放大约 20% 让小窗口下也清晰可见。
    static constexpr int WIDTH = 170;
    static constexpr int HEIGHT = 228;

    // ── 素材布局说明 ──────────────────────────────────
    // 8BitDeck.png：1846x760，13列x4行，每格 142x190
    // 列 0-12 = 2,3,4,5,6,7,8,9,10,J,Q,K,A
    // 行 0=♥  行 1=♣  行 2=♦  行 3=♠
    // 注意：牌面是透明背景，需先铺 Enhancers[0,1] 白色底色
    //
    // Enhancers.png：994x950，7列x5行，每格 142x190
    // 行0: [0,0]=牌背  [0,1]=白色底色  [0,2]=金色烙印
    //      [0,5]=石头(Stone)  [0,6]=金色增强(Gold)
    // 行1: [1,1]=Bonus  [1,2]=Mult  [1,3]=Wild
    //      [1,4]=Lucky  [1,5]=Glass  [1,6]=Steel
    // 行4: [4,4]=紫色印章  [4,5]=红色印章  [4,6]=蓝色印章
    // ──────────────────────────────────────────────────

    static void loadResources();
    explicit CardItem(const CardData &data, QGraphicsItem *parent = nullptr);
    ~CardItem() override;
    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    const CardData &cardData() const {return mData;}
    void setCardData(const CardData &data);
    void setCardSelected(bool selected);
    bool isCardSelected() const {return mSelected;}
    void moveTo(const QPointF &target, int durationMs = 300);
    void flip();
    void juiceUp(double scaleAmount = 1.2, int durationMs = 240);
    void setBaseRotation(double deg) { mBaseRotation = deg; applyTransform(); }
    // 平滑改变 scale；hover 进入/离开时用它代替 setScale() 的瞬时跳变。
    void animateScale(qreal target, int durationMs = 110);
    // 控制是否允许拖拽；牌堆右下角的"查看牌组"卡牌需要 false，禁用拖动行为。
    void setDraggable(bool d) { mDraggable = d; }
    bool isDraggable() const { return mDraggable; }
    // 关闭跟随鼠标的 3D 倾斜——牌堆按钮 / 计分时的 played 卡需要静态展示。
    void setHoverTiltEnabled(bool enabled) { mHoverTiltEnabled = enabled; }
    // boundingRect 默认 (-12, -78, W+24, H+92) 比可见牌面更大（给悬浮标签预留位置）。
    // 牌堆按钮不希望"在牌面上方就触发悬浮"，可调用此函数让 hit-test 只覆盖真正的牌面矩形。
    void setStrictHoverShape(bool s) { mStrictHoverShape = s; }
    // 触发对齐 card.lua Card:hover() 的极小抖动（旋转 +/-0.7°，scale ~2% 弹动）。
    void triggerHoverJitter();
    qreal flipXScale() const { return mFlipXScale; }
    void setFlipXScale(qreal s) { mFlipXScale = s; applyTransform(); }
    qreal shadowLift() const { return mShadowLift; }
    void setShadowLift(qreal v);
    // 计分阶段抬升：mainwindow.cpp 在 playScoreEvent / animatePlayedCardsToDiscardThen
    // 期间调用，让正在被计分的牌阴影最大化（off + blur 增大），看上去卡牌悬浮起来。
    void setScoringLifted(bool lifted);
signals:
    void clicked(CardItem *card);
    void dragMoved(CardItem *card, QPointF scenePos);
    void dragReleased(CardItem *card, QPointF scenePos);
    void hoverChanged(CardItem *card, bool hovered);
protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent  *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent  *event) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
private:
    CardData mData;
    bool mSelected = false;
    bool mHovered = false;
    bool mPressed = false;
    bool mDragging = false;
    bool mDraggable = true;
    bool mHoverTiltEnabled = true;
    bool mStrictHoverShape = false;
    QPointF mPressScenePos;

    double mBaseRotation = 0.0;       // Z 轴旋转(扇形)
    double mHoverTiltX  = 0.0;        // 绕 X 轴倾斜(度,-25 ~ +25)
    double mHoverTiltY  = 0.0;        // 绕 Y 轴倾斜
    double mJitterRot = 0.0;          // 悬浮抖动当前叠加的 Z 旋转(度)，仅短暂存在
    // 拖拽时基于水平速度的瞬时倾斜（度），对齐原版 moveable.lua move_r()：
    //   des_r = T.r + 0.015 * vel.x / dt
    // 拖动方向越快、卡片倾斜越大；释放时用 QVariantAnimation 平滑回 0。
    double mDragTilt = 0.0;
    QPointF mLastDragScenePos;
    qint64  mLastDragTimeMs = 0;
    qreal  mFlipXScale = 1.0;         // 翻牌时的 X 方向缩放(1→0→1)，对齐原版 pinch.x
    qreal  mShadowLift = 0.0;         // 阴影抬升量 0..1，由 hover/press/select/scoring 驱动。
    bool   mScoringLifted = false;    // 计分阶段强制保持最高阴影抬升。
    qreal  mPressRestZ = 0.0;         // 按下前 zValue，release 还原；防止覆盖 layout 给的 z。
    CardShadowItem *mShadow = nullptr; // sibling 阴影项，z=-1000 在所有牌之下；按下时升到本牌之下。
    void applyTransform();
    void animateShadowLift(qreal target, int durationMs = 120);
    qreal currentShadowTarget() const; // 根据 hover/press/select/scoring 状态返回目标值。
    void updateShadowZ();              // 根据 active 状态决定阴影 z；release 后回到 -1000。

    QRect whiteBaseSrcRect() const; // 白色底片
    QRect deckSrcRect() const; // 牌面
    QRect enhanceSrcRect() const; // 增强类型
    QRect sealSrcRect() const; // 印章叠加层

    void paintFront(QPainter *painter);
    void paintBack(QPainter *painter);

    static QPixmap *sDeckSheet;
    static QPixmap *sEnhSheet;
    static QPixmap *sJokerSheet;
};

#endif // CARDITEM_H
