#ifndef CARDSHADOW_H
#define CARDSHADOW_H

#include <QGraphicsItem>
#include <QPixmap>
#include <functional>

// 卡牌阴影 sibling 项：每个 CardItem / JokerItem / ConsumableItem 在 scene 里挂一只这种
// QGraphicsItem，z=-1000 落在所有牌底下；按下 / 拖动 / 计分时上调到拥有者 z 之下、其他牌之上。
// 形状几乎与拥有者牌面同尺寸，按 lift 拉出左下偏移 + 半透明。
//
// 为什么用 sibling 不用 child：QGraphicsItem 的 ItemStacksBehindParent 只在拥有者自身的
// z 槽里向后排，不会真正下沉到全场最底——fan 出来的相邻牌仍会被本牌的阴影覆盖。
// sibling + 独立 zValue 才能保证阴影永远在其他牌之下（除非主动抬升）。
class CardShadowItem : public QGraphicsItem {
public:
    using LiftGetter = std::function<qreal()>;

    CardShadowItem(int w, int h, LiftGetter getter);

    QRectF boundingRect() const override;
    void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override;

    // 异形小丑（j_half / j_wee / j_square 等）：把可见矩形从默认 (0,0,w,h) 改成实际显示
    // 区域，阴影才不会"溢出"到看得见的形状之外。
    void setVisibleRect(const QRectF &r) { mVisibleRect = r; }

    // 阴影按物品真实轮廓投影：传入物品 sprite 的黑色剪影（RGB=黑、alpha=sprite alpha）。
    // 烧焦/异形小丑、优惠券、补充包等非矩形外形不再被圆角矩形阴影"溢出"。空则回退圆角矩形。
    void setSilhouette(const QPixmap &sil) { mSilhouette = sil; update(); }

    // 把 sprite 转成黑色剪影（RGB=黑、保留 alpha），供 setSilhouette 用。
    static QPixmap makeSilhouette(const QPixmap &src);

private:
    int mW;
    int mH;
    LiftGetter mGetLift;
    QRectF mVisibleRect;
    QPixmap mSilhouette;
};

#endif // CARDSHADOW_H
