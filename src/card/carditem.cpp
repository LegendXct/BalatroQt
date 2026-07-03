#include "carditem.h"
#include <QPainter>
#include <QColor>
#include <QGraphicsSceneMouseEvent>
#include <QPropertyAnimation>
#include <QCursor>
#include <QSequentialAnimationGroup>
#include <QPointer>
#include <QLineF>
#include <QDateTime>
#include <QElapsedTimer>
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
#include "../audio/audiomanager.h"
#include "../utils/balatromotion.h"
#include "../utils/shadereffects.h"
#include "cardshadow.h"
#include "deckskin.h"
#include <QGraphicsScene>

QPixmap *CardItem::sDeckSheet = nullptr;
QPixmap *CardItem::sEnhSheet = nullptr;
QPixmap *CardItem::sJokerSheet = nullptr;
QFont CardItem::sLinkTagFont;
QPoint CardItem::sBackSpritePos = {0, 0};
QPixmap *CardItem::sCustomBackPixmap = nullptr;

namespace {
QSet<CardItem*> sAnimatedCards;
QTimer *sCardShaderTimer = nullptr;
QSet<CardItem*> sAmbientCards;
QTimer *sCardAmbientTimer = nullptr;
QElapsedTimer sCardAmbientClock;

// 弹簧移动（速度保持）共享 60Hz 时基
QSet<CardItem*> sSpringCards;
QTimer *sSpringTimer = nullptr;
QElapsedTimer sSpringClock;
qint64 sSpringLastMs = 0;

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

void CardItem::drawIteratorOverlay(QPainter *p, const QRectF &dst)
{
    static QPixmap overlay(QStringLiteral(":/textures/images/enh_cs_iterator.png"));
    if (overlay.isNull()) return;
    const bool smooth = p->renderHints().testFlag(QPainter::SmoothPixmapTransform);
    p->setRenderHint(QPainter::SmoothPixmapTransform, true);
    p->drawPixmap(dst, overlay, QRectF(overlay.rect()));
    p->setRenderHint(QPainter::SmoothPixmapTransform, smooth);
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
    mAmbientId = BalatroMotion::nextCardLikeId();
    ensureAmbientTimer();
    sAmbientCards.insert(this);

    if (cardNeedsShaderTick(mData)) {
        ensureCardShaderTimer();
        sAnimatedCards.insert(this);
    }
}

CardItem::~CardItem()
{
    sAnimatedCards.remove(this);
    sAmbientCards.remove(this);
    sSpringCards.remove(this);
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
    // 飞行速度扭曲：位置每变化一次就采样水平速度，按速度给出朝运动方向的倾斜。
    // 复刻原版 Moveable:move_r（des_r ∝ vel.x）。仅飞行期间开启，拖拽有独立 mDragTilt 不重复。
    if (change == ItemPositionHasChanged && mVelTilt && !mDragging) {
        const QPointF np = value.toPointF();
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (mVelLastMs != 0) {
            const double dts = (now - mVelLastMs) / 1000.0;
            if (dts > 0.0) {
                const double vx = (np.x() - mVelLastPos.x()) / dts;   // 水平速度 px/s
                // px/s → 角度并夹在 ±5°；系数 0.005，保持"正常飞入"，倾斜只作轻微点缀。
                const double targetDeg = qBound(-5.0, vx * 0.005, 5.0);
                // 指数逼近平滑（对齐 velocity.r 的弹簧收敛），避免逐帧抖动。
                mMoveTilt = 0.5 * mMoveTilt + 0.5 * targetDeg;
                applyTransform();
            }
        }
        mVelLastPos = np;
        mVelLastMs = now;
    }
    return QGraphicsObject::itemChange(change, value);
}

void CardItem::setVelocityTiltTracking(bool on)
{
    if (on == mVelTilt) {
        if (on) mVelLastMs = 0;   // 重新开一段飞行：清掉上次的基准帧
        return;
    }
    mVelTilt = on;
    if (on) {
        mVelLastMs = 0;           // 首帧只记基准，不算速度
        mVelLastPos = pos();
        return;
    }
    // 结束追踪：把当前速度倾斜平滑收回 0，避免定格在最后一帧的角度。
    if (auto *t = findChild<QVariantAnimation*>(QStringLiteral("CardMoveTilt"),
                                                Qt::FindDirectChildrenOnly))
        t->stop();
    if (qFuzzyIsNull(mMoveTilt)) return;
    auto *settle = new QVariantAnimation(this);
    settle->setObjectName(QStringLiteral("CardMoveTilt"));
    settle->setDuration(160);
    settle->setStartValue(mMoveTilt);
    settle->setEndValue(0.0);
    settle->setEasingCurve(QEasingCurve::OutCubic);
    connect(settle, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        mMoveTilt = v.toDouble();
        applyTransform();
    });
    connect(settle, &QVariantAnimation::finished, this, [this]() {
        mMoveTilt = 0.0; applyTransform();
    });
    settle->start(QAbstractAnimation::DeleteWhenStopped);
}

void CardItem::updateShadowZ()
{
    if (!mShadow) return;
    // 用户要求：手牌 / 计分 / 拖动场景下阴影永远不与邻牌本体重叠。
    // 阴影固定 z=-1000（场景最底层）→ 任何其他卡牌（z≥20）都画在它上面 → 阴影即使
    // 因投影方向（左下）延伸到隔壁卡牌区域，也被隔壁牌本体盖住，视觉上没有重叠。
    // 之前 active 时升到 zValue()-0.5 是为了让本牌"看起来浮起来"，但代价是本牌阴影
    // 盖过左侧邻牌本体——用户指出这就是重叠，所以放弃这个分支。
    mShadow->setZValue(-1000.0);
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
    if (mData.faceUp) {
        paintFront(painter);
        if (!mLinkTag.isEmpty()) paintLinkTag(painter);
    } else {
        paintBack(painter);
    }

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
                      + QString::number(frame) + QLatin1Char('|')
                      // 掺入换肤代数：切换定制牌组后旧缓存条目自然失效，J/Q/K/A 立即换面。
                      + QString::number(DeckSkin::generation());

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
                bp.drawPixmap(cacheRect, DeckSkin::deckSheet(), deckSrcRect());
            // 程设整卡人像：背景式增强以不透明"边框"叠在人像上（玻璃整张叠加），角标回贴。
            if (DeckSkin::enhancementOverArt(mData.rank, mData.enhancement))
                DeckSkin::drawEnhancementOverArt(&bp, *sEnhSheet, enh,
                                                 mData.rank, mData.suit, mData.enhancement);
            // 迭代器增强：画在 body 里，让版本 shader / debuff 滤镜一并作用于装饰框。
            if (mData.enhancement == Enhancement::Iterator)
                drawIteratorOverlay(&bp, QRectF(cacheRect));
        }

        if (mData.edition != Edition::None)
            body = BalatroShaders::renderEditionPixmap(body, mData.edition);

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
                if (mData.seal == Seal::Gold) {
                    // 原版 card.lua:4476: G.shared_seals['Gold']:draw_shader('voucher',...)
                    // —— 金色蜡封实际走的是 voucher shader 的金色 sparkle，不是 gold_seal
                    // shader（那是商店招牌 / 火焰用的）。
                    sealPix = BalatroShaders::renderVoucherPixmap(sealPix, 1.0);
                }
                fp.drawPixmap(cacheRect, sealPix);
            }
            if (mData.isDebuffed) {
                const QPixmap debuffOverlay = BalatroShaders::renderDebuffedPixmap(body);
                fp.drawPixmap(cacheRect, debuffOverlay);
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
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    if (sCustomBackPixmap && !sCustomBackPixmap->isNull()) {
        painter->drawPixmap(dst, *sCustomBackPixmap,
                            QRectF(0, 0,
                                   sCustomBackPixmap->width(),
                                   sCustomBackPixmap->height()));
        return;
    }
    QRect backSrc(sBackSpritePos.x() * SRC_W, sBackSpritePos.y() * SRC_H, SRC_W, SRC_H);
    painter->drawPixmap(dst, *sEnhSheet, backSrc);
}

QPixmap CardItem::cardBackPixmap() {
    if (sCustomBackPixmap && !sCustomBackPixmap->isNull()) return *sCustomBackPixmap;
    if (!sEnhSheet || sEnhSheet->isNull()) return QPixmap();
    return sEnhSheet->copy(sBackSpritePos.x() * SRC_W,
                           sBackSpritePos.y() * SRC_H,
                           SRC_W,
                           SRC_H);
}

void CardItem::setCardBackSpritePos(const QPoint &pos)
{
    sBackSpritePos = pos;
    delete sCustomBackPixmap;
    sCustomBackPixmap = nullptr;
}

void CardItem::setCustomCardBackPixmap(const QPixmap &pixmap)
{
    if (!sCustomBackPixmap) {
        sCustomBackPixmap = new QPixmap(pixmap);
    } else {
        *sCustomBackPixmap = pixmap;
    }
}

void CardItem::setLinkTagFont(const QFont &f)
{
    sLinkTagFont = f;
    sLinkTagFont.setPixelSize(17);
}

void CardItem::setLinkTag(const QString &tag)
{
    if (mLinkTag == tag) return;
    mLinkTag = tag;
    update();
}

// 浅拷贝共享地址角标：牌面下缘中央一块深色小铭牌，链接两侧文案相同，
// 隐喻"两个指针指向同一块内存"。画在缓存图层之上，不进 paintFront 的缓存 key。
void CardItem::paintLinkTag(QPainter *p)
{
    p->setFont(sLinkTagFont);
    const QFontMetrics fm(sLinkTagFont);
    const qreal w = fm.horizontalAdvance(mLinkTag) + 14;
    const qreal h = 20;
    const QRectF plate((WIDTH - w) / 2.0, HEIGHT - h - 7, w, h);
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setPen(Qt::NoPen);
    p->setBrush(QColor(20, 24, 28, 210));
    p->drawRoundedRect(plate, 5, 5);
    p->setPen(QColor(154, 232, 255));
    p->drawText(plate, Qt::AlignCenter, mLinkTag);
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
    // 若正处于弹簧移动中，先退出弹簧，避免弹簧与本动画同时写 pos。
    if (mSpringActive) {
        mSpringActive = false;
        mSpringVel = QPointF();
        sSpringCards.remove(this);
    }
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

    // 重排倾斜（对齐原版 Moveable:move_r：des_r ∝ 水平速度）：被其它牌挤动横向滑动时
    // 朝运动方向倾斜，随到位回正。被拖动的牌走 setPos+mDragTilt，这里跳过避免叠加。
    // 飞行速度追踪开启时（发牌/出牌/弃牌），由 itemChange 逐帧算真实速度倾斜，这里不再叠加位移倾斜。
    if (mDragging || mVelTilt) return;
    const double dx = target.x() - current.x();
    double tiltMax = dx * 0.06;
    if (tiltMax > 10.0) tiltMax = 10.0;
    if (tiltMax < -10.0) tiltMax = -10.0;
    QVariantAnimation *tilt = findChild<QVariantAnimation*>(QStringLiteral("CardMoveTilt"),
                                                            Qt::FindDirectChildrenOnly);
    if (qAbs(tiltMax) < 0.2) {
        if (tilt) tilt->stop();
        if (!qFuzzyIsNull(mMoveTilt)) { mMoveTilt = 0.0; applyTransform(); }
        return;
    }
    if (!tilt) {
        tilt = new QVariantAnimation(this);
        tilt->setObjectName(QStringLiteral("CardMoveTilt"));
        connect(tilt, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
            mMoveTilt = v.toDouble();
            applyTransform();
        });
        connect(tilt, &QVariantAnimation::finished, this, [this]() {
            mMoveTilt = 0.0; applyTransform();
        });
    } else {
        tilt->stop();
    }
    // 倾斜衰减时长与移动时长解耦：重排时 moveTo 常用 60ms 的小步移动，若倾斜也只持续 60ms
    // 就一闪而过看不见。固定用 ~300ms 衰减，让"被挤走→倾斜→回正"清晰可见。
    tilt->setDuration(qMax(durationMs, 300));
    tilt->setStartValue(tiltMax);
    tilt->setEndValue(0.0);
    tilt->setEasingCurve(QEasingCurve::OutCubic);
    tilt->start();
}

void CardItem::flip(double pivotDir) {
    // 对齐原版 card.lua Card:flip()：触发 pinch.x，使牌宽收缩到 0（绕 Y 轴翻面效果），
    // 然后在 sprite 翻面后再扩张回 1。仅缩 X，不缩 Y，避免出现垂直方向的塌缩。
    // pivotDir 决定 pinch 的枢轴：+1 绕右缘 → 视觉上"从右向左"翻面。
    mFlipPivotDir = pivotDir;
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
        // 翻面收尾后回到中心枢轴，避免影响后续 hover/juice 变换。
        connect(expand, &QPropertyAnimation::finished, this, [this]() {
            mFlipPivotDir = 0.0;
            applyTransform();
        });
        expand->start(QAbstractAnimation::DeleteWhenStopped);
    });

    shrink->start(QAbstractAnimation::DeleteWhenStopped);
}

void CardItem::flip3D(int durationMs) {
    // 以牌面左缘为竖轴、右缘朝观察者方向（屏幕外）转出：φ 从 0(背面正对)扫到 π(正面正对)，
    // 到 π/2（侧棱）时换牌面。applyTransform 的 mFlip3D 分支据此做透视翻面。
    mFlip3D = true;
    mFlipAngle = 0.0;
    mFlipFaceSwapped = false;
    applyTransform();
    auto *anim = new QVariantAnimation(this);
    anim->setDuration(qMax(1, durationMs));
    anim->setStartValue(0.0);
    anim->setEndValue(M_PI);
    // 角度匀速推进：宽度 = |cosφ| 自然给出"两端(整面)停留、正中(侧棱)飞快掠过"的翻面手感，
    // 对齐原版 pinch.x 的观感，不再额外叠缓动。
    anim->setEasingCurve(QEasingCurve::Linear);
    connect(anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        mFlipAngle = v.toDouble();
        if (!mFlipFaceSwapped && mFlipAngle >= M_PI / 2.0) {
            mData.faceUp = !mData.faceUp;   // 侧棱处换面：背面 → 正面
            mFlipFaceSwapped = true;
            update();
        }
        applyTransform();
    });
    connect(anim, &QVariantAnimation::finished, this, [this]() {
        mFlip3D = false;
        mFlipAngle = 0.0;
        mFlipFaceSwapped = false;
        applyTransform();
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void CardItem::showBackImmediate() {
    mFlip3D = false;
    mFlipAngle = 0.0;
    mFlipFaceSwapped = false;
    mFlipXScale = 1.0;
    if (mData.faceUp) mData.faceUp = false;
    update();
    applyTransform();
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

void CardItem::playDealFlourish(int durationMs)
{
    setTransformOriginPoint(WIDTH / 2.0, HEIGHT / 2.0);

    // 由远及近：从 0.86 放大到 1.0（缩放属性与 hover/juice 共用 "scale"，入场时不会被 hover 打断）。
    auto *sc = new QPropertyAnimation(this, "scale", this);
    sc->setDuration(durationMs);
    sc->setStartValue(0.86);
    sc->setEndValue(1.0);
    sc->setEasingCurve(QEasingCurve::OutCubic);

    // 绕 Y 轴翻面入场：flipXScale 0.62→1.0（applyTransform 里作用为 X 方向 pinch），
    // 让卡在飞行中像立体翻转过来，而不是平面滑入。OutBack 收尾有轻微回弹更有"落位"感。
    setFlipXScale(0.62);
    auto *fx = new QPropertyAnimation(this, "flipXScale", this);
    fx->setDuration(durationMs);
    fx->setStartValue(0.62);
    fx->setEndValue(1.0);
    fx->setEasingCurve(QEasingCurve::OutBack);

    sc->start(QAbstractAnimation::DeleteWhenStopped);
    fx->start(QAbstractAnimation::DeleteWhenStopped);
}

void CardItem::dealFlyIn(const QPointF &from, const QPointF &target, int delayMs, int durationMs,
                        bool flipUp)
{
    setPos(from);
    // scale 绕牌面中心缩放：否则默认绕 (0,0) 左上角缩，起飞时小牌会偏离牌堆中心。
    // 让 0.86 的小牌稳稳压在牌堆顶上，视觉上就是"从牌堆顶浮起"。
    setTransformOriginPoint(WIDTH / 2.0, HEIGHT / 2.0);
    // 停在牌库时"背面朝上"（像牌堆上一张待发的牌）；起飞时再翻到正面。
    // 仅当这张牌最终应正面朝上时才这么做——Boss 发的背面牌保持背面、不翻。
    if (flipUp) {
        mData.faceUp = false;
        mFlipXScale = 1.0;
        update();
    }
    // 起飞：用与常规重排同一套 moveTo（"CardMoveAnim"）驱动位移——这样飞行途中若被后续
    // 发牌挤到新位置，moveTo 能从当前位置平滑改向（复刻原版多张牌同时用弹簧移动、互相让位），
    // 而不是各自跑一段互不相干的动画产生打架。伴随由远及近(scale)、绕 Y 轴翻面(flip)、速度扭曲。
    QPointer<CardItem> self = this;
    auto launch = [self, target, durationMs, flipUp]() {
        if (!self) return;
        auto *sc = new QPropertyAnimation(self, "scale", self);
        sc->setDuration(qMax(1, durationMs - 20));
        sc->setStartValue(0.86);
        sc->setEndValue(1.0);
        sc->setEasingCurve(QEasingCurve::OutCubic);
        sc->start(QAbstractAnimation::DeleteWhenStopped);
        self->setVelocityTiltTracking(true);          // 起飞即开启速度扭曲追踪
        self->springTo(target);                       // 速度保持弹簧：被后续发牌改向也丝滑
        // 背面朝上飞入 → 在"即将落位"时才翻面：翻面与飞行末段收尾重叠进行，落位即翻完，
        // 不再是"落定 → 停一拍 → 才翻"，消掉明显的后摇。
        QTimer::singleShot(durationMs * 55 / 100, self, [self, flipUp]() {
            if (!self) return;
            self->setVelocityTiltTracking(false);     // 收回速度倾斜
            if (flipUp) self->flip3D(170);            // 临近落位翻到正面
        });
    };
    if (delayMs > 0) QTimer::singleShot(delayMs, this, launch);
    else launch();
}

void CardItem::applyTransform()
{
    // 中心点
    qreal cx = WIDTH  / 2.0;
    qreal cy = HEIGHT / 2.0;

    // 透视参数:鼠标越往边缘,倾斜越大,深度感越明显
    const double totalTiltX = mHoverTiltEnabled
        ? (mHovered ? mHoverTiltX : mAmbientTiltX)
        : 0.0;
    const double totalTiltY = mHoverTiltEnabled
        ? (mHovered ? mHoverTiltY : mAmbientTiltY)
        : 0.0;
    qreal tiltX = qDegreesToRadians(totalTiltX);
    qreal tiltY = qDegreesToRadians(totalTiltY);
    qreal zRot  = qDegreesToRadians(mBaseRotation);

    // ── 翻面分支（发牌入场）──────────────────────────────────
    // 100% 对齐原版 pinch.x：move_wh 只收 VT.w、VT.x(左缘) 不动 → 卡牌以【左缘为轴】做纯水平
    // 收缩。牌始终保持矩形（只是变窄），收到 0 宽换面再展开，绝不产生透视梯形那种别扭形状。
    if (mFlip3D) {
        const double w = std::abs(std::cos(mFlipAngle));   // 宽度比例 1→0→1（对齐 VT.w/T.w）
        // 绕左缘(局部 x=0)的水平收缩：右缘向左收拢、左缘不动，矩形保持矩形。
        QTransform pinch; pinch.scale(w, 1.0);
        // 扇形 Z 旋转（+抖动/速度倾斜）绕牌面中心，叠在收缩之后。
        QTransform Bpre;  Bpre.translate(-cx, -cy);
        QTransform R;     R.rotateRadians(zRot + qDegreesToRadians(mJitterRot + mDragTilt + mMoveTilt));
        QTransform Bpost; Bpost.translate(cx, cy);
        setTransform(pinch * (Bpre * R * Bpost));
        if (mShadow) {
            mShadow->setTransformOriginPoint(WIDTH / 2.0, HEIGHT / 2.0);
            mShadow->setRotation(mBaseRotation + mJitterRot + mDragTilt + mMoveTilt);
        }
        return;
    }

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

    // 应用 Z 轴扇形旋转 + 悬浮抖动 + 拖拽速度倾斜 + 重排移动倾斜叠加。
    t.rotateRadians(zRot + qDegreesToRadians(mJitterRot + mDragTilt + mMoveTilt));

    // 翻牌时的水平 pinch（仅 X 缩放）→ 模拟绕 Y 轴翻面，对齐原版 pinch.x。
    // 当前局部坐标系原点在牌面中心，右缘 = +cx、左缘 = -cx。
    // mFlipPivotDir=+1 时把枢轴移到右缘：牌向右缘收拢/展开，视觉上呈"从右向左"扫过翻面。
    if (mFlipXScale != 1.0) {
        const qreal pivot = mFlipPivotDir * cx;
        if (!qFuzzyIsNull(pivot)) {
            t.translate(pivot, 0.0);
            t.scale(mFlipXScale, 1.0);
            t.translate(-pivot, 0.0);
        } else {
            t.scale(mFlipXScale, 1.0);
        }
    }

    t.translate(-cx, -cy);
    setTransform(t);

    // 阴影只跟 z 旋转（扇形 + jitter + dragTilt），不吃 hover 透视——避免 hover 时阴影
    // 被透视投影"拽下来"。位置和缩放各自通过 itemChange 同步。
    if (mShadow) {
        mShadow->setTransformOriginPoint(WIDTH / 2.0, HEIGHT / 2.0);
        mShadow->setRotation(mBaseRotation + mJitterRot + mDragTilt + mMoveTilt);
    }
}

void CardItem::triggerHoverJitter()
{
    // 加强版 hover 抖动：原版进入悬浮时有一个明显的"弹一下"，之前 0.8° 太弱。
    // 这里把峰值放大到 ±2.4°，并把回弹做成 over-shoot（先回到 -0.8° 反弹再回 0），
    // 同时同步一只快速 scale 脉冲（×1.06 → ×1.0）让卡片有"被指尖戳了一下"的弹性。
    const double dir = (QRandomGenerator::global()->bounded(2) == 0) ? -1.0 : 1.0;
    const double peakRot = 0.7 * dir;
    const double overshoot = -0.18 * dir;

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
    up->setEndValue(qMax(scale() + 0.012, 1.052));
    up->setEasingCurve(QEasingCurve::OutQuad);
    auto *down = new QPropertyAnimation(this, "scale");
    down->setDuration(150);
    down->setStartValue(qMax(scale() + 0.012, 1.052));
    down->setEndValue(1.04);
    down->setEasingCurve(QEasingCurve::OutQuad);
    scaleAnim->addAnimation(up);
    scaleAnim->addAnimation(down);
    up->setParent(scaleAnim); down->setParent(scaleAnim);
    scaleAnim->start(QAbstractAnimation::DeleteWhenStopped);
}

void CardItem::ensureAmbientTimer()
{
    if (sCardAmbientTimer) return;
    sCardAmbientClock.start();
    sCardAmbientTimer = new QTimer(QCoreApplication::instance());
    sCardAmbientTimer->setTimerType(Qt::CoarseTimer);
    QObject::connect(sCardAmbientTimer, &QTimer::timeout, []() {
        const double seconds = sCardAmbientClock.elapsed() / 1000.0;
        const auto items = sAmbientCards.values();
        for (CardItem *item : items) {
            if (item) item->updateAmbientTilt(seconds);
        }
    });
    sCardAmbientTimer->start(33);
}

void CardItem::springTo(const QPointF &target)
{
    // 停掉基于 QPropertyAnimation 的位移动画，避免与弹簧同时写 pos。
    if (auto *old = findChild<QPropertyAnimation*>(QStringLiteral("CardMoveAnim"),
                                                   Qt::FindDirectChildrenOnly))
        old->stop();
    mSpringTarget = target;
    if (!mSpringActive) {
        mSpringActive = true;
        sSpringCards.insert(this);
        ensureSpringTimer();
    }
    // 已在弹簧中：只更新目标，速度自然延续 → 改向平滑，不重启缓动。
}

void CardItem::advanceSpring(double dt)
{
    if (dt <= 0.0) return;
    if (dt > 0.05) dt = 0.05;                 // 掉帧后限制单步，避免一大跳
    const double omega = 18.0;                // 角频率：临界阻尼下 settle ≈ 0.22s（收尾更利落、后摇更小）
    QPointF x = pos();
    QPointF d = mSpringTarget - x;
    // 临界阻尼弹簧（半隐式，速度保持）：v += (ω²·d − 2ω·v)·dt；x += v·dt
    mSpringVel += (d * (omega * omega) - mSpringVel * (2.0 * omega)) * dt;
    x += mSpringVel * dt;
    if (QLineF(x, mSpringTarget).length() < 0.4 &&
        std::hypot(mSpringVel.x(), mSpringVel.y()) < 6.0) {
        mSpringVel = QPointF();
        mSpringActive = false;
        sSpringCards.remove(this);
        setPos(mSpringTarget);                 // 收敛：精确落到目标
        return;
    }
    setPos(x);
}

void CardItem::ensureSpringTimer()
{
    if (sSpringTimer) return;
    sSpringClock.start();
    sSpringLastMs = sSpringClock.elapsed();
    sSpringTimer = new QTimer(QCoreApplication::instance());
    sSpringTimer->setTimerType(Qt::PreciseTimer);   // 位移要顺，用精确时基
    QObject::connect(sSpringTimer, &QTimer::timeout, []() {
        const qint64 now = sSpringClock.elapsed();
        double dt = (now - sSpringLastMs) / 1000.0;
        sSpringLastMs = now;
        const auto items = sSpringCards.values();
        for (CardItem *item : items) if (item) item->advanceSpring(dt);
    });
    sSpringTimer->start(16);                          // ~60Hz
}

void CardItem::updateAmbientTilt(double seconds)
{
    if (!mHoverTiltEnabled || mHovered || mDragging || !scene() || !isVisible()) return;
    const QPointF tilt = BalatroMotion::ambientTiltDegrees(mAmbientId, seconds, mAmbientTiltStrength);
    mAmbientTiltY = tilt.x();
    mAmbientTiltX = tilt.y();
    applyTransform();
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
    // paper1 是很短很轻的纸声，原版 0.35 在我们的混音里几乎听不到——提到 0.8 让悬浮反馈清晰可闻。
    AudioManager::instance()->play(QStringLiteral("paper1"),
                                   0.9 + QRandomGenerator::global()->generateDouble() * 0.2,
                                   0.8);
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
    // hover 时明显的伪 3D 立体倾斜：鼠标在牌面边缘时 ±7°，中心几乎无倾斜。
    // （之前 ±3° 太弱几乎看不出立体感，用户反馈"没有悬浮效果"。）
    mHoverTiltY = qBound(-7.0, nx * 14.0, 7.0);
    mHoverTiltX = qBound(-7.0, ny * 14.0, 7.0);
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
