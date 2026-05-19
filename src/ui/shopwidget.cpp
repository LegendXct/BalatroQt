#include "shopwidget.h"
#include "../card/consumableitem.h"
#include "../card/jokeritem.h"
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
#include <QGuiApplication>
#include <QScreen>
#include <QCursor>
#include <QtGlobal>
#include <cmath>
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
    }

    void setDisplayPixmap(const QPixmap &pix)
    {
        mPixmap = pix;
        update();
    }

protected:
    void enterEvent(QEnterEvent *event) override
    {
        mHovered = true;
        QPushButton::enterEvent(event);
        update();
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
            mTiltY = qBound(-10.0, nx * 20.0, 10.0);
            mTiltX = qBound(-10.0, ny * 20.0, 10.0);
        }
        QPushButton::mouseMoveEvent(event);
        update();
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, false);

        // 原版卡牌/卡包自己带阴影和高光，商店槽位按钮不应该再画一层黑底。
        // 之前的黑色矩形和卡片后面的脏阴影就是这里的按钮背景造成的。
        if (mPixmap.isNull()) return;
        // 卡图按 1:1 填满 button 矩形，与右上角消耗品 / 小丑等场景里的卡牌等大。
        // hover 时的 1.045 缩放也只是稍微溢出，QPainter 默认不会裁出 button rect。
        QSize target(int(width() * 0.96), int(height() * 0.96));
        QSize pixSize = mPixmap.size();
        pixSize.scale(target, Qt::KeepAspectRatio);
        QRectF pr(QPointF(-pixSize.width() / 2.0, -pixSize.height() / 2.0), pixSize);

        p.save();
        p.setOpacity(isEnabled() ? 1.0 : 0.42);
        p.translate(width() / 2.0, height() / 2.0 + (mHovered ? -4.0 : 0.0));

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
        const qreal hoverScale = 1.0 + (mHovered ? 0.045 : 0.0);
        t.scale(hoverScale, hoverScale);
        t = persp * t;
        p.setTransform(t, true);

        p.drawPixmap(pr, mPixmap, QRectF(0, 0, mPixmap.width(), mPixmap.height()));
        p.restore();
    }

private:
    QPixmap mPixmap;
    bool mHovered = false;
    qreal mTiltX = 0.0; // degrees, same direction as CardItem/JokerItem
    qreal mTiltY = 0.0;
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
    // 商品紧凑后的设计最小尺寸：≈ 820×620 dp。layoutPanel 仍按容器宽度做平滑伸缩。
    mPanel->setMinimumSize(dp(820), dp(620));
    mPanel->setAttribute(Qt::WA_StyledBackground, true);
    mPanel->setStyleSheet(
        "QWidget#shopPanel {"
        "  background:rgba(35,48,51,235);"
        // 三边描边 + 顶部圆角；下边缘无 border、底部圆角为 0，让面板贴到屏幕底部时看不到下边线。
        "  border-top: 3px solid #fe5f55;"
        "  border-left: 3px solid #fe5f55;"
        "  border-right: 3px solid #fe5f55;"
        "  border-bottom: 0px;"
        "  border-top-left-radius: 18px;"
        "  border-top-right-radius: 18px;"
        "  border-bottom-left-radius: 0px;"
        "  border-bottom-right-radius: 0px;"
        "}"
        );

    auto *root = new QVBoxLayout(mPanel);
    root->setContentsMargins(dp(18), dp(16), dp(18), dp(16));
    root->setSpacing(dp(10));

    // ── 顶部:右上角金币 ──
    auto *topRow = new QWidget(mPanel);
    auto *thbl = new QHBoxLayout(topRow);
    thbl->setContentsMargins(0, 0, 0, 0);
    thbl->addStretch();

    auto *goldBox = new QWidget(topRow);
    goldBox->setAttribute(Qt::WA_StyledBackground, true);
    goldBox->setStyleSheet("background:#4f6367; border-radius:8px;");
    auto *gbl = new QHBoxLayout(goldBox);
    gbl->setContentsMargins(dp(12), dp(4), dp(12), dp(4));
    mLblGold = new QLabel("$0", goldBox);
    QFont gf = mCNFont; gf.setPixelSize(fontPx(30)); gf.setBold(true);
    mLblGold->setFont(gf);
    mLblGold->setStyleSheet("color:#f3b958; background:transparent;");
    gbl->addWidget(mLblGold);
    thbl->addWidget(goldBox);

    root->addWidget(topRow);

    // ── 上栏:[Next 红 + Reroll 绿] | 商品区(2 槽) ──
    auto *upperRow = new QWidget(mPanel);
    auto *uhbl = new QHBoxLayout(upperRow);
    uhbl->setContentsMargins(0, 0, 0, 0);
    uhbl->setSpacing(dp(12));

    // 左:两按钮竖排
    auto *btnCol = new QWidget(upperRow);
    btnCol       ->setFixedWidth(dp(160));
    auto *bvbl = new QVBoxLayout(btnCol);
    bvbl->setContentsMargins(0, 0, 0, 0);
    bvbl->setSpacing(dp(8));

    mBtnNextRound = new QPushButton("下一个\n回合", btnCol);
    mBtnNextRound->setFixedSize(dp(160), dp(126));
    QFont nrf = mCNFont; nrf.setPixelSize(fontPx(18)); nrf.setBold(true);
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
    mBtnReroll   ->setFixedSize(dp(160), dp(126));
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
    auto *voucherBox = new QWidget(lowerRow);
    voucherBox->setObjectName("voucherBox");
    voucherBox->setMinimumSize(dp(200), dp(340));
    voucherBox->setAttribute(Qt::WA_StyledBackground, true);
    voucherBox->setStyleSheet(
        "QWidget#voucherBox { background:rgba(57,72,76,230); border:none; border-radius:14px; }"
        );
    auto *vbl = new QVBoxLayout(voucherBox);
    vbl->setContentsMargins(dp(10), dp(8), dp(10), dp(8));
    vbl->setAlignment(Qt::AlignCenter);

    OfferUi vu = createOfferSlot(voucherBox, false);
    vu.card->setProperty("voucherSlotIdx", 0);
    connect(vu.cardBtn, &QPushButton::clicked, this, [this]() { onVoucherCardClicked(0); });
    if (vu.buyBtn)
        connect(vu.buyBtn, &QPushButton::clicked, this, [this]() { onBuyVoucher(0); });
    vbl->addWidget(vu.card);
    mVoucherUi.append(vu);
    lhbl->addWidget(voucherBox);

    // Booster 区
    auto *boosterBox = new QWidget(lowerRow);
    boosterBox->setObjectName("boosterBox");
    boosterBox->setAttribute(Qt::WA_StyledBackground, true);
    boosterBox->setStyleSheet(
        "QWidget#boosterBox { background:rgba(57,72,76,230); border:none; border-radius:14px; }"
        );
    auto *bhbl = new QHBoxLayout(boosterBox);
    // booster 行间距比 shop 上方略宽，但比之前 12dp 紧，避免显得稀疏。
    bhbl->setContentsMargins(dp(10), dp(6), dp(10), dp(6));
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
    // 原版 UI_definitions.lua:656 给 booster 列 (2.4*CARD_W, 1.15*CARD_H, card_w = 1.27*CARD_W)：
    //   booster slot width  = 1.27 * G.CARD_W
    //   booster slot height = 1.15 * G.CARD_H
    // 普通 shop slot 与 G.CARD_W × G.CARD_H 同号（线 641 中 height 系数 1.05 几乎可以忽略）。
    // 我们的 CardItem 是 170×228，shop slot 与之等高等宽；booster 按 1.27W × 1.15H 放大。
    // 卡图与右上角消耗品 / 小丑场景里的卡牌使用同一尺寸 (CardItem::WIDTH×HEIGHT = 170×228)，
    // 不再做 0.80 缩小，鼠标 hover 时的 1.045 放大本身就够明显。
    const int slotW = isBooster ? 210 : 170;
    const int slotH = isBooster ? 252 : 228;
    // 容器右侧多留 60 px 给"购买&使用"按钮伸出；这样按钮可以贴卡片右沿而不被剪。
    const int containerW = slotW + 4 + (isBooster ? 0 : 60);
    // 价格标签 32 + 间距 2 + 卡图 slotH + 间距 2 + 名字 50（整体压得更紧凑）。
    const int containerH = 32 + 2 + slotH + 2 + 50;

    ou.card = new QWidget(parent);
    ou.card->setFixedSize(dp(containerW), dp(containerH));
    ou.card->setStyleSheet("background:transparent;");

    auto *vbl = new QVBoxLayout(ou.card);
    vbl->setContentsMargins(0, 0, 0, 0);
    vbl->setSpacing(dp(2));
    vbl->setAlignment(Qt::AlignCenter);

    // 顶部 $X 价格标签(深底圆角)
    ou.priceLbl = new QLabel("$0", ou.card);
    ou.priceLbl->setFixedSize(dp(72), dp(32));
    QFont pf = mCNFont; pf.setPixelSize(fontPx(19)); pf.setBold(true);
    ou.priceLbl->setFont(pf);
    ou.priceLbl->setAlignment(Qt::AlignCenter);
    ou.priceLbl->setStyleSheet(
        "color:#f3b958; background:#374244; border-radius:6px;"
        );
    vbl->addWidget(ou.priceLbl, 0, Qt::AlignCenter);

    // 卡图(整张点击)
    ou.cardBtn = new ShopCardButton(ou.card);
    ou.cardBtn->setFixedSize(dp(slotW), dp(slotH));
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

    // 收窄到卡牌宽 ≈ 45%，紧贴卡片底部居中，看起来"挂在卡片下方"。
    ou.buyBtn = new QPushButton("购买", ou.card);
    ou.buyBtn->setCursor(Qt::PointingHandCursor);
    ou.buyBtn->setFixedSize(dp(slotW * 45 / 100), dp(28));
    ou.buyBtn->setFont(bbf);
    ou.buyBtn->hide();

    // 购买&使用：贴卡片右侧，宽 38、高 50。
    ou.useBtn = new QPushButton("购买\n&使用", ou.card);
    ou.useBtn->setCursor(Qt::PointingHandCursor);
    ou.useBtn->setFixedSize(dp(38), dp(50));
    ou.useBtn->setFont(bbf);
    ou.useBtn->setStyleSheet(QString(
        "QPushButton { background:#fe5f55; color:white; border:2px solid rgba(255,255,255,90);"
        " border-radius:8px; font-weight:bold; padding:0; }"
        "QPushButton:hover { background:#ff7066; border:2px solid rgba(255,255,255,170); }"
        "QPushButton:disabled { background:#7a3c39; color:#cab4b2; border:2px solid #5a2a28; }"
    ));
    ou.useBtn->hide();
    ou.actionRow = nullptr;
    Q_UNUSED(isBooster);

    ou.nameLbl = new QLabel("", ou.card);
    QFont nf = mCNFont; nf.setPixelSize(fontPx(16));
    ou.nameLbl->setFont(nf);
    ou.nameLbl->setStyleSheet("color:white; background:transparent;");
    ou.nameLbl->setAlignment(Qt::AlignCenter);
    ou.nameLbl->setWordWrap(true);
    ou.nameLbl->setFixedHeight(dp(50));
    vbl->addWidget(ou.nameLbl);

    return ou;
}

void ShopWidget::buildInfoPanel()
{
    if (mInfoPanel) return;
    mInfoPanel = new QWidget(this);
    mInfoPanel->setAttribute(Qt::WA_StyledBackground, true);
    mInfoPanel->setStyleSheet(
        "background:rgba(31,37,42,245);"
        "border:2px solid #fda200;"
        "border-radius:12px;"
    );

    auto *vbl = new QVBoxLayout(mInfoPanel);
    vbl->setContentsMargins(dp(12), dp(10), dp(12), dp(10));
    vbl->setSpacing(dp(6));

    mInfoTitle = new QLabel(mInfoPanel);
    QFont tf = mCNFont; tf.setPixelSize(fontPx(22)); tf.setBold(true);
    mInfoTitle->setFont(tf);
    mInfoTitle->setAlignment(Qt::AlignCenter);
    mInfoTitle->setStyleSheet("color:#ffe9a8; background:transparent; border:none;");
    mInfoTitle->setWordWrap(true);
    mInfoTitle->setFixedWidth(dp(310));
    vbl->addWidget(mInfoTitle);

    mInfoBody = new QLabel(mInfoPanel);
    QFont bf = mCNFont; bf.setPixelSize(fontPx(18));
    mInfoBody->setFont(bf);
    mInfoBody->setAlignment(Qt::AlignCenter);
    mInfoBody->setWordWrap(true);
    mInfoBody->setStyleSheet("color:white; background:transparent; border:none;");
    mInfoBody->setFixedWidth(dp(310));
    vbl->addWidget(mInfoBody);

    mInfoPanel->setFixedWidth(dp(344));
    mInfoPanel->hide();
}

void ShopWidget::showOfferInfo(QWidget *source)
{
    if (!source) return;
    buildInfoPanel();
    const QString title = source->property("infoTitle").toString();
    const QString body = source->property("infoBody").toString();
    if (title.isEmpty() && body.isEmpty()) return;

    mInfoTitle->setText(title);
    mInfoBody->setText(body);
    mInfoBody->setVisible(!body.isEmpty());
    if (auto *lay = mInfoPanel->layout()) lay->activate();
    mInfoPanel->adjustSize();
    mInfoPanel->resize(dp(344), qBound(dp(120), mInfoPanel->height(), dp(340)));

    QPoint pos = source->mapTo(this, QPoint(source->width() + dp(12), 0));
    if (pos.x() + mInfoPanel->width() > width() - 8)
        pos.setX(source->mapTo(this, QPoint(-mInfoPanel->width() - dp(12), 0)).x());
    pos.setX(qBound(dp(6), pos.x(), qMax(dp(6), width() - mInfoPanel->width() - dp(6))));
    pos.setY(qBound(dp(6), pos.y(), qMax(dp(6), height() - mInfoPanel->height() - dp(6))));

    mInfoPanel->move(pos);
    mInfoPanel->raise();
    mInfoPanel->show();
}

void ShopWidget::hideOfferInfo()
{
    if (mInfoPanel) mInfoPanel->hide();
}

bool ShopWidget::eventFilter(QObject *obj, QEvent *event)
{
    QWidget *w = qobject_cast<QWidget *>(obj);
    if (w && w->property("infoTitle").isValid()) {
        if (event->type() == QEvent::Enter) {
            showOfferInfo(w);
        } else if (event->type() == QEvent::Leave || event->type() == QEvent::Hide) {
            hideOfferInfo();
        }
    }
    return QWidget::eventFilter(obj, event);
}

void ShopWidget::refresh()
{
    mLblGold->setText(QString("$%1").arg(mGS->gold()));

    auto fillSlot = [this](OfferUi &ou, const ShopOffer &o, bool canBuy, bool isBooster) {
        Q_UNUSED(isBooster);
        if (o.sold) {
            ou.card->setVisible(false);
            ou.cardBtn->setToolTip(QString());
            ou.cardBtn->setProperty("infoTitle", QString());
            ou.cardBtn->setProperty("infoBody", QString());
            if (ou.buyBtn) ou.buyBtn->hide();
            if (ou.useBtn) ou.useBtn->hide();
            return;
        }
        ou.card->setVisible(true);

        QString name;
        QString body;
        if (o.kind == OfferKind::Joker) {
            Joker tmp = createJoker(o.joker);
            QString ed = editionDisplayName(o.jokerEdition);
            name = ed.isEmpty() ? tmp.name : (ed + " " + tmp.name);
            body = tmp.description;
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
        const QString offerHash = QString("%1|%2|%3|%4|%5")
                                      .arg(int(o.kind))
                                      .arg(int(o.joker))
                                      .arg(int(o.jokerEdition))
                                      .arg(int(o.consumable))
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
            ou.buyBtn->setEnabled(canBuy && mGS->gold() >= o.cost);
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
        if (selectedHere) positionSlotActionButtons(ou, ou.useBtn && ou.useBtn->isVisible());

        // 选中卡片轻微上浮——对齐原版 card.lua highlighted 时的 highlight_offset。
        if (ou.cardBtn) {
            QPointer<QWidget> btnGuard(ou.cardBtn);
            QTimer::singleShot(0, this, [btnGuard, selectedHere]() {
                if (!btnGuard) return;
                QWidget *btn = btnGuard.data();
                QVariant base = btn->property("baseY");
                int baseY = base.isValid() ? base.toInt() : btn->y();
                btn->setProperty("baseY", baseY);
                const int targetY = selectedHere ? baseY - dp(18) : baseY;
                if (btn->y() == targetY) return;
                auto *anim = new QPropertyAnimation(btn, "pos", btn);
                anim->setDuration(160);
                anim->setStartValue(btn->pos());
                anim->setEndValue(QPoint(btn->x(), targetY));
                anim->setEasingCurve(QEasingCurve::OutCubic);
                anim->start(QAbstractAnimation::DeleteWhenStopped);
            });
        }
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
                 mGS->shop().canBuyShop(i, mGS->gold()) && slotOk, false);
    }

    const auto &voucherOffers = mGS->shop().voucherOffers();
    for (int i = 0; i < mVoucherUi.size() && i < voucherOffers.size(); ++i) {
        fillSlot(mVoucherUi[i], voucherOffers[i],
                 mGS->shop().canBuyVoucher(i, mGS->gold()), false);
    }

    const auto &boosterOffers = mGS->shop().boosterOffers();
    for (int i = 0; i < mBoosterUi.size() && i < boosterOffers.size(); ++i) {
        fillSlot(mBoosterUi[i], boosterOffers[i],
                 mGS->shop().canBuyBooster(i, mGS->gold()), true);
    }

    int rcost = mGS->shop().rerollCost();
    mBtnReroll->setText(QString("重抽\n$%1").arg(rcost));
    mBtnReroll->setEnabled(mGS->gold() >= rcost);
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
        QPoint c = packSpritePos(o.pack, o.packSize);
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
            sealPix = BalatroShaders::renderVoucherPixmap(sealPix, 1.0);
        QPainter fp(&pix);
        fp.drawPixmap(QRect(0, 0, W, H), sealPix);
    }

    if (c.isDebuffed)
        pix = BalatroShaders::renderDebuffedPixmap(pix);
    return pix;
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
        // cardBtn 在选中后向上抬起 dp(18)；按钮要跟着抬起后的卡片走，否则会和卡片之间出现"空挡"。
        if (buyGuard && buyGuard->isVisible()) {
            // 紧贴卡片下边沿（卡片底下方 +dp(2) 处）。
            int x = p.x() + (cardGuard->width() - buyGuard->width()) / 2;
            int y = p.y() + cardGuard->height() + dp(2);
            buyGuard->move(x, y);
            buyGuard->raise();
        }
        if (useGuard && hasUseBtn) {
            // 左边沿贴卡片右侧（+dp(2) 让按钮"挂"在卡边上）。
            int x = p.x() + cardGuard->width() + dp(2);
            int y = p.y() + cardGuard->height() / 2 - useGuard->height() / 2;
            useGuard->move(x, y);
            useGuard->raise();
        }
    });
}

void ShopWidget::onBuyShop(int slot) {
    if (mGS->buyShopOffer(slot)) {
        mSelectedShopSlot = -1;
        refresh();
    }
}

void ShopWidget::onBuyAndUseShop(int slot) {
    if (mGS->buyAndUseShopConsumable(slot, {})) {
        mSelectedShopSlot = -1;
        refresh();
    }
}

// 兑换券 / 开包前先清选中再交给原逻辑。

void ShopWidget::onBuyVoucher(int slot) {
    if (mGS->buyVoucherOffer(slot)) {
        mSelectedVoucherSlot = -1;
        refresh();
    }
}

void ShopWidget::onBuyBooster(int slot) {
    const auto &offers = mGS->shop().boosterOffers();
    if (slot >= offers.size()) return;
    mSelectedBoosterSlot = -1;
    emit packBuyRequested(slot);   // 让 MainWindow 唤起开包界面
}

void ShopWidget::onReroll()        { mGS->rerollShop(); refresh(); }

void ShopWidget::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    layoutPanel();
}

void ShopWidget::layoutPanel()
{
    if (!mPanel) return;
    // 商店设计宽：160 (btn col) + 12 spacing + 4 slot * 178 + 3 * 6 spacing + shopBox padding 16 = 922 dp
    //                                    + root padding 36 ≈ 960 dp
    // 把 maxW 收到接近这个值，避免在大屏上把内容稀疏地撑开。
    const int minW = dp(820);
    const int minH = dp(620);
    const int maxW = dp(1040);
    const int maxH = dp(880);
    int panelW = qBound(minW, int(width()  * 0.74), maxW);
    int panelH = qBound(minH, int(height() * 0.82), maxH);
    // 宽高比 ≈ 1.18 (940 / 800)：上方 shop 行 + 下方 voucher/booster 行的合理纵向。
    const double targetAspect = 940.0 / 800.0;
    panelH = qMin(panelH, int(panelW / targetAspect) + dp(20));
    // 让面板底部贴到 widget 底沿——下方圆角已置 0，看上去就和屏幕底部融为一体。
    if (panelH > height()) panelH = height();
    mPanel->resize(panelW, panelH);
    int x = (width() - mPanel->width()) / 2;
    int y = qMax(0, height() - mPanel->height());
    mPanel->move(x, y);
}
