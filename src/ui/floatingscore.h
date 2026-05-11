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

private:
    QString mText;
    QColor mBg;
    QFont  mFont;
    static constexpr int SIZE = 56;   // 正方形边长
};

#endif
