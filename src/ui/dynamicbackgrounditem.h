#ifndef DYNAMICBACKGROUNDITEM_H
#define DYNAMICBACKGROUNDITEM_H

#include <QGraphicsObject>
#include <QColor>
#include <QTimer>

class DynamicBackgroundItem : public QGraphicsObject
{
    Q_OBJECT
public:
    enum class Mood {
        Default,
        Shop,
        Tarot,
        Spectral,
        Celestial,
        Buffoon,
        Standard,
        BlindSelect,
    };

    explicit DynamicBackgroundItem(QGraphicsItem *parent = nullptr);
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    void setSceneSize(qreal w, qreal h);
    void setMood(Mood mood);

private:
    qreal mW = 1280;
    qreal mH = 720;
    qreal mTime = 0.0;
    Mood mMood = Mood::Default;
    QTimer mTimer;

    QColor baseA() const;
    QColor baseB() const;
    QColor accent() const;
};

#endif // DYNAMICBACKGROUNDITEM_H
