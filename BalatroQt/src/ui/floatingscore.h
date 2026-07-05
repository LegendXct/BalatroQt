#ifndef FLOATINGSCORE_H
#define FLOATINGSCORE_H

#include <QGraphicsObject>
#include <QFont>
#include <QColor>

class FloatingScore : public QGraphicsObject
{
    Q_OBJECT
public:
    FloatingScore(const QString &text, const QColor &bg, const QFont &font,
                  QGraphicsItem *parent = nullptr);
    QRectF boundingRect() const override;
    void paint(QPainter *p, const QStyleOptionGraphicsItem *o, QWidget *w) override;

    // 原版 Particles 的 facing = random()*2π,单个方块粒子随机角度旋转。
    // 通过外部动画驱动每帧 setTiltDeg(deg+=Δ),对应 r_vel 缓慢自转。
    double tiltDeg() const { return mTiltDeg; }
    void   setTiltDeg(double d) { mTiltDeg = d; update(); }

private:
    QString mText;
    QColor  mBg;
    QFont   mFont;
    double  mTiltDeg = 0.0;            // 当前旋转角(度)
    static constexpr int SIZE = 56;    // 正方形边长(对应 Particles scale)
};

#endif
