#ifndef CARDITEM_H
#define CARDITEM_H

#include <QGraphicsObject>
#include <QPixmap>
#include <QPropertyAnimation>
#include "carddata.h"

class CardItem : public QGraphicsObject
{
    Q_OBJECT
    Q_PROPERTY(QPointF pos READ pos WRITE setPos)
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity)
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
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    const CardData &cardData() const {return mData;}
    void setCardData(const CardData &data);
    void setCardSelected(bool selected);
    bool isCardSelected() const {return mSelected;}
    void moveTo(const QPointF &target, int durationMs = 300);
    void flip();
    void juiceUp(double scaleAmount = 1.2, int durationMs = 240);
    void setBaseRotation(double deg) { mBaseRotation = deg; applyTransform(); }
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
private:
    CardData mData;
    bool mSelected = false;
    bool mHovered = false;
    bool mPressed = false;
    bool mDragging = false;
    QPointF mPressScenePos;

    double mBaseRotation = 0.0;       // Z 轴旋转(扇形)
    double mHoverTiltX  = 0.0;        // 绕 X 轴倾斜(度,-25 ~ +25)
    double mHoverTiltY  = 0.0;        // 绕 Y 轴倾斜
    void applyTransform();

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
