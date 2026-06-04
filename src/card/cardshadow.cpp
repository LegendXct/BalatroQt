#include "cardshadow.h"
#include <QPainter>
#include <QGraphicsScene>
#include <QtMath>

CardShadowItem::CardShadowItem(int w, int h, LiftGetter getter)
    : mW(w), mH(h), mGetLift(std::move(getter)),
      mVisibleRect(0, 0, w, h) {}

QPixmap CardShadowItem::makeSilhouette(const QPixmap &src)
{
    if (src.isNull()) return QPixmap();
    QPixmap sil(src.size());
    sil.fill(Qt::transparent);
    QPainter sp(&sil);
    sp.drawPixmap(0, 0, src);                                  // 先铺上 sprite（取其 alpha）
    sp.setCompositionMode(QPainter::CompositionMode_SourceIn); // 只在已有 alpha 处上色
    sp.fillRect(sil.rect(), Qt::black);                        // RGB 全染黑，alpha 不变
    sp.end();
    return sil;
}

QRectF CardShadowItem::boundingRect() const
{
    // 阴影固定方向 (-0.5*offY, +offY)，最大 lift offY=27 + expand=2 → 29 px。
    // 留 ±18 px 横向 / ~40 px 纵向余量。
    return QRectF(-18, -4, mW + 36, mH + 40);
}

void CardShadowItem::paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *)
{
    const qreal lift = qBound(0.0, mGetLift ? mGetLift() : 0.0, 1.0);

    // 所有卡牌阴影方向 + 大小完全一致，不随 scene 位置(nx)摆动。
    // 光源固定在右上方 → 阴影投到左下角 → (offX < 0, offY > 0)。
    //
    // 用户反馈：之前 18 px baseline 偏大，再缩 25%（18 → 13.5）。
    //   rest:    offY = 13.5,  |offX| ≈ 6.75
    //   lift=1:  offY = 27,    |offX| ≈ 13.5
    // joker 槽间隙 14 px，最大 |offX| = 13.5 仍在间隙内；z=-1000 还兜底防视觉重叠。
    const qreal offY = 13.5 + 13.5 * lift;
    const qreal offX = -0.5 * offY;
    const qreal expand = 0.5 + 1.5 * lift;

    p->setRenderHint(QPainter::Antialiasing, true);
    p->setRenderHint(QPainter::SmoothPixmapTransform, true);
    p->setPen(Qt::NoPen);

    // 复刻原版 dissolve 阴影（card.lua:4362 → dissolve.fs:18）：阴影就是物品自身 sprite
    // 的黑色剪影，单层 ~30% alpha，按物品真实轮廓投影——异形小丑（半张 / 微缩 / 方形）、
    // 优惠券、补充包的圆角与撕裂边都能正确成形，而不是统一的圆角矩形。
    if (!mSilhouette.isNull()) {
        // 原版阴影相对本体略微缩小（1-0.2*height）并贴向本体，这里随 lift 做轻微收缩。
        const qreal shrink = 1.0 - 0.06 * lift;
        const qreal w = mW * shrink;
        const qreal h = mH * shrink;
        const qreal x = (mW - w) / 2.0 + offX;
        const qreal y = (mH - h) / 2.0 + offY;
        p->setOpacity(0.32 + 0.16 * lift);
        p->drawPixmap(QRectF(x, y, w, h), mSilhouette,
                      QRectF(0, 0, mSilhouette.width(), mSilhouette.height()));
        p->setOpacity(1.0);
        return;
    }

    // 回退：没有喂入剪影的矩形 sprite（如手牌）仍画双层圆角矩形贴合卡面圆角。
    const QRectF vr = mVisibleRect;
    const int alphaOuter = int(30 + 30 * lift);
    const int alphaInner = int(78 + 45 * lift);

    p->setBrush(QColor(0, 0, 0, alphaOuter));
    p->drawRoundedRect(
        QRectF(vr.x() + offX - expand, vr.y() + offY - expand,
               vr.width() + 2 * expand, vr.height() + 2 * expand),
        10, 10);

    p->setBrush(QColor(0, 0, 0, alphaInner));
    p->drawRoundedRect(QRectF(vr.x() + offX, vr.y() + offY, vr.width(), vr.height()), 9, 9);
}
