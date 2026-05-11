#include "floatingscore.h"
#include <QPainter>

FloatingScore::FloatingScore(const QString &text, const QColor &bg, const QFont &font,
                             QGraphicsItem *parent)
    : QGraphicsObject(parent), mText(text), mBg(bg), mFont(font)
{}

QRectF FloatingScore::boundingRect() const
{
    int half = SIZE / 2 + 8;     // 留 padding 容纳旋转 45° 后伸出来的角
    return QRectF(-half, -half, half * 2, half * 2);
}

void FloatingScore::paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *)
{
    p->setRenderHint(QPainter::Antialiasing);

    // 旋转 45° 画圆角方框
    p->save();
    p->rotate(45);
    QPen pen(Qt::white, 2);
    p->setPen(pen);
    p->setBrush(mBg);
    int half = (SIZE - 8) / 2;
    p->drawRoundedRect(-half, -half, half * 2, half * 2, 6, 6);
    p->restore();

    // 文字水平居中 (0,0)
    QFont f = mFont;
    f.setPixelSize(22);
    f.setBold(true);
    p->setFont(f);
    p->setPen(Qt::white);
    p->drawText(boundingRect(), Qt::AlignCenter, mText);
}
