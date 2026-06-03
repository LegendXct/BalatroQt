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
    // x 方向最大偏移 ≈ ±18 px（卡在场景边缘 + lift=1），加 expand 3 → 留 ±25 余量。
    // y 方向最大 ≈ +18 px。
    return QRectF(-30, -6, mW + 60, mH + 60);
}

void CardShadowItem::paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *)
{
    const qreal lift = qBound(0.0, mGetLift ? mGetLift() : 0.0, 1.0);

    // 复刻原版 card.lua/moveable.lua：
    //   shadow_height = highlighted/dragging ? 0.35 : 0.1
    //   shadow_parrallax.x = (centerX - roomCenterX) / (roomW/2) * 1.5  (lua units)
    //   shadow_parrallax.y = -1.5
    //   final offset = -parrallax * shadow_height （翻负号意味着阴影向 room 中心收拢）
    //   每 lua 单位 = G.TILESIZE = 32 px
    //
    // 用户反馈：点击/计分时阴影距离再加大一些。这里把 lift=1 时的 height 从 0.35 提到 0.55，
    // 视觉上拉开"卡牌悬浮起来"的层次感。rest 不变。
    const qreal shadowHeight = 0.1 + 0.45 * lift;

    qreal nx = 0.0;
    if (auto *s = scene()) {
        const QRectF r = s->sceneRect();
        if (r.width() > 0) {
            const QPointF center = scenePos() + QPointF(mW / 2.0, mH / 2.0);
            nx = (center.x() - r.center().x()) / (r.width() / 2.0);
            nx = qBound(-1.0, nx, 1.0);
        }
    }
    // 原版 sprite.lua：VT.y -= shadow_parrallax.y(-1.5)*shadow_height，即向下偏 1.5*shadow_height
    // 个 card 单位。1 card 单位 = 本牌像素高/CARD_H(2.75) ≈ 像素宽/CARD_W(2.0488)。
    // 之前误用固定 32 px/单位（其实我们的牌放大后约 83 px/单位），导致阴影离本体太近、几乎看不见。
    const qreal kCardHUnits = 2.7512;   // G.CARD_H = 2.4*47/41
    const qreal kCardWUnits = 2.0488;   // G.CARD_W = 2.4*35/41
    // 牌在场景左半边 (nx<0) → offX 正 → 阴影向右偏 → 朝场景中心；右半边相反。
    const qreal offX = -nx * 1.5 * shadowHeight * (mW / kCardWUnits);
    const qreal offY =  1.5 * shadowHeight * (mH / kCardHUnits);
    const qreal expand = 0.5 + 2.0 * lift;

    p->setRenderHint(QPainter::Antialiasing, true);
    p->setRenderHint(QPainter::SmoothPixmapTransform, true);
    p->setPen(Qt::NoPen);

    // 异形卡牌用 mVisibleRect 作为阴影几何，默认 = 全卡。
    const QRectF vr = mVisibleRect;
    const int alphaOuter = int(20 + 25 * lift);
    const int alphaInner = int(45 + 40 * lift);

    if (!mSilhouette.isNull()) {
        // 按真实轮廓投影：画两层黑色剪影（外圈放大、低透明 = 软边；内圈实尺寸、较实）。
        // 剪影本身已是黑色 RGB + sprite 的 alpha，用画笔不透明度调出半透明阴影。
        p->setBrush(Qt::NoBrush);
        p->setOpacity(alphaOuter / 255.0);
        p->drawPixmap(QRectF(vr.x() + offX - expand, vr.y() + offY - expand,
                             vr.width() + 2 * expand, vr.height() + 2 * expand),
                      mSilhouette, QRectF(mSilhouette.rect()));
        p->setOpacity(alphaInner / 255.0);
        p->drawPixmap(QRectF(vr.x() + offX, vr.y() + offY, vr.width(), vr.height()),
                      mSilhouette, QRectF(mSilhouette.rect()));
        p->setOpacity(1.0);
        return;
    }

    // 回退：无剪影时画圆角矩形（矩形卡牌足够）。
    p->setBrush(QColor(0, 0, 0, alphaOuter));
    p->drawRoundedRect(
        QRectF(vr.x() + offX - expand, vr.y() + offY - expand,
               vr.width() + 2 * expand, vr.height() + 2 * expand),
        10, 10);

    p->setBrush(QColor(0, 0, 0, alphaInner));
    p->drawRoundedRect(QRectF(vr.x() + offX, vr.y() + offY, vr.width(), vr.height()), 9, 9);
}
