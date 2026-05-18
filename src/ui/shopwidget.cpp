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
        // 留出 hover 透视和放大余量；否则鼠标靠角落时 QPainter 会被按钮矩形裁掉。
        // 卡包、塔罗、优惠券都按原版同一卡牌宽高节奏展示，不再把卡包撑大。
        QSize target(int(width() * 0.80), int(height() * 0.80));
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
    // 原版商店比例：上方商品区横向更宽，下方左 Voucher、右 Booster。
    // 最小宽度保持紧凑，layoutPanel 会按容器宽度等比放大，避免商品边缘被裁。
    mPanel->setMinimumSize(dp(720), dp(560));
    mPanel->setAttribute(Qt::WA_StyledBackground, true);
    mPanel->setStyleSheet(
        "QWidget#shopPanel {"
        "  background:rgba(35,48,51,235);"
        "  border: 3px solid #fe5f55;"
        "  border-radius: 18px;"
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
    QFont nrf = mCNFont; nrf.setPixelSize(fontPx(26)); nrf.setBold(true);
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
    shbl->setContentsMargins(dp(12), dp(12), dp(12), dp(12));
    shbl->setSpacing(dp(12));
    shbl->setAlignment(Qt::AlignCenter);

    for (int i = 0; i < 4; ++i) {
        OfferUi ou = createOfferSlot(shopBox, false);
        connect(ou.cardBtn, &QPushButton::clicked, this, [this, i]() { onBuyShop(i); });
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

    // Voucher 单槽：原版下半区左侧优惠券，固定售价 $10
    auto *voucherBox = new QWidget(lowerRow);
    voucherBox->setObjectName("voucherBox");
    voucherBox->setMinimumSize(dp(172), dp(258));
    voucherBox->setAttribute(Qt::WA_StyledBackground, true);
    voucherBox->setStyleSheet(
        "QWidget#voucherBox { background:rgba(57,72,76,230); border:none; border-radius:14px; }"
        );
    auto *vbl = new QVBoxLayout(voucherBox);
    vbl->setContentsMargins(dp(10), dp(8), dp(10), dp(8));
    vbl->setAlignment(Qt::AlignCenter);

    OfferUi vu = createOfferSlot(voucherBox, false);
    connect(vu.cardBtn, &QPushButton::clicked, this, [this]() { onBuyVoucher(0); });
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
    bhbl->setContentsMargins(dp(12), dp(8), dp(12), dp(8));
    bhbl->setSpacing(dp(12));
    bhbl->setAlignment(Qt::AlignCenter);

    for (int i = 0; i < 2; ++i) {
        OfferUi ou = createOfferSlot(boosterBox, true);
        connect(ou.cardBtn, &QPushButton::clicked, this, [this, i]() { onBuyBooster(i); });
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
    ou.card = new QWidget(parent);
    if (isBooster) ou.card->setFixedSize(dp(166), dp(262));
    else           ou.card->setFixedSize(dp(166), dp(262));
    ou.card->setStyleSheet("background:transparent;");

    auto *vbl = new QVBoxLayout(ou.card);
    vbl->setContentsMargins(0, 0, 0, 0);
    vbl->setSpacing(dp(4));
    vbl->setAlignment(Qt::AlignCenter);

    // 顶部 $X 价格标签(深底圆角)
    ou.priceLbl = new QLabel("$0", ou.card);
    ou.priceLbl->setFixedSize(dp(82), dp(36));
    QFont pf = mCNFont; pf.setPixelSize(fontPx(21)); pf.setBold(true);
    ou.priceLbl->setFont(pf);
    ou.priceLbl->setAlignment(Qt::AlignCenter);
    ou.priceLbl->setStyleSheet(
        "color:#f3b958; background:#374244; border-radius:6px;"
        );
    vbl->addWidget(ou.priceLbl, 0, Qt::AlignCenter);

    // 卡图(整张点击)
    ou.cardBtn = new ShopCardButton(ou.card);
    // 原版卡包和普通卡牌使用同一张卡牌宽高比例，只是贴图本身是卡包。
    // 之前这里给卡包单独放大，导致商店里卡包明显大一圈并且向下错位。
    ou.cardBtn->setFixedSize(dp(148), dp(186));
    ou.cardBtn->setCursor(Qt::PointingHandCursor);
    ou.cardBtn->installEventFilter(this);
    ou.cardBtn->setStyleSheet(
        "QPushButton { background:transparent; border-radius:0px; border:none; }"
        "QPushButton:hover { background:transparent; }"
        "QPushButton:disabled { background:transparent; }"
        );

    // 图片用 QIcon，booster 贴图本身已经在 offerPixmap 里额外绘制厚度/高光。
    vbl->addWidget(ou.cardBtn, 0, Qt::AlignCenter);

    ou.imageLbl = nullptr;   // 没用,删掉

    ou.nameLbl = new QLabel("", ou.card);
    QFont nf = mCNFont; nf.setPixelSize(fontPx(19));
    ou.nameLbl->setFont(nf);
    ou.nameLbl->setStyleSheet("color:white; background:transparent;");
    ou.nameLbl->setAlignment(Qt::AlignCenter);
    ou.nameLbl->setWordWrap(true);
    ou.nameLbl->setFixedHeight(dp(58));
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
        if (o.sold) {
            ou.card->setVisible(false);
            ou.cardBtn->setToolTip(QString());
            ou.cardBtn->setProperty("infoTitle", QString());
            ou.cardBtn->setProperty("infoBody", QString());
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
        QPixmap pix = offerPixmap(o);
        if (auto *tiltBtn = dynamic_cast<ShopCardButton *>(ou.cardBtn))
            tiltBtn->setDisplayPixmap(pix);
        else if (!pix.isNull()) {
            ou.cardBtn->setIcon(QIcon(pix));
            ou.cardBtn->setIconSize(pix.size());
        }
        ou.cardBtn->setEnabled(canBuy);
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

void ShopWidget::onBuyShop(int slot) {
    if (mGS->buyShopOffer(slot)) refresh();
}

void ShopWidget::onBuyVoucher(int slot) {
    if (mGS->buyVoucherOffer(slot)) refresh();
}

void ShopWidget::onBuyBooster(int slot) {
    const auto &offers = mGS->shop().boosterOffers();
    if (slot >= offers.size()) return;
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
    // 优先适应容器宽度：宽度不够时让面板缩小，避免按钮/价格/booster 贴图被裁出屏外。
    const int minW = dp(720);
    const int minH = dp(560);
    const int maxW = dp(1280);
    const int maxH = dp(880);
    int panelW = qBound(minW, int(width()  - dp(20)), maxW);
    int panelH = qBound(minH, int(height() - dp(20)), maxH);
    // 高度跟着宽度的纵横比同步缩放，保持上下两栏比例自然。
    const double targetAspect = 980.0 / 720.0; // 设计稿宽高比
    panelH = qMin(panelH, int(panelW / targetAspect) + dp(16));
    mPanel->resize(panelW, panelH);
    int x = (width()  - mPanel->width())  / 2;
    int y = (height() - mPanel->height()) / 2;
    mPanel->move(x, y);
}
