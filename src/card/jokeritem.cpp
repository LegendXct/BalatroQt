#include "jokeritem.h"
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QCursor>
#include <QPropertyAnimation>
#include <QVariantAnimation>
#include <QSequentialAnimationGroup>
#include <QRandomGenerator>
#include <QGraphicsSceneHoverEvent>
#include <QLineF>
#include <QDateTime>
#include <QTimer>
#include <QPainterPath>
#include <QCoreApplication>
#include <QSet>
#include <QHash>
#include <QStringList>
#include <cmath>
#include <QtMath>
#include "../audio/audiomanager.h"
#include "../utils/shadereffects.h"
#include "cardshadow.h"
#include "cardfloat.h"
#include <QGraphicsScene>

QPixmap *JokerItem::sSheet = nullptr;

static bool legendarySoulPos(JokerType t, QPoint &out);
static bool hologramSoulPos(JokerType t, QPoint &out);

namespace {
QSet<JokerItem*> sAnimatedJokers;
QTimer *sJokerShaderTimer = nullptr;

bool jokerNeedsShaderTick(const Joker &j)
{
    Q_UNUSED(j.edition);
    // 多彩/闪箔/镭射都按静态贴图缓存绘制；只有真正带 soul 浮动层的小丑需要刷新。
    // 这样多彩买进槽位后仍保留多彩外观，但不会像动画 shader 一样一直闪动。
    QPoint unused;
    return legendarySoulPos(j.type, unused) || hologramSoulPos(j.type, unused);
}

void ensureJokerShaderTimer()
{
    if (sJokerShaderTimer) return;
    sJokerShaderTimer = new QTimer(QCoreApplication::instance());
    sJokerShaderTimer->setTimerType(Qt::CoarseTimer);
    QObject::connect(sJokerShaderTimer, &QTimer::timeout, []() {
        const auto items = sAnimatedJokers.values();
        for (JokerItem *item : items) if (item) item->update();
    });
    // 100ms (10FPS)：原本 67ms 在装满 6 张小丑 + 多张带版本 shader 的演示局里
    // 触发肉眼可见的卡顿；降到 10FPS 几乎看不出动画差，但 CPU/GPU 负载减半。
    sJokerShaderTimer->start(100);
}

int shaderCacheFrame()
{
    // 与上面 100ms timer 对齐——shaderCacheFrame() 用于 paint() 的缓存 key，
    // 频率必须和 timer tick 一致，否则要么持续 cache miss、要么过度持有旧帧。
    return int(BalatroShaders::shaderTime() * 10.0);
}
}

static bool legendarySoulPos(JokerType t, QPoint &out)
{
    // 原版 game.lua：传奇小丑本体 pos 在第 8 行，前景 soul_pos 在第 9 行。
    // card.lua 会把 soul_pos 作为 floating_sprite 单独绘制，并持续做 dissolve shader 浮动。
    switch (t) {
    case JokerType::Caino:     out = {3, 9}; return true;
    case JokerType::Triboulet: out = {4, 9}; return true;
    case JokerType::Yorick:    out = {5, 9}; return true;
    case JokerType::Chicot:    out = {6, 9}; return true;
    case JokerType::Perkeo:    out = {7, 9}; return true;
    default: break;
    }
    return false;
}


static bool hologramSoulPos(JokerType t, QPoint &out)
{
    // 原版 game.lua: j_hologram soul_pos = {x=2, y=9}
    if (t == JokerType::Hologram) { out = {2, 9}; return true; }
    return false;
}

void JokerItem::drawFloatingSprite(QPainter *p, const QRectF &dst, JokerType type, bool animated)
{
    if (!p || !sSheet || sSheet->isNull()) return;

    QPoint soul;
    bool isLegendary = legendarySoulPos(type, soul);
    bool isHologram  = !isLegendary && hologramSoulPos(type, soul);
    if (!isLegendary && !isHologram) return;

    const qreal t = animated ? (QDateTime::currentMSecsSinceEpoch() / 1000.0) : 0.0;

    QRect src(soul.x() * JokerItem::SRC_W, soul.y() * JokerItem::SRC_H,
              JokerItem::SRC_W, JokerItem::SRC_H);

    if (isHologram) {
        const qreal scaleMod = 1.0 + 0.07 + 0.02 * std::sin(1.8 * t);
        const qreal rotateDeg = 0.05 * std::sin(1.219 * t) * 180.0 / 3.14159265358979323846;
        const qreal yBob = animated ? std::sin(t * 1.7) * 2.0 : 0.0;

        QPixmap sprite(JokerItem::SRC_W, JokerItem::SRC_H);
        sprite.fill(Qt::transparent);
        {
            QPainter sp(&sprite);
            sp.setRenderHint(QPainter::SmoothPixmapTransform, true);
            sp.drawPixmap(QRect(0, 0, JokerItem::SRC_W, JokerItem::SRC_H), *sSheet, src);
        }
        sprite = BalatroShaders::renderHologramPixmap(sprite, 1.2);

        p->save();
        p->setRenderHint(QPainter::SmoothPixmapTransform, true);
        p->translate(dst.center().x(), dst.center().y() + yBob);
        p->rotate(rotateDeg);
        p->scale(scaleMod, scaleMod);
        p->translate(-dst.width() / 2.0, -dst.height() / 2.0);
        p->drawPixmap(QRectF(0, 0, dst.width(), dst.height()), sprite, QRectF(0, 0, sprite.width(), sprite.height()));
        p->restore();
        return;
    }

    // 传奇小丑：scale_mod 较平缓，叠一层 dissolve 发光再画肖像。
    const qreal scaleMod = animated ? (1.07 + 0.02 * std::sin(1.8 * t)) : 1.0;
    const qreal rotateMod = animated ? (2.9 * std::sin(1.219 * t)) : 0.0;
    const qreal yBob = animated ? (2.6 * std::sin(1.65 * t)) : 0.0;

    auto drawLayer = [&](qreal scale, qreal opacity, QPainter::CompositionMode mode) {
        p->save();
        p->setRenderHint(QPainter::SmoothPixmapTransform, true);
        p->setCompositionMode(mode);
        p->setOpacity(opacity);
        p->translate(dst.center().x(), dst.center().y() + yBob);
        p->rotate(rotateMod);
        p->scale(scale, scale);
        p->translate(-dst.width() / 2.0, -dst.height() / 2.0);
        p->drawPixmap(QRectF(0, 0, dst.width(), dst.height()), *sSheet, src);
        p->restore();
    };

    drawLayer(scaleMod + 0.05, 0.40, QPainter::CompositionMode_Screen);
    drawLayer(scaleMod,        1.00, QPainter::CompositionMode_SourceOver);

}

static void paintHologramFloatingSprite(QPainter *p, QPixmap *sheet, JokerType type)
{
    Q_UNUSED(sheet);
    // 缓存绘制阶段使用原始图集尺寸；display 缩放在调用方完成。
    JokerItem::drawFloatingSprite(p, QRectF(0, 0, JokerItem::SRC_W, JokerItem::SRC_H), type, true);
}

static void paintLegendaryFloatingSprite(QPainter *p, QPixmap *sheet, JokerType type)
{
    Q_UNUSED(sheet);
    JokerItem::drawFloatingSprite(p, QRectF(0, 0, JokerItem::SRC_W, JokerItem::SRC_H), type, true);
}

void JokerItem::loadResources() {
    sSheet = new QPixmap(":/textures/images/Jokers.png");
    if (sSheet->isNull())
        qWarning("JokerItem: 加载 Jokers.png 失败");
}

// 坐标取自原版 game.lua 的 j_xxx pos = {x=?, y=?}
QPoint JokerItem::spritePos(JokerType t) {
    switch (t) {
    case JokerType::Joker:           return {0, 0};
    case JokerType::JollyJoker:      return {2, 0};
    case JokerType::ZanyJoker:       return {3, 0};
    case JokerType::MadJoker:        return {4, 0};
    case JokerType::CrazyJoker:      return {5, 0};
    case JokerType::DrollJoker:      return {6, 0};
    case JokerType::HalfJoker:       return {7, 0};
    case JokerType::GreedyJoker:     return {6, 1};
    case JokerType::LustyJoker:      return {7, 1};
    case JokerType::WrathfulJoker:   return {8, 1};
    case JokerType::GluttonousJoker: return {9, 1};
    case JokerType::GoldenJoker:     return {9, 2};
    case JokerType::ToDoList:        return {4, 11};
    case JokerType::SlyJoker:        return {0, 14};
    case JokerType::WilyJoker:       return {1, 14};
    case JokerType::CleverJoker:     return {2, 14};
    case JokerType::DeviousJoker:    return {3, 14};
    case JokerType::CraftyJoker:     return {4, 14};
    case JokerType::Banner:          return {1,  2};
    case JokerType::MysticSummit:    return {2,  2};
    case JokerType::Misprint:        return {6,  2};
    case JokerType::RaisedFist:      return {8,  2};
    case JokerType::Fibonacci:       return {1,  5};
    case JokerType::EvenSteven:      return {8,  3};
    case JokerType::OddTodd:         return {9,  3};
    case JokerType::Scholar:         return {0,  4};
    case JokerType::Bull:            return {7, 14};
    case JokerType::Bootstraps:      return {9,  8};
    case JokerType::AbstractJoker:   return {3,  3};
    case JokerType::Supernova:       return {4,  2};
    case JokerType::GrosMichel:      return {7,  6};
    case JokerType::Cavendish:       return {5, 11};
    case JokerType::IceCream:        return {4, 10};
    case JokerType::Stuntman:        return {8,  6};
    case JokerType::TheDuo:          return {5,  4};
    case JokerType::TheTrio:         return {6,  4};
    case JokerType::TheFamily:       return {7,  4};
    case JokerType::TheOrder:        return {8,  4};
    case JokerType::TheTribe:        return {9,  4};
    case JokerType::Blackboard:      return {2, 10};
    case JokerType::ScaryFace:       return {2,  3};
    case JokerType::SmileyFace:      return {6, 15};
    case JokerType::WalkieTalkie:    return {8, 15};
    case JokerType::Arrowhead:       return {1,  8};
    case JokerType::OnyxAgate:       return {2,  8};
    case JokerType::RoughGem:        return {9,  7};
    case JokerType::Bloodstone:      return {0,  8};
    case JokerType::ShootTheMoon:    return {2,  6};
    case JokerType::Baron:           return {6, 12};
    case JokerType::FlowerPot:       return {0,  6};
    case JokerType::Acrobat:         return {2,  1};
    case JokerType::Swashbuckler:    return {9,  5};
    case JokerType::Ramen:           return {2, 15};
    case JokerType::DriversLicense:  return {0,  7};
    case JokerType::Hiker:           return {0, 11};
    case JokerType::CardSharp:       return {6, 11};
    case JokerType::Hologram:        return {4, 12};
    case JokerType::MidasMask:       return {0, 13};
    case JokerType::Vampire:         return {2, 12};
    case JokerType::Constellation:   return {9, 10};
    case JokerType::Photograph:      return {2, 13};
    case JokerType::HangingChad:     return {9,  6};
    case JokerType::SockAndBuskin:   return {3,  1};
    case JokerType::Blueprint:       return {0,  3};
    case JokerType::Mime:            return {4,  1};
    case JokerType::DNA:             return {5, 10};
    case JokerType::Brainstorm:      return {7,  7};
    case JokerType::Caino:           return {3,  8};
    case JokerType::Triboulet:       return {4,  8};
    case JokerType::Yorick:          return {5,  8};
    case JokerType::Chicot:          return {6,  8};
    case JokerType::Perkeo:          return {7,  8};
    case JokerType::JokerStencil:    return {2,  5};
    case JokerType::SteelJoker:      return {7,  2};
    case JokerType::StoneJoker:      return {9,  0};
    case JokerType::BlueJoker:       return {7, 10};
    case JokerType::Erosion:         return {5, 13};
    case JokerType::BusinessCard:    return {1,  4};
    case JokerType::FacelessJoker:   return {1, 11};
    case JokerType::Cloud9:          return {7, 12};
    case JokerType::GoldenTicket:    return {5,  3};
    case JokerType::SeeingDouble:    return {4,  4};
    case JokerType::SquareJoker:     return {9, 11};
    case JokerType::Runner:          return {3, 10};
    case JokerType::Castle:          return {9, 15};
    case JokerType::GreenJoker:      return {2, 11};
    case JokerType::Obelisk:         return {9, 12};
    case JokerType::RideTheBus:      return {1,  6};
    case JokerType::SpareTrousers:   return {4, 15};
    case JokerType::WeeJoker:        return {0,  0};
    case JokerType::HitTheRoad:      return {8,  5};
    case JokerType::GlassJoker:      return {1,  3};
    case JokerType::LuckyCat:        return {5, 14};
    case JokerType::Popcorn:         return {1, 15};
    case JokerType::Juggler:         return {0,  1};
    case JokerType::Drunkard:        return {1,  1};
    case JokerType::MerryAndy:       return {8,  0};
    case JokerType::Troubadour:      return {0,  2};
    case JokerType::DelayedGratification: return {4, 3};
    case JokerType::ToTheMoon:       return {8, 13};
    case JokerType::ReservedParking: return {6, 13};
    case JokerType::MailInRebate:    return {7, 13};
    case JokerType::AncientJoker:    return {7, 15};
    case JokerType::TheIdol:         return {6,  7};
    case JokerType::SpaceJoker:      return {3,  5};
    case JokerType::Hack:            return {5,  2};
    case JokerType::RiffRaff:        return {1, 12};
    case JokerType::MarbleJoker:     return {3,  2};
    case JokerType::Burglar:         return {1, 10};
    case JokerType::Cartomancer:     return {7,  3};
    case JokerType::Certificate:     return {8,  8};
    case JokerType::Madness:         return {8, 11};
    case JokerType::EightBall:       return {0,  5};
    case JokerType::Seance:          return {0, 12};
    case JokerType::Vagabond:        return {5, 12};
    case JokerType::Superposition:   return {3, 11};
    case JokerType::FlashCard:       return {0, 15};
    case JokerType::Throwback:       return {5,  7};
    case JokerType::Campfire:        return {5, 15};
    case JokerType::FortuneTeller:   return {7,  5};
    case JokerType::LoyaltyCard:     return {4,  2};
    case JokerType::Egg:             return {0, 10};
    case JokerType::Rocket:          return {8, 12};
    case JokerType::Satellite:       return {8,  7};
    case JokerType::GiftCard:        return {3, 13};
    case JokerType::Shortcut:        return {3, 12};
    case JokerType::SmearedJoker:    return {4,  6};
    case JokerType::Splash:          return {6, 10};
    case JokerType::Showman:         return {6,  5};
    case JokerType::Dusk:            return {4,  7};
    case JokerType::CeremonialDagger:return {5,  5};
    case JokerType::TurtleBean:      return {4, 13};
    case JokerType::Seltzer:         return {3, 15};
    case JokerType::BurntJoker:      return {3,  7};
    case JokerType::ChaosTheClown:   return {1,  0};
    case JokerType::Pareidolia:      return {6,  3};
    case JokerType::Hallucination:   return {9, 13};
    case JokerType::Luchador:        return {1, 13};
    case JokerType::InvisibleJoker:  return {1,  7};
    case JokerType::CreditCard:      return {5,  1};
    case JokerType::MrBones:         return {3,  4};
    case JokerType::DietCola:        return {8, 14};
    case JokerType::FourFingers:     return {6,  6};
    case JokerType::OopsAllSixes:    return {5,  6};
    }
    return {0, 0};
}

JokerItem::JokerItem(const Joker &j, QGraphicsItem *parent)
    : QGraphicsObject(parent), mJoker(j)
{
    setAcceptHoverEvents(true);
    setCursor(Qt::PointingHandCursor);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    mShadow = new CardShadowItem(WIDTH, HEIGHT, [this]() { return mShadowLift; });
    mShadow->setZValue(-1000.0);
    // 异形小丑：把阴影几何收紧到真实可见区域，避免在透明像素下方还印一块矩形阴影。
    switch (j.type) {
    case JokerType::HalfJoker:
        // 美术图里上半部分是脸、下半部分被撕成碎条——撕碎部分的阴影按高度的 70% 收。
        mShadow->setVisibleRect(QRectF(0, 0, WIDTH, HEIGHT * 0.70));
        break;
    case JokerType::WeeJoker:
        // 原版 Wee Joker 在卡面里实际只占约中央 70% 区域。
        mShadow->setVisibleRect(QRectF(WIDTH * 0.15, HEIGHT * 0.15, WIDTH * 0.70, HEIGHT * 0.70));
        break;
    case JokerType::SquareJoker:
        // 方块小丑：原版做成方形（h ≈ w），阴影也按方形收。
        mShadow->setVisibleRect(QRectF(0, HEIGHT * 0.15, WIDTH, WIDTH));
        break;
    default: break;
    }

    if (jokerNeedsShaderTick(mJoker)) {
        ensureJokerShaderTimer();
        sAnimatedJokers.insert(this);
        QObject::connect(this, &QObject::destroyed, [ptr = this]() { sAnimatedJokers.remove(ptr); });
    }

    mFloatPhase = QRandomGenerator::global()->generateDouble() * 6.2831853;
    CardFloat::add(this, [this](double t) { updateAmbientFloat(t); });
}

JokerItem::~JokerItem()
{
    CardFloat::remove(this);
    if (mShadow) {
        if (auto *s = mShadow->scene()) s->removeItem(mShadow);
        delete mShadow;
        mShadow = nullptr;
    }
}

void JokerItem::updateAmbientFloat(double t)
{
    const bool idle = !mHovered && !mPressed && !mDragging;
    if (!idle || !isVisible()) {
        if (mAmbientTiltX != 0.0 || mAmbientTiltY != 0.0) {
            mAmbientTiltX = 0.0;
            mAmbientTiltY = 0.0;
            applyHoverTransform();
        }
        return;
    }
    const double a = t * 1.6 + mFloatPhase;
    mAmbientTiltX = 2.2 * std::sin(a);
    mAmbientTiltY = 2.2 * std::cos(a * 0.92 + 1.3);
    applyHoverTransform();
}

QVariant JokerItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemSceneHasChanged) {
        QGraphicsScene *s = scene();
        if (s && mShadow && mShadow->scene() != s) s->addItem(mShadow);
        if (!s && mShadow && mShadow->scene())
            mShadow->scene()->removeItem(mShadow);
    } else if (mShadow) {
        switch (change) {
        case ItemPositionHasChanged:   mShadow->setPos(value.toPointF()); break;
        case ItemRotationHasChanged:   mShadow->setRotation(value.toReal()); break;
        case ItemScaleHasChanged:      mShadow->setScale(value.toReal()); break;
        // 不同步 ItemTransformHasChanged——applyHoverTransform 的透视矩阵不应该套到阴影上。
        case ItemOpacityHasChanged:    mShadow->setOpacity(value.toReal()); break;
        case ItemVisibleHasChanged:    mShadow->setVisible(value.toBool()); break;
        case ItemZValueHasChanged:     updateShadowZ(); break;
        default: break;
        }
    }
    return QGraphicsObject::itemChange(change, value);
}

void JokerItem::updateShadowZ()
{
    if (!mShadow) return;
    // 阴影永远 z=-1000，保证不与邻牌本体重叠（详见 CardItem::updateShadowZ 注释）。
    mShadow->setZValue(-1000.0);
}

QRectF JokerItem::boundingRect() const {
    // 阴影现在是 sibling CardShadowItem，独立绘制；本 item 的 boundingRect 收紧到
    // 牌面本体，确保 hover hit-test 只覆盖可见牌面（避免 hover 在阴影区不消失）。
    return QRectF(0, 0, WIDTH, HEIGHT);
}

// 计算 sprite 不透明区域的垂直中心相对 cell 中心的偏移（SRC px，正=内容偏上需下移）。
// 异形小丑（半张/微缩等）art 常画在 cell 上半部，据此把可见内容整体居中。
static qreal jokerContentDySrc(const QPixmap &pix)
{
    const QImage img = pix.toImage().convertToFormat(QImage::Format_ARGB32);
    int minY = img.height(), maxY = -1;
    for (int y = 0; y < img.height(); ++y) {
        const QRgb *row = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            if (qAlpha(row[x]) > 8) { if (y < minY) minY = y; maxY = y; break; }
        }
    }
    if (maxY < minY) return 0.0;
    return img.height() / 2.0 - (minY + maxY) / 2.0;
}

void JokerItem::paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) {
    p->setRenderHint(QPainter::SmoothPixmapTransform, false);

    // 阴影由 mShadow（sibling CardShadowItem）单独绘制——z=-1000 落到所有牌之下。

    const bool floatingAnimated = jokerNeedsShaderTick(mJoker);
    const int frame = floatingAnimated ? shaderCacheFrame() : -1;
    const QString key = QString::number(int(mJoker.type)) + QLatin1Char('|')
                      + QString::number(int(mJoker.edition)) + QLatin1Char('|')
                      + QString::number(mJoker.isDebuffed ? 1 : 0) + QLatin1Char('|')
                      + QString::number(frame);
    static QHash<QString, QPixmap> cache;
    static QStringList order;

    QPixmap pix = cache.value(key);
    if (pix.isNull()) {
        // 缓存按图集原始 142×190 渲染，paint() 时再缩放到场景显示尺寸。
        pix = QPixmap(SRC_W, SRC_H);
        pix.fill(Qt::transparent);
        QPainter cp(&pix);
        cp.setRenderHint(QPainter::SmoothPixmapTransform, false);
        cp.setRenderHint(QPainter::Antialiasing, true);
        QPoint c = spritePos(mJoker.type);
        QRect src(c.x() * SRC_W, c.y() * SRC_H, SRC_W, SRC_H);
        QPixmap body = sSheet->copy(src);
        if (mJoker.edition != Edition::None)
            body = BalatroShaders::renderEditionPixmap(body, mJoker.edition);
        if (mJoker.isDebuffed)
            body = BalatroShaders::renderDebuffedPixmap(body);
        cp.drawPixmap(QRect(0, 0, SRC_W, SRC_H), body);
        paintLegendaryFloatingSprite(&cp, sSheet, mJoker.type);
        paintHologramFloatingSprite(&cp, sSheet, mJoker.type);
        cache.insert(key, pix);
        order.append(key);
        while (order.size() > 160) cache.remove(order.takeFirst());
    }
    // 外形变了（type/edition/debuff）才重算：垂直居中偏移 + 阴影黑色剪影。
    const QString silKey = QString::number(int(mJoker.type)) + QLatin1Char('|')
                         + QString::number(int(mJoker.edition)) + QLatin1Char('|')
                         + QString::number(mJoker.isDebuffed ? 1 : 0);
    if (silKey != mShadowSilKey) {
        const qreal dySrc = jokerContentDySrc(pix);
        mContentDyScreen = dySrc * qreal(HEIGHT) / qreal(SRC_H);
        if (mShadow) {
            // 剪影也按同样的下移量平移，保证阴影与居中后的 sprite 对齐。
            QPixmap silSrc = pix;
            if (qAbs(dySrc) >= 1.0) {
                silSrc = QPixmap(pix.size());
                silSrc.fill(Qt::transparent);
                QPainter sp(&silSrc);
                sp.drawPixmap(QPointF(0, dySrc), pix);
            }
            mShadow->setSilhouette(CardShadowItem::makeSilhouette(silSrc));
        }
        mShadowSilKey = silKey;
    }

    p->setRenderHint(QPainter::SmoothPixmapTransform, true);
    // 异形小丑：直接画完整 sprite（原版美术里 j_half / j_wee 等本身就把"撕碎/微缩"画进卡面），
    // 并按实际内容垂直居中（下移 mContentDyScreen），不再贴在槽位最上方。
    p->drawPixmap(QRectF(0, mContentDyScreen, WIDTH, HEIGHT), pix, QRectF(0, 0, SRC_W, SRC_H));

    // 不再画 hover 选中轮廓——它是固定的 WIDTH×HEIGHT 圆角矩形，与异形小丑大小对不上。
}

void JokerItem::mousePressEvent(QGraphicsSceneMouseEvent *e)
{
    if (e->button() == Qt::LeftButton || e->button() == Qt::RightButton) {
        mPressed = true;
        mDragging = false;
        // 按下立即隐藏悬停描述（原版 grab 时 h_popup 消失），不必等到开始拖动。
        emit hoverChanged(this, false);
        mHoverTiltX = 0.0;
        mHoverTiltY = 0.0;
        mDragTilt = 0.0;
        mLastDragScenePos = e->scenePos();
        mLastDragTimeMs = QDateTime::currentMSecsSinceEpoch();
        applyHoverTransform();
        mPressScenePos = e->scenePos();
        mRestZ = zValue();
        updateShadowZ();
        animateShadowLift(currentShadowTarget(), 100);
        e->accept();
        return;
    }
    QGraphicsObject::mousePressEvent(e);
}

void JokerItem::mouseMoveEvent(QGraphicsSceneMouseEvent *e)
{
    if (!mPressed) {
        QGraphicsObject::mouseMoveEvent(e);
        return;
    }
    if (!mDragging && QLineF(e->scenePos(), mPressScenePos).length() > 7.0) {
        mDragging = true;
        setZValue(650);
        updateShadowZ();
        mLastDragScenePos = e->scenePos();
        mLastDragTimeMs = QDateTime::currentMSecsSinceEpoch();
        animateShadowLift(currentShadowTarget(), 120);
    }

    if (mDragging) {
        // 拖拽水平速度倾斜——参数与 CardItem::mouseMoveEvent 保持一致（用户反馈9：
        // 小丑/塔罗/星球也要有这种甩动倾斜）。
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        const qint64 dtMs = qMax<qint64>(1, nowMs - mLastDragTimeMs);
        const double dx = e->scenePos().x() - mLastDragScenePos.x();
        const double vxPerSec = dx * 1000.0 / double(dtMs);
        double desTilt = vxPerSec * 0.022;
        if (desTilt > 15.0) desTilt = 15.0;
        if (desTilt < -15.0) desTilt = -15.0;
        mDragTilt = mDragTilt * 0.65 + desTilt * 0.35;
        mLastDragScenePos = e->scenePos();
        mLastDragTimeMs = nowMs;
        setPos(e->scenePos() - QPointF(WIDTH / 2.0, HEIGHT / 2.0));
        // 旋转使用 setRotation，围绕 transformOriginPoint（中心）。
        setTransformOriginPoint(WIDTH / 2.0, HEIGHT / 2.0);
        setRotation(mDragTilt + mMoveTilt);
        emit dragMoved(this, e->scenePos());
        e->accept();
        return;
    }
    QGraphicsObject::mouseMoveEvent(e);
}

void JokerItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *e)
{
    if ((e->button() == Qt::LeftButton || e->button() == Qt::RightButton) && mPressed) {
        mPressed = false;
        if (mDragging) {
            mDragging = false;
            // 平滑把拖拽倾斜衰减回 0。
            if (qFuzzyIsNull(mDragTilt)) {
                mDragTilt = 0.0;
                setRotation(0.0);
            } else {
                auto *decay = new QVariantAnimation(this);
                decay->setDuration(220);
                decay->setStartValue(mDragTilt);
                decay->setEndValue(0.0);
                decay->setEasingCurve(QEasingCurve::OutCubic);
                connect(decay, &QVariantAnimation::valueChanged, this,
                        [this](const QVariant &v) {
                    mDragTilt = v.toDouble();
                    setRotation(mDragTilt + mMoveTilt);
                });
                decay->start(QAbstractAnimation::DeleteWhenStopped);
            }
            emit dragReleased(this, e->scenePos());
        } else {
            emit pressed(this, e->button());
            if (e->button() == Qt::LeftButton) emit clicked(this);
        }
        animateShadowLift(currentShadowTarget(), 160);
        updateShadowZ();
        e->accept();
        return;
    }
    QGraphicsObject::mouseReleaseEvent(e);
}

void JokerItem::applyHoverTransform()
{
    const qreal cx = WIDTH / 2.0;
    const qreal cy = HEIGHT / 2.0;
    const qreal tiltX = qDegreesToRadians(mHoverTiltX + mAmbientTiltX);
    const qreal tiltY = qDegreesToRadians(mHoverTiltY + mAmbientTiltY);

    const qreal cosY = std::cos(tiltY);
    const qreal cosX = std::cos(tiltX);
    const qreal sinY = std::sin(tiltY);
    const qreal sinX = std::sin(tiltX);

    QTransform t;
    t.translate(cx, cy);
    QTransform persp;
    persp.setMatrix(
        cosY,           sinY * sinX,    0.0048 * sinY,
        0,              cosX,           0.0048 * sinX,
        0,              0,              1
    );
    t = persp * t;
    t.translate(-cx, -cy);
    setTransform(t);
}

void JokerItem::hoverEnterEvent(QGraphicsSceneHoverEvent *e)
{
    mHovered = true;
    AudioManager::instance()->play(QStringLiteral("paper1"),
                                   0.9 + QRandomGenerator::global()->generateDouble() * 0.2,
                                   0.35);
    setTransformOriginPoint(WIDTH / 2.0, HEIGHT / 2.0);
    animateScale(1.08, 100);
    animateShadowLift(currentShadowTarget(), 120);
    triggerHoverJitter();
    emit hoverChanged(this, true);
    update();
    QGraphicsObject::hoverEnterEvent(e);
}

void JokerItem::triggerHoverJitter()
{
    if (mDragging) return; // 拖拽时不抖
    const double dir = (QRandomGenerator::global()->bounded(2) == 0) ? -1.0 : 1.0;
    const double peakRot   = 2.4 * dir;
    const double overshoot = -0.6 * dir;

    auto *rotOut = new QPropertyAnimation(this, "rotation");
    rotOut->setDuration(70);
    rotOut->setStartValue(0.0);
    rotOut->setEndValue(peakRot);
    rotOut->setEasingCurve(QEasingCurve::OutQuad);

    auto *rotBack = new QPropertyAnimation(this, "rotation");
    rotBack->setDuration(110);
    rotBack->setStartValue(peakRot);
    rotBack->setEndValue(overshoot);
    rotBack->setEasingCurve(QEasingCurve::InOutQuad);

    auto *rotSettle = new QPropertyAnimation(this, "rotation");
    rotSettle->setDuration(120);
    rotSettle->setStartValue(overshoot);
    rotSettle->setEndValue(0.0);
    rotSettle->setEasingCurve(QEasingCurve::OutCubic);

    auto *seq = new QSequentialAnimationGroup(this);
    seq->addAnimation(rotOut);
    seq->addAnimation(rotBack);
    seq->addAnimation(rotSettle);
    seq->start(QAbstractAnimation::DeleteWhenStopped);
}

void JokerItem::hoverMoveEvent(QGraphicsSceneHoverEvent *e)
{
    if (!mHovered || mDragging) {
        QGraphicsObject::hoverMoveEvent(e);
        return;
    }
    QGraphicsObject::hoverMoveEvent(e);
}

void JokerItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *e)
{
    mHovered = false;
    mHoverTiltX = 0.0;
    mHoverTiltY = 0.0;
    applyHoverTransform();
    animateScale(1.0, 110);
    animateShadowLift(currentShadowTarget(), 160);
    emit hoverChanged(this, false);
    update();
    QGraphicsObject::hoverLeaveEvent(e);
}

void JokerItem::animateScale(qreal target, int durationMs)
{
    auto *anim = new QPropertyAnimation(this, "scale", this);
    anim->setDuration(durationMs);
    anim->setStartValue(scale());
    anim->setEndValue(target);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}


void JokerItem::moveTo(const QPointF &target, int durationMs)
{
    const QPointF from = pos();
    auto *anim = new QPropertyAnimation(this, "pos", this);
    anim->setDuration(durationMs);
    anim->setStartValue(from);
    anim->setEndValue(target);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);

    // 重排倾斜（对齐原版 Moveable:move_r：des_r ∝ 水平速度）：被挤动而横向滑动时朝运动方向
    // 倾斜，随到位而回正。不被拖动的牌也因此有"甩"的动感，不再只有被拖那张倾斜。
    if (mDragging) return;   // 正在被拖动的牌用 mDragTilt，别叠加
    const double dx = target.x() - from.x();
    double tiltMax = dx * 0.09;
    if (tiltMax > 16.0) tiltMax = 16.0;
    if (tiltMax < -16.0) tiltMax = -16.0;
    if (qAbs(tiltMax) < 0.2) return;
    setTransformOriginPoint(WIDTH / 2.0, HEIGHT / 2.0);
    auto *tilt = new QVariantAnimation(this);
    // 倾斜衰减时长与移动时长解耦：重排 moveTo 常用 60ms 小步移动，倾斜也只 60ms 就一闪而过；
    // 固定 ~300ms 让"被挤走→倾斜→回正"清晰可见。
    tilt->setDuration(qMax(durationMs, 300));
    tilt->setStartValue(tiltMax);
    tilt->setEndValue(0.0);
    tilt->setEasingCurve(QEasingCurve::OutCubic);
    connect(tilt, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        mMoveTilt = v.toDouble();
        setRotation(mDragTilt + mMoveTilt);
    });
    connect(tilt, &QVariantAnimation::finished, this, [this]() {
        mMoveTilt = 0.0;
        setRotation(mDragTilt + mMoveTilt);
    });
    tilt->start(QAbstractAnimation::DeleteWhenStopped);
}

void JokerItem::juiceUp(double scaleAmount, int durationMs)
{
    setTransformOriginPoint(WIDTH / 2.0, HEIGHT / 2.0);

    auto *up = new QPropertyAnimation(this, "scale");
    up->setDuration(durationMs / 2);
    up->setStartValue(1.0);
    up->setEndValue(scaleAmount);

    auto *down = new QPropertyAnimation(this, "scale");
    down->setDuration(durationMs / 2);
    down->setStartValue(scaleAmount);
    down->setEndValue(1.0);

    auto *seq = new QSequentialAnimationGroup(this);
    seq->addAnimation(up);
    seq->addAnimation(down);
    up->setParent(seq); down->setParent(seq);
    seq->start(QAbstractAnimation::DeleteWhenStopped);
}

void JokerItem::setShadowLift(qreal v)
{
    v = qBound(0.0, v, 1.0);
    if (qFuzzyCompare(v + 1.0, mShadowLift + 1.0)) return;
    mShadowLift = v;
    if (mShadow) mShadow->update();
}

void JokerItem::setScoringLifted(bool lifted)
{
    if (mScoringLifted == lifted) return;
    mScoringLifted = lifted;
    updateShadowZ();
    animateShadowLift(currentShadowTarget(), 180);
}

qreal JokerItem::currentShadowTarget() const
{
    if (mScoringLifted) return 1.0;
    if (mDragging)      return 0.85;
    if (mPressed)       return 0.55;
    if (mHovered)       return 0.30;
    return 0.0;
}

void JokerItem::animateShadowLift(qreal target, int durationMs)
{
    auto *anim = findChild<QPropertyAnimation*>(QStringLiteral("JokerShadowAnim"),
                                                 Qt::FindDirectChildrenOnly);
    if (!anim) {
        anim = new QPropertyAnimation(this, "shadowLift", this);
        anim->setObjectName(QStringLiteral("JokerShadowAnim"));
        anim->setEasingCurve(QEasingCurve::OutCubic);
    } else if (anim->state() == QAbstractAnimation::Running) {
        anim->stop();
    }
    anim->setDuration(durationMs);
    anim->setStartValue(mShadowLift);
    anim->setEndValue(qBound(0.0, target, 1.0));
    anim->start();
}
