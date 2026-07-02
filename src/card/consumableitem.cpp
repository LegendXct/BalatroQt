#include "consumableitem.h"
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QtMath>
#include <QPropertyAnimation>
#include <QVariantAnimation>
#include <QSequentialAnimationGroup>
#include <QRandomGenerator>
#include <QLineF>
#include <QGraphicsSceneHoverEvent>
#include "../utils/balatromotion.h"
#include "../utils/shadereffects.h"
#include "cardshadow.h"
#include <QGraphicsScene>
#include <QCursor>
#include <QTimer>
#include <QDateTime>
#include <QElapsedTimer>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QCoreApplication>
#include <QSet>
#include <QHash>
#include <QStringList>
#include <cmath>
#include "../audio/audiomanager.h"

QPixmap *ConsumableItem::sSheet = nullptr;

namespace {
QSet<ConsumableItem*> sAnimatedConsumables;
QTimer *sConsumableShaderTimer = nullptr;
QSet<ConsumableItem*> sAmbientConsumables;
QTimer *sConsumableAmbientTimer = nullptr;
QElapsedTimer sConsumableAmbientClock;

bool consumableNeedsShaderTick(const Consumable &c)
{
    return c.type == ConsumableType::Spectral_Soul;
}

void ensureConsumableShaderTimer()
{
    if (sConsumableShaderTimer) return;
    sConsumableShaderTimer = new QTimer(QCoreApplication::instance());
    sConsumableShaderTimer->setTimerType(Qt::CoarseTimer);
    QObject::connect(sConsumableShaderTimer, &QTimer::timeout, []() {
        const auto items = sAnimatedConsumables.values();
        for (ConsumableItem *item : items) if (item) item->update();
    });
    sConsumableShaderTimer->start(67);
}
}

void ConsumableItem::loadResources() {
    sSheet = new QPixmap(":/textures/images/Tarots.png");
    if (sSheet->isNull()) qWarning("ConsumableItem: 加载 Tarots.png 失败");
}

namespace {
// 程设扩展塔罗（迭代器/浅拷贝）的专属整卡贴图（142×190）；
// 用图集愚者格 (0,0) 的 alpha 裁圆角，轮廓与其它塔罗一致。非扩展类型返回空。
QPixmap customConsumablePixmap(ConsumableType t, const QPixmap *sheet)
{
    const char *res = nullptr;
    switch (t) {
    case ConsumableType::Tarot_Iterator:    res = ":/textures/images/tarot_cs_iterator.png"; break;
    case ConsumableType::Tarot_ShallowCopy: res = ":/textures/images/tarot_cs_shallow.png";  break;
    default: return QPixmap();
    }
    static QHash<int, QPixmap> cache;
    const auto it = cache.constFind(int(t));
    if (it != cache.constEnd()) return *it;

    QPixmap cell(ConsumableItem::SRC_W, ConsumableItem::SRC_H);
    cell.fill(Qt::transparent);
    const QPixmap card{QString::fromLatin1(res)};
    if (!card.isNull() && sheet && !sheet->isNull()) {
        QPainter p(&cell);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.drawPixmap(QRect(0, 0, ConsumableItem::SRC_W, ConsumableItem::SRC_H), card);
        p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        p.drawPixmap(QRect(0, 0, ConsumableItem::SRC_W, ConsumableItem::SRC_H), *sheet,
                     QRect(0, 0, ConsumableItem::SRC_W, ConsumableItem::SRC_H));
    }
    cache.insert(int(t), cell);
    return cell;
}
}

// 坐标取自原版 game.lua c_xxx pos
QPoint ConsumableItem::spritePos(ConsumableType t) {
    switch (t) {
    // 程设扩展：专属整卡贴图见 renderPixmap 的 custom 分支，不在图集里。
    case ConsumableType::Tarot_Iterator:
    case ConsumableType::Tarot_ShallowCopy:   return {0, 0};
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

    const bool animated = (type == ConsumableType::Spectral_Soul) || negative;
    const int frame = animated ? int(BalatroShaders::shaderTime() * 15.0) : -1;
    const QString key = QString::number(int(type)) + QLatin1Char('|')
                      + QString::number(negative ? 1 : 0) + QLatin1Char('|')
                      + QString::number(frame);

    static QHash<QString, QPixmap> cache;
    static QStringList order;
    QPixmap pix = cache.value(key);
    if (!pix.isNull()) return pix;

    // 缓存按图集原始 142×190 渲染，paint() / 调用方再缩放到目标尺寸。
    pix = QPixmap(SRC_W, SRC_H);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QPixmap custom = customConsumablePixmap(type, sSheet);
    if (!custom.isNull()) {
        p.drawPixmap(QRect(0, 0, SRC_W, SRC_H), custom);
    } else if (sSheet && !sSheet->isNull()) {
        QPoint c = spritePos(type);
        QRect src(c.x() * SRC_W, c.y() * SRC_H, SRC_W, SRC_H);
        p.drawPixmap(QRect(0, 0, SRC_W, SRC_H), *sSheet, src);
    }

    p.end();

    if (negative) {
        pix = BalatroShaders::renderEditionPixmap(pix, Edition::Negative);
    }

    if (type == ConsumableType::Spectral_Soul) {
        QPainter soulPainter(&pix);
        soulPainter.setRenderHint(QPainter::SmoothPixmapTransform, false);
        soulPainter.setRenderHint(QPainter::Antialiasing, true);
        static QPixmap enhSheet(QStringLiteral(":/textures/images/Enhancers.png"));
        BalatroShaders::paintSoulCrystal(&soulPainter, QRectF(0, 0, SRC_W, SRC_H), enhSheet);
    }

    cache.insert(key, pix);
    order.append(key);
    while (order.size() > 96) cache.remove(order.takeFirst());
    return pix;
}

ConsumableItem::ConsumableItem(const Consumable &c, QGraphicsItem *parent)
    : QGraphicsObject(parent), mC(c)
{
    setAcceptHoverEvents(true);
    setCursor(Qt::PointingHandCursor);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    mShadow = new CardShadowItem(WIDTH, HEIGHT, [this]() { return mShadowLift; });
    mShadow->setZValue(-1000.0);
    mAmbientId = BalatroMotion::nextCardLikeId();
    ensureAmbientTimer();
    sAmbientConsumables.insert(this);
    // 不再用 Qt 原生 QToolTip——它会和 MainWindow::mHoverTooltip (BalatroInfoPanel)
    // 同时弹出，造成"暗色 info + 浅色系统 tooltip"重叠。统一由场景级浮窗处理。

    if (consumableNeedsShaderTick(mC)) {
        ensureConsumableShaderTimer();
        sAnimatedConsumables.insert(this);
        QObject::connect(this, &QObject::destroyed, [ptr = this]() { sAnimatedConsumables.remove(ptr); });
    }
}

ConsumableItem::~ConsumableItem()
{
    sAnimatedConsumables.remove(this);
    sAmbientConsumables.remove(this);
    if (mShadow) {
        if (auto *s = mShadow->scene()) s->removeItem(mShadow);
        delete mShadow;
        mShadow = nullptr;
    }
}

QVariant ConsumableItem::itemChange(GraphicsItemChange change, const QVariant &value)
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
        // 不同步 ItemTransformHasChanged——透视矩阵不应该套到阴影上，否则 hover 时阴影漂移。
        case ItemOpacityHasChanged:    mShadow->setOpacity(value.toReal()); break;
        case ItemVisibleHasChanged:    mShadow->setVisible(value.toBool()); break;
        case ItemZValueHasChanged:     updateShadowZ(); break;
        default: break;
        }
    }
    return QGraphicsObject::itemChange(change, value);
}

void ConsumableItem::updateShadowZ()
{
    if (!mShadow) return;
    // 阴影永远 z=-1000，保证不与邻牌本体重叠（详见 CardItem::updateShadowZ 注释）。
    mShadow->setZValue(-1000.0);
}

void ConsumableItem::paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) {
    // 阴影由 mShadow（sibling CardShadowItem）单独绘制——z=-1000 落到所有牌之下。

    // 缓存渲染在 SRC_W×SRC_H，在场景里平滑放大到 WIDTH×HEIGHT。
    const QPixmap pix = renderPixmap(mC.type, mC.negative);

    // 阴影按真实轮廓投影：外形变了才重算黑色剪影喂给阴影。
    const QString silKey = QString::number(int(mC.type)) + QLatin1Char('|')
                         + QString::number(mC.negative ? 1 : 0);
    if (mShadow && silKey != mShadowSilKey) {
        mShadow->setSilhouette(CardShadowItem::makeSilhouette(pix));
        mShadowSilKey = silKey;
    }

    p->setRenderHint(QPainter::SmoothPixmapTransform, true);
    p->drawPixmap(QRectF(0, 0, WIDTH, HEIGHT), pix, QRectF(0, 0, SRC_W, SRC_H));
}

void ConsumableItem::mousePressEvent(QGraphicsSceneMouseEvent *e)
{
    if (e->button() == Qt::LeftButton || e->button() == Qt::RightButton) {
        mPressed = true;
        mDragging = false;
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

void ConsumableItem::mouseMoveEvent(QGraphicsSceneMouseEvent *e)
{
    if (!mPressed) {
        QGraphicsObject::mouseMoveEvent(e);
        return;
    }

    if (!mDragging && QLineF(e->scenePos(), mPressScenePos).length() > 7.0) {
        mDragging = true;
        setZValue(700);
        updateShadowZ();
        animateScale(1.0, 70);
        mLastDragScenePos = e->scenePos();
        mLastDragTimeMs = QDateTime::currentMSecsSinceEpoch();
        animateShadowLift(currentShadowTarget(), 120);
    }

    if (mDragging) {
        // 水平速度倾斜（用户反馈9）：参数与 CardItem 保持一致。
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
        setTransformOriginPoint(WIDTH / 2.0, HEIGHT / 2.0);
        setRotation(mDragTilt + mMoveTilt);
        emit dragMoved(this, e->scenePos());
        e->accept();
        return;
    }

    QGraphicsObject::mouseMoveEvent(e);
}

void ConsumableItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *e)
{
    if ((e->button() == Qt::LeftButton || e->button() == Qt::RightButton) && mPressed) {
        mPressed = false;
        if (mDragging) {
            mDragging = false;
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
            emit clicked(this, e->button());
        }
        animateShadowLift(currentShadowTarget(), 160);
        updateShadowZ();
        e->accept();
        return;
    }
    QGraphicsObject::mouseReleaseEvent(e);
}

void ConsumableItem::applyHoverTransform()
{
    const qreal cx = WIDTH / 2.0;
    const qreal cy = HEIGHT / 2.0;
    const double totalTiltX = (mHovered || mDragging) ? mHoverTiltX : mAmbientTiltX;
    const double totalTiltY = (mHovered || mDragging) ? mHoverTiltY : mAmbientTiltY;
    const qreal tiltX = qDegreesToRadians(totalTiltX);
    const qreal tiltY = qDegreesToRadians(totalTiltY);

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

void ConsumableItem::ensureAmbientTimer()
{
    if (sConsumableAmbientTimer) return;
    sConsumableAmbientClock.start();
    sConsumableAmbientTimer = new QTimer(QCoreApplication::instance());
    sConsumableAmbientTimer->setTimerType(Qt::CoarseTimer);
    QObject::connect(sConsumableAmbientTimer, &QTimer::timeout, []() {
        const double seconds = sConsumableAmbientClock.elapsed() / 1000.0;
        const auto items = sAmbientConsumables.values();
        for (ConsumableItem *item : items) {
            if (item) item->updateAmbientTilt(seconds);
        }
    });
    sConsumableAmbientTimer->start(33);
}

void ConsumableItem::updateAmbientTilt(double seconds)
{
    if (mHovered || mDragging || !scene() || !isVisible()) return;
    const QPointF tilt = BalatroMotion::ambientTiltDegrees(mAmbientId, seconds, mAmbientTiltStrength);
    mAmbientTiltY = tilt.x();
    mAmbientTiltX = tilt.y();
    applyHoverTransform();
}

void ConsumableItem::hoverEnterEvent(QGraphicsSceneHoverEvent *e)
{
    mHovered = true;
    AudioManager::instance()->play(QStringLiteral("paper1"),
                                   0.9 + QRandomGenerator::global()->generateDouble() * 0.2,
                                   0.35);
    setTransformOriginPoint(WIDTH / 2.0, HEIGHT / 2.0);
    animateScale(1.05, 100);
    animateShadowLift(currentShadowTarget(), 120);
    triggerHoverJitter();
    update();
    emit hoverChanged(this, true);
    QGraphicsObject::hoverEnterEvent(e);
}

void ConsumableItem::triggerHoverJitter()
{
    if (mDragging) return;
    const double dir = (QRandomGenerator::global()->bounded(2) == 0) ? -1.0 : 1.0;
    const double peakRot   = 0.7 * dir;
    const double overshoot = -0.18 * dir;

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

void ConsumableItem::hoverMoveEvent(QGraphicsSceneHoverEvent *e)
{
    if (!mHovered || mDragging) {
        QGraphicsObject::hoverMoveEvent(e);
        return;
    }
    const qreal lx = qBound<qreal>(0.0, e->pos().x(), qreal(WIDTH));
    const qreal ly = qBound<qreal>(0.0, e->pos().y(), qreal(HEIGHT));
    const qreal nx = lx / WIDTH - 0.5;
    const qreal ny = ly / HEIGHT - 0.5;
    mHoverTiltY = qBound(-3.0, nx * 6.0, 3.0);
    mHoverTiltX = qBound(-3.0, ny * 6.0, 3.0);
    applyHoverTransform();
    QGraphicsObject::hoverMoveEvent(e);
}

void ConsumableItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *e)
{
    mHovered = false;
    mHoverTiltX = 0.0;
    mHoverTiltY = 0.0;
    applyHoverTransform();
    animateScale(1.0, 110);
    animateShadowLift(currentShadowTarget(), 160);
    update();
    emit hoverChanged(this, false);
    QGraphicsObject::hoverLeaveEvent(e);
}

void ConsumableItem::animateScale(qreal target, int durationMs)
{
    auto *anim = new QPropertyAnimation(this, "scale", this);
    anim->setDuration(durationMs);
    anim->setStartValue(scale());
    anim->setEndValue(target);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void ConsumableItem::moveTo(const QPointF &target, int durationMs)
{
    const QPointF from = pos();
    auto *anim = new QPropertyAnimation(this, "pos", this);
    anim->setDuration(durationMs);
    anim->setStartValue(from);
    anim->setEndValue(target);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);

    // 重排倾斜（对齐原版 Moveable:move_r）：横向滑动时朝运动方向倾斜，到位回正。
    if (mDragging) return;
    const double dx = target.x() - from.x();
    double tiltMax = dx * 0.09;
    if (tiltMax > 16.0) tiltMax = 16.0;
    if (tiltMax < -16.0) tiltMax = -16.0;
    if (qAbs(tiltMax) < 0.2) return;
    setTransformOriginPoint(WIDTH / 2.0, HEIGHT / 2.0);
    auto *tilt = new QVariantAnimation(this);
    // 倾斜衰减时长与移动时长解耦（重排 moveTo 常用 60ms 小步移动），固定 ~300ms 才看得见。
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

void ConsumableItem::juiceUp(double scaleAmount, int durationMs)
{
    setTransformOriginPoint(WIDTH / 2.0, HEIGHT / 2.0);

    auto *up = new QPropertyAnimation(this, "scale");
    up->setDuration(durationMs / 2);
    up->setStartValue(scale());
    up->setEndValue(scaleAmount);

    auto *down = new QPropertyAnimation(this, "scale");
    down->setDuration(durationMs / 2);
    down->setStartValue(scaleAmount);
    down->setEndValue(1.0);

    auto *seq = new QSequentialAnimationGroup(this);
    seq->addAnimation(up);
    seq->addAnimation(down);
    up->setParent(seq);
    down->setParent(seq);
    seq->start(QAbstractAnimation::DeleteWhenStopped);
}

void ConsumableItem::setShadowLift(qreal v)
{
    v = qBound(0.0, v, 1.0);
    if (qFuzzyCompare(v + 1.0, mShadowLift + 1.0)) return;
    mShadowLift = v;
    if (mShadow) mShadow->update();
}

void ConsumableItem::setScoringLifted(bool lifted)
{
    if (mScoringLifted == lifted) return;
    mScoringLifted = lifted;
    updateShadowZ();
    animateShadowLift(currentShadowTarget(), 180);
}

qreal ConsumableItem::currentShadowTarget() const
{
    if (mScoringLifted) return 1.0;
    if (mDragging)      return 0.85;
    if (mPressed)       return 0.55;
    if (mHovered)       return 0.30;
    return 0.0;
}

void ConsumableItem::animateShadowLift(qreal target, int durationMs)
{
    auto *anim = findChild<QPropertyAnimation*>(QStringLiteral("ConsumableShadowAnim"),
                                                 Qt::FindDirectChildrenOnly);
    if (!anim) {
        anim = new QPropertyAnimation(this, "shadowLift", this);
        anim->setObjectName(QStringLiteral("ConsumableShadowAnim"));
        anim->setEasingCurve(QEasingCurve::OutCubic);
    } else if (anim->state() == QAbstractAnimation::Running) {
        anim->stop();
    }
    anim->setDuration(durationMs);
    anim->setStartValue(mShadowLift);
    anim->setEndValue(qBound(0.0, target, 1.0));
    anim->start();
}
