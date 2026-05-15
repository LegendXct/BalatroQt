#include "floatingscore.h"
#include <QPainter>
#include <QRandomGenerator>

FloatingScore::FloatingScore(const QString &text, const QColor &bg, const QFont &font,
                             QGraphicsItem *parent)
    : QGraphicsObject(parent), mText(text), mBg(bg), mFont(font)
{
    // 原版 engine/particles.lua: facing = math.random()*2*math.pi
    // 每个粒子初始为 0~360° 之间的任意角度,而非固定 45°(菱形)。
    mTiltDeg = QRandomGenerator::global()->bounded(360);
}

QRectF FloatingScore::boundingRect() const
{
    // 留余地容纳旋转后伸出的角(对角线 ≈ SIZE * 1.42 / 2)
    int half = int(SIZE * 0.72) + 8;
    return QRectF(-half, -half, half * 2, half * 2);
}

void FloatingScore::paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *)
{
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setRenderHint(QPainter::SmoothPixmapTransform, true);

    // 原版背景:love.graphics.rectangle('fill', -s/2, -s/2, s, s) + love.graphics.rotate(facing)
    // 即一个填充正方形,任意角度旋转。无边框、无圆角。
    p->save();
    p->rotate(mTiltDeg);
    p->setPen(Qt::NoPen);
    p->setBrush(mBg);
    int half = SIZE / 2;
    p->drawRect(-half, -half, SIZE, SIZE);
    p->restore();

    // 文字水平居中(对应 DynaText,不跟随 Particles 自转)
    QFont f = mFont;
    f.setPixelSize(22);
    f.setBold(true);
    p->setFont(f);
    p->setPen(Qt::white);
    // 文字范围比背景略宽,允许 "+30000" 这种较长数字横向溢出方块。
    QRectF textRect(-SIZE, -SIZE / 2, SIZE * 2, SIZE);
    p->drawText(textRect, Qt::AlignCenter, mText);
}
