#include "cardshadow.h"
#include <QPainter>
#include <QGraphicsScene>
#include <QtMath>

// 原版卡牌高度（tile 单位）：globals.lua CARD_H = 2.4*47/41。阴影偏移在原版里是
// shadow_parrallax * shadow_height（tile），换算到我们的像素只需乘 mH/CARD_H_TILES。
static constexpr qreal kCardHTiles = 2.4 * 47.0 / 41.0;   // ≈ 2.7512

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
    // 最大 lift（shadow_height=0.35）时偏移 ≈ 0.19*mH，旋转后任意方向，留 0.3*mH 余量。
    const qreal m = 0.30 * mH;
    return QRectF(-m, -8, mW + 2 * m, mH + m);
}

void CardShadowItem::paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *)
{
    const qreal lift = qBound(0.0, mGetLift ? mGetLift() : 0.0, 1.0);

    // —— 完全复刻原版 card.lua / sprite.lua 的阴影几何 ——
    // 原版 shadow_height：rest 0.1，highlighted/拖动 0.35（card.lua:4361）。
    // 我们的 lift(0..1) 线性映射到这一区间。
    const qreal sh = 0.1 + 0.25 * lift;            // shadow_height

    // 偏移 = shadow_parrallax * shadow_height（tile）→ 乘 mH/CARD_H_TILES 换算成像素。
    // shadow_parrallax.y = -1.5（moveable.lua:71），VT.y -= parrallax.y*height → 向下 +1.5*sh。
    const qreal pxPerTile = mH / kCardHTiles;
    const qreal worldOffY = 1.5 * sh * pxPerTile;

    // 横向是“视差”：shadow_parrallax.x = (cardCx - roomCx)/roomCx*1.5（moveable.lua:461）。
    // 屏幕中央 ≈ 0（阴影正下方），越靠边越向中线外扩。VT.x -= parrallax.x*height。
    qreal nx = 0.0;
    if (const QGraphicsScene *sc = scene()) {
        const qreal half = sc->sceneRect().width() * 0.5;
        if (half > 0.0) {
            const qreal cx = mapToScene(mVisibleRect.center()).x();
            nx = qBound(-1.0, (cx - sc->sceneRect().center().x()) / half, 1.0);
        }
    }
    const qreal worldOffX = -nx * 1.5 * sh * pxPerTile;

    // 阴影偏移在原版里是世界空间（加在 VT 平移上，不随卡牌旋转 r 转动）。我们的阴影 item
    // 跟随卡牌的旋转/缩放，所以这里把世界偏移反旋转/反缩放回本地坐标，保证扇形手牌、拖动
    // 倾斜时阴影始终落在卡牌的正下方而非歪向一侧。
    const qreal k = qFuzzyIsNull(scale()) ? 1.0 : scale();
    const qreal th = -qDegreesToRadians(rotation());
    const qreal cs = std::cos(th), sn = std::sin(th);
    const qreal offX = (worldOffX * cs - worldOffY * sn) / k;
    const qreal offY = (worldOffX * sn + worldOffY * cs) / k;

    // 阴影比卡牌略小：原版 VT.scale*(1-0.2*shadow_height)（sprite.lua:79），让阴影“贴”住牌。
    const qreal s = 1.0 - 0.2 * sh;

    p->setRenderHint(QPainter::Antialiasing, true);
    p->setRenderHint(QPainter::SmoothPixmapTransform, true);

    // 原版阴影只有一层：卡牌剪影染黑、alpha = sprite.a * 0.3（dissolve.fs:18，恒定 30%，
    // 不随抬升变深）。优先用真实轮廓剪影（异形小丑 / 优惠券 / 卡包按 sprite 形状投影），
    // 没有剪影（普通矩形卡牌）才回退圆角矩形。
    if (!mSilhouette.isNull()) {
        // 剪影铺满整张卡 cell（mW×mH），按 s 缩小、偏移 (offX,offY) 后绘制；
        // 30% 不透明叠在 item 自身透明度之上 = dissolve.fs 的 tex.a*0.3。
        const qreal w = mW * s;
        const qreal h = mH * s;
        const QRectF tr(mW / 2.0 - w / 2.0 + offX, mH / 2.0 - h / 2.0 + offY, w, h);
        p->setOpacity(p->opacity() * 0.3);
        p->drawPixmap(tr, mSilhouette, QRectF(mSilhouette.rect()));
        return;
    }

    p->setPen(Qt::NoPen);
    const QRectF vr = mVisibleRect;
    const QPointF cc = vr.center();
    const qreal w = vr.width() * s;
    const qreal h = vr.height() * s;
    const QRectF sr(cc.x() - w / 2.0 + offX, cc.y() - h / 2.0 + offY, w, h);

    p->setBrush(QColor(0, 0, 0, 76));     // 0.3 * 255 ≈ 76
    p->drawRoundedRect(sr, 9.0 * s, 9.0 * s);
}
