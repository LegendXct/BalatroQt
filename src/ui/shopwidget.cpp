#include "shopwidget.h"
#include "../card/consumableitem.h"
#include "../card/jokeritem.h"
#include "../audio/audiomanager.h"
#include "cardtooltipformat.h"
#include "balatroinfopanel.h"
#include <QRandomGenerator>
#include <QSequentialAnimationGroup>
#include <QVariantAnimation>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QPainter>
#include <QEvent>
#include <QPainterPath>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QTimer>
#include <QPointer>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QAbstractAnimation>
#include <QGraphicsOpacityEffect>
#include <QGuiApplication>
#include <QScreen>
#include <QCursor>
#include <QtGlobal>
#include <QVariant>
#include <QByteArray>
#include <cmath>
#include <limits>
#include "../utils/shadereffects.h"


static double shopUiScale()
{
    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (!screen) return 1.0;

    // 同 mainwindow.cpp / blindselectwidget.cpp：只用逻辑像素，避免高 DPR 屏被双倍缩放。
    const QSize logical = screen->availableGeometry().size();
    double scale = qMin(logical.width() / 1920.0, logical.height() / 1080.0);

    bool ok = false;
    const double overrideScale = QString::fromLocal8Bit(qgetenv("QT_BALATRO_UI_SCALE")).toDouble(&ok);
    if (ok && overrideScale > 0.1) scale = overrideScale;

    return qBound(0.58, scale, 2.35);
}

static int dp(int px)
{
    return qMax(1, int(std::round(px * shopUiScale())));
}

static int fontPx(int px)
{
    return qMax(1, int(std::round(px * 1.18 * shopUiScale())));
}

static QString editionDisplayName(Edition e)
{
    switch (e) {
    case Edition::Foil:        return "闪箔";
    case Edition::Holographic: return "镭射";
    case Edition::Polychrome:  return "多彩";
    case Edition::Negative:    return "负片";
    default:                   return "";
    }
}

static QString editionDescription(Edition e)
{
    switch (e) {
    case Edition::Foil:        return "闪箔：计分时 +50 筹码";
    case Edition::Holographic: return "镭射：计分时 +10 倍率";
    case Edition::Polychrome:  return "多彩：计分时 ×1.5 倍率";
    case Edition::Negative:    return "负片：不占用小丑槽，并增加 1 个小丑槽位";
    default:                   return "";
    }
}

static double shopAudioPitchJitter(double spread = 0.04)
{
    const double r = QRandomGenerator::global()->generateDouble() * 2.0 - 1.0;
    return 1.0 + r * spread;
}

static QString soundForOfferKind(OfferKind kind)
{
    switch (kind) {
    case OfferKind::Tarot:       return QStringLiteral("tarot1");
    case OfferKind::Planet:      return QStringLiteral("timpani");
    case OfferKind::Spectral:    return QStringLiteral("magic_crumple");
    case OfferKind::Pack:        return QStringLiteral("cardFan2");
    case OfferKind::Voucher:     return QStringLiteral("card3");
    case OfferKind::PlayingCard: return QStringLiteral("cardSlide1");
    case OfferKind::Joker:       return QStringLiteral("card1");
    }
    return QStringLiteral("card1");
}


namespace {
class ShopCardButton : public QPushButton
{
public:
    explicit ShopCardButton(QWidget *parent = nullptr) : QPushButton(parent)
    {
        setMouseTracking(true);
        setIcon(QIcon());
        setText(QString());
        setFlat(true);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setAutoFillBackground(false);
        setProperty("balatroAudio", QStringLiteral("card1"));
    }

    void setDisplayPixmap(const QPixmap &pix)
    {
        mPixmap = pix;
        // 同时缓存"剪影阴影"——把源 pixmap 的 alpha 通道复制出来填成半透明黑色。
        // 这样 booster 等异形卡包的阴影会自动贴合包外形（圆角、卡包翻角等），
        // 不再是一个比可见图像更宽的矩形。
        if (!pix.isNull()) {
            QImage img = pix.toImage().convertToFormat(QImage::Format_ARGB32);
            for (int y = 0; y < img.height(); ++y) {
                QRgb *row = reinterpret_cast<QRgb *>(img.scanLine(y));
                for (int x = 0; x < img.width(); ++x) {
                    const int a = qAlpha(row[x]);
                    if (a > 0)
                        row[x] = qRgba(0, 0, 0, a);
                }
            }
            mShadowPixmap = QPixmap::fromImage(img);
        } else {
            mShadowPixmap = QPixmap();
        }
        update();
    }

    // 被选中的商品 cardBtn 会移动到 -dp(36) 位置；shadow lift 同步拉到 0.5，让阴影"远一些"。
    void setLifted(bool lifted)
    {
        if (mLifted == lifted) return;
        mLifted = lifted;
        update();
    }

    // booster (异形) 用 alpha 剪影；joker / tarot / planet / spectral / voucher / playing card
    // 都是矩形 sprite，用双层圆角阴影更贴合手牌槽位的质感。
    void setUseSilhouetteShadow(bool useSilhouette)
    {
        mUseSilhouetteShadow = useSilhouette;
        update();
    }

    // 可见卡牌尺寸——button 本身比这个大一圈，用以容纳 hover 缩放。paint 都按这个尺寸居中。
    void setVisibleCardSize(const QSize &sz)
    {
        mVisibleCardSize = sz;
        update();
    }
    QSize visibleCardSize() const { return mVisibleCardSize; }
    bool isHovered() const { return mHovered; }
    bool hoverLiftEnabled() const { return !mDisableHoverLift; }
    void setDisableHoverLift(bool disabled)
    {
        if (mDisableHoverLift == disabled) return;
        mDisableHoverLift = disabled;
        update();
    }
    void setStaticRotation(double deg)
    {
        if (qFuzzyCompare(mStaticRotDeg + 1.0, deg + 1.0)) return;
        mStaticRotDeg = deg;
        update();
    }

protected:
    void enterEvent(QEnterEvent *event) override
    {
        mHovered = true;
        AudioManager::instance()->play(QStringLiteral("paper1"),
                                       0.9 + QRandomGenerator::global()->generateDouble() * 0.2,
                                       0.35);
        QPushButton::enterEvent(event);
        triggerHoverJitter();
        update();
    }

    // 鼠标按下 → mPressed=true 让阴影 lift 拉到拖动级别（与手牌点击一致）。
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            mPressed = true;
            update();
        }
        QPushButton::mousePressEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            mPressed = false;
            update();
        }
        QPushButton::mouseReleaseEvent(event);
    }

    // 与 CardItem / JokerItem 同款 hover 抖动：进入悬浮时 ±2.4° 顶峰 + -0.6° 反弹再回零。
    void triggerHoverJitter()
    {
        const double dir = (QRandomGenerator::global()->bounded(2) == 0) ? -1.0 : 1.0;
        const double peakRot   = 2.4 * dir;
        const double overshoot = -0.6 * dir;
        if (mJitterAnim) {
            mJitterAnim->stop();
            mJitterAnim->deleteLater();
            mJitterAnim = nullptr;
        }
        auto *seq = new QSequentialAnimationGroup(this);
        mJitterAnim = seq;
        auto makeStep = [this, seq](double from, double to, int dur, QEasingCurve::Type curve) {
            auto *a = new QVariantAnimation(seq);
            a->setDuration(dur);
            a->setStartValue(from);
            a->setEndValue(to);
            a->setEasingCurve(curve);
            QObject::connect(a, &QVariantAnimation::valueChanged, this,
                             [this](const QVariant &v) {
                mJitterRotDeg = v.toDouble();
                update();
            });
            seq->addAnimation(a);
        };
        makeStep(0.0, peakRot, 70, QEasingCurve::OutQuad);
        makeStep(peakRot, overshoot, 110, QEasingCurve::InOutQuad);
        makeStep(overshoot, 0.0, 120, QEasingCurve::OutCubic);
        QObject::connect(seq, &QSequentialAnimationGroup::finished, this, [this]() {
            mJitterRotDeg = 0.0;
            mJitterAnim = nullptr;
            update();
        });
        seq->start(QAbstractAnimation::DeleteWhenStopped);
    }

    void leaveEvent(QEvent *event) override
    {
        mHovered = false;
        mTiltX = 0.0;
        mTiltY = 0.0;
        QPushButton::leaveEvent(event);
        update();
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (mHovered && !mPixmap.isNull()) {
            // 和 CardItem / JokerItem 同方向：鼠标在牌面哪个角，牌面就按同一套
            // 原版顶点倾斜感响应，不再使用 QWidget shear 的反向近似。
            const qreal nx = event->position().x() / qMax(1, width()) - 0.5;
            const qreal ny = event->position().y() / qMax(1, height()) - 0.5;
            mTiltY = 0.0;
            mTiltX = 0.0;
        }
        QPushButton::mouseMoveEvent(event);
        update();
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, false);

        // ── 剪影阴影：用 mShadowPixmap (源 pixmap 的 alpha 复制) 偏移后绘制，
        //    booster 等异形包阴影会自动贴合包形状；jokers/consumables 的源是矩形 sprite，
        //    剪影也是矩形，自然就和手牌的圆角矩形阴影一致。
        if (!mPixmap.isNull() && !mShadowPixmap.isNull()) {
            // 沿父链找到 ShopWidget 作为"场景中央"参考。
            QWidget *topShop = parentWidget();
            while (topShop && topShop->parentWidget() &&
                   QString(topShop->metaObject()->className()) != QStringLiteral("ShopWidget")) {
                topShop = topShop->parentWidget();
            }
            qreal nx = 0.0;
            if (topShop && topShop->width() > 0) {
                const QPoint centerInShop = mapTo(topShop, QPoint(width()/2, height()/2));
                nx = (centerInShop.x() - topShop->width() / 2.0) / (topShop->width() / 2.0);
                nx = qBound(-1.0, nx, 1.0);
            }
            // 与手牌一致的 lift 优先级：press > selected(mLifted) > hover > rest。
            // 按下时拖到 0.85（手牌拖动也是这个值），阴影距离明显拉远。
            const qreal lift = mPressed ? 0.85
                                        : (mLifted ? 0.55
                                                   : ((mHovered && !mDisableHoverLift) ? 0.30 : 0.0));
            const qreal shadowHeight = 0.1 + 0.45 * lift;

            // 卡图实际落点：button 中心 + hover 时 -4 px。剪影按 pixSize 绘制——
            // 与下面 drawPixmap 用同一套几何，shadow 就完全跟随可见图像形状。
            QSize pixSize = mPixmap.size();
            // 按"可见卡牌"尺寸缩放（button 比这一圈大，预留 hover 放大溢出空间）。
            const QSize target = mVisibleCardSize.isValid() ? mVisibleCardSize
                                                            : QSize(width(), height());
            pixSize.scale(target, Qt::KeepAspectRatio);
            const qreal cx = width() / 2.0;
            const qreal cy = height() / 2.0 + ((mHovered && !mDisableHoverLift) ? -8.0 : 0.0);
            const qreal w = pixSize.width();
            const qreal h = pixSize.height();
            // 偏移按原版 card 单位换算到可见卡牌像素（1 单位 = h/CARD_H(2.75) ≈ w/CARD_W(2.05)），
            // 不再用固定 32 px——否则阴影离本体太近几乎看不见。原版 shadow_parrallax.y=-1.5。
            const qreal offX = -nx * 1.5 * shadowHeight * (w / 2.0488);
            const qreal offY =  1.5 * shadowHeight * (h / 2.7512);
            const QRectF shadowRect(cx - w/2.0 + offX, cy - h/2.0 + offY, w, h);

            p.save();
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setRenderHint(QPainter::SmoothPixmapTransform, true);
            p.setPen(Qt::NoPen);
            // 所有商品都按真实轮廓投影：用 alpha 剪影 pixmap 画两层（外圈放大低透明=软边、
            // 内圈实尺寸），与 cardshadow.cpp 的 CardShadowItem 同一套配色/比例。优惠券、
            // 烧焦、异形小丑、卡包等非矩形外形都能贴合，不再溢出成圆角矩形。
            const QRectF srcR(0, 0, mShadowPixmap.width(), mShadowPixmap.height());
            const qreal expand = 0.5 + 2.0 * lift;
            p.setOpacity((20 + 25 * lift) / 255.0);
            p.drawPixmap(shadowRect.adjusted(-expand, -expand, expand, expand), mShadowPixmap, srcR);
            p.setOpacity((45 + 40 * lift) / 255.0);
            p.drawPixmap(shadowRect, mShadowPixmap, srcR);
            p.restore();
        }

        // 原版卡牌/卡包自己带阴影和高光，商店槽位按钮不应该再画一层黑底。
        // 之前的黑色矩形和卡片后面的脏阴影就是这里的按钮背景造成的。
        if (mPixmap.isNull()) return;
        // 卡图按"可见卡牌"尺寸绘制——button 本身比这个大一圈（hover 缩放预留），
        // 这样 hover 1.045 后还在 button rect 内，卡顶不会被裁。
        QSize target = mVisibleCardSize.isValid() ? mVisibleCardSize
                                                  : QSize(width(), height());
        QSize pixSize = mPixmap.size();
        pixSize.scale(target, Qt::KeepAspectRatio);
        QRectF pr(QPointF(-pixSize.width() / 2.0, -pixSize.height() / 2.0), pixSize);

        p.save();
        p.setOpacity(isEnabled() ? 1.0 : 0.42);
        p.translate(width() / 2.0, height() / 2.0 + ((mHovered && !mDisableHoverLift) ? -8.0 : 0.0));

        const qreal tiltX = mTiltX * 3.14159265358979323846 / 180.0;
        const qreal tiltY = mTiltY * 3.14159265358979323846 / 180.0;
        const qreal cosY = std::cos(tiltY);
        const qreal cosX = std::cos(tiltX);
        const qreal sinY = std::sin(tiltY);
        const qreal sinX = std::sin(tiltX);

        QTransform persp;
        persp.setMatrix(
            cosY,           sinY * sinX,    0.0048 * sinY,
            0,              cosX,           0.0048 * sinX,
            0,              0,              1
        );
        QTransform t;
        // hover 1.045 放大现在能正常工作——button 多留了 14 px overflow 空间。
        const qreal hoverScale = 1.0 + (mHovered ? 0.045 : 0.0);
        t.scale(hoverScale, hoverScale);
        // hover 抖动：进入悬浮时 ±2.4° 弹一下；和 CardItem/JokerItem 一致。
        if (!qFuzzyIsNull(mStaticRotDeg + mJitterRotDeg))
            t.rotate(mStaticRotDeg + mJitterRotDeg);
        t = persp * t;
        p.setTransform(t, true);

        p.drawPixmap(pr, mPixmap, QRectF(0, 0, mPixmap.width(), mPixmap.height()));
        p.restore();
    }

private:
    QPixmap mPixmap;
    QPixmap mShadowPixmap;     // 缓存的剪影阴影，setDisplayPixmap 时按 alpha 重建。
    QSize mVisibleCardSize;    // 可见卡牌尺寸（button 比这个大一圈给 hover 缩放）。空则用 button rect。
    bool mHovered = false;
    bool mLifted = false;
    bool mPressed = false;
    bool mDisableHoverLift = false;
    bool mUseSilhouetteShadow = false; // true 给 booster 用 (异形)，否则双层圆角矩形 (joker/consumable/voucher)
    qreal mTiltX = 0.0; // degrees, same direction as CardItem/JokerItem
    qreal mTiltY = 0.0;
    double mStaticRotDeg = 0.0;
    double mJitterRotDeg = 0.0;
    QSequentialAnimationGroup *mJitterAnim = nullptr;
};
}

ShopWidget::ShopWidget(GameState *gs,
                       const QFont &cnFont, const QFont &pixelFont,
                       QWidget *parent)
    : QWidget(parent), mGS(gs), mCNFont(cnFont), mPixelFont(pixelFont)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setStyleSheet("background: transparent;");   // 原版商店外侧不额外铺黑色遮罩
    buildUi();
}

void ShopWidget::buildUi()
{
    mPanel = new QWidget(this);
    mPanel->setObjectName("shopPanel");
    // 商品区按原版商店的“两行紧凑比例”排布；实际面板大小在 layoutPanel() 中随可用区域缩放。
    mPanel->setMinimumSize(dp(900), dp(620));
    mPanel->setAttribute(Qt::WA_StyledBackground, true);
    mPanel->setStyleSheet(
        "QWidget#shopPanel {"
        "  background:rgba(35,48,51,235);"
        "  border: 3px solid #fe5f55;"
        "  border-radius: 18px;"
        "}"
        );

    auto *root = new QVBoxLayout(mPanel);
    root->setContentsMargins(dp(16), dp(12), dp(16), dp(12));
    root->setSpacing(dp(8));

    // 金额栏已移除——金币显示在主场景顶部，商店内不再重复。商品和礼包两行因此可以
    // 各自占用面板更大比例的高度。mLblGold 保留指针为 nullptr，refresh 时跳过。
    mLblGold = nullptr;

    // ── 上栏:[Next 红 + Reroll 绿] | 商品区(2 槽) ──
    auto *upperRow = new QWidget(mPanel);
    auto *uhbl = new QHBoxLayout(upperRow);
    uhbl->setContentsMargins(0, 0, 0, 0);
    uhbl->setSpacing(dp(12));

    // 左:两按钮竖排
    auto *btnCol = new QWidget(upperRow);
    btnCol       ->setFixedWidth(dp(168));
    auto *bvbl = new QVBoxLayout(btnCol);
    bvbl->setContentsMargins(0, 0, 0, 0);
    bvbl->setSpacing(dp(10));

    mBtnNextRound = new QPushButton("下一个\n回合", btnCol);
    mBtnNextRound->setFixedSize(dp(168), dp(130));
    QFont nrf = mCNFont; nrf.setPixelSize(fontPx(22)); nrf.setBold(true);
    mBtnNextRound->setFont(nrf);
    mBtnNextRound->setCursor(Qt::PointingHandCursor);
    mBtnNextRound->setStyleSheet(
        "QPushButton { background:#fe5f55; color:white; border:none;"
        "              border-radius: 10px; padding: 4px; }"
        "QPushButton:hover { background:#ff7066; }"
        );
    connect(mBtnNextRound, &QPushButton::clicked, this, &ShopWidget::leaveClicked);
    bvbl->addWidget(mBtnNextRound);

    mBtnReroll = new QPushButton("重抽\n$5", btnCol);
    mBtnReroll   ->setFixedSize(dp(168), dp(130));
    mBtnReroll->setFont(nrf);
    mBtnReroll->setCursor(Qt::PointingHandCursor);
    mBtnReroll->setStyleSheet(
        "QPushButton { background:#4bc292; color:white; border:none;"
        "              border-radius: 10px; padding: 4px; }"
        "QPushButton:hover { background:#5ed4a4; }"
        "QPushButton:disabled { background:#4f6367; color:#888; }"
        );
    connect(mBtnReroll, &QPushButton::clicked, this, &ShopWidget::onReroll);
    bvbl->addWidget(mBtnReroll);

    uhbl->addWidget(btnCol);

    // 右:商品区内框
    auto *shopBox = new QWidget(upperRow);
    shopBox->setObjectName("shopBox");
    shopBox->setAttribute(Qt::WA_StyledBackground, true);
    shopBox->setStyleSheet(
        "QWidget#shopBox { background:rgba(57,72,76,230); border:none; border-radius:14px; }"
        );
    auto *shbl = new QHBoxLayout(shopBox);
    // 原版 shop 槽位间几乎贴着排，间距比 booster 行更紧；这里把 padding/spacing 压到 8/6 dp。
    shbl->setContentsMargins(dp(8), dp(8), dp(8), dp(8));
    shbl->setSpacing(dp(6));
    shbl->setAlignment(Qt::AlignCenter);

    for (int i = 0; i < 4; ++i) {
        OfferUi ou = createOfferSlot(shopBox, false);
        ou.card->setProperty("shopOfferIdx", i);   // refresh() 用来判断 useBtn 是否可点
        // 整张卡点击 → 切换选中状态（不再直接购买）。
        connect(ou.cardBtn, &QPushButton::clicked, this, [this, i]() { onShopCardClicked(i); });
        if (ou.buyBtn)
            connect(ou.buyBtn, &QPushButton::clicked, this, [this, i]() { onBuyShop(i); });
        if (ou.useBtn)
            connect(ou.useBtn, &QPushButton::clicked, this, [this, i]() { onBuyAndUseShop(i); });
        shbl->addWidget(ou.card);
        mShopUi.append(ou);
    }
    uhbl->addWidget(shopBox, 1);

    root->addWidget(upperRow);

    // ── 下栏:[Voucher 槽] | [Booster 区 2 槽] ──
    auto *lowerRow = new QWidget(mPanel);
    auto *lhbl = new QHBoxLayout(lowerRow);
    lhbl->setContentsMargins(0, 0, 0, 0);
    lhbl->setSpacing(dp(12));

    // Voucher 单槽：原版下半区左侧优惠券，固定售价 $10。
    // 最小尺寸跟随放大后的 normal slot（containerW=180、containerH≈330）。
    mVoucherBox = new QWidget(lowerRow);
    mVoucherBox->setObjectName("voucherBox");
    // 原版优惠券区只比一张牌宽,但我们引入了 Voucher Tag 复制后可能出现 2~3 张,
    // 用一个较宽的初始尺寸+足够高度,确保扇形+弧形排列不会被裁切;真实宽度仍由
    // ensureVoucherBoxSize() 根据 offers.size() 动态调整。
    mVoucherBox->setMinimumSize(dp(220), dp(372));
    mVoucherBox->setAttribute(Qt::WA_StyledBackground, true);
    mVoucherBox->setStyleSheet(
        "QWidget#voucherBox { background:rgba(57,72,76,230); border:none; border-radius:14px; }"
        );

    OfferUi vu = createOfferSlot(mVoucherBox, false);
    vu.card->setProperty("voucherSlotIdx", 0);
    connect(vu.cardBtn, &QPushButton::clicked, this, [this]() { onVoucherCardClicked(0); });
    if (vu.buyBtn)
        connect(vu.buyBtn, &QPushButton::clicked, this, [this]() { onBuyVoucher(0); });
    mVoucherUi.append(vu);
    lhbl->addWidget(mVoucherBox);

    // Booster 区
    auto *boosterBox = new QWidget(lowerRow);
    boosterBox->setObjectName("boosterBox");
    boosterBox->setAttribute(Qt::WA_StyledBackground, true);
    boosterBox->setStyleSheet(
        "QWidget#boosterBox { background:rgba(57,72,76,230); border:none; border-radius:14px; }"
        );
    auto *bhbl = new QHBoxLayout(boosterBox);
    // booster 行间距比 shop 上方略宽，但比之前 12dp 紧，避免显得稀疏。
    bhbl->setContentsMargins(dp(8), dp(6), dp(8), dp(6));
    bhbl->setSpacing(dp(8));
    bhbl->setAlignment(Qt::AlignCenter);

    for (int i = 0; i < 2; ++i) {
        OfferUi ou = createOfferSlot(boosterBox, true);
        ou.card->setProperty("boosterSlotIdx", i);
        connect(ou.cardBtn, &QPushButton::clicked, this, [this, i]() { onBoosterCardClicked(i); });
        if (ou.buyBtn)
            connect(ou.buyBtn, &QPushButton::clicked, this, [this, i]() { onBuyBooster(i); });
        bhbl->addWidget(ou.card);
        mBoosterUi.append(ou);
    }
    lhbl->addWidget(boosterBox, 1);

    root->addWidget(lowerRow);

    buildInfoPanel();
}

ShopWidget::OfferUi ShopWidget::createOfferSlot(QWidget *parent, bool isBooster)
{
    OfferUi ou;
    const bool isVoucherSlot = parent && parent->objectName() == QStringLiteral("voucherBox");
    // 原版 game.lua:3153：booster 用 G.CARD_W*1.27 × G.CARD_H*1.27 显示——两个轴都乘 1.27
    // 保持与扑克牌相同的 0.747 长宽比。我们的 JokerItem/ConsumableItem 是 162×218，所以：
    //   非礼包槽：162 × 218
    //   礼包槽 ：162×1.27 × 218×1.27 ≈ 206 × 278
    const int slotW = isBooster ? 206 : 162;
    const int slotH = isBooster ? 278 : 218;
    // 普通商品槽右侧专门预留给“购买&使用”侧按钮。
    // 关键点：预留宽度不参与卡牌列居中，否则按钮只拿到一半预留空间，右侧会被容器裁掉。
    const int actionReserveW = (isBooster || isVoucherSlot) ? 0 : 48;
    // cardBtn 给 hover 1.045 缩放预留 ±7 px overflow 空间：实际 paint pixmap 大小是
    // slotW × slotH（与槽位 JokerItem/ConsumableItem 同尺寸），但 button 本身略大，
    // 这样 hover 放大时图像顶部不会被 widget rect 裁掉。
    const int btnOverflow = 14;     // 7 px each side
    const int btnW = slotW + btnOverflow;
    const int btnH = slotH + btnOverflow;
    const int containerW = btnW + actionReserveW;
    const int priceTabH = 26;
    // 选中时 cardBtn 会向上 move dp(36)，必须在 ou.card 顶部预留同等空间，
    // 否则 Qt 把超出 ou.card 顶边的 cardBtn 像素裁掉，看着像"商品上半截没了"。
    const int selectionLiftReserve = isVoucherSlot ? 24 : 36;
    const int topReserve = (priceTabH - 1) + selectionLiftReserve;
    const int containerH = topReserve + btnH + (isVoucherSlot ? 64 : 12);

    ou.card = new QWidget(parent);
    ou.card->setFixedSize(dp(containerW), dp(containerH));
    ou.card->setStyleSheet("background:transparent;");

    auto *vbl = new QVBoxLayout(ou.card);
    // 给上面留出 price tab 露出 + 选中升起预留——cardBtn 从这之下开始。
    vbl->setContentsMargins(0, dp(topReserve), dp(actionReserveW), 0);
    vbl->setSpacing(dp(2));
    vbl->setAlignment(Qt::AlignCenter);

    // 顶部 $X 价格标签(深底圆角)
    // 关键：parent 改成 ShopWidget(this) 而不是 ou.card——cardBtn 选中上升 36 px 后
    // priceLbl 会跑到 ou.card 上方甚至超出 mPanel，子 widget 会被父矩形裁切。
    // 挂在 ShopWidget 这层就能延伸到 mPanel 之外（金额栏移除后空出来的区域）。
    ou.priceLbl = new QLabel("$0", this);
    // 默认隐藏——fillSlot 在 offer 实际存在时显式 setVisible(true)。否则空槽位的
    // priceLbl 会带着默认 "$0" 文字在 ShopWidget 左上角浮着。
    ou.priceLbl->hide();
    ou.priceLbl->raise();
    // 价格 tab：上圆下方风格，几乎完全位于卡顶上方（1 px overlap）；
    // cardBtn 选中升起时 eventFilter 会捕获 QEvent::Move 把这只 label 同步上移。
    ou.priceLbl->setFixedSize(dp(86), dp(priceTabH));
    QFont pf = mCNFont; pf.setPixelSize(fontPx(17)); pf.setBold(true);
    ou.priceLbl->setFont(pf);
    ou.priceLbl->setAlignment(Qt::AlignCenter);
    // 原版价格标签：外层近黑 #1c2426 + 内层深灰 #374244，统一上圆下方风格。
    // 用 3 px 边框模拟"外层黑色背景包住内层灰色前景"的双圆角矩形效果。
    ou.priceLbl->setStyleSheet(
        "color:#f3b958; background:#374244;"
        "border:3px solid #1c2426;"
        "border-top-left-radius:10px;"
        "border-top-right-radius:10px;"
        "border-bottom-left-radius:0px;"
        "border-bottom-right-radius:0px;"
        "border-bottom: 0px;"
    );
    // 不加入 QVBoxLayout——靠 eventFilter 与 cardBtn 同步位置（坐标在 ShopWidget 内）。

    // 卡图(整张点击)
    auto *scb = new ShopCardButton(ou.card);
    ou.cardBtn = scb;
    // 礼包用剪影阴影（包形不规则），其它矩形 sprite 走和槽位一致的双层圆角阴影。
    scb->setUseSilhouetteShadow(isBooster);
    scb->setDisableHoverLift(isVoucherSlot);
    // button 留出 hover 缩放 overflow 空间——内部 pixmap 仍按 slotW × slotH 居中绘制，
    // hover 1.045 时仍在 button rect 内不被裁。
    ou.cardBtn->setFixedSize(dp(btnW), dp(btnH));
    scb->setVisibleCardSize(QSize(dp(slotW), dp(slotH)));
    ou.cardBtn->setProperty("balatroAudio", isBooster ? QStringLiteral("cardFan2")
                                                       : QStringLiteral("card1"));
    ou.cardBtn->setCursor(Qt::PointingHandCursor);
    ou.cardBtn->installEventFilter(this);
    ou.cardBtn->setStyleSheet(
        "QPushButton { background:transparent; border-radius:0px; border:none; }"
        "QPushButton:hover { background:transparent; }"
        "QPushButton:disabled { background:transparent; }"
        );

    vbl->addWidget(ou.cardBtn, 0, Qt::AlignCenter);

    ou.imageLbl = nullptr;

    // 选中商品时显示的操作按钮：
    //   - buyBtn (下方横条) 颜色按 offer 类型在 refresh() 中改写：
    //     · 礼包 → 绿色"打开"
    //     · 优惠券 → 黄色"兑换"
    //     · 小丑 / 扑克牌 → 黄色"购买"
    //     · 塔罗 / 星球 / 幻灵 → 黄色"购买" + 右侧 useBtn 红色"购买&使用"
    //   - useBtn (右侧竖向) 只有消耗牌位才显示，对应原版 UI_definitions.lua 的 buy_and_use 按钮。
    QFont bbf = mCNFont; bbf.setPixelSize(fontPx(14)); bbf.setBold(true);

    // 原版购买/打开/兑换按钮是挂在卡牌底边的短横条，宽度略大、厚度够，
    // 这样卡牌上浮后按钮不会显得又窄又矮。
    ou.buyBtn = new QPushButton("购买", ou.card);
    ou.buyBtn->setCursor(Qt::PointingHandCursor);
    ou.buyBtn->setFixedSize(dp(isVoucherSlot ? slotW * 78 / 100 : slotW * 64 / 100), dp(38));
    ou.buyBtn->setFont(bbf);
    ou.buyBtn->hide();

    // 购买&使用：贴卡片右侧。左边与牌面相接，不做圆角；只保留右侧圆角。
    ou.useBtn = new QPushButton("购买\n&使用", ou.card);
    ou.useBtn->setCursor(Qt::PointingHandCursor);
    ou.useBtn->setFixedSize(dp(40), dp(54));
    ou.useBtn->setFont(bbf);
    ou.useBtn->setStyleSheet(QString(
        "QPushButton { background:#fe5f55; color:white; border:2px solid rgba(255,255,255,90);"
        " border-top-left-radius:0px; border-bottom-left-radius:0px;"
        " border-top-right-radius:8px; border-bottom-right-radius:8px;"
        " font-weight:bold; padding:0; }"
        "QPushButton:hover { background:#ff7066; border:2px solid rgba(255,255,255,170);"
        " border-top-left-radius:0px; border-bottom-left-radius:0px;"
        " border-top-right-radius:8px; border-bottom-right-radius:8px; }"
        "QPushButton:disabled { background:#7a3c39; color:#cab4b2; border:2px solid #5a2a28;"
        " border-top-left-radius:0px; border-bottom-left-radius:0px;"
        " border-top-right-radius:8px; border-bottom-right-radius:8px; }"
    ));
    ou.useBtn->hide();
    ou.actionRow = nullptr;
    Q_UNUSED(isBooster);

    // 商品名字不再常驻——悬浮 info 已经显示完整名字 + 描述。保留 ou.nameLbl 指针但永远 hide()，
    // 不进 layout，避免槽位底部多一片空白。
    ou.nameLbl = new QLabel("", ou.card);
    ou.nameLbl->hide();

    return ou;
}

void ShopWidget::buildInfoPanel()
{
    if (mInfoPanel) return;
    // 与主场景 mHoverTooltip 同款 BalatroInfoCluster：暗底圆角 + 白底文字栏 + 并排副面板。
    mInfoPanel = new BalatroInfoCluster(mCNFont, this);
    mInfoPanel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    mInfoPanel->hide();
}

void ShopWidget::showOfferInfo(QWidget *source)
{
    if (!source) return;
    buildInfoPanel();

    // 通过 cardBtn 的 parent (ou.card) 上的 shopOfferIdx/voucherSlotIdx/boosterSlotIdx
    // 反查当前 ShopOffer——拿到 offer 后可按 kind 装配 rarity / edition / granted enhancement
    // 等副面板，跟主场景 hover 表现一致。
    QWidget *container = source->parentWidget();
    const ShopOffer *offer = nullptr;
    if (container) {
        const QVariant shopIdxV    = container->property("shopOfferIdx");
        const QVariant voucherIdxV = container->property("voucherSlotIdx");
        const QVariant boosterIdxV = container->property("boosterSlotIdx");
        if (shopIdxV.isValid()) {
            const auto &offers = mGS->shop().shopOffers();
            const int i = shopIdxV.toInt();
            if (i >= 0 && i < offers.size()) offer = &offers[i];
        } else if (voucherIdxV.isValid()) {
            const auto &offers = mGS->shop().voucherOffers();
            const int i = voucherIdxV.toInt();
            if (i >= 0 && i < offers.size()) offer = &offers[i];
        } else if (boosterIdxV.isValid()) {
            const auto &offers = mGS->shop().boosterOffers();
            const int i = boosterIdxV.toInt();
            if (i >= 0 && i < offers.size()) offer = &offers[i];
        }
    }

    // 退化：找不到 offer 时，用 widget 上缓存的 title/body 字符串构造一个最简 cluster。
    const QString fallbackTitle = source->property("infoTitle").toString();
    const QString fallbackBody  = source->property("infoBody").toString();
    if (!offer && fallbackTitle.isEmpty() && fallbackBody.isEmpty()) return;

    mInfoPanel->clear();
    QVector<BalatroInfoPanel::Badge> mainBadges;
    QString name = fallbackTitle;
    QString bodyHtml;
    bool nameWhiteBox = false;
    int mainWidth = 175;

    auto addEditionSide = [this](Edition e) {
        if (e == Edition::None) return;
        BalatroInfoPanel::SideEntry s;
        s.name = CardTooltipFormat::editionName(e);
        s.body = CardTooltipFormat::editionBodyHtml(e);
        s.badges.append({CardTooltipFormat::editionName(e),
                         BalatroInfoPanel::editionPillColor()});
        s.preferredWidth = 130;
        mInfoPanel->addSidePanel(s);
    };

    if (offer) {
        switch (offer->kind) {
        case OfferKind::Joker: {
            Joker tmp = createJoker(offer->joker);
            // name 不再前缀 edition——edition 全部放副面板
            name = tmp.name;
            QString desc = tmp.description;
            if (offer->joker == JokerType::Throwback && mGS) {
                const double x = 1.0 + 0.25 * qMax(0, mGS->totalSkipsThisRun());
                desc += QString("\n{C:inactive}当前：{X:mult,C:white}X%1{} 倍率")
                            .arg(x, 0, 'f', 2);
            }
            bodyHtml = CardTooltipFormat::fromLuaMarkup(desc);
            const JokerRarity rr = jokerRarity(offer->joker);
            mainBadges.append({CardTooltipFormat::rarityName(rr),
                               CardTooltipFormat::rarityColor(rr)});
            mInfoPanel->setMainContent(name, bodyHtml, mainBadges, mainWidth, nameWhiteBox);
            addEditionSide(offer->jokerEdition);
            break;
        }
        case OfferKind::Tarot:
        case OfferKind::Planet:
        case OfferKind::Spectral: {
            Consumable tmp = createConsumable(offer->consumable);
            name = tmp.name;
            bodyHtml = CardTooltipFormat::fromLuaMarkup(tmp.description);
            const ConsumableKind k = kindOf(offer->consumable);
            QColor setColor;
            QString setLabel;
            switch (k) {
            case ConsumableKind::Tarot:
                setLabel = QStringLiteral("塔罗牌");
                setColor = BalatroInfoPanel::tarotPillColor(); break;
            case ConsumableKind::Planet:
                setLabel = QStringLiteral("行星牌");
                setColor = BalatroInfoPanel::planetPillColor(); break;
            case ConsumableKind::Spectral:
                setLabel = QStringLiteral("幻灵牌");
                setColor = BalatroInfoPanel::spectralPillColor(); break;
            }
            mainBadges.append({setLabel, setColor});
            mInfoPanel->setMainContent(name, bodyHtml, mainBadges, mainWidth, nameWhiteBox);
            // 副面板：被授予的增强 / 蜡封 / 随机版本——与主场景悬浮同一套逻辑。
            if (Enhancement gE = CardTooltipFormat::consumableGrantsEnhancement(offer->consumable);
                gE != Enhancement::None) {
                BalatroInfoPanel::SideEntry s;
                s.name = CardTooltipFormat::enhancementName(gE);
                s.body = CardTooltipFormat::enhancementBodyHtml(gE);
                s.preferredWidth = 140;
                mInfoPanel->addSidePanel(s);
            }
            if (Seal gS = CardTooltipFormat::consumableGrantsSeal(offer->consumable);
                gS != Seal::None) {
                BalatroInfoPanel::SideEntry s;
                s.name = CardTooltipFormat::sealName(gS);
                s.body = CardTooltipFormat::sealBodyHtml(gS);
                const int sealKind = (gS == Seal::Gold ? 0 :
                                      gS == Seal::Red  ? 1 :
                                      gS == Seal::Blue ? 2 : 3);
                s.badges.append({QStringLiteral("蜡封"),
                                 BalatroInfoPanel::sealPillColor(sealKind)});
                s.preferredWidth = 130;
                mInfoPanel->addSidePanel(s);
            }
            if (CardTooltipFormat::consumableGrantsRandomEdition(offer->consumable)) {
                BalatroInfoPanel::SideEntry s;
                s.name = QStringLiteral("随机版本");
                s.body = QStringLiteral("闪箔 / 全息 / 多彩 三选一");
                s.preferredWidth = 130;
                mInfoPanel->addSidePanel(s);
            }
            break;
        }
        case OfferKind::Voucher: {
            VoucherData v = voucherData(offer->voucher);
            name = v.name;
            bodyHtml = CardTooltipFormat::fromLuaMarkup(v.description);
            mainBadges.append({QStringLiteral("优惠券"),
                               BalatroInfoPanel::voucherPillColor()});
            mInfoPanel->setMainContent(name, bodyHtml, mainBadges, mainWidth, nameWhiteBox);
            break;
        }
        case OfferKind::Pack: {
            name = fallbackTitle;
            // 计算补充包尺寸与可选数：与 boosterpack.cpp::optionsFor / choicesFor 完全对齐。
            const int options = (offer->pack == PackKind::Buffoon || offer->pack == PackKind::Spectral)
                ? (offer->packSize == PackSize::Normal ? 2 : 4)
                : (offer->packSize == PackSize::Normal ? 3 : 5);
            const int choices = (offer->packSize == PackSize::Mega) ? 2 : 1;
            QString kindLabel, kindColor, takeSuffix;
            switch (offer->pack) {
            case PackKind::Arcana:
                kindLabel = QStringLiteral("塔罗牌");
                kindColor = QStringLiteral("#a782d1");
                takeSuffix = QStringLiteral("（即选即用）");
                break;
            case PackKind::Celestial:
                kindLabel = QStringLiteral("行星牌");
                kindColor = QStringLiteral("#13afce");
                takeSuffix = QStringLiteral("（即选即用）");
                break;
            case PackKind::Spectral:
                kindLabel = QStringLiteral("幻灵牌");
                kindColor = QStringLiteral("#4584fa");
                takeSuffix = QStringLiteral("（即选即用）");
                break;
            case PackKind::Buffoon:
                kindLabel = QStringLiteral("小丑牌");
                kindColor = QStringLiteral("#fe5f55");
                takeSuffix = QStringLiteral("（加入小丑槽）");
                break;
            case PackKind::Standard:
            default:
                kindLabel = QStringLiteral("扑克牌");
                kindColor = QStringLiteral("#cdd2d7");
                takeSuffix = QStringLiteral("（加入牌组）");
                break;
            }
            bodyHtml = QString(
                "从最多 <span style=\"color:%1;font-weight:bold\">%2</span> 张"
                "<span style=\"color:%3;font-weight:bold\">%4</span><br/>"
                "中选择 <span style=\"color:%1;font-weight:bold\">%5</span> 张"
                "<br/><span style=\"color:#7a8489\">%6</span>"
            ).arg(QStringLiteral("#4bc292"),  // attention/green：数字
                  QString::number(options),
                  kindColor, kindLabel,
                  QString::number(choices),
                  takeSuffix);
            // 底部统一 "补充包" 类型 pill——按补充包种类用对应集合色，文字本身保持统一。
            mainBadges.append({QStringLiteral("补充包"), QColor(kindColor)});
            mInfoPanel->setMainContent(name, bodyHtml, mainBadges, mainWidth, nameWhiteBox);
            break;
        }
        case OfferKind::PlayingCard: {
            const CardData &c = offer->playingCard;
            name = CardTooltipFormat::cardTitleHtml(c);
            bodyHtml = CardTooltipFormat::cardBodyHtml(c);
            nameWhiteBox = true;
            mInfoPanel->setMainContent(name, bodyHtml, mainBadges, 160, nameWhiteBox);
            break;
        }
        }
    } else {
        bodyHtml = CardTooltipFormat::fromLuaMarkup(fallbackBody);
        mInfoPanel->setMainContent(name, bodyHtml, mainBadges, mainWidth, nameWhiteBox);
    }
    mInfoPanel->relayout();

    QPoint pos = source->mapTo(this, QPoint(source->width() + dp(12), 0));
    if (pos.x() + mInfoPanel->width() > width() - 8)
        pos.setX(source->mapTo(this, QPoint(-mInfoPanel->width() - dp(12), 0)).x());
    pos.setX(qBound(dp(6), pos.x(), qMax(dp(6), width() - mInfoPanel->width() - dp(6))));
    pos.setY(qBound(dp(6), pos.y(), qMax(dp(6), height() - mInfoPanel->height() - dp(6))));

    mInfoPanel->move(pos);
    mInfoPanel->raise();
    mInfoPanel->show();
    mInfoSource = source;
}

void ShopWidget::hideOfferInfo()
{
    if (mInfoPanel) mInfoPanel->hide();
    mInfoSource = nullptr;
}

void ShopWidget::syncPriceLblForCardBtn(QWidget *cardBtn)
{
    if (!cardBtn) return;
    auto *scb = dynamic_cast<ShopCardButton*>(cardBtn);
    auto findOu = [cardBtn](const QVector<OfferUi> &vec) -> const OfferUi* {
        for (const auto &ou : vec) if (ou.cardBtn == cardBtn) return &ou;
        return nullptr;
    };
    const OfferUi *ou = findOu(mShopUi);
    if (!ou) ou = findOu(mVoucherUi);
    if (!ou) ou = findOu(mBoosterUi);
    if (!ou || !ou->priceLbl) return;
    if (cardBtn->property("suppressPrice").toBool()) {
        ou->priceLbl->hide();
        return;
    }

    // 可见卡牌在 button 内的垂直偏移：button 比可见尺寸大一圈给 hover 缩放预留。
    const int visibleTopOffset = (scb && scb->visibleCardSize().isValid())
        ? (cardBtn->height() - scb->visibleCardSize().height()) / 2
        : 0;
    // hover 时画面上移 -4 px（与 ShopCardButton::paintEvent 的 translate 对齐）。
    const int hoverShift = (scb && scb->isHovered() && scb->hoverLiftEnabled()) ? -8 : 0;
    const QPoint cardTopLeftInThis = cardBtn->mapTo(this, QPoint(0, 0));
    const int px = cardTopLeftInThis.x() + (cardBtn->width() - ou->priceLbl->width()) / 2;
    const int py = cardTopLeftInThis.y() + visibleTopOffset
                   - ou->priceLbl->height() + 1 + hoverShift;
    ou->priceLbl->move(px, py);
    // 关键守卫：cardBtn 不可见（卖完 / 空槽）时显式 hide()，确保 priceLbl 的默认可见状态
    // （创建时的 "$0" 残影）一定被消掉；否则刚启动 / 拖动结束遍历会把空槽 priceLbl 也拉出来。
    if (cardBtn->isVisible()) {
        ou->priceLbl->show();
        ou->priceLbl->raise();
    } else {
        ou->priceLbl->hide();
    }
}

void ShopWidget::ensureVoucherUiCount(int count)
{
    if (!mVoucherBox) return;
    while (mVoucherUi.size() < count) {
        const int idx = mVoucherUi.size();
        OfferUi vu = createOfferSlot(mVoucherBox, false);
        vu.card->setProperty("voucherSlotIdx", idx);
        connect(vu.cardBtn, &QPushButton::clicked, this, [this, idx]() { onVoucherCardClicked(idx); });
        if (vu.buyBtn)
            connect(vu.buyBtn, &QPushButton::clicked, this, [this, idx]() { onBuyVoucher(idx); });
        mVoucherUi.append(vu);
    }
}

void ShopWidget::ensureVoucherBoxSize(int activeCount)
{
    if (!mVoucherBox) return;
    // 1 张 → 基础宽 200dp;2 张 → +110;3 张 → +220 …… 上限 4 张以内能完整显示。
    const int extra = qMax(0, activeCount - 1);
    const int targetW = dp(220) + extra * dp(132);
    const int targetH = dp(activeCount > 1 ? 382 : 352);
    if (mVoucherBox->size() != QSize(targetW, targetH)) {
        mVoucherBox->setMinimumSize(targetW, targetH);
        mVoucherBox->setMaximumSize(targetW, targetH);
        mVoucherBox->resize(targetW, targetH);
        mVoucherBox->updateGeometry();
        if (mVoucherBox->parentWidget() && mVoucherBox->parentWidget()->layout())
            mVoucherBox->parentWidget()->layout()->activate();
    }
}

void ShopWidget::layoutVoucherFan()
{
    if (!mVoucherBox) return;

    // 只把"未售出"的优惠券计入排列;已售出的槽位仍保留 UI 不参与定位/计数。
    // 这样:2 张里买掉 1 张 → 剩下 1 张回正居中、不再倾斜;3 张里买掉 1 张 → 剩 2 张左右对称。
    const auto &voucherOffers = mGS ? mGS->shop().voucherOffers() : QVector<ShopOffer>{};
    QVector<int> activeSlots;   // 未售出的 mVoucherUi 槽位下标
    for (int i = 0; i < mVoucherUi.size() && i < voucherOffers.size(); ++i) {
        if (!voucherOffers[i].sold) activeSlots.append(i);
    }
    const int activeCount = activeSlots.size();
    ensureVoucherBoxSize(activeCount);

    // 已售出槽位:隐藏价格/购买按钮,回正旋转,但保留 ou.card 显示(留位避免布局抖动)。
    QSet<int> activeSet(activeSlots.begin(), activeSlots.end());
    for (int i = 0; i < mVoucherUi.size(); ++i) {
        if (activeSet.contains(i)) continue;
        OfferUi &ou = mVoucherUi[i];
        if (auto *scb = dynamic_cast<ShopCardButton *>(ou.cardBtn)) scb->setStaticRotation(0.0);
        if (ou.cardBtn) ou.cardBtn->setProperty("suppressPrice", true);
        if (ou.priceLbl) ou.priceLbl->hide();
        if (ou.buyBtn) ou.buyBtn->hide();
    }
    if (activeCount <= 0) return;

    const double mid = (activeCount - 1) / 2.0;
    const int centerX = mVoucherBox->width() / 2;
    // 优惠券基线 Y 与右侧 boosterBox 对齐:以 voucherBox 中心为准,
    // 不再 +dp(4) 偏移(以前会让初始时优惠券比右侧礼包高一点)。
    const int centerY = mVoucherBox->height() / 2;
    const int visibleCardW = (!mVoucherUi.isEmpty() && mVoucherUi[0].cardBtn)
        ? mVoucherUi[0].cardBtn->width() : dp(176);
    const int spreadX = (activeCount > 1)
        ? qBound(dp(82), (mVoucherBox->width() - visibleCardW + dp(28)) / (activeCount - 1), dp(130))
        : 0;

    // 仅 "点击选中" 才上抬,**不再因 hover 上抬**——按原版 G.shop_vouchers 行为。
    int focusOrder = -1;   // activeSlots 中的索引
    if (mSelectedVoucherSlot >= 0) {
        focusOrder = activeSlots.indexOf(mSelectedVoucherSlot);
    }
    if (focusOrder < 0) focusOrder = activeCount - 1;   // 默认把最右那张放在最上层

    QVector<int> order;   // 渲染顺序:把 focus 放到最后,确保其在最上层
    for (int k = 0; k < activeCount; ++k)
        if (k != focusOrder) order.append(k);
    order.append(focusOrder);
    order.clear();
    for (int k = 0; k < activeCount; ++k) order.append(k);

    for (int renderIdx = 0; renderIdx < order.size(); ++renderIdx) {
        const int k = order[renderIdx];
        const int slot = activeSlots[k];
        OfferUi &ou = mVoucherUi[slot];
        if (!ou.card) continue;

        if (ou.card->layout()) ou.card->layout()->activate();
        QWidget *btn = ou.cardBtn ? ou.cardBtn : ou.card;
        const double rel = k - mid;
        const int targetCenterX = centerX + int(std::round(rel * spreadX));
        const bool focused = (slot == mSelectedVoucherSlot);
        // 弧形排列保留(rel*22 垂直 + |rel|*8 弧形),但 hover 不再额外上抬。
        const int targetCenterY = centerY + int(std::round(rel * dp(22) + std::abs(rel) * dp(8))) - (focused ? dp(24) : 0);
        const int btnCenterXInCard = btn->x() + btn->width() / 2;
        const int btnCenterYInCard = btn->y() + btn->height() / 2;
        const QPoint target(targetCenterX - btnCenterXInCard,
                            targetCenterY - btnCenterYInCard);
        if (ou.card->property("voucherTargetPos").toPoint() != target) {
            ou.card->setProperty("voucherTargetPos", target);
            auto *anim = new QPropertyAnimation(ou.card, "pos", ou.card);
            anim->setDuration(150);
            anim->setStartValue(ou.card->pos());
            anim->setEndValue(target);
            anim->setEasingCurve(QEasingCurve::OutCubic);
            QPointer<ShopWidget> self(this);
            QPointer<QWidget> btnGuard(ou.cardBtn);
            connect(anim, &QPropertyAnimation::valueChanged, this, [self, btnGuard]() {
                if (self && btnGuard) self->syncPriceLblForCardBtn(btnGuard);
            });
            connect(anim, &QPropertyAnimation::finished, this, [self, btnGuard]() {
                if (self && btnGuard) self->syncPriceLblForCardBtn(btnGuard);
            });
            anim->start(QAbstractAnimation::DeleteWhenStopped);
        } else if (ou.card->pos() != target && !ou.card->findChild<QPropertyAnimation*>()) {
            ou.card->move(target);
        }
        ou.card->raise();
        if (auto *scb = dynamic_cast<ShopCardButton *>(ou.cardBtn))
            scb->setStaticRotation(rel * (focused ? 3.5 : 5.5));

        // 价格标签和商店其他牌一样常驻在卡牌上方,不再因为 focus/选中状态隐藏。
        if (ou.cardBtn)
            ou.cardBtn->setProperty("suppressPrice", false);
        if (!focused && ou.buyBtn && slot != mSelectedVoucherSlot)
            ou.buyBtn->hide();
        syncPriceLblForCardBtn(ou.cardBtn);
    }
}

bool ShopWidget::eventFilter(QObject *obj, QEvent *event)
{
    QWidget *w = qobject_cast<QWidget *>(obj);

    // 商店同一区块内拖拽换位：拖动时使用顶层 ghost，避免被 shopBox/boosterBox 裁切；
    // 其它商品会在鼠标悬停到目标槽前就预先让位，和小丑槽拖拽的手感一致。
    if (w) {
        int slot = -1;
        DragGroup group = dragGroupForWidget(w, &slot);
        if (group == DragGroup::Voucher) {
            // 仅记录 hover 槽位以便其它逻辑(如信息面板)使用,**不再触发 layoutVoucherFan**——
            // 优惠券只有在点击选中时才上抬,hover 不再让它向上移。
            if (event->type() == QEvent::Enter) {
                mHoveredVoucherSlot = slot;
            } else if (event->type() == QEvent::Leave && mHoveredVoucherSlot == slot) {
                mHoveredVoucherSlot = -1;
            }
        }
        if (group != DragGroup::None) {
            QWidget *container = w->parentWidget();
            if (event->type() == QEvent::MouseButtonPress) {
                auto *me = static_cast<QMouseEvent *>(event);
                if (me->button() == Qt::LeftButton && container) {
                    mDragGroup = group;
                    mDragFromSlot = slot;
                    mDragWidget = container;
                    mDragStartGlobal = me->globalPosition().toPoint();
                    mDragPressOffset = container->mapFromGlobal(mDragStartGlobal);
                    mDragSlotBasePos.clear();
                    const QVector<OfferUi> *vec = nullptr;
                    if (group == DragGroup::Shop) vec = &mShopUi;
                    else if (group == DragGroup::Voucher) vec = &mVoucherUi;
                    else if (group == DragGroup::Booster) vec = &mBoosterUi;
                    if (vec) {
                        for (const OfferUi &ou : *vec)
                            mDragSlotBasePos.append(ou.card ? ou.card->pos() : QPoint());
                    }
                    mDragPreviewSlot = slot;
                    mDraggingOffer = false;
                }
            } else if (event->type() == QEvent::MouseMove && mDragGroup != DragGroup::None && mDragWidget) {
                auto *me = static_cast<QMouseEvent *>(event);
                if (me->buttons() & Qt::LeftButton) {
                    const QPoint gp = me->globalPosition().toPoint();
                    if (!mDraggingOffer && (gp - mDragStartGlobal).manhattanLength() > dp(8)) {
                        mDraggingOffer = true;
                        hideOfferInfo();
                        // 拖动开始：不再藏起 priceLbl——下面的 MouseMove 路径会把它同步到
                        // ghost 的顶部，跟随拖动移动。
                        destroyDragGhost();
                        mDragGhost = new QLabel(this);
                        mDragGhost->setAttribute(Qt::WA_TranslucentBackground, true);
                        mDragGhost->setScaledContents(true);
                        mDragGhost->setPixmap(mDragWidget->grab());
                        mDragGhost->setFixedSize(mDragWidget->size());
                        const QPoint startInThis = mapFromGlobal(mDragWidget->mapToGlobal(QPoint(0, 0)));
                        mDragGhost->move(startInThis);
                        mDragGhost->show();
                        mDragGhost->raise();

                        // 保留源 widget 在原 layout 里占位，但把它淡掉，避免 layout 收缩或出现双贴图。
                        auto *eff = new QGraphicsOpacityEffect(mDragWidget);
                        eff->setOpacity(0.02);
                        mDragWidget->setGraphicsEffect(eff);
                        mDragHiddenEffect = eff;
                        w->setCursor(Qt::ClosedHandCursor);
                    }
                    if (mDraggingOffer) {
                        if (mDragGhost) {
                            mDragGhost->move(mapFromGlobal(gp) - mDragPressOffset);
                            mDragGhost->raise();
                            // 价格 tab 跟随 ghost——ghost 是整张 ou.card grab，cardBtn 位于
                            // ghost 内 x=cardBtn.pos().x(), y=cardBtn.pos().y()，宽度=cardBtn.width。
                            // 价格 tab 必须以 cardBtn 中心为基准（非 ghost 中心），否则非礼包槽
                            // 的右侧 actionReserveW 会把 tab 整体推向右。
                            auto syncPriceWithGhost = [this, w](const QVector<OfferUi> &vec) -> bool {
                                for (const auto &ou : vec) {
                                    if (ou.cardBtn != w || !ou.priceLbl) continue;
                                    auto *scb = dynamic_cast<ShopCardButton*>(w);
                                    const int visOff = scb && scb->visibleCardSize().isValid()
                                        ? (w->height() - scb->visibleCardSize().height()) / 2 : 0;
                                    const QPoint gp2 = mDragGhost->pos();
                                    const QPoint cardBtnInCard = w->pos();
                                    const int cardCenterX = cardBtnInCard.x() + w->width() / 2;
                                    const int px = gp2.x() + cardCenterX - ou.priceLbl->width() / 2;
                                    const int py = gp2.y() + cardBtnInCard.y() + visOff
                                                 - ou.priceLbl->height() + 1;
                                    ou.priceLbl->move(px, py);
                                    ou.priceLbl->show();
                                    ou.priceLbl->raise();
                                    return true;
                                }
                                return false;
                            };
                            if (!syncPriceWithGhost(mShopUi))
                                if (!syncPriceWithGhost(mVoucherUi))
                                    syncPriceWithGhost(mBoosterUi);
                        }
                        int to = slotAtGlobalPos(mDragGroup, gp);
                        if (to < 0) to = mDragFromSlot;
                        if (to != mDragPreviewSlot) {
                            mDragPreviewSlot = to;
                            updateDragPreview(mDragGroup, mDragFromSlot, to);
                        }
                        return true;
                    }
                }
            } else if (event->type() == QEvent::MouseButtonRelease && mDragGroup != DragGroup::None) {
                auto *me = static_cast<QMouseEvent *>(event);
                const DragGroup releaseGroup = mDragGroup;
                const int from = mDragFromSlot;
                const bool wasDragging = mDraggingOffer;
                const int to = slotAtGlobalPos(releaseGroup, me->globalPosition().toPoint());
                QWidget *groupParent = nullptr;
                if (releaseGroup == DragGroup::Shop && !mShopUi.isEmpty() && mShopUi[0].card)
                    groupParent = mShopUi[0].card->parentWidget();
                else if (releaseGroup == DragGroup::Voucher && !mVoucherUi.isEmpty() && mVoucherUi[0].card)
                    groupParent = mVoucherUi[0].card->parentWidget();
                else if (releaseGroup == DragGroup::Booster && !mBoosterUi.isEmpty() && mBoosterUi[0].card)
                    groupParent = mBoosterUi[0].card->parentWidget();

                QPoint dropPos;
                if (mDragGhost && groupParent)
                    dropPos = groupParent->mapFromGlobal(mapToGlobal(mDragGhost->pos()));
                else if (mDragWidget)
                    dropPos = mDragWidget->pos();

                mDragGroup = DragGroup::None;
                mDragFromSlot = -1;
                mDraggingOffer = false;
                mDragPreviewSlot = -1;
                w->unsetCursor();

                if (wasDragging) {
                    const bool canMove = (to >= 0 && from >= 0 && to != from
                                          && moveOfferInGroup(releaseGroup, from, to));
                    if (canMove) {
                        if (mDragWidget) mDragWidget->setGraphicsEffect(nullptr);
                        mDragHiddenEffect = nullptr;
                        destroyDragGhost();
                        // 不在这里同步 priceLbl——animateOfferReorder 会和 ou.card 一起跑
                        // priceLbl 动画，保证两者同步移动到最终位置。
                        animateOfferReorder(releaseGroup, from, to, dropPos);
                    } else {
                        mDragGroup = releaseGroup;
                        clearDragPreview(true);
                        mDragGroup = DragGroup::None;
                        if (mDragGhost && from >= 0 && from < mDragSlotBasePos.size() && mDragWidget) {
                            const QPoint endInThis = mapFromGlobal(mDragWidget->parentWidget()->mapToGlobal(mDragSlotBasePos[from]));
                            auto *anim = new QPropertyAnimation(mDragGhost, "pos", mDragGhost);
                            anim->setDuration(170);
                            anim->setStartValue(mDragGhost->pos());
                            anim->setEndValue(endInThis);
                            anim->setEasingCurve(QEasingCurve::OutCubic);

                            // 被拖商品的 priceLbl 跟 ghost 一起回到原槽位——之前依赖 sync
                            // 立即归位，priceLbl 会瞬移。这里改成并行 QPropertyAnimation。
                            QLabel *draggedPrice = nullptr;
                            QWidget *draggedCardBtn = nullptr;
                            auto findDragged = [this, &draggedPrice, &draggedCardBtn](const QVector<OfferUi> &vec) {
                                for (const auto &ou : vec)
                                    if (ou.card == mDragWidget) {
                                        draggedPrice = ou.priceLbl;
                                        draggedCardBtn = ou.cardBtn;
                                        return true;
                                    }
                                return false;
                            };
                            if (!findDragged(mShopUi)) if (!findDragged(mVoucherUi)) findDragged(mBoosterUi);
                            if (draggedPrice && draggedCardBtn && draggedPrice->isVisible()) {
                                auto *scb = dynamic_cast<ShopCardButton*>(draggedCardBtn);
                                const int visOff = (scb && scb->visibleCardSize().isValid())
                                    ? (draggedCardBtn->height() - scb->visibleCardSize().height()) / 2 : 0;
                                const QPoint cardBtnInCard = draggedCardBtn->pos();
                                const QPoint priceEnd(
                                    endInThis.x() + cardBtnInCard.x()
                                        + (draggedCardBtn->width() - draggedPrice->width()) / 2,
                                    endInThis.y() + cardBtnInCard.y()
                                        + visOff - draggedPrice->height() + 1);
                                for (QObject *child : draggedPrice->children()) {
                                    if (auto *oldAnim = qobject_cast<QPropertyAnimation*>(child)) {
                                        if (oldAnim->propertyName() == QByteArray("pos")) oldAnim->stop();
                                    }
                                }
                                auto *priceAnim = new QPropertyAnimation(draggedPrice, "pos", draggedPrice);
                                priceAnim->setDuration(170);
                                priceAnim->setStartValue(draggedPrice->pos());
                                priceAnim->setEndValue(priceEnd);
                                priceAnim->setEasingCurve(QEasingCurve::OutCubic);
                                priceAnim->start(QAbstractAnimation::DeleteWhenStopped);
                            }

                            QPointer<QWidget> sourceGuard(mDragWidget);
                            QPointer<ShopWidget> self(this);
                            connect(anim, &QPropertyAnimation::finished, this, [self, sourceGuard]() {
                                if (sourceGuard) sourceGuard->setGraphicsEffect(nullptr);
                                if (self) {
                                    self->mDragHiddenEffect = nullptr;
                                    self->destroyDragGhost();
                                }
                            });
                            anim->start(QAbstractAnimation::DeleteWhenStopped);
                        } else {
                            if (mDragWidget) mDragWidget->setGraphicsEffect(nullptr);
                            mDragHiddenEffect = nullptr;
                            destroyDragGhost();
                        }
                    }
                    mDragWidget = nullptr;
                    return true;
                }
                mDragWidget = nullptr;
            }
        }
    }

    if (w && w->property("infoTitle").isValid()) {
        if (event->type() == QEvent::Enter) {
            showOfferInfo(w);
        } else if (event->type() == QEvent::Leave || event->type() == QEvent::Hide) {
            hideOfferInfo();
        }
        // cardBtn 移动 / 显示 → 重新计算价格 tab 的"贴可见卡牌顶部"位置。
        if (event->type() == QEvent::Move || event->type() == QEvent::Show) {
            if (auto *scb = dynamic_cast<ShopCardButton*>(w)) syncPriceLblForCardBtn(scb);
        }
        // hover 进出 → 延迟一帧再 sync。eventFilter 跑在 widget 自己 enterEvent 之前，
        // 此时 ShopCardButton::mHovered 还没被翻成 true，直接 sync 会拿到 stale 状态，
        // 导致 hover 时价格 tab 反而停留在 rest 位置（视觉上看像"hover 时价格下降"）。
        if (event->type() == QEvent::Enter || event->type() == QEvent::Leave) {
            if (auto *scb = dynamic_cast<ShopCardButton*>(w)) {
                QPointer<QWidget> guard(scb);
                QPointer<ShopWidget> self(this);
                QTimer::singleShot(0, this, [self, guard]() {
                    if (self && guard) self->syncPriceLblForCardBtn(guard);
                });
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void ShopWidget::refresh()
{
    if (mVoucherPurchaseAnimating) return;

    if (mLblGold) mLblGold->setText(QString("$%1").arg(mGS->gold()));

    auto fillSlot = [this](OfferUi &ou, const ShopOffer &o, bool canBuy, bool isBooster) {
        Q_UNUSED(isBooster);
        if (o.sold) {
            // 卖完后保持外层 ou.card 占位（仍 setVisible(true)），仅隐藏内部 cardBtn / 价格 / 按钮，
            // 确保当前行的另一槽位高度和宽度不会被 layout 重新瓜分。
            ou.card->setVisible(true);
            if (ou.cardBtn) {
                ou.cardBtn->setVisible(false);
                ou.cardBtn->setToolTip(QString());
                ou.cardBtn->setProperty("infoTitle", QString());
                ou.cardBtn->setProperty("infoBody", QString());
            }
            if (ou.priceLbl) ou.priceLbl->setVisible(false);
            if (ou.nameLbl)  ou.nameLbl->setText(QString());
            if (ou.buyBtn) ou.buyBtn->hide();
            if (ou.useBtn) ou.useBtn->hide();
            return;
        }
        ou.card->setVisible(true);
        if (ou.cardBtn) ou.cardBtn->setVisible(true);
        if (ou.priceLbl) ou.priceLbl->setVisible(true);

        QString name;
        QString body;
        if (o.kind == OfferKind::Joker) {
            Joker tmp = createJoker(o.joker);
            QString ed = editionDisplayName(o.jokerEdition);
            name = ed.isEmpty() ? tmp.name : (ed + " " + tmp.name);
            body = tmp.description;
            if (o.joker == JokerType::Throwback && mGS) {
                const double x = 1.0 + 0.25 * qMax(0, mGS->totalSkipsThisRun());
                body += QString("\n{C:inactive}当前：{X:mult,C:white}X%1{} 倍率")
                            .arg(x, 0, 'f', 2);
            }
            if (!ed.isEmpty()) body += "\n" + editionDescription(o.jokerEdition);
        } else if (o.kind == OfferKind::Pack) {
            name = packDisplayName(o.pack, o.packSize);
            body = "购买后打开，选择其中的牌。";
        } else if (o.kind == OfferKind::Voucher) {
            name = voucherData(o.voucher).name;
            body = voucherData(o.voucher).description;
        } else if (o.kind == OfferKind::PlayingCard) {
            name = o.playingCard.toString();
            body = "购买后加入牌组。";
        } else {
            Consumable tmp = createConsumable(o.consumable);
            name = tmp.name;
            body = tmp.description;
        }
        ou.cardBtn->setToolTip(QString());
        ou.cardBtn->setProperty("infoTitle", name);
        ou.cardBtn->setProperty("infoBody", body);
        ou.nameLbl->setText(name);
        ou.priceLbl->setText(QString("$%1").arg(o.cost));

        // 卡图由 ShopCardButton 自己绘制，保留原始像素清晰度，并在 hover 时做原版式顶点透视。
        // 关键：选中切换 / 价格刷新都会触发 refresh()，但 offer 本身没变时不要重渲卡图，
        // 否则带 shader 的牌面（Foil/Holographic 等）每次会产生轻微色差，看上去就是"光泽闪一下"。
        const QString offerHash = QString("%1|j%2|e%3|c%4|p%5|s%6|pv%7|v%8|r%9|u%10|h%11|d%12|l%13|b%14|sold%15")
                                      .arg(int(o.kind))
                                      .arg(int(o.joker))
                                      .arg(int(o.jokerEdition))
                                      .arg(int(o.consumable))
                                      .arg(int(o.pack))
                                      .arg(int(o.packSize))
                                      .arg(o.packVariant)
                                      .arg(int(o.voucher))
                                      .arg(int(o.playingCard.rank))
                                      .arg(int(o.playingCard.suit))
                                      .arg(int(o.playingCard.enhancement))
                                      .arg(int(o.playingCard.edition))
                                      .arg(int(o.playingCard.seal))
                                      .arg(o.playingCard.isDebuffed ? 1 : 0)
                                      .arg(o.sold ? 1 : 0);
        if (ou.cardBtn->property("offerHash").toString() != offerHash) {
            QPixmap pix = offerPixmap(o);
            if (auto *tiltBtn = dynamic_cast<ShopCardButton *>(ou.cardBtn))
                tiltBtn->setDisplayPixmap(pix);
            else if (!pix.isNull()) {
                ou.cardBtn->setIcon(QIcon(pix));
                ou.cardBtn->setIconSize(pix.size());
            }
            ou.cardBtn->setProperty("offerHash", offerHash);
        }
        // cardBtn 永远可点（click = 选中/取消选中），但启用与否由 canBuy 决定外观置灰。
        ou.cardBtn->setEnabled(true);

        // 按 offer 类型决定按钮的颜色 / 文本 / 选中来源。
        const bool isConsumable = (o.kind == OfferKind::Tarot ||
                                   o.kind == OfferKind::Planet ||
                                   o.kind == OfferKind::Spectral);
        bool selectedHere = false;
        QString buyText = QStringLiteral("购买");
        QString buyBg = QStringLiteral("#fda200");
        QString buyHover = QStringLiteral("#ffb730");

        const QVariant shopIdxV = ou.card->property("shopOfferIdx");
        const QVariant voucherIdxV = ou.card->property("voucherSlotIdx");
        const QVariant boosterIdxV = ou.card->property("boosterSlotIdx");

        if (boosterIdxV.isValid()) {
            // 礼包：绿色"打开"
            selectedHere = (boosterIdxV.toInt() == mSelectedBoosterSlot);
            buyText = QStringLiteral("打开");
            buyBg = QStringLiteral("#4ca893");
            buyHover = QStringLiteral("#5fbfa8");
        } else if (voucherIdxV.isValid()) {
            // 优惠券：黄色"兑换"
            selectedHere = (voucherIdxV.toInt() == mSelectedVoucherSlot);
            buyBg = QStringLiteral("#4BC292");
            buyHover = QStringLiteral("#63d4a8");
            buyText = QStringLiteral("兑换");
        } else if (shopIdxV.isValid()) {
            selectedHere = (shopIdxV.toInt() == mSelectedShopSlot);
            buyText = QStringLiteral("购买");
        }

        if (ou.buyBtn) {
            ou.buyBtn->setText(buyText);
            ou.buyBtn->setStyleSheet(QString(
                "QPushButton { background:%1; color:white; border:2px solid rgba(255,255,255,90);"
                " border-radius:8px; font-weight:bold; }"
                "QPushButton:hover { background:%2; border:2px solid rgba(255,255,255,170); }"
                "QPushButton:disabled { background:#3b4347; color:#7c8488; border:2px solid #3b4347; }"
            ).arg(buyBg, buyHover));
            ou.buyBtn->setVisible(selectedHere);
            // CreditCard 小丑允许金币透支至 -$20：买入判定走 spendableGold()。
            ou.buyBtn->setEnabled(canBuy && mGS->spendableGold() >= o.cost);
        }
        if (ou.useBtn) {
            // 只有满足"可立即使用"条件的消耗牌才显示购买&使用；需要选牌的塔罗
            // (如魔术师、皇帝、太阳/月亮) 在商店里没有手牌可选，直接隐藏右侧红按钮。
            const int idx = shopIdxV.isValid() ? shopIdxV.toInt() : -1;
            const bool canUseHere = isConsumable && shopIdxV.isValid()
                                    && mGS->canBuyAndUseShopConsumable(idx);
            const bool showUse = selectedHere && canUseHere;
            ou.useBtn->setVisible(showUse);
            if (showUse) ou.useBtn->setEnabled(true);
        }
        // 选中卡片轻微上浮——对齐原版 card.lua highlighted 时的 highlight_offset。
        if (ou.cardBtn) {
            // 同步阴影 lift 状态：选中即抬升。
            if (auto *sc = dynamic_cast<ShopCardButton*>(ou.cardBtn)) sc->setLifted(selectedHere);
            const bool animateSelectedLift = selectedHere && !voucherIdxV.isValid();
            QPointer<QWidget> btnGuard(ou.cardBtn);
            QTimer::singleShot(0, this, [btnGuard, animateSelectedLift]() {
                if (!btnGuard) return;
                QWidget *btn = btnGuard.data();
                QVariant base = btn->property("baseY");
                int baseY = base.isValid() ? base.toInt() : btn->y();
                // layout 可能在窗口缩放后重算 y；如果当前按钮回到了布局给的位置，更新基准。
                if (!base.isValid() || (!animateSelectedLift && qAbs(btn->y() - baseY) > dp(40))) baseY = btn->y();
                btn->setProperty("baseY", baseY);
                const int targetY = baseY + (animateSelectedLift ? -dp(36) : 0);
                btn->setProperty("cardTargetY", targetY);
                if (btn->y() == targetY) return;
                auto *anim = new QPropertyAnimation(btn, "pos", btn);
                anim->setDuration(160);
                anim->setStartValue(btn->pos());
                anim->setEndValue(QPoint(btn->x(), targetY));
                anim->setEasingCurve(QEasingCurve::OutCubic);
                anim->start(QAbstractAnimation::DeleteWhenStopped);
            });
        }
        if (selectedHere) positionSlotActionButtons(ou, ou.useBtn && ou.useBtn->isVisible());
    };

    const auto &shopOffers = mGS->shop().shopOffers();
    for (int i = 0; i < mShopUi.size(); ++i) {
        if (i >= shopOffers.size()) {
            mShopUi[i].card->setVisible(false);
            continue;
        }
        bool slotOk = true;
        const ShopOffer &o = shopOffers[i];
        if (o.kind == OfferKind::Joker)
            // 负片小丑自身提供 +1 小丑槽，原版满槽时也允许购买。
            slotOk = mGS->canAddJokerWithEdition(o.jokerEdition);
        else if (o.kind == OfferKind::Tarot || o.kind == OfferKind::Planet || o.kind == OfferKind::Spectral)
            slotOk = mGS->canAddConsumable();
        else if (o.kind == OfferKind::PlayingCard)
            slotOk = true;
        fillSlot(mShopUi[i], o,
                 mGS->shop().canBuyShop(i, mGS->spendableGold()) && slotOk, false);
    }

    const auto &voucherOffers = mGS->shop().voucherOffers();
    ensureVoucherUiCount(voucherOffers.size());
    for (int i = 0; i < mVoucherUi.size(); ++i) {
        if (i < voucherOffers.size()) {
            fillSlot(mVoucherUi[i], voucherOffers[i],
                     mGS->shop().canBuyVoucher(i, mGS->spendableGold()), false);
        } else {
            // 当前 Ante 不出优惠券（小/大盲）——必须把这一槽彻底藏掉，
            // 否则上一次商店残留的 priceLbl/nameLbl/cardBtn 会以 "$0" 形式继续显示。
            OfferUi &ou = mVoucherUi[i];
            if (ou.card)     ou.card->setVisible(false);   // 外层占位卡也藏掉
            if (ou.cardBtn)  ou.cardBtn->setVisible(false);
            if (ou.priceLbl) ou.priceLbl->setVisible(false);
            if (ou.nameLbl)  ou.nameLbl->setText(QString());
            if (ou.buyBtn)   ou.buyBtn->hide();
            if (ou.useBtn)   ou.useBtn->hide();
        }
    }
    layoutVoucherFan();

    const auto &boosterOffers = mGS->shop().boosterOffers();
    for (int i = 0; i < mBoosterUi.size() && i < boosterOffers.size(); ++i) {
        fillSlot(mBoosterUi[i], boosterOffers[i],
                 mGS->shop().canBuyBooster(i, mGS->spendableGold()), true);
    }

    int rcost = mGS->shop().rerollCost();
    mBtnReroll->setText(QString("重抽\n$%1").arg(rcost));
    // 混沌小丑：每次进商店首次重摇免费（rcost 在 hasFreeReroll() 时不应阻挡按钮）。
    // CreditCard：允许透支购买，重摇同样走 spendableGold()。
    const int effRerollCost = mGS->hasFreeShopReroll() ? 0 : rcost;
    mBtnReroll->setEnabled(mGS->spendableGold() >= effRerollCost);
    if (mGS->hasFreeShopReroll() && rcost > 0)
        mBtnReroll->setText(QString("重抽\n免费"));

    // 槽位变化（如 Overstock 优惠券新增槽位）后，外层 ou.card 会被 QHBoxLayout
    // 重新分配位置，但内部 cardBtn 不会收到 Move 事件，eventFilter 也就不会触发
    // syncPriceLblForCardBtn——结果价格标签停留在旧位置，直到玩家 hover 才纠正。
    // 这里在 layout 应用完毕后（singleShot(0)）主动把所有可见价格标签重新贴到位。
    QPointer<ShopWidget> guard(this);
    QTimer::singleShot(0, this, [guard]() {
        if (!guard) return;
        auto syncAll = [g = guard.data()](const QVector<OfferUi> &vec) {
            for (const auto &ou : vec) if (ou.cardBtn) g->syncPriceLblForCardBtn(ou.cardBtn);
        };
        syncAll(guard->mShopUi);
        syncAll(guard->mVoucherUi);
        syncAll(guard->mBoosterUi);
    });
}

QPixmap ShopWidget::offerPixmap(const ShopOffer &o) const
{
    if (o.kind == OfferKind::Joker) {
        QPixmap sheet(":/textures/images/Jokers.png");
        if (sheet.isNull()) return QPixmap();
        QPoint c = JokerItem::spritePos(o.joker);
        QPixmap pix = sheet.copy(c.x() * JokerItem::SRC_W, c.y() * JokerItem::SRC_H,
                                 JokerItem::SRC_W, JokerItem::SRC_H);
        // 全息投影 / 五张传奇牌：原版 card.lua:4512-4523 走 floating_sprite 浮动层。
        // 商店里如果只画 pos 主体，Hologram 看上去就是个空相框，传奇牌也少了肖像。
        {
            QPainter p(&pix);
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setRenderHint(QPainter::SmoothPixmapTransform, true);
            JokerItem::drawFloatingSprite(&p, QRectF(0, 0, pix.width(), pix.height()),
                                          o.joker, /*animated=*/false);
        }
        if (o.jokerEdition != Edition::None)
            pix = BalatroShaders::renderEditionPixmap(pix, o.jokerEdition);
        return pix;
    }

    if (o.kind == OfferKind::Tarot || o.kind == OfferKind::Planet || o.kind == OfferKind::Spectral) {
        return ConsumableItem::renderPixmap(o.consumable);
    }

    if (o.kind == OfferKind::Pack) {
        QPixmap sheet(":/textures/images/boosters.png");
        if (sheet.isNull()) return QPixmap();
        QPoint c = packSpritePos(o.pack, o.packSize, o.packVariant);
        QPixmap base = sheet.copy(c.x() * ConsumableItem::SRC_W,
                                  c.y() * ConsumableItem::SRC_H,
                                  ConsumableItem::SRC_W, ConsumableItem::SRC_H);

        // 原版 booster.fs：卡包有动态蓝紫/金色波纹。这里统一走 shader 转写层，
        // 同时修正透明边距，避免 QIcon 把卡包压小。
        return BalatroShaders::makeBooster3DPixmap(base);
    }
    if (o.kind == OfferKind::PlayingCard) {
        return playingCardPixmap(o.playingCard);
    }

    if (o.kind == OfferKind::Voucher) {
        // 如果你把队友的 Vouchers.png 加进 Qt 资源，这里会自动使用贴图；
        // 如果资源不存在，则走下面的文字券兜底。
        QPixmap voucherSheet(":/textures/images/Vouchers.png");
        if (!voucherSheet.isNull()) {
            QPoint c = voucherData(o.voucher).spritePos;
            QPixmap pix = voucherSheet.copy(c.x() * ConsumableItem::SRC_W,
                                           c.y() * ConsumableItem::SRC_H,
                                           ConsumableItem::SRC_W,
                                           ConsumableItem::SRC_H);
            return BalatroShaders::renderVoucherPixmap(pix, 1.0);
        }

        QPixmap pix(ConsumableItem::SRC_W, ConsumableItem::SRC_H);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(QColor("#2f9e44"));
        p.setPen(QPen(QColor("#e9ffd9"), 3));
        p.drawRoundedRect(4, 4, pix.width() - 8, pix.height() - 8, 10, 10);
        p.setPen(Qt::white);
        QFont f = mCNFont;
        f.setPixelSize(fontPx(18));
        f.setBold(true);
        p.setFont(f);
        p.drawText(pix.rect().adjusted(8, 8, -8, -8), Qt::AlignCenter | Qt::TextWordWrap,
                   voucherData(o.voucher).name);
        return pix;
    }

    return QPixmap();
}

QPixmap ShopWidget::playingCardPixmap(const CardData &c) const
{
    // 在图集原始 142×190 上合成；显示尺寸由 ShopCardButton 的绘制流程缩放。
    constexpr int W = ConsumableItem::SRC_W, H = ConsumableItem::SRC_H;
    QPixmap deckSheet(":/textures/images/8BitDeck.png");
    QPixmap enhSheet (":/textures/images/Enhancers.png");
    QPixmap pix(W, H); pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    int eCol = 1, eRow = 0;
    switch (c.enhancement) {
    case Enhancement::Bonus: eCol = 1; eRow = 1; break;
    case Enhancement::Mult:  eCol = 2; eRow = 1; break;
    case Enhancement::Wild:  eCol = 3; eRow = 1; break;
    case Enhancement::Lucky: eCol = 4; eRow = 1; break;
    case Enhancement::Glass: eCol = 5; eRow = 1; break;
    case Enhancement::Steel: eCol = 6; eRow = 1; break;
    case Enhancement::Stone: eCol = 5; eRow = 0; break;
    case Enhancement::Gold:  eCol = 6; eRow = 0; break;
    default: break;
    }
    if (!enhSheet.isNull())
        p.drawPixmap(QRect(0, 0, W, H), enhSheet, QRect(eCol*W, eRow*H, W, H));

    if (c.enhancement != Enhancement::Stone && !deckSheet.isNull()) {
        int col = static_cast<int>(c.rank) - 2;
        int row = 0;
        switch (c.suit) {
        case Suit::Hearts: row = 0; break;
        case Suit::Clubs:  row = 1; break;
        case Suit::Diamonds: row = 2; break;
        case Suit::Spades: row = 3; break;
        }
        p.drawPixmap(QRect(0, 0, W, H), deckSheet, QRect(col*W, row*H, W, H));
    }
    p.end();

    if (c.edition != Edition::None)
        pix = BalatroShaders::renderEditionPixmap(pix, c.edition);

    if (c.seal != Seal::None && !enhSheet.isNull()) {
        int sCol = 0, sRow = 0;
        switch (c.seal) {
        case Seal::Gold:   sCol = 2; sRow = 0; break;
        case Seal::Purple: sCol = 4; sRow = 4; break;
        case Seal::Red:    sCol = 5; sRow = 4; break;
        case Seal::Blue:   sCol = 6; sRow = 4; break;
        default: break;
        }
        QPixmap sealPix(W, H);
        sealPix.fill(Qt::transparent);
        {
            QPainter sp(&sealPix);
            sp.setRenderHint(QPainter::SmoothPixmapTransform, true);
            sp.drawPixmap(QRect(0, 0, W, H), enhSheet, QRect(sCol * W, sRow * H, W, H));
        }
        if (c.seal == Seal::Gold)
            sealPix = BalatroShaders::renderGoldSealPixmap(sealPix, 1.0);
        QPainter fp(&pix);
        fp.drawPixmap(QRect(0, 0, W, H), sealPix);
    }

    if (c.isDebuffed)
        pix = BalatroShaders::renderDebuffedPixmap(pix);
    return pix;
}

ShopWidget::DragGroup ShopWidget::dragGroupForWidget(QWidget *w, int *slotOut) const
{
    if (slotOut) *slotOut = -1;
    if (!w) return DragGroup::None;
    QWidget *container = w->parentWidget();
    if (!container) return DragGroup::None;

    QVariant shopIdx = container->property("shopOfferIdx");
    if (shopIdx.isValid()) {
        if (slotOut) *slotOut = shopIdx.toInt();
        return DragGroup::Shop;
    }
    QVariant voucherIdx = container->property("voucherSlotIdx");
    if (voucherIdx.isValid()) {
        if (slotOut) *slotOut = voucherIdx.toInt();
        return DragGroup::Voucher;
    }
    QVariant boosterIdx = container->property("boosterSlotIdx");
    if (boosterIdx.isValid()) {
        if (slotOut) *slotOut = boosterIdx.toInt();
        return DragGroup::Booster;
    }
    return DragGroup::None;
}

int ShopWidget::slotAtGlobalPos(DragGroup group, const QPoint &globalPos) const
{
    const QVector<OfferUi> *vec = nullptr;
    if (group == DragGroup::Shop) vec = &mShopUi;
    else if (group == DragGroup::Voucher) vec = &mVoucherUi;
    else if (group == DragGroup::Booster) vec = &mBoosterUi;
    else return -1;

    if (!vec || vec->isEmpty()) return -1;

    QWidget *parent = nullptr;
    for (const OfferUi &ou : *vec) {
        if (ou.card && ou.card->parentWidget()) { parent = ou.card->parentWidget(); break; }
    }
    if (!parent) return -1;

    // 不要求鼠标正好压在某张卡上。只要在同一个商品区附近，就按横向中心找最近槽，
    // 这样从左拖到右时右侧卡会提前让位，不用精确压到目标卡牌才触发。
    const QRect groupRect = QRect(parent->mapToGlobal(QPoint(0, 0)), parent->size())
                                .adjusted(-dp(90), -dp(100), dp(90), dp(100));
    if (!groupRect.contains(globalPos)) return -1;

    int best = -1;
    int bestDist = std::numeric_limits<int>::max();
    const bool useStableBaseSlots = (mDragGroup == group && mDragSlotBasePos.size() == vec->size());
    for (int i = 0; i < vec->size(); ++i) {
        QWidget *card = (*vec)[i].card;
        if (!card || !card->isVisible()) continue;
        QPoint center;
        if (useStableBaseSlots) {
            // 拖拽过程中其它卡牌会被动画挪到预览槽位；如果继续用当前 card->pos()
            // 判断最近槽，目标会在 0/1 之间来回跳，导致“移回去不让位”和抖动。
            // 这里固定用按下瞬间记录的槽位中心，和主小丑槽拖动逻辑一致。
            const QPoint base = mDragSlotBasePos.value(i, card->pos());
            center = parent->mapToGlobal(base + QPoint(card->width() / 2, card->height() / 2));
        } else {
            center = card->mapToGlobal(QPoint(card->width() / 2, card->height() / 2));
        }
        int dist = std::abs(globalPos.x() - center.x());
        if (dist < bestDist) {
            bestDist = dist;
            best = i;
        }
    }
    return best;
}

bool ShopWidget::moveOfferInGroup(DragGroup group, int from, int to)
{
    if (from == to) return true;
    bool ok = false;
    if (group == DragGroup::Shop) {
        ok = mGS->moveShopOffer(from, to);
        if (ok) {
            if (mSelectedShopSlot == from) mSelectedShopSlot = to;
            else if (from < to && mSelectedShopSlot > from && mSelectedShopSlot <= to) --mSelectedShopSlot;
            else if (from > to && mSelectedShopSlot >= to && mSelectedShopSlot < from) ++mSelectedShopSlot;
        }
    } else if (group == DragGroup::Voucher) {
        auto &offers = mGS->shop().voucherOffersMutable();
        ok = (from >= 0 && from < offers.size() && to >= 0 && to < offers.size());
        if (ok) {
            offers.move(from, to);
            if (mSelectedVoucherSlot == from) mSelectedVoucherSlot = to;
            else if (from < to && mSelectedVoucherSlot > from && mSelectedVoucherSlot <= to) --mSelectedVoucherSlot;
            else if (from > to && mSelectedVoucherSlot >= to && mSelectedVoucherSlot < from) ++mSelectedVoucherSlot;

            if (mHoveredVoucherSlot == from) mHoveredVoucherSlot = to;
            else if (from < to && mHoveredVoucherSlot > from && mHoveredVoucherSlot <= to) --mHoveredVoucherSlot;
            else if (from > to && mHoveredVoucherSlot >= to && mHoveredVoucherSlot < from) ++mHoveredVoucherSlot;
        }
    } else if (group == DragGroup::Booster) {
        ok = mGS->moveBoosterOffer(from, to);
        if (ok) {
            if (mSelectedBoosterSlot == from) mSelectedBoosterSlot = to;
            else if (from < to && mSelectedBoosterSlot > from && mSelectedBoosterSlot <= to) --mSelectedBoosterSlot;
            else if (from > to && mSelectedBoosterSlot >= to && mSelectedBoosterSlot < from) ++mSelectedBoosterSlot;
        }
    }
    if (ok && from != to)
        AudioManager::instance()->play(QStringLiteral("cardSlide1"), shopAudioPitchJitter(0.03), 0.62);
    return ok;
}


void ShopWidget::updateDragPreview(DragGroup group, int from, int to)
{
    QVector<OfferUi> *vec = nullptr;
    if (group == DragGroup::Shop) vec = &mShopUi;
    else if (group == DragGroup::Voucher) vec = &mVoucherUi;
    else if (group == DragGroup::Booster) vec = &mBoosterUi;
    if (!vec || from < 0 || from >= vec->size() || mDragSlotBasePos.size() != vec->size()) return;

    if (to < 0 || to >= vec->size()) to = from;
    for (int i = 0; i < vec->size(); ++i) {
        QWidget *card = (*vec)[i].card;
        if (!card || card == mDragWidget) continue;

        int visualSlot = i;
        if (from < to) {
            if (i > from && i <= to) visualSlot = i - 1;
        } else if (from > to) {
            if (i >= to && i < from) visualSlot = i + 1;
        }
        QPoint target = mDragSlotBasePos.value(visualSlot, card->pos());
        // 只要目标槽位没变，就不要反复启动新的 pos 动画。
        // 之前每次 mouseMove 都会在旧动画还没跑完时再塞一条同目标动画，
        // 看起来就像卡牌在目标附近抖动、晃来晃去。
        const QVariant previewProp = card->property("dragPreviewPos");
        if (previewProp.isValid() && previewProp.toPoint() == target)
            continue;
        card->setProperty("dragPreviewPos", target);
        for (QObject *child : card->children()) {
            if (auto *oldAnim = qobject_cast<QPropertyAnimation *>(child)) {
                if (oldAnim->propertyName() == QByteArray("pos")) oldAnim->stop();
            }
        }
        card->raise();
        auto *anim = new QPropertyAnimation(card, "pos", card);
        anim->setDuration(120);
        anim->setStartValue(card->pos());
        anim->setEndValue(target);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->start(QAbstractAnimation::DeleteWhenStopped);

        // priceLbl 跟随 ou.card 同步移动——ou.card 移动不会让 cardBtn 触发 Move 事件
        // （cardBtn 在 ou.card 内的 pos 不变），所以这里直接平行启动一只 priceLbl 动画。
        QLabel *price = (*vec)[i].priceLbl;
        QWidget *cardBtn = (*vec)[i].cardBtn;
        if (price && cardBtn && price->isVisible()) {
            auto *scb = dynamic_cast<ShopCardButton*>(cardBtn);
            const int visOff = (scb && scb->visibleCardSize().isValid())
                ? (cardBtn->height() - scb->visibleCardSize().height()) / 2 : 0;
            const int hoverShift = (scb && scb->isHovered() && scb->hoverLiftEnabled()) ? -8 : 0;
            const QPoint cardTopLeftInThis =
                card->parentWidget() ? card->parentWidget()->mapTo(this, target) : target;
            const QPoint cardBtnInCard = cardBtn->pos();
            const int targetPx = cardTopLeftInThis.x() + cardBtnInCard.x()
                                 + (cardBtn->width() - price->width()) / 2;
            const int targetPy = cardTopLeftInThis.y() + cardBtnInCard.y()
                                 + visOff - price->height() + 1 + hoverShift;
            for (QObject *child : price->children()) {
                if (auto *oldAnim = qobject_cast<QPropertyAnimation*>(child)) {
                    if (oldAnim->propertyName() == QByteArray("pos")) oldAnim->stop();
                }
            }
            auto *priceAnim = new QPropertyAnimation(price, "pos", price);
            priceAnim->setDuration(120);
            priceAnim->setStartValue(price->pos());
            priceAnim->setEndValue(QPoint(targetPx, targetPy));
            priceAnim->setEasingCurve(QEasingCurve::OutCubic);
            priceAnim->start(QAbstractAnimation::DeleteWhenStopped);
        }
    }
}

void ShopWidget::clearDragPreview(bool animateBack)
{
    QVector<OfferUi> *vec = nullptr;
    if (mDragGroup == DragGroup::Shop) vec = &mShopUi;
    else if (mDragGroup == DragGroup::Voucher) vec = &mVoucherUi;
    else if (mDragGroup == DragGroup::Booster) vec = &mBoosterUi;
    if (!vec || mDragSlotBasePos.size() != vec->size()) return;

    for (int i = 0; i < vec->size(); ++i) {
        QWidget *card = (*vec)[i].card;
        if (!card || card == mDragWidget) continue;
        QPoint target = mDragSlotBasePos.value(i, card->pos());
        card->setProperty("dragPreviewPos", QVariant());
        for (QObject *child : card->children()) {
            if (auto *oldAnim = qobject_cast<QPropertyAnimation *>(child)) {
                if (oldAnim->propertyName() == QByteArray("pos")) oldAnim->stop();
            }
        }
        // priceLbl 跟随：与 updateDragPreview 同一套——计算 cardBtn 移动后的目标 priceLbl 位置。
        QLabel *price = (*vec)[i].priceLbl;
        QWidget *cardBtn = (*vec)[i].cardBtn;
        QPoint priceTarget;
        if (price && cardBtn && price->isVisible()) {
            auto *scb = dynamic_cast<ShopCardButton*>(cardBtn);
            const int visOff = (scb && scb->visibleCardSize().isValid())
                ? (cardBtn->height() - scb->visibleCardSize().height()) / 2 : 0;
            const int hoverShift = (scb && scb->isHovered() && scb->hoverLiftEnabled()) ? -8 : 0;
            const QPoint cardTopLeftInThis =
                card->parentWidget() ? card->parentWidget()->mapTo(this, target) : target;
            const QPoint cardBtnInCard = cardBtn->pos();
            priceTarget = QPoint(
                cardTopLeftInThis.x() + cardBtnInCard.x()
                    + (cardBtn->width() - price->width()) / 2,
                cardTopLeftInThis.y() + cardBtnInCard.y()
                    + visOff - price->height() + 1 + hoverShift);
            for (QObject *child : price->children()) {
                if (auto *oldAnim = qobject_cast<QPropertyAnimation*>(child)) {
                    if (oldAnim->propertyName() == QByteArray("pos")) oldAnim->stop();
                }
            }
        }
        if (!animateBack) {
            card->move(target);
            if (price && cardBtn && price->isVisible()) price->move(priceTarget);
            continue;
        }
        auto *anim = new QPropertyAnimation(card, "pos", card);
        anim->setDuration(140);
        anim->setStartValue(card->pos());
        anim->setEndValue(target);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
        if (price && cardBtn && price->isVisible()) {
            auto *priceAnim = new QPropertyAnimation(price, "pos", price);
            priceAnim->setDuration(140);
            priceAnim->setStartValue(price->pos());
            priceAnim->setEndValue(priceTarget);
            priceAnim->setEasingCurve(QEasingCurve::OutCubic);
            priceAnim->start(QAbstractAnimation::DeleteWhenStopped);
        }
    }
}

void ShopWidget::destroyDragGhost()
{
    if (mDragGhost) {
        mDragGhost->hide();
        mDragGhost->deleteLater();
        mDragGhost = nullptr;
    }
}


void ShopWidget::animateOfferReorder(DragGroup group, int from, int to, const QPoint &dropPos)
{
    QVector<OfferUi> *vec = nullptr;
    if (group == DragGroup::Shop) vec = &mShopUi;
    else if (group == DragGroup::Voucher) vec = &mVoucherUi;
    else if (group == DragGroup::Booster) vec = &mBoosterUi;
    if (!vec || from < 0 || to < 0 || from >= vec->size() || to >= vec->size()) {
        refresh();
        return;
    }

    QVector<QPoint> slotPos = mDragSlotBasePos;
    if (slotPos.size() != vec->size()) {
        slotPos.clear();
        for (const OfferUi &ou : *vec) slotPos.append(ou.card ? ou.card->pos() : QPoint());
    }

    refresh();

    // 用户反馈：非拖动商品在 release 时不应该"先跳回原位再动画到目标"——预览阶段它们
    // 已经被 updateDragPreview 移到了视觉上的目标位置（对应 layout 重排后的最终 slot），
    // 应该 stay-put 直接落定；只有被拖动的那张才从 dropPos 平滑动画到目标 slot。
    for (int i = 0; i < vec->size(); ++i) {
        QWidget *card = (*vec)[i].card;
        if (!card || i >= slotPos.size()) continue;

        const QPoint endCardPos = slotPos.value(i, card->pos());
        card->setProperty("dragPreviewPos", QVariant());
        for (QObject *child : card->children()) {
            if (auto *oldAnim = qobject_cast<QPropertyAnimation *>(child)) {
                if (oldAnim->propertyName() == QByteArray("pos")) oldAnim->stop();
            }
        }

        QLabel *price = (*vec)[i].priceLbl;
        QWidget *cardBtn = (*vec)[i].cardBtn;
        auto computePriceTopLeft = [&](const QPoint &cardLocalPos) -> QPoint {
            if (!price || !cardBtn) return QPoint();
            auto *scb = dynamic_cast<ShopCardButton*>(cardBtn);
            const int visOff = (scb && scb->visibleCardSize().isValid())
                ? (cardBtn->height() - scb->visibleCardSize().height()) / 2 : 0;
            QWidget *cardParent = card->parentWidget();
            const QPoint inThis = cardParent ? cardParent->mapTo(this, cardLocalPos) : cardLocalPos;
            const QPoint cardBtnInCard = cardBtn->pos();
            return QPoint(
                inThis.x() + cardBtnInCard.x() + (cardBtn->width() - price->width()) / 2,
                inThis.y() + cardBtnInCard.y() + visOff - price->height() + 1);
        };
        const QPoint priceEnd = computePriceTopLeft(endCardPos);

        if (i == to) {
            // 被拖动的商品：从 dropPos（ghost 释放位置）动画到目标 slot。
            const int duration = 230;
            card->move(dropPos);
            card->raise();
            auto *anim = new QPropertyAnimation(card, "pos", card);
            anim->setDuration(duration);
            anim->setStartValue(dropPos);
            anim->setEndValue(endCardPos);
            anim->setEasingCurve(QEasingCurve::OutCubic);
            anim->start(QAbstractAnimation::DeleteWhenStopped);

            if (price && cardBtn) {
                for (QObject *child : price->children()) {
                    if (auto *oldAnim = qobject_cast<QPropertyAnimation*>(child)) {
                        if (oldAnim->propertyName() == QByteArray("pos")) oldAnim->stop();
                    }
                }
                const QPoint priceStart = computePriceTopLeft(dropPos);
                if (price->isVisible()) {
                    price->move(priceStart);
                    auto *priceAnim = new QPropertyAnimation(price, "pos", price);
                    priceAnim->setDuration(duration);
                    priceAnim->setStartValue(priceStart);
                    priceAnim->setEndValue(priceEnd);
                    priceAnim->setEasingCurve(QEasingCurve::OutCubic);
                    priceAnim->start(QAbstractAnimation::DeleteWhenStopped);
                } else {
                    price->move(priceEnd);
                }
            }
        } else {
            // 非拖动商品：直接落定到 end 位置。预览已经把它们移到了视觉对应位置，
            // 这次 move(end) 把 widget 真正贴到 layout 槽位上，用户感受不到跳变。
            card->move(endCardPos);
            if (price && cardBtn) {
                for (QObject *child : price->children()) {
                    if (auto *oldAnim = qobject_cast<QPropertyAnimation*>(child)) {
                        if (oldAnim->propertyName() == QByteArray("pos")) oldAnim->stop();
                    }
                }
                price->move(priceEnd);
            }
        }
    }
    mDragSlotBasePos.clear();
}

void ShopWidget::onShopCardClicked(int slot)
{
    if (slot < 0 || slot >= mGS->shop().shopOffers().size()) return;
    const ShopOffer &o = mGS->shop().shopOffers()[slot];
    if (o.sold) return;

    // 切换选中：同一格再点一次 → 取消；点别的格 → 单选切换。同时清掉其他行的选中。
    mSelectedShopSlot = (mSelectedShopSlot == slot) ? -1 : slot;
    mSelectedVoucherSlot = -1;
    mSelectedBoosterSlot = -1;
    refresh();
}

void ShopWidget::onVoucherCardClicked(int slot)
{
    if (slot < 0 || slot >= mGS->shop().voucherOffers().size()) return;
    const ShopOffer &o = mGS->shop().voucherOffers()[slot];
    if (o.sold) return;
    mSelectedVoucherSlot = (mSelectedVoucherSlot == slot) ? -1 : slot;
    mSelectedShopSlot = -1;
    mSelectedBoosterSlot = -1;
    refresh();
}

void ShopWidget::onBoosterCardClicked(int slot)
{
    if (slot < 0 || slot >= mGS->shop().boosterOffers().size()) return;
    const ShopOffer &o = mGS->shop().boosterOffers()[slot];
    if (o.sold) return;
    mSelectedBoosterSlot = (mSelectedBoosterSlot == slot) ? -1 : slot;
    mSelectedShopSlot = -1;
    mSelectedVoucherSlot = -1;
    refresh();
}

void ShopWidget::positionSlotActionButtons(OfferUi &ou, bool hasUseBtn)
{
    // 等当前 layout 周期完成后才能读到 cardBtn 的位置；用 singleShot 延后。
    // 购买按钮贴卡片下沿（中间）；购买&使用贴卡片右侧中线。
    QPointer<QWidget> buyGuard(ou.buyBtn);
    QPointer<QWidget> useGuard(ou.useBtn);
    QPointer<QWidget> cardGuard(ou.cardBtn);
    QPointer<QWidget> containerGuard(ou.card);
    QTimer::singleShot(0, this, [buyGuard, useGuard, cardGuard, containerGuard, hasUseBtn]() {
        if (!cardGuard || !containerGuard) return;
        QPoint p = cardGuard->mapTo(containerGuard.data(), QPoint(0, 0));
        // refresh() 会提前写入 cardTargetY。按钮按目标位置摆放，
        // 这样卡牌正在上浮动画时，按钮也能贴在上浮后的卡牌底边。
        bool ok = false;
        int visualY = cardGuard->property("cardTargetY").toInt(&ok);
        if (ok) p.setY(visualY);
        QSize visualSize = cardGuard->size();
        QPoint visualTopLeft = p;
        if (auto *scb = dynamic_cast<ShopCardButton *>(cardGuard.data())) {
            if (scb->visibleCardSize().isValid()) {
                visualSize = scb->visibleCardSize();
                visualTopLeft += QPoint((cardGuard->width() - visualSize.width()) / 2,
                                        (cardGuard->height() - visualSize.height()) / 2);
            }
        }
        if (buyGuard && buyGuard->isVisible()) {
            // 挂在卡牌下方，和牌面留出细微缝隙；不再半压在牌面上，避免选中时按钮与卡片重叠。
            int x = visualTopLeft.x() + (visualSize.width() - buyGuard->width()) / 2;
            int y = visualTopLeft.y() + visualSize.height() + dp(3);
            y = qMin(y, containerGuard->height() - buyGuard->height());
            buyGuard->move(x, y);
            buyGuard->raise();
        }
        if (useGuard && hasUseBtn) {
            // 左边沿压住卡片右边框 1px：视觉上贴合卡牌，且左侧直角不会露出缝。
            int x = visualTopLeft.x() + visualSize.width() - dp(1);
            int y = visualTopLeft.y() + visualSize.height() / 2 - useGuard->height() / 2;
            useGuard->move(x, y);
            useGuard->raise();
        }
    });
}

void ShopWidget::onBuyShop(int slot) {
    if (slot < 0 || slot >= mGS->shop().shopOffers().size()) return;
    const ShopOffer offerBeforeBuy = mGS->shop().shopOffers()[slot];
    QPixmap flyPix = offerPixmap(offerBeforeBuy);
    QPoint flyCenter;
    if (slot >= 0 && slot < mShopUi.size() && mShopUi[slot].cardBtn)
        flyCenter = mShopUi[slot].cardBtn->mapToGlobal(
            QPoint(mShopUi[slot].cardBtn->width() / 2, mShopUi[slot].cardBtn->height() / 2));

    int targetArea = 0;
    if (offerBeforeBuy.kind == OfferKind::Joker) targetArea = 1;
    else if (offerBeforeBuy.kind == OfferKind::Tarot ||
             offerBeforeBuy.kind == OfferKind::Planet ||
             offerBeforeBuy.kind == OfferKind::Spectral) targetArea = 2;
    else if (offerBeforeBuy.kind == OfferKind::PlayingCard) targetArea = 3;

    // 小丑/消耗牌购买后会立刻触发 GameState 的 changed 信号。先把起点交给 MainWindow，
    // 让新生成的真实卡片直接从购买位置飞入槽位，避免“槽位先生成一张 + 又飞来一张”的重影。
    const bool needsSlotFlyIn = (targetArea == 1 || targetArea == 2);
    if (needsSlotFlyIn && !flyPix.isNull())
        emit shopItemBoughtForAnimation(flyPix, flyCenter, targetArea);

    if (mGS->buyShopOffer(slot)) {
        QTimer::singleShot(100, this, [cost = offerBeforeBuy.cost]() {
            AudioManager::instance()->play(QStringLiteral("card1"), 1.0, 1.0);
            if (cost != 0)
                AudioManager::instance()->play(QStringLiteral("coin1"), 1.0, 1.0);
        });
        if (!needsSlotFlyIn && targetArea != 0 && !flyPix.isNull())
            emit shopItemBoughtForAnimation(flyPix, flyCenter, targetArea);
        mSelectedShopSlot = -1;
        refresh();
    } else if (needsSlotFlyIn) {
        AudioManager::instance()->play(QStringLiteral("cancel"), 1.0, 0.65);
        emit shopItemBoughtForAnimation(QPixmap(), QPoint(), 0); // 购买失败时清掉预备动画
    }
}

void ShopWidget::onBuyAndUseShop(int slot) {
    const auto offers = mGS->shop().shopOffers();
    const int cost = (slot >= 0 && slot < offers.size()) ? offers[slot].cost : 0;
    const QString useSound = (slot >= 0 && slot < offers.size())
        ? soundForOfferKind(offers[slot].kind)
        : QStringLiteral("tarot1");

    // 行星/黑洞买并立即使用:卡牌不会进消耗牌槽,因此 MainWindow 看不到一个可抬升的
    // ConsumableItem。这里在买入前先抓拍它的全局中心,如果买入成功就让 MainWindow
    // 在那个位置生成一张幽灵卡演完动画——和消耗牌槽内使用的视觉对齐。
    ConsumableType animType = ConsumableType::Tarot_Fool;
    QPoint useCenter;
    bool wantUseAnim = false;
    if (slot >= 0 && slot < offers.size() && slot < mShopUi.size()) {
        const ShopOffer &o = offers[slot];
        const bool isPlanetLike = (o.kind == OfferKind::Planet)
            || (o.kind == OfferKind::Spectral && o.consumable == ConsumableType::Spectral_BlackHole);
        const bool isWheel = (o.kind == OfferKind::Tarot && o.consumable == ConsumableType::Tarot_Wheel);
        QWidget *cardBtn = mShopUi[slot].cardBtn;
        if ((isPlanetLike || isWheel) && cardBtn && cardBtn->isVisible()) {
            animType = o.consumable;
            useCenter = cardBtn->mapToGlobal(QPoint(cardBtn->width() / 2, cardBtn->height() / 2));
            wantUseAnim = true;
        }
    }

    if (wantUseAnim)
        emit shopConsumableUseAnimation(int(animType), useCenter);

    if (mGS->buyAndUseShopConsumable(slot, {})) {
        QTimer::singleShot(100, this, [cost]() {
            AudioManager::instance()->play(QStringLiteral("card1"), 1.0, 1.0);
            if (cost != 0)
                AudioManager::instance()->play(QStringLiteral("coin1"), 1.0, 1.0);
        });
        QTimer::singleShot(180, this, [useSound]() {
            AudioManager::instance()->play(useSound, 1.0, 1.0);
        });
        mSelectedShopSlot = -1;
        refresh();
    } else {
        AudioManager::instance()->play(QStringLiteral("cancel"), 1.0, 0.65);
    }
}

// 兑换券 / 开包前先清选中再交给原逻辑。

void ShopWidget::onBuyVoucher(int slot) {
    if (mVoucherPurchaseAnimating) return;

    if (slot < 0 || slot >= mVoucherUi.size()) {
        if (mGS->buyVoucherOffer(slot)) {
            AudioManager::instance()->play(QStringLiteral("card1"), 1.0, 1.0);
            AudioManager::instance()->play(QStringLiteral("coin1"), 1.0, 1.0);
            mSelectedVoucherSlot = -1;
            refresh();
        } else {
            AudioManager::instance()->play(QStringLiteral("cancel"), 1.0, 0.65);
        }
        return;
    }
    const auto voucherOffers = mGS->shop().voucherOffers();
    if (slot < 0 || slot >= voucherOffers.size() ||
        !mGS->shop().canBuyVoucher(slot, mGS->spendableGold())) {
        AudioManager::instance()->play(QStringLiteral("cancel"), 1.0, 0.65);
        return;
    }
    const QString voucherRedeemName = voucherData(voucherOffers[slot].voucher).name;
    const QPoint voucherShopStart = pos();
    const int parentH = parentWidget() ? parentWidget()->height() : (y() + height());
    const QPoint voucherShopDown(voucherShopStart.x(),
                                 qMax(parentH + dp(28), voucherShopStart.y() + height() + dp(28)));
    bool voucherHasAnimation = false;

    // 兑换动画：完整 3 段——
    //   Phase A (260ms)：截下 cardBtn 像素 → ghost 沿屏幕中心平移 + 放大到 2.4x；
    //   Phase B (240ms)：在中心轻微摇晃 (±5° 抖动)；
    //   Phase C (320ms)：dissolve——再放大一档 + opacity 1→0。
    // 这是对原版 booster_open 类似的"挥发"序列在 voucher 上的复刻。
    QWidget *cardBtn = mVoucherUi[slot].cardBtn;
    if (cardBtn && cardBtn->isVisible()) {
        QPixmap snap(cardBtn->size());
        snap.fill(Qt::transparent);
        cardBtn->render(&snap);

        // 把 ghost 挂到 top-level 窗口而不是 ShopWidget——否则后续 refresh() 触发的
        // syncPriceLblForCardBtn()->priceLbl->raise() 会把价格标签盖回 ghost 上面。
        QWidget *ghostHost = window() ? window() : this;
        auto *ghost = new QLabel(ghostHost);
        ghost->setPixmap(snap);
        ghost->setFixedSize(snap.size());
        ghost->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        ghost->setStyleSheet("background:transparent;");
        ghost->setAlignment(Qt::AlignCenter);
        ghost->setScaledContents(true);
        const QPoint globalTopLeft = cardBtn->mapToGlobal(QPoint(0, 0));
        const QPoint hostTopLeft = ghostHost->mapFromGlobal(globalTopLeft);
        ghost->move(hostTopLeft);
        ghost->show();
        ghost->raise();

        auto *opacity = new QGraphicsOpacityEffect(ghost);
        opacity->setOpacity(1.0);
        ghost->setGraphicsEffect(opacity);

        auto makeRedeemText = [this, ghostHost](const QString &text, int px) {
            auto *label = new QLabel(text, ghostHost);
            QFont f = mCNFont;
            f.setPixelSize(fontPx(px));
            f.setBold(true);
            label->setFont(f);
            label->setStyleSheet(QStringLiteral("color:white; background:transparent;"));
            label->setAlignment(Qt::AlignCenter);
            label->setAttribute(Qt::WA_TransparentForMouseEvents, true);
            label->adjustSize();
            label->hide();
            return label;
        };
        QLabel *topText = makeRedeemText(voucherRedeemName, 24);
        QLabel *botText = makeRedeemText(QStringLiteral("兑换！"), 24);

        const QSize startSize = snap.size();
        const QPoint startTopLeft = hostTopLeft;
        const QPoint startCenter(startTopLeft.x() + startSize.width()  / 2,
                                  startTopLeft.y() + startSize.height() / 2);
        // 中心点要用 ghost 实际宿主（top-level）的坐标系。
        const QPoint screenCenter(ghostHost->width() / 2, ghostHost->height() / 2);

        QPointer<QLabel> ghostGuard(ghost);
        QPointer<QLabel> topTextGuard(topText);
        QPointer<QLabel> botTextGuard(botText);
        QPointer<QGraphicsOpacityEffect> opGuard(opacity);
        auto placeRedeemText = [ghostGuard, topTextGuard, botTextGuard]() {
            if (!ghostGuard) return;
            const QRect g = ghostGuard->geometry();
            if (topTextGuard)
                topTextGuard->move(g.center().x() - topTextGuard->width() / 2,
                                   g.top() - topTextGuard->height() - 6);
            if (botTextGuard)
                botTextGuard->move(g.center().x() - botTextGuard->width() / 2,
                                   g.bottom() + 6);
        };
        auto setGhostGeom = [ghostGuard, startSize, placeRedeemText](qreal scale, QPoint centerInThis) {
            if (!ghostGuard) return;
            const QSize ns(int(startSize.width()  * scale),
                           int(startSize.height() * scale));
            ghostGuard->setFixedSize(ns);
            ghostGuard->move(centerInThis.x() - ns.width()  / 2,
                             centerInThis.y() - ns.height() / 2);
            placeRedeemText();
        };

        mVoucherPurchaseAnimating = true;
        voucherHasAnimation = true;
        hideOfferInfo();
        if (mVoucherUi[slot].card) mVoucherUi[slot].card->hide();
        if (mVoucherUi[slot].priceLbl) mVoucherUi[slot].priceLbl->hide();
        placeRedeemText();
        QTimer::singleShot(400, this, [topTextGuard, botTextGuard, placeRedeemText]() {
            placeRedeemText();
            if (topTextGuard) { topTextGuard->show(); topTextGuard->raise(); }
            if (botTextGuard) { botTextGuard->show(); botTextGuard->raise(); }
        });

        auto *shopOut = new QPropertyAnimation(this, "pos", this);
        shopOut->setDuration(260);
        shopOut->setStartValue(voucherShopStart);
        shopOut->setEndValue(voucherShopDown);
        shopOut->setEasingCurve(QEasingCurve::InCubic);
        connect(shopOut, &QPropertyAnimation::finished, this, [this]() { hide(); });
        shopOut->start(QAbstractAnimation::DeleteWhenStopped);

        // Phase A：平移到屏幕中心 + 放大到 1.5（之前 2.4 太大，与开包视觉量级不一致）
        auto *phaseA = new QVariantAnimation(this);
        phaseA->setDuration(260);
        phaseA->setStartValue(0.0);
        phaseA->setEndValue(1.0);
        phaseA->setEasingCurve(QEasingCurve::OutCubic);
        connect(phaseA, &QVariantAnimation::valueChanged, ghost,
                [setGhostGeom, startCenter, screenCenter](const QVariant &v) {
            const qreal t = v.toReal();
            const qreal scale = 1.0 + 0.5 * t;
            const QPoint c(startCenter.x() + int((screenCenter.x() - startCenter.x()) * t),
                           startCenter.y() + int((screenCenter.y() - startCenter.y()) * t));
            setGhostGeom(scale, c);
        });

        // Phase B：在中央摇晃（位移近似旋转）；scale 保持在 1.5。
        auto *phaseB = new QSequentialAnimationGroup(this);
        for (int i = 0; i < 4; ++i) {
            auto *step = new QVariantAnimation(phaseB);
            step->setDuration(60);
            step->setStartValue(0.0);
            step->setEndValue(1.0);
            step->setEasingCurve(QEasingCurve::InOutQuad);
            const int dx = (i % 2 == 0 ? 6 : -6);
            connect(step, &QVariantAnimation::valueChanged, ghost,
                    [setGhostGeom, screenCenter, dx](const QVariant &v) {
                const qreal t = v.toReal();
                const qreal s = std::sin(t * M_PI);
                setGhostGeom(1.5, QPoint(screenCenter.x() + int(dx * s), screenCenter.y()));
            });
            phaseB->addAnimation(step);
        }

        // Phase C：dissolve——走 BalatroShaders::renderDissolvePixmap，与小丑包/Blind 切换
        //          一致的"灼烧 -> 颗粒消散"效果；同时继续放大到 3.2。
        auto *phaseC = new QVariantAnimation(this);
        phaseC->setDuration(380);
        phaseC->setStartValue(0.0);
        phaseC->setEndValue(1.0);
        phaseC->setEasingCurve(QEasingCurve::InCubic);
        const QPixmap dissolveBase = snap;  // 用截下来的 cardBtn 像素做溶解基底
        connect(phaseC, &QVariantAnimation::valueChanged, ghost,
                [ghostGuard, setGhostGeom, screenCenter, dissolveBase](const QVariant &v) {
            if (!ghostGuard) return;
            const qreal t = qBound(0.0, v.toDouble(), 1.0);
            const qreal scale = 1.5 + 0.3 * t;
            setGhostGeom(scale, screenCenter);
            // 溶解 shader：burn1 暖白 / burn2 橙红，匹配优惠券"被使用掉"的能量耗散感。
            QPixmap dissolved = BalatroShaders::renderDissolvePixmap(
                dissolveBase, t,
                QColor(255, 230, 130, 200),
                QColor(255, 110, 70, 160),
                1.0);
            if (!dissolved.isNull()) ghostGuard->setPixmap(dissolved);
        });

        auto *seq = new QSequentialAnimationGroup(this);
        seq->addAnimation(phaseA);
        seq->addAnimation(phaseB);
        seq->addAnimation(phaseC);
        connect(seq, &QSequentialAnimationGroup::finished, this,
                [this, ghost, topText, botText, voucherShopStart, voucherShopDown]() {
            if (topText) topText->deleteLater();
            if (botText) botText->deleteLater();
            ghost->deleteLater();
            move(voucherShopDown);
            mVoucherPurchaseAnimating = false;
            refresh();
            show();
            raise();

            auto *shopIn = new QPropertyAnimation(this, "pos", this);
            shopIn->setDuration(320);
            shopIn->setStartValue(voucherShopDown);
            shopIn->setEndValue(voucherShopStart);
            shopIn->setEasingCurve(QEasingCurve::OutCubic);
            shopIn->start(QAbstractAnimation::DeleteWhenStopped);
        });
        seq->start(QAbstractAnimation::DeleteWhenStopped);
    }
    if (mGS->buyVoucherOffer(slot)) {
        auto playRedeemSounds = []() {
            AudioManager::instance()->play(QStringLiteral("card1"), 1.0, 1.0);
            AudioManager::instance()->play(QStringLiteral("coin1"), 1.0, 1.0);
        };
        if (voucherHasAnimation)
            QTimer::singleShot(400, this, playRedeemSounds);
        else
            playRedeemSounds();
        mSelectedVoucherSlot = -1;
        refresh();
    } else {
        if (voucherHasAnimation) {
            mVoucherPurchaseAnimating = false;
            move(voucherShopStart);
            show();
            refresh();
        }
        AudioManager::instance()->play(QStringLiteral("cancel"), 1.0, 0.65);
    }
}

void ShopWidget::onBuyBooster(int slot) {
    const auto &offers = mGS->shop().boosterOffers();
    if (slot >= offers.size()) return;
    mSelectedBoosterSlot = -1;
    emit packBuyRequested(slot);   // 让 MainWindow 唤起开包界面
}

void ShopWidget::onReroll()
{
    mGS->rerollShop();
    AudioManager::instance()->play(QStringLiteral("coin2"), 1.0, 1.0);
    AudioManager::instance()->play(QStringLiteral("other1"), 1.0, 1.0);
    refresh();
}

void ShopWidget::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    layoutPanel();
    layoutVoucherFan();
}

void ShopWidget::layoutPanel()
{
    if (!mPanel) return;
    // 原版商店面板几乎吃满下半屏宽度，且高度优先保证两行商品完整显示。
    // 这里按当前 overlay 的实际尺寸取比例，而不是只按物理屏幕 dp，避免窗口/截图较小时
    // 固定控件仍按大屏尺寸排布，导致下排卡包名和按钮被裁掉。
    // 顶部留 dp(12) 与小丑槽位框隔开；面板向下延伸到 overlay 底边，不截断底部内容。
    // setMinimumHeight(0) 必须先于 resize()，否则 buildUi() 设置的 dp(620) 下限会阻止缩小。
    const int topGap = dp(70);
    const int minW = qMin(width(), dp(900));
    const int maxW = qMin(width(), dp(1240));
    int panelW = qBound(minW, int(width() * 0.96), maxW)-15;
    int panelH = qMax(dp(200), height() - topGap-80);
    mPanel->setMinimumHeight(0);
    mPanel->resize(panelW, panelH);
    int x = (width() - mPanel->width()) / 2;
    mPanel->move(x, topGap);
}
