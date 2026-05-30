#include "cardshadow.h"
#include <QPainter>
#include <QGraphicsScene>
#include <QtMath>

CardShadowItem::CardShadowItem(int w, int h, LiftGetter getter)
    : mW(w), mH(h), mGetLift(std::move(getter)),
      mVisibleRect(0, 0, w, h) {}

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
    const qreal kPxPerLua = 32.0;
    // 牌在场景左半边 (nx<0) → offX 正 → 阴影向右偏 → 朝场景中心；右半边相反。
    const qreal offX = -nx * 1.5 * shadowHeight * kPxPerLua;
    const qreal offY =  1.5 * shadowHeight * kPxPerLua;
    const qreal expand = 0.5 + 2.0 * lift;

    p->setRenderHint(QPainter::Antialiasing, true);
    p->setPen(Qt::NoPen);

    // 异形卡牌用 mVisibleRect 作为阴影几何，默认 = 全卡。
    const QRectF vr = mVisibleRect;
    const int alphaOuter = int(20 + 25 * lift);
    p->setBrush(QColor(0, 0, 0, alphaOuter));
    p->drawRoundedRect(
        QRectF(vr.x() + offX - expand, vr.y() + offY - expand,
               vr.width() + 2 * expand, vr.height() + 2 * expand),
        10, 10);

    const int alphaInner = int(45 + 40 * lift);
    p->setBrush(QColor(0, 0, 0, alphaInner));
    p->drawRoundedRect(QRectF(vr.x() + offX, vr.y() + offY, vr.width(), vr.height()), 9, 9);
}
