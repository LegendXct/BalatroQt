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

QString rankLabel(Rank r)
{
    switch (r) {
    case Rank::Two: return "2";
    case Rank::Three: return "3";
    case Rank::Four: return "4";
    case Rank::Five: return "5";
    case Rank::Six: return "6";
    case Rank::Seven: return "7";
    case Rank::Eight: return "8";
    case Rank::Nine: return "9";
    case Rank::Ten: return "10";
    case Rank::Jack: return "J";
    case Rank::Queen: return "Q";
    case Rank::King: return "K";
    case Rank::Ace: return "A";
    }
    return "?";
}

QString suitLabel(Suit s)
{
    switch (s) {
    case Suit::Spades: return QStringLiteral("黑桃");
    case Suit::Hearts: return QStringLiteral("红桃");
    case Suit::Diamonds: return QStringLiteral("方块");
    case Suit::Clubs: return QStringLiteral("梅花");
    }
    return QString();
}

QString hoverTitleForCard(const CardData &d)
{
    if (d.enhancement == Enhancement::Stone)
        return QStringLiteral("石头牌");
    return suitLabel(d.suit) + rankLabel(d.rank);
}

QString hoverDescForCard(const CardData &d)
{
    int chips = d.chipValue() + d.permanentBonusChips;
    if (d.enhancement == Enhancement::Bonus) chips += 30;
    if (d.enhancement == Enhancement::Stone) chips = 50 + d.permanentBonusChips;
    return QStringLiteral("+%1筹码").arg(chips);
}

void drawBalatroHoverTag(QPainter *painter, const CardData &d)
{
    const QString title = hoverTitleForCard(d);
    const QString desc = hoverDescForCard(d);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, false);

    const QString hoverFamily = qApp ? qApp->font().family() : QStringLiteral("Arial");
    QFont titleFont(hoverFamily);
    titleFont.setPixelSize(18);
    titleFont.setBold(true);
    QFont descFont(hoverFamily);
    descFont.setPixelSize(17);
    descFont.setBold(true);

    QFontMetrics titleFm(titleFont);
    QFontMetrics descFm(descFont);
    const int w = qMax(76, qMax(titleFm.horizontalAdvance(title), descFm.horizontalAdvance(desc)) + 24);
    const int titleH = 32;
    const int descH = 31;
    const int x = int(CardItem::WIDTH / 2 - w / 2);
    const int y = -70;

    QRectF outer(x, y, w, titleH + descH + 4);
    QPainterPath shadowPath;
    shadowPath.addRoundedRect(outer.adjusted(2, 3, 2, 3), 8, 8);
    painter->fillPath(shadowPath, QColor(0, 0, 0, 70));

    QRectF titleRect(x, y, w, titleH + 2);
    QRectF descRect(x, y + titleH - 1, w, descH + 3);

    QPainterPath titlePath;
    titlePath.addRoundedRect(titleRect, 7, 7);
    painter->fillPath(titlePath, QColor(248, 255, 250));
    painter->setPen(QPen(QColor(34, 45, 48), 2.2));
    painter->drawPath(titlePath);

    QPainterPath descPath;
    descPath.addRoundedRect(descRect, 7, 7);
    painter->fillPath(descPath, QColor(248, 255, 250));
    painter->setPen(QPen(QColor(34, 45, 48), 2.2));
    painter->drawPath(descPath);

    // 遮掉两个圆角框交界处的中间描边，让它更像正版的上下拼接标签。
    painter->fillRect(QRectF(x + 3, y + titleH - 3, w - 6, 6), QColor(248, 255, 250));
    painter->setPen(QPen(QColor(34, 45, 48), 2.0));
    painter->drawLine(QPointF(x + 3, y + titleH), QPointF(x + w - 3, y + titleH));

    painter->setFont(titleFont);
    painter->setPen(QColor(31, 45, 48));
    painter->drawText(titleRect.adjusted(2, 0, -2, 0), Qt::AlignCenter, title);

    painter->setFont(descFont);
    painter->setPen(QColor(42, 132, 205));
    painter->drawText(descRect.adjusted(2, 0, -2, 0), Qt::AlignCenter, desc);

    painter->restore();
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

    if (mHovered && mData.faceUp) {
        drawBalatroHoverTag(painter, mData);
    }
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
    mData = data;
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
    auto *shrink = new QPropertyAnimation(this, "scale", this);
    shrink->setDuration(120);
    shrink->setStartValue(1.0);
    shrink->setEndValue(0.0);
    shrink->setEasingCurve(QEasingCurve::InQuad);

    connect(shrink, &QPropertyAnimation::finished, this, [this]() {
        mData.faceUp = !mData.faceUp;
        update();
        auto *expand = new QPropertyAnimation(this, "scale", this);
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
    if (!mDragging && QLineF(event->scenePos(), mPressScenePos).length() > kCardDragStartDistance) {
        mDragging = true;
        setZValue(600);
    }

    if (mDragging) {
        setPos(event->scenePos() - QPointF(WIDTH / 2.0, HEIGHT / 2.0));
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

    // 应用 Z 轴扇形旋转
    t.rotateRadians(zRot);

    t.translate(-cx, -cy);
    setTransform(t);
}

void CardItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
    mHovered = true;
    setScale(1.04);
    update();
    emit hoverChanged(this, true);
    QGraphicsObject::hoverEnterEvent(event);
}

void CardItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    if (!mHovered) return;
    // 标签区域也属于 boundingRect，所以这里把鼠标位置限制在牌面内部，避免移到标签上方时产生夸张倾斜。
    qreal lx = qBound<qreal>(0.0, event->pos().x(), qreal(WIDTH));
    qreal ly = qBound<qreal>(0.0, event->pos().y(), qreal(HEIGHT));
    qreal nx = (lx / WIDTH)  - 0.5;     // [-0.5, 0.5]
    qreal ny = (ly / HEIGHT) - 0.5;
    // 对齐原版 card.lua 的 tilt：鼠标在哪个角，牌面就朝那个角倾斜；
    // JokerItem / ConsumableItem 都是 ±10°，CardItem 也用同一节奏。
    mHoverTiltY = qBound(-10.0, nx * 20.0, 10.0);
    mHoverTiltX = qBound(-10.0, ny * 20.0, 10.0);
    applyTransform();
    QGraphicsObject::hoverMoveEvent(event);
}

void CardItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
    mHovered = false;
    emit hoverChanged(this, false);
    mHoverTiltX = 0;
    mHoverTiltY = 0;
    setScale(1.0);
    applyTransform();
    update();
    QGraphicsObject::hoverLeaveEvent(event);
}
