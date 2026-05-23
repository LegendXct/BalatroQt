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
#include "cardshadow.h"
#include <QGraphicsScene>

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
    // 启用 ItemSendsGeometryChanges，让 itemChange 能收到 pos / transform / scale / rotation
    // 变化通知，便于把 sibling 阴影项同步。
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    mShadow = new CardShadowItem(WIDTH, HEIGHT, [this]() { return mShadowLift; });
    mShadow->setZValue(-1000.0);
    QObject::connect(this, &QObject::destroyed, [ptr = this]() { sAnimatedCards.remove(ptr); });

    if (cardNeedsShaderTick(mData)) {
        ensureCardShaderTimer();
        sAnimatedCards.insert(this);
    }
}

CardItem::~CardItem()
{
    if (mShadow) {
        if (auto *s = mShadow->scene()) s->removeItem(mShadow);
        delete mShadow;
        mShadow = nullptr;
    }
}

QVariant CardItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemSceneHasChanged) {
        QGraphicsScene *s = scene();
        if (s && mShadow && mShadow->scene() != s) s->addItem(mShadow);
        if (!s && mShadow && mShadow->scene())
            mShadow->scene()->removeItem(mShadow);
    } else if (mShadow) {
        switch (change) {
        case ItemPositionHasChanged:   mShadow->setPos(value.toPointF()); break;
        case ItemScaleHasChanged:      mShadow->setScale(value.toReal()); break;
        // 注意：不同步 ItemTransformHasChanged。applyTransform 给牌套了 hover 的 3D 透视
        // 矩阵，如果让阴影也吃这个矩阵，hover 时阴影会被透视投影"拽下来"，看着像突然下移。
        // 阴影只需要继承 z 旋转（扇形 + jitter + dragTilt），由 applyTransform 末尾显式
        // mShadow->setRotation() 同步。
        case ItemOpacityHasChanged:    mShadow->setOpacity(value.toReal()); break;
        case ItemVisibleHasChanged:    mShadow->setVisible(value.toBool()); break;
        case ItemZValueHasChanged:     updateShadowZ(); break;
        default: break;
        }
    }
    return QGraphicsObject::itemChange(change, value);
}

void CardItem::updateShadowZ()
{
    if (!mShadow) return;
    // 按下 / 拖动 / 计分时阴影升到本牌之下、其他牌之上；否则深沉到 -1000 全场最底。
    const bool active = mPressed || mDragging || mScoringLifted;
    mShadow->setZValue(active ? zValue() - 0.5 : -1000.0);
}

QRectF CardItem::boundingRect() const {
    // 阴影由 sibling CardShadowItem 单独绘制；本 item 的 bounding 收紧到牌面本体。
    return QRectF(0, 0, WIDTH, HEIGHT);
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
    // 阴影由 mShadow（sibling CardShadowItem）单独绘制——z=-1000 保证落在其他牌之下，
    // 按下/拖动/计分时由 updateShadowZ() 升到本牌之下。这里 paint() 只画牌面本体。
    if (mData.faceUp) paintFront(painter);
    else paintBack(painter);

    // hover / selected 不再画蓝/黄描边——原版没有这个轮廓线，状态变化由"抬升 + 阴影距离"
    // 表达，info 浮窗负责承载文字信息。
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
    animateShadowLift(currentShadowTarget(), 140);
    update();
}

void CardItem::setShadowLift(qreal v)
{
    v = qBound(0.0, v, 1.0);
    if (qFuzzyCompare(v + 1.0, mShadowLift + 1.0)) return;
    mShadowLift = v;
    if (mShadow) mShadow->update();
}

void CardItem::setScoringLifted(bool lifted)
{
    if (mScoringLifted == lifted) return;
    mScoringLifted = lifted;
    updateShadowZ();
    animateShadowLift(currentShadowTarget(), 180);
}

qreal CardItem::currentShadowTarget() const
{
    // 优先级：计分 > 拖拽 > 选中 > hover > rest。
    if (mScoringLifted) return 1.0;
    if (mDragging)      return 0.85;
    if (mPressed)       return 0.55;
    if (mSelected)      return 0.45;
    if (mHovered)       return 0.30;
    return 0.0;
}

void CardItem::animateShadowLift(qreal target, int durationMs)
{
    auto *anim = findChild<QPropertyAnimation*>(QStringLiteral("CardShadowAnim"),
                                                 Qt::FindDirectChildrenOnly);
    if (!anim) {
        anim = new QPropertyAnimation(this, "shadowLift", this);
        anim->setObjectName(QStringLiteral("CardShadowAnim"));
        anim->setEasingCurve(QEasingCurve::OutCubic);
    } else if (anim->state() == QAbstractAnimation::Running) {
        anim->stop();
    }
    anim->setDuration(durationMs);
    anim->setStartValue(mShadowLift);
    anim->setEndValue(qBound(0.0, target, 1.0));
    anim->start();
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
        // 按下时把卡牌临时抬到上层并放大，配合阴影 lift 让卡牌看起来"被指尖捏起来"。
        // 记 mPressRestZ 以便 release 还原（不打断 layoutHandCards 给的 z）。
        mPressRestZ = zValue();
        setZValue(500);
        updateShadowZ();
        animateScale(1.10, 90);
        animateShadowLift(currentShadowTarget(), 100);
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
        updateShadowZ();
        // 开始拖拽前重置速度采样，避免长按时的"前一刻"位置被算成大速度突然倾斜。
        mLastDragScenePos = event->scenePos();
        mLastDragTimeMs = QDateTime::currentMSecsSinceEpoch();
        animateShadowLift(currentShadowTarget(), 120);
    }

    if (mDragging) {
        // 对齐原版 Moveable:move_r() 的 des_r = T.r + 0.015 * vel.x / dt 公式：
        //   vel.x = 当帧水平位移（lua 单位/帧），dt = 1/60。
        //   在 Qt 里把它折算成"水平速度（px/s）→ 度"。
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        const qint64 dtMs = qMax<qint64>(1, nowMs - mLastDragTimeMs);
        const double dx = event->scenePos().x() - mLastDragScenePos.x();
        const double vxPerSec = dx * 1000.0 / double(dtMs);
        // 系数 0.022 deg / (px/s) + 上限 ±15°——比此前 0.045 / ±30° 收敛一半，
        // 拖动时倾斜更克制，对齐用户反馈9"惯性倾斜削弱一些"。
        double desTilt = vxPerSec * 0.022;
        if (desTilt > 15.0) desTilt = 15.0;
        if (desTilt < -15.0) desTilt = -15.0;
        // 指数平滑：偏向旧值多一点，让倾斜回正更柔和，不再跟手抖。
        mDragTilt = mDragTilt * 0.65 + desTilt * 0.35;
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
        const bool wasDragging = mDragging;
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
        // 松手 / 拖拽结束后立即向 hover 或 selected 目标过渡，让阴影"落"下去。
        animateShadowLift(currentShadowTarget(), 160);
        // 还原按下时临时抬高的 z 与放大；hover 仍在的话回到 hover 的 1.04 而不是 1.0。
        if (!wasDragging) setZValue(mPressRestZ);
        animateScale(mHovered ? 1.04 : 1.0, 140);
        // 退出 active 状态：阴影 z 回到 -1000。
        updateShadowZ();
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

    // 阴影只跟 z 旋转（扇形 + jitter + dragTilt），不吃 hover 透视——避免 hover 时阴影
    // 被透视投影"拽下来"。位置和缩放各自通过 itemChange 同步。
    if (mShadow) {
        mShadow->setTransformOriginPoint(WIDTH / 2.0, HEIGHT / 2.0);
        mShadow->setRotation(mBaseRotation + mJitterRot + mDragTilt);
    }
}

void CardItem::triggerHoverJitter()
{
    // 加强版 hover 抖动：原版进入悬浮时有一个明显的"弹一下"，之前 0.8° 太弱。
    // 这里把峰值放大到 ±2.4°，并把回弹做成 over-shoot（先回到 -0.8° 反弹再回 0），
    // 同时同步一只快速 scale 脉冲（×1.06 → ×1.0）让卡片有"被指尖戳了一下"的弹性。
    const double dir = (QRandomGenerator::global()->bounded(2) == 0) ? -1.0 : 1.0;
    const double peakRot = 2.4 * dir;
    const double overshoot = -0.6 * dir;

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
    rotBack->setDuration(110);
    rotBack->setStartValue(peakRot);
    rotBack->setEndValue(overshoot);
    rotBack->setEasingCurve(QEasingCurve::InOutQuad);
    connect(rotBack, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        mJitterRot = v.toDouble();
        applyTransform();
    });

    auto *rotSettle = new QVariantAnimation(this);
    rotSettle->setDuration(120);
    rotSettle->setStartValue(overshoot);
    rotSettle->setEndValue(0.0);
    rotSettle->setEasingCurve(QEasingCurve::OutCubic);
    connect(rotSettle, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        mJitterRot = v.toDouble();
        applyTransform();
    });

    auto *seq = new QSequentialAnimationGroup(this);
    seq->addAnimation(rotOut);
    seq->addAnimation(rotBack);
    seq->addAnimation(rotSettle);
    seq->start(QAbstractAnimation::DeleteWhenStopped);

    // 配合 scale 微脉冲——hoverEnter 主动画走 1.04，这里在 70ms 内再加一个 1.04→1.08→1.04
    // 的小爬升，让"弹一下"在视觉上更扎实。
    auto *scaleAnim = new QSequentialAnimationGroup(this);
    auto *up = new QPropertyAnimation(this, "scale");
    up->setDuration(60);
    up->setStartValue(scale());
    up->setEndValue(qMax(scale() + 0.04, 1.08));
    up->setEasingCurve(QEasingCurve::OutQuad);
    auto *down = new QPropertyAnimation(this, "scale");
    down->setDuration(150);
    down->setStartValue(qMax(scale() + 0.04, 1.08));
    down->setEndValue(1.04);
    down->setEasingCurve(QEasingCurve::OutQuad);
    scaleAnim->addAnimation(up);
    scaleAnim->addAnimation(down);
    up->setParent(scaleAnim); down->setParent(scaleAnim);
    scaleAnim->start(QAbstractAnimation::DeleteWhenStopped);
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
    animateShadowLift(currentShadowTarget(), 120);
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
    animateShadowLift(currentShadowTarget(), 160);
    applyTransform();
    update();
    QGraphicsObject::hoverLeaveEvent(event);
}
