#include "consumableitem.h"
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include "../utils/shadereffects.h"
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
        static QPixmap tarotSheet(QStringLiteral(":/textures/images/Tarots.png"));
        BalatroShaders::paintSoulCrystal(&p, QRectF(0, 0, WIDTH, HEIGHT), tarotSheet);
    }

    if (negative) {
        BalatroShaders::paintEdition(&p, QRectF(0, 0, WIDTH, HEIGHT), Edition::Negative);
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

    if (mC.type == ConsumableType::Spectral_Soul || mC.negative) {
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
