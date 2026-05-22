#include "carditem.h"
#include <QPainter>
#include <QColor>
#include <QGraphicsSceneMouseEvent>
#include <QPropertyAnimation>
#include <QCursor>
#include <QSequentialAnimationGroup>
#include <QLineF>
#include <QDateTime>
#include <QTimer>
#include <QPainterPath>
#include <QCoreApplication>
#include <QSet>
#include <QHash>
#include <QStringList>
#include <QFontMetrics>
#include <QtGlobal>
#include <QApplication>
#include <QRandomGenerator>
#include <QVariantAnimation>
#include <cmath>
#include "../utils/shadereffects.h"

QPixmap *CardItem::sDeckSheet = nullptr;
QPixmap *CardItem::sEnhSheet = nullptr;
QPixmap *CardItem::sJokerSheet = nullptr;

namespace {
QSet<CardItem*> sAnimatedCards;
QTimer *sCardShaderTimer = nullptr;

bool cardNeedsShaderTick(const CardData &d)
{
    Q_UNUSED(d);
    // 版本/蜡封/debuff 走 GPU 离屏一次性渲染缓存，不再定时刷新每张手牌。
    return false;
}

void ensureCardShaderTimer()
{
    if (sCardShaderTimer) return;
    sCardShaderTimer = new QTimer(QCoreApplication::instance());
    sCardShaderTimer->setTimerType(Qt::CoarseTimer);
    QObject::connect(sCardShaderTimer, &QTimer::timeout, []() {
        const auto items = sAnimatedCards.values();
        for (CardItem *item : items) if (item) item->update();
    });
    sCardShaderTimer->start(67);
}

int cardShaderCacheFrame()
{
    return int(BalatroShaders::shaderTime() * 15.0);
}

// 手牌排序拖拽的启动距离。原来 8px 太敏感，点击时横向轻微抖动就会被判为拖动；
// 阈值与卡牌高度成比例（约 11.5% × HEIGHT），卡牌放大后阈值同步增大才不会"点一下就以为在拖"。
constexpr qreal kCardDragStartDistance = 26.0;
}


void CardItem::loadResources() {
    sDeckSheet = new QPixmap(":/textures/images/8BitDeck.png");
    sEnhSheet = new QPixmap(":/textures/images/Enhancers.png");
    sJokerSheet = new QPixmap(":/textures/images/Jokers.png");

    if (sDeckSheet->isNull())
        qWarning("CardItem: Fail to Load 8BitDeck.png");
    if (sEnhSheet->isNull())
        qWarning("CardItem: Fail to Load Enhancers.png");
    if (sJokerSheet->isNull())
        qWarning("CardItem: Fail to Load Jokers.png");
}

CardItem::CardItem(const CardData &data, QGraphicsItem *parent)
    : QGraphicsObject(parent)
    , mData(data)
{
    setAcceptHoverEvents(true);
    setCursor(Qt::PointingHandCursor);
    setTransformOriginPoint(WIDTH / 2.0, HEIGHT / 2.0);
    QObject::connect(this, &QObject::destroyed, [ptr = this]() { sAnimatedCards.remove(ptr); });

    if (cardNeedsShaderTick(mData)) {
        ensureCardShaderTimer();
        sAnimatedCards.insert(this);
    }
}

QRectF CardItem::boundingRect() const {
    // 预留外发光、选中描边，以及悬停时卡牌上方“梅花3 / +3筹码”标签的空间。
    return QRectF(-12, -78, WIDTH + 24, HEIGHT + 92);
}

QPainterPath CardItem::shape() const {
    QPainterPath p;
    if (mStrictHoverShape) {
        // 严格命中：只命中真正的牌面矩形，不把上方悬浮标签 / 外发光区域算进 hit-test。
        // 这样鼠标在牌堆按钮"上方那条空白带"里时不会触发 hoverEnter。
        p.addRect(QRectF(0, 0, WIDTH, HEIGHT));
    } else {
        p.addRect(boundingRect());
    }
    return p;
}

void CardItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
    if (mData.faceUp) paintFront(painter);
    else paintBack(painter);

    if (mHovered || mSelected) {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        const QColor mainColor = mSelected ? QColor(255, 211, 72, 245)
                                           : QColor(31, 183, 255, 245);
        QColor glowColor = mainColor;
        glowColor.setAlpha(90);

        // 选中黄色框和悬停蓝色框使用统一粗细；只保留一圈柔光，避免之前黄色框过厚。
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(glowColor, 3.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->drawRoundedRect(QRectF(-1.1, -1.1, WIDTH + 2.2, HEIGHT + 2.2), 10, 10);
        painter->setPen(QPen(mainColor, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->drawRoundedRect(QRectF(2.0, 2.0, WIDTH - 4.0, HEIGHT - 4.0), 8, 8);

        painter->restore();
    }

    // 注意：原本这里会调用 drawBalatroHoverTag 在牌头上方绘制一张白色卡名+筹码标签。
    // 这跟 MainWindow / PackOpenWidget / DeckViewWidget 里那只统一风格的 BalatroInfoPanel
    // 浮窗冲突——同一张牌悬浮时会出现"上方一只浅色小标签 + 暗色 BalatroInfoPanel"两张 info。
    // 这里直接去掉内嵌绘制，hover 信息统一交给上层的 BalatroInfoPanel。
}

QRect CardItem::whiteBaseSrcRect() const {
    return QRect(1 * SRC_W, 0 * SRC_H, SRC_W, SRC_H);
}

QRect CardItem::deckSrcRect() const {
    int col = static_cast<int>(mData.rank) - 2;
    int row = 0;
    switch (mData.suit) {
    case Suit::Hearts: row = 0; break;
    case Suit::Clubs: row = 1; break;
    case Suit::Diamonds: row = 2; break;
    case Suit::Spades: row = 3; break;
    }
    return QRect(col * SRC_W, row * SRC_H, SRC_W, SRC_H);
}

QRect CardItem::enhanceSrcRect() const {
    switch (mData.enhancement) {
    case Enhancement::Bonus: return QRect(1 * SRC_W, 1 * SRC_H, SRC_W, SRC_H);
    case Enhancement::Mult: return QRect(2 * SRC_W, 1 * SRC_H, SRC_W, SRC_H);
    case Enhancement::Wild: return QRect(3 * SRC_W, 1 * SRC_H, SRC_W, SRC_H);
    case Enhancement::Lucky: return QRect(4 * SRC_W, 1 * SRC_H, SRC_W, SRC_H);
    case Enhancement::Glass: return QRect(5 * SRC_W, 1 * SRC_H, SRC_W, SRC_H);
    case Enhancement::Steel: return QRect(6 * SRC_W, 1 * SRC_H, SRC_W, SRC_H);
    case Enhancement::Stone: return QRect(5 * SRC_W, 0 * SRC_H, SRC_W, SRC_H);
    case Enhancement::Gold: return QRect(6 * SRC_W, 0 * SRC_H, SRC_W, SRC_H);
    default: return whiteBaseSrcRect();
    }
}

QRect CardItem::sealSrcRect() const {
    switch (mData.seal) {
    case Seal::Gold: return QRect(2 * SRC_W, 0 * SRC_H, SRC_W, SRC_H);
    case Seal::Purple: return QRect(4 * SRC_W, 4 * SRC_H, SRC_W, SRC_H);
    case Seal::Red: return QRect(5 * SRC_W, 4 * SRC_H, SRC_W, SRC_H);
    case Seal::Blue: return QRect(6 * SRC_W, 4 * SRC_H, SRC_W, SRC_H);
    default: return QRect();
    }
}

void CardItem::paintFront(QPainter *painter)
{
    // 缓存按图集原始 142×190 渲染，绘制时再放大到场景显示尺寸 WIDTH×HEIGHT。
    // 这样切换显示尺寸只改 WIDTH/HEIGHT，不会让缓存全部失效或采样越界。
    QRect cacheRect(0, 0, SRC_W, SRC_H);
    QRectF dst(0, 0, WIDTH, HEIGHT);

    const bool animated = cardNeedsShaderTick(mData);
    const int frame = animated ? cardShaderCacheFrame() : -1;
    const QString key = QString::number(int(mData.suit)) + QLatin1Char('|')
                      + QString::number(int(mData.rank)) + QLatin1Char('|')
                      + QString::number(int(mData.enhancement)) + QLatin1Char('|')
                      + QString::number(int(mData.edition)) + QLatin1Char('|')
                      + QString::number(int(mData.seal)) + QLatin1Char('|')
                      + QString::number(mData.isDebuffed ? 1 : 0) + QLatin1Char('|')
                      + QString::number(frame);

    static QHash<QString, QPixmap> cache;
    static QStringList order;
    QPixmap finalPix = cache.value(key);
    if (finalPix.isNull()) {
        finalPix = QPixmap(SRC_W, SRC_H);
        finalPix.fill(Qt::transparent);

        QPixmap body(SRC_W, SRC_H);
        body.fill(Qt::transparent);
        {
            QPainter bp(&body);
            bp.setRenderHint(QPainter::SmoothPixmapTransform, false);
            QRect enh = enhanceSrcRect();
            if (!enh.isNull()) bp.drawPixmap(cacheRect, *sEnhSheet, enh);
            if (mData.enhancement != Enhancement::Stone)
                bp.drawPixmap(cacheRect, *sDeckSheet, deckSrcRect());
        }

        if (mData.edition != Edition::None)
            body = BalatroShaders::renderEditionPixmap(body, mData.edition);
        if (mData.isDebuffed)
            body = BalatroShaders::renderDebuffedPixmap(body);

        {
            QPainter fp(&finalPix);
            fp.setRenderHint(QPainter::SmoothPixmapTransform, false);
            fp.drawPixmap(cacheRect, body);

            QRect seal = sealSrcRect();
            if (!seal.isNull()) {
                QPixmap sealPix(SRC_W, SRC_H);
                sealPix.fill(Qt::transparent);
                {
                    QPainter sp(&sealPix);
                    sp.setRenderHint(QPainter::SmoothPixmapTransform, false);
                    sp.drawPixmap(cacheRect, *sEnhSheet, seal);
                }
                if (mData.seal == Seal::Gold)
                    sealPix = BalatroShaders::renderGoldSealPixmap(sealPix, 0.95);
                fp.drawPixmap(cacheRect, sealPix);
            }
        }

        cache.insert(key, finalPix);
        order.append(key);
        while (order.size() > 256) cache.remove(order.takeFirst());
    }

    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter->drawPixmap(dst, finalPix, QRectF(cacheRect));
}

void CardItem::paintBack(QPainter *painter) {
    QRectF dst(0, 0, WIDTH, HEIGHT);
    QRect backSrc(0 * SRC_W, 0 * SRC_H, SRC_W, SRC_H);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter->drawPixmap(dst, *sEnhSheet, backSrc);
}

void CardItem::setCardData(const CardData &data) {
    const bool wasAnimated = cardNeedsShaderTick(mData);
    // 保留当前的翻面状态——塔罗 / 幻灵牌应用增强期间，UI 会先把目标牌 flip() 翻成背面，
    // 这时若 setCardData 把 faceUp 重置成 true（CardData 默认值），翻牌动画就被破坏了。
    const bool keepFaceUp = mData.faceUp;
    mData = data;
    mData.faceUp = keepFaceUp;
    const bool nowAnimated = cardNeedsShaderTick(mData);
    if (nowAnimated && !wasAnimated) {
        ensureCardShaderTimer();
        sAnimatedCards.insert(this);
    } else if (!nowAnimated && wasAnimated) {
        sAnimatedCards.remove(this);
    }
    update();
}

void CardItem::setCardSelected(bool selected) {
    mSelected = selected;
    update();
}

void CardItem::moveTo(const QPointF &target, int durationMs) {
    // 复用一只 QPropertyAnimation（之前每次 new 一只造成多动画在同一 property 上互相覆盖，
    // 表现为选牌、弃牌时卡牌"卡一下/抖一下"）。
    QPropertyAnimation *anim = findChild<QPropertyAnimation*>(QStringLiteral("CardMoveAnim"),
                                                              Qt::FindDirectChildrenOnly);
    const QPointF current = pos();
    const bool atTarget = qFuzzyCompare(current.x() + 1.0, target.x() + 1.0)
                       && qFuzzyCompare(current.y() + 1.0, target.y() + 1.0);

    // 关键修正：即使 pos == target，也要看正在运行的旧动画终点是不是同一个；
    // 不是就必须 stop()，否则在 layoutHandCards 一回合内被先后调用两次时，
    // "看着已经在位"的牌会被前一只未完的动画拽走，产生弃牌时的间距错乱。
    if (anim && anim->state() == QAbstractAnimation::Running) {
        const QVariant ev = anim->endValue();
        QPointF endP = ev.canConvert<QPointF>() ? ev.toPointF() : QPointF(NAN, NAN);
        const bool sameTargetAsRunning =
            qFuzzyCompare(endP.x() + 1.0, target.x() + 1.0)
         && qFuzzyCompare(endP.y() + 1.0, target.y() + 1.0);
        if (sameTargetAsRunning) return;     // 已经在朝同一个目标飞，无需打断
        anim->stop();
    } else if (atTarget) {
        return; // 没有运行中的动画且已经在位，直接什么都不做
    }

    if (!anim) {
        anim = new QPropertyAnimation(this, "pos", this);
        anim->setObjectName(QStringLiteral("CardMoveAnim"));
        anim->setEasingCurve(QEasingCurve::OutCubic);
    }
    anim->setDuration(durationMs);
    anim->setStartValue(current);
    anim->setEndValue(target);
    anim->start();
}

void CardItem::flip() {
    // 对齐原版 card.lua Card:flip()：触发 pinch.x，使牌宽收缩到 0（绕 Y 轴翻面效果），
    // 然后在 sprite 翻面后再扩张回 1。仅缩 X，不缩 Y，避免出现垂直方向的塌缩。
    auto *shrink = new QPropertyAnimation(this, "flipXScale", this);
    shrink->setDuration(120);
    shrink->setStartValue(1.0);
    shrink->setEndValue(0.0);
    shrink->setEasingCurve(QEasingCurve::InQuad);

    connect(shrink, &QPropertyAnimation::finished, this, [this]() {
        mData.faceUp = !mData.faceUp;
        update();
        auto *expand = new QPropertyAnimation(this, "flipXScale", this);
        expand->setDuration(120);
        expand->setStartValue(0.0);
        expand->setEndValue(1.0);
        expand->setEasingCurve(QEasingCurve::OutQuad);
        expand->start(QAbstractAnimation::DeleteWhenStopped);
    });

    shrink->start(QAbstractAnimation::DeleteWhenStopped);
}

void CardItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        mPressed = true;
        mDragging = false;
        mPressScenePos = event->scenePos();
        mLastDragScenePos = event->scenePos();
        mLastDragTimeMs = QDateTime::currentMSecsSinceEpoch();
        event->accept();
    }
    else QGraphicsObject::mousePressEvent(event);
}

void CardItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (!mPressed) {
        QGraphicsObject::mouseMoveEvent(event);
        return;
    }
    if (mDraggable && !mDragging
        && QLineF(event->scenePos(), mPressScenePos).length() > kCardDragStartDistance) {
        mDragging = true;
        setZValue(600);
        // 开始拖拽前重置速度采样，避免长按时的"前一刻"位置被算成大速度突然倾斜。
        mLastDragScenePos = event->scenePos();
        mLastDragTimeMs = QDateTime::currentMSecsSinceEpoch();
    }

    if (mDragging) {
        // 对齐原版 Moveable:move_r() 的 des_r = T.r + 0.015 * vel.x / dt 公式：
        //   vel.x = 当帧水平位移（lua 单位/帧），dt = 1/60。
        //   在 Qt 里把它折算成"水平速度（px/s）→ 度"。
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        const qint64 dtMs = qMax<qint64>(1, nowMs - mLastDragTimeMs);
        const double dx = event->scenePos().x() - mLastDragScenePos.x();
        const double vxPerSec = dx * 1000.0 / double(dtMs);
        // 系数 0.045 deg / (px/s)：移动 ~600 px/s 时约 27 度，更明显的"惯性甩动"手感。
        double desTilt = vxPerSec * 0.045;
        if (desTilt > 30.0) desTilt = 30.0;
        if (desTilt < -30.0) desTilt = -30.0;
        // 指数平滑系数也偏向 des 多一点，让倾斜跟随手感更紧。
        mDragTilt = mDragTilt * 0.50 + desTilt * 0.50;
        mLastDragScenePos = event->scenePos();
        mLastDragTimeMs = nowMs;

        setPos(event->scenePos() - QPointF(WIDTH / 2.0, HEIGHT / 2.0));
        applyTransform();
        emit dragMoved(this, event->scenePos());
        event->accept();
        return;
    }
    QGraphicsObject::mouseMoveEvent(event);
}

void CardItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && mPressed) {
        mPressed = false;
        if (mDragging) {
            mDragging = false;
            // 释放后平滑把拖拽倾斜衰减回 0——对齐原版 velocity.r → 0 的回弹。
            if (qFuzzyIsNull(mDragTilt)) {
                mDragTilt = 0.0;
                applyTransform();
            } else {
                auto *decay = new QVariantAnimation(this);
                decay->setDuration(220);
                decay->setStartValue(mDragTilt);
                decay->setEndValue(0.0);
                decay->setEasingCurve(QEasingCurve::OutCubic);
                connect(decay, &QVariantAnimation::valueChanged, this,
                        [this](const QVariant &v) {
                    mDragTilt = v.toDouble();
                    applyTransform();
                });
                decay->start(QAbstractAnimation::DeleteWhenStopped);
            }
            emit dragReleased(this, event->scenePos());
        } else {
            emit clicked(this);
        }
        event->accept();
        return;
    }
    QGraphicsObject::mouseReleaseEvent(event);
}

void CardItem::juiceUp(double scaleAmount, int durationMs)
{
    // 设变换原点为中心,避免缩放时位置漂移
    setTransformOriginPoint(WIDTH / 2.0, HEIGHT / 2.0);

    auto *up = new QPropertyAnimation(this, "scale");
    up->setDuration(durationMs / 2);
    up->setStartValue(1.0);
    up->setEndValue(scaleAmount);
    up->setEasingCurve(QEasingCurve::OutQuad);

    auto *down = new QPropertyAnimation(this, "scale");
    down->setDuration(durationMs / 2);
    down->setStartValue(scaleAmount);
    down->setEndValue(1.0);
    down->setEasingCurve(QEasingCurve::InQuad);

    auto *seq = new QSequentialAnimationGroup(this);
    seq->addAnimation(up);
    seq->addAnimation(down);
    up->setParent(seq);
    down->setParent(seq);
    seq->start(QAbstractAnimation::DeleteWhenStopped);
}

void CardItem::applyTransform()
{
    // 中心点
    qreal cx = WIDTH  / 2.0;
    qreal cy = HEIGHT / 2.0;

    // 透视参数:鼠标越往边缘,倾斜越大,深度感越明显
    qreal tiltX = qDegreesToRadians(mHoverTiltX);
    qreal tiltY = qDegreesToRadians(mHoverTiltY);
    qreal zRot  = qDegreesToRadians(mBaseRotation);

    // 步骤:平移到中心 → 绕 Y 倾斜 → 绕 X 倾斜 → 绕 Z 扇形 → 平移回去
    QTransform t;
    t.translate(cx, cy);

    // 模拟绕 Y 轴旋转:左右两边远近不同 → 水平缩放 + 透视投影
    // 简化:用 m11/m13 实现(perspective)
    // y轴倾斜 = 水平倾斜效果(左右翻),用 shear + scale 近似
    qreal cosY = std::cos(tiltY);
    qreal cosX = std::cos(tiltX);
    qreal sinY = std::sin(tiltY);
    qreal sinX = std::sin(tiltX);

    QTransform persp;
    persp.setMatrix(
        cosY,           sinY * sinX,    0.005 * sinY,
        0,              cosX,           0.005 * sinX,
        0,              0,              1
        );
    t = persp * t;

    // 应用 Z 轴扇形旋转 + 悬浮抖动 + 拖拽速度倾斜叠加。
    t.rotateRadians(zRot + qDegreesToRadians(mJitterRot + mDragTilt));

    // 翻牌时的水平 pinch（仅 X 缩放），围绕中心 → 模拟绕 Y 轴翻面，对齐原版 pinch.x。
    if (mFlipXScale != 1.0) {
        t.scale(mFlipXScale, 1.0);
    }

    t.translate(-cx, -cy);
    setTransform(t);
}

void CardItem::triggerHoverJitter()
{
    // 对齐 card.lua: Card:hover() -> juice_up(0.05, 0.03) -> Moveable.juice_up(scale=0.02, rot=±0.012rad ≈ ±0.7°)
    // 这里用两段短旋转动画：先转到 +/-0.8°，再回 0；并叠加一个 scale 微脉冲。
    const double dir = (QRandomGenerator::global()->bounded(2) == 0) ? -1.0 : 1.0;
    const double peakRot = 0.8 * dir;

    auto *rotOut = new QVariantAnimation(this);
    rotOut->setDuration(70);
    rotOut->setStartValue(0.0);
    rotOut->setEndValue(peakRot);
    rotOut->setEasingCurve(QEasingCurve::OutQuad);
    connect(rotOut, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        mJitterRot = v.toDouble();
        applyTransform();
    });

    auto *rotBack = new QVariantAnimation(this);
    rotBack->setDuration(140);
    rotBack->setStartValue(peakRot);
    rotBack->setEndValue(0.0);
    rotBack->setEasingCurve(QEasingCurve::OutQuad);
    connect(rotBack, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        mJitterRot = v.toDouble();
        applyTransform();
    });

    auto *seq = new QSequentialAnimationGroup(this);
    seq->addAnimation(rotOut);
    seq->addAnimation(rotBack);
    seq->start(QAbstractAnimation::DeleteWhenStopped);
}

void CardItem::animateScale(qreal target, int durationMs) {
    // 单只复用的 "CardScaleAnim"，hover 反复进出时不会堆叠多只动画。
    QPropertyAnimation *anim = findChild<QPropertyAnimation*>(QStringLiteral("CardScaleAnim"),
                                                              Qt::FindDirectChildrenOnly);
    if (!anim) {
        anim = new QPropertyAnimation(this, "scale", this);
        anim->setObjectName(QStringLiteral("CardScaleAnim"));
        anim->setEasingCurve(QEasingCurve::OutCubic);
    } else {
        anim->stop();
    }
    anim->setDuration(durationMs);
    anim->setStartValue(scale());
    anim->setEndValue(target);
    anim->start();
}

void CardItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
    mHovered = true;
    // 不再直接 setScale 让放大瞬切——平滑过渡 90ms，避免选中升起动画同时遇上瞬间缩放产生顿挫感。
    animateScale(1.04, 90);
    // 对齐 card.lua 中 Card:hover() 的 self:juice_up(0.05, 0.03)：进入悬浮时
    // 给一个非常细微的"抖一下"，让卡牌像被指尖触碰一样轻微弹动。
    triggerHoverJitter();
    update();
    emit hoverChanged(this, true);
    QGraphicsObject::hoverEnterEvent(event);
}

void CardItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    if (!mHovered) return;
    if (!mHoverTiltEnabled) {
        QGraphicsObject::hoverMoveEvent(event);
        return;
    }
    // 标签区域也属于 boundingRect，所以这里把鼠标位置限制在牌面内部，避免移到标签上方时产生夸张倾斜。
    qreal lx = qBound<qreal>(0.0, event->pos().x(), qreal(WIDTH));
    qreal ly = qBound<qreal>(0.0, event->pos().y(), qreal(HEIGHT));
    qreal nx = (lx / WIDTH)  - 0.5;     // [-0.5, 0.5]
    qreal ny = (ly / HEIGHT) - 0.5;
    // 原版 card.lua 的 tilt_factor = 0.3，相比起以前 ±10° 视觉效果上要轻得多；
    // 这里收敛到 ±5°（边缘）并保留中心几乎无倾斜的手感。
    mHoverTiltY = qBound(-3.0, nx * 6.0, 3.0);
    mHoverTiltX = qBound(-3.0, ny * 6.0, 3.0);
    applyTransform();
    QGraphicsObject::hoverMoveEvent(event);
}

void CardItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
    mHovered = false;
    emit hoverChanged(this, false);
    mHoverTiltX = 0;
    mHoverTiltY = 0;
    // scale 平滑回 1.0；tilt 用直接 setTransform 重置（应用 applyTransform）。
    animateScale(1.0, 130);
    applyTransform();
    update();
    QGraphicsObject::hoverLeaveEvent(event);
}
