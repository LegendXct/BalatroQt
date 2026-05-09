#ifndef JOKERITEM_H
#define JOKERITEM_H

#include <QGraphicsObject>
#include <QPixmap>
#include "joker.h"

class JokerItem : public QGraphicsObject
{
    Q_OBJECT
public:
    static constexpr int WIDTH = 142;
    static constexpr int HEIGHT = 190;

    static void loadResources();                  // main.cpp 里调一次
    static QPoint spritePos(JokerType type);      // 在 Jokers.png 里的(列,行)

    explicit JokerItem(const Joker &joker, QGraphicsItem *parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override;

    const Joker &joker() const { return mJoker; }

signals:
    void clicked(JokerItem *self);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *e) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *) override { mHovered = true;  update(); }
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *) override { mHovered = false; update(); }

private:
    Joker mJoker;
    bool mHovered = false;
    static QPixmap *sSheet;
};

#endif
