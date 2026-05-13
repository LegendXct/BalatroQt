#include "consumableitem.h"
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QCursor>
#include <QTimer>
#include <QDateTime>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <cmath>

QPixmap *ConsumableItem::sSheet = nullptr;

void ConsumableItem::loadResources() {
    sSheet = new QPixmap(":/textures/images/Tarots.png");
    if (sSheet->isNull()) qWarning("ConsumableItem: 加载 Tarots.png 失败");
}

// 坐标取自原版 game.lua c_xxx pos
QPoint ConsumableItem::spritePos(ConsumableType t) {
    switch (t) {
    // 塔罗（原版 game.lua c_xxx pos）
    case ConsumableType::Tarot_Fool:          return {0, 0};
    case ConsumableType::Tarot_Magician:      return {1, 0};
    case ConsumableType::Tarot_HighPriestess: return {2, 0};
    case ConsumableType::Tarot_Empress:       return {3, 0};
    case ConsumableType::Tarot_Emperor:       return {4, 0};
    case ConsumableType::Tarot_Hierophant:    return {5, 0};
    case ConsumableType::Tarot_Lovers:        return {6, 0};
    case ConsumableType::Tarot_Chariot:       return {7, 0};
    case ConsumableType::Tarot_Justice:       return {8, 0};
    case ConsumableType::Tarot_Hermit:        return {9, 0};
    case ConsumableType::Tarot_Wheel:         return {0, 1};
    case ConsumableType::Tarot_Strength:      return {1, 1};
    case ConsumableType::Tarot_HangedMan:     return {2, 1};
    case ConsumableType::Tarot_Death:         return {3, 1};
    case ConsumableType::Tarot_Temperance:    return {4, 1};
    case ConsumableType::Tarot_Devil:         return {5, 1};
    case ConsumableType::Tarot_Tower:         return {6, 1};
    case ConsumableType::Tarot_Star:          return {7, 1};
    case ConsumableType::Tarot_Moon:          return {8, 1};
    case ConsumableType::Tarot_Sun:           return {9, 1};
    case ConsumableType::Tarot_Judgement:     return {0, 2};
    case ConsumableType::Tarot_World:         return {1, 2};

    // 行星
    case ConsumableType::Planet_Mercury:   return {0, 3};
    case ConsumableType::Planet_Venus:     return {1, 3};
    case ConsumableType::Planet_Earth:     return {2, 3};
    case ConsumableType::Planet_Mars:      return {3, 3};
    case ConsumableType::Planet_Jupiter:   return {4, 3};
    case ConsumableType::Planet_Saturn:    return {5, 3};
    case ConsumableType::Planet_Uranus:    return {6, 3};
    case ConsumableType::Planet_Neptune:   return {7, 3};
    case ConsumableType::Planet_Pluto:     return {8, 3};
    case ConsumableType::Planet_PlanetX:   return {9, 2};
    case ConsumableType::Planet_Ceres:     return {8, 2};
    case ConsumableType::Planet_Eris:      return {3, 2};

    // 幻灵（原版 Tarots.png 第 4、5 行；The Soul 本体背景在 {2,2}）
    case ConsumableType::Spectral_Familiar: return {0, 4};
    case ConsumableType::Spectral_Grim:     return {1, 4};
    case ConsumableType::Spectral_Incantation:return {2, 4};
    case ConsumableType::Spectral_Talisman: return {3, 4};
    case ConsumableType::Spectral_Aura:     return {4, 4};
    case ConsumableType::Spectral_Wraith:   return {5, 4};
    case ConsumableType::Spectral_Sigil:    return {6, 4};
    case ConsumableType::Spectral_Ouija:    return {7, 4};
    case ConsumableType::Spectral_Ectoplasm:return {8, 4};
    case ConsumableType::Spectral_Immolate: return {9, 4};
    case ConsumableType::Spectral_Ankh:     return {0, 5};
    case ConsumableType::Spectral_DejaVu:   return {1, 5};
    case ConsumableType::Spectral_Hex:      return {2, 5};
    case ConsumableType::Spectral_Trance:   return {3, 5};
    case ConsumableType::Spectral_Medium:   return {4, 5};
    case ConsumableType::Spectral_Cryptid:  return {5, 5};
    case ConsumableType::Spectral_Soul:     return {2, 2};
    case ConsumableType::Spectral_BlackHole:return {9, 3};
    }
    return {0, 0};
}

static QPolygonF makeCrystalFace(const QPointF &top, const QPointF &right,
                                 const QPointF &bottom, const QPointF &left)
{
    QPolygonF poly;
    poly << top << right << bottom << left;
    return poly;
}

static void paintNegativeConsumableOverlay(QPainter *p)
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setCompositionMode(QPainter::CompositionMode_Screen);
    QLinearGradient g(0, 0, ConsumableItem::WIDTH, ConsumableItem::HEIGHT);
    g.setColorAt(0.00, QColor(45, 15, 75, 145));
    g.setColorAt(0.35, QColor(110, 45, 160, 115));
    g.setColorAt(0.70, QColor(15, 185, 210, 75));
    g.setColorAt(1.00, QColor(190, 80, 255, 95));
    p->fillRect(QRectF(0, 0, ConsumableItem::WIDTH, ConsumableItem::HEIGHT), g);
    p->setCompositionMode(QPainter::CompositionMode_SourceOver);
    p->setPen(QPen(QColor(220, 160, 255, 210), 3));
    p->setBrush(Qt::NoBrush);
    p->drawRoundedRect(3, 3, ConsumableItem::WIDTH - 6, ConsumableItem::HEIGHT - 6, 12, 12);
    p->restore();
}

static void paintSoulCrystal(QPainter *p)
{
    constexpr qreal kPi = 3.14159265358979323846;

    // 原版 card.lua：The Soul 不是画一个手工多边形，
    // 而是在卡牌中心层额外绘制 G.shared_soul。
    // game.lua: G.shared_soul = Sprite(..., ASSET_ATLAS["centers"], P_CENTERS.soul.pos)
    // P_CENTERS.soul.pos = {x=0,y=1}。
    // 本项目里 centers 对应资源是 Enhancers.png；如果资源存在，就直接裁切原版白水晶贴图。
    static QPixmap soulSheet(QStringLiteral(":/textures/images/Enhancers.png"));
    QPixmap soulSprite;
    if (!soulSheet.isNull()) {
        soulSprite = soulSheet.copy(0 * ConsumableItem::WIDTH,
                                    1 * ConsumableItem::HEIGHT,
                                    ConsumableItem::WIDTH,
                                    ConsumableItem::HEIGHT);
    }

    const qreal t = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    const qreal frac = t - std::floor(t);

    // 直接照原版参数公式转写：
    // scale_mod = 0.05 + 0.05*sin(1.8*t) + 0.07*sin(frac*pi*14)*(1-frac)^3
    // rotate_mod = 0.1*sin(1.219*t) + 0.07*sin(t*pi*5)*(1-frac)^2
    const qreal scaleMod = 0.05
        + 0.05 * std::sin(1.8 * t)
        + 0.07 * std::sin(frac * kPi * 14.0) * std::pow(1.0 - frac, 3.0);
    const qreal rotateModRad = 0.10 * std::sin(1.219 * t)
        + 0.07 * std::sin(t * kPi * 5.0) * std::pow(1.0 - frac, 2.0);

    // 原版 shared_soul 是整张牌尺寸的透明前景层，和牌面完全对齐。
    const QPointF center(ConsumableItem::WIDTH * 0.5, ConsumableItem::HEIGHT * 0.5);
    const qreal finalScale = 1.0 + scaleMod;
    const qreal finalRotateDeg = rotateModRad * 180.0 / kPi;

    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setRenderHint(QPainter::SmoothPixmapTransform, true);

    if (!soulSprite.isNull()) {
        // 第一层：近似原版 dissolve shader 的冷白光晕。
        p->save();
        p->setCompositionMode(QPainter::CompositionMode_Screen);
        QRadialGradient halo(center, ConsumableItem::HEIGHT * 0.48);
        halo.setColorAt(0.00, QColor(255, 255, 255, 115));
        halo.setColorAt(0.32, QColor(220, 240, 255, 72));
        halo.setColorAt(0.70, QColor(125, 190, 255, 24));
        halo.setColorAt(1.00, QColor(255, 255, 255, 0));
        p->setPen(Qt::NoPen);
        p->setBrush(halo);
        p->drawEllipse(center, ConsumableItem::WIDTH * 0.48, ConsumableItem::HEIGHT * 0.39);
        p->restore();

        // 第二层：绘制原版 shared_soul 贴图本体。
        p->save();
        p->translate(center);
        p->rotate(finalRotateDeg);
        p->scale(finalScale, finalScale);
        p->translate(-center);
        p->setOpacity(0.96);
        p->drawPixmap(QRectF(0, 0, ConsumableItem::WIDTH, ConsumableItem::HEIGHT), soulSprite,
                      QRectF(0, 0, soulSprite.width(), soulSprite.height()));
        p->restore();

        // 第三层：模拟第二次 dissolve 调用带来的闪白脉冲。
        p->save();
        p->setCompositionMode(QPainter::CompositionMode_Screen);
        p->translate(center);
        p->rotate(finalRotateDeg * 0.65);
        p->scale(finalScale, finalScale);
        p->translate(-center);
        const qreal sweep = -ConsumableItem::WIDTH * 0.60
            + std::fmod(t * 95.0, ConsumableItem::WIDTH * 1.55);
        QLinearGradient shine(QPointF(sweep, 0), QPointF(sweep + ConsumableItem::WIDTH * 0.40, ConsumableItem::HEIGHT));
        shine.setColorAt(0.00, QColor(255, 255, 255, 0));
        shine.setColorAt(0.48, QColor(255, 255, 255, 95));
        shine.setColorAt(0.55, QColor(255, 255, 255, 160));
        shine.setColorAt(1.00, QColor(255, 255, 255, 0));
        p->fillRect(QRectF(0, 0, ConsumableItem::WIDTH, ConsumableItem::HEIGHT), shine);
        p->restore();

        p->restore();
        return;
    }

    // 没找到 Enhancers.png 时的兜底：保持一个白蓝水晶，避免前景层消失。
    const QPointF top(center.x(), center.y() - 62);
    const QPointF upperL(center.x() - 30, center.y() - 20);
    const QPointF upperR(center.x() + 30, center.y() - 20);
    const QPointF mid(center.x(), center.y() + 6);
    const QPointF lowerL(center.x() - 22, center.y() + 34);
    const QPointF lowerR(center.x() + 22, center.y() + 34);
    const QPointF bottom(center.x(), center.y() + 70);

    p->translate(center);
    p->rotate(finalRotateDeg);
    p->scale(finalScale, finalScale);
    p->translate(-center);

    QPainterPath clip;
    clip.moveTo(top); clip.lineTo(upperR); clip.lineTo(lowerR); clip.lineTo(bottom);
    clip.lineTo(lowerL); clip.lineTo(upperL); clip.closeSubpath();

    QRadialGradient halo(center, 80);
    halo.setColorAt(0.0, QColor(255,255,255,175));
    halo.setColorAt(1.0, QColor(180,220,255,0));
    p->setPen(Qt::NoPen); p->setBrush(halo); p->drawEllipse(center, 76, 68);

    QLinearGradient g(top, bottom);
    g.setColorAt(0.0, QColor(255,255,255,245));
    g.setColorAt(0.5, QColor(230,246,255,215));
    g.setColorAt(1.0, QColor(145,205,255,195));
    p->setPen(QPen(QColor(255,255,255,230), 2.2));
    p->setBrush(g);
    p->drawPath(clip);
    p->setPen(QPen(QColor(255,255,255,135), 1.1));
    p->drawLine(top, mid); p->drawLine(mid, bottom); p->drawLine(upperL, mid); p->drawLine(upperR, mid);
    p->restore();
}


QPixmap ConsumableItem::renderPixmap(ConsumableType type, bool negative)
{
    if (!sSheet || sSheet->isNull()) {
        loadResources();
    }

    QPixmap pix(WIDTH, HEIGHT);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.setRenderHint(QPainter::Antialiasing, true);

    if (sSheet && !sSheet->isNull()) {
        QPoint c = spritePos(type);
        QRect src(c.x() * WIDTH, c.y() * HEIGHT, WIDTH, HEIGHT);
        p.drawPixmap(QRect(0, 0, WIDTH, HEIGHT), *sSheet, src);
    }

    // 原版 The Soul 不是单张平面图：背景牌面之外，额外绘制 G.shared_soul
    // 这个前景白水晶层必须出现在所有地方：仓库、商店、开包选项。
    if (type == ConsumableType::Spectral_Soul) {
        paintSoulCrystal(&p);
    }

    if (negative) {
        paintNegativeConsumableOverlay(&p);
    }

    return pix;
}

ConsumableItem::ConsumableItem(const Consumable &c, QGraphicsItem *parent)
    : QGraphicsObject(parent), mC(c)
{
    setAcceptHoverEvents(true);
    setCursor(Qt::PointingHandCursor);
    setToolTip(QString("%1\n%2\n左键: 使用    右键: 卖出 (+$%3)")
                   .arg(mC.name, mC.description).arg(mC.sellValue));

    if (mC.type == ConsumableType::Spectral_Soul) {
        auto *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this]() { update(); });
        timer->start(60);
    }
}

void ConsumableItem::paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) {
    p->setRenderHint(QPainter::SmoothPixmapTransform);
    p->drawPixmap(QRect(0, 0, WIDTH, HEIGHT), renderPixmap(mC.type, mC.negative));

    if (mHovered) {
        p->setPen(QPen(QColor(255, 240, 96, 220), 4));
        p->setBrush(Qt::NoBrush);
        p->drawRoundedRect(2, 2, WIDTH - 4, HEIGHT - 4, 10, 10);
    }
}

void ConsumableItem::mousePressEvent(QGraphicsSceneMouseEvent *e) {
    emit clicked(this, e->button());
    e->accept();
}
