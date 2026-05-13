#include "shopwidget.h"
#include "../card/consumableitem.h"
#include "../card/jokeritem.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QPainter>
#include <QGraphicsDropShadowEffect>
#include "../utils/shadereffects.h"


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

ShopWidget::ShopWidget(GameState *gs,
                       const QFont &cnFont, const QFont &pixelFont,
                       QWidget *parent)
    : QWidget(parent), mGS(gs), mCNFont(cnFont), mPixelFont(pixelFont)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet("background: rgba(0, 0, 0, 60);");   // 商店露出动态背景
    buildUi();
}

void ShopWidget::buildUi()
{
    mPanel = new QWidget(this);
    mPanel->setObjectName("shopPanel");
    // 原版商店比例：上方商品区横向更宽，下方左 Voucher、右 Booster。
    // 不再固定 900×520，避免超级/巨型包贴图被面板裁掉。
    mPanel->setMinimumSize(900, 650);
    mPanel->setAttribute(Qt::WA_StyledBackground, true);
    mPanel->setStyleSheet(
        "QWidget#shopPanel {"
        "  background:rgba(35,48,51,235);"
        "  border: 3px solid #fe5f55;"
        "  border-radius: 18px;"
        "}"
        );

    auto *root = new QVBoxLayout(mPanel);
    root->setContentsMargins(18, 16, 18, 16);
    root->setSpacing(10);

    // ── 顶部:右上角金币 ──
    auto *topRow = new QWidget(mPanel);
    auto *thbl = new QHBoxLayout(topRow);
    thbl->setContentsMargins(0, 0, 0, 0);
    thbl->addStretch();

    auto *goldBox = new QWidget(topRow);
    goldBox->setAttribute(Qt::WA_StyledBackground, true);
    goldBox->setStyleSheet("background:#4f6367; border-radius:8px;");
    auto *gbl = new QHBoxLayout(goldBox);
    gbl->setContentsMargins(12, 4, 12, 4);
    mLblGold = new QLabel("$0", goldBox);
    QFont gf = mCNFont; gf.setPixelSize(22); gf.setBold(true);
    mLblGold->setFont(gf);
    mLblGold->setStyleSheet("color:#f3b958; background:transparent;");
    gbl->addWidget(mLblGold);
    thbl->addWidget(goldBox);

    root->addWidget(topRow);

    // ── 上栏:[Next 红 + Reroll 绿] | 商品区(2 槽) ──
    auto *upperRow = new QWidget(mPanel);
    auto *uhbl = new QHBoxLayout(upperRow);
    uhbl->setContentsMargins(0, 0, 0, 0);
    uhbl->setSpacing(12);

    // 左:两按钮竖排
    auto *btnCol = new QWidget(upperRow);
    btnCol       ->setFixedWidth(140);
    auto *bvbl = new QVBoxLayout(btnCol);
    bvbl->setContentsMargins(0, 0, 0, 0);
    bvbl->setSpacing(8);

    mBtnNextRound = new QPushButton("下一个\n回合", btnCol);
    mBtnNextRound->setFixedSize(140, 110);
    QFont nrf = mCNFont; nrf.setPixelSize(20); nrf.setBold(true);
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
    mBtnReroll   ->setFixedSize(140, 110);
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
        "QWidget#shopBox { background:rgba(57,72,76,230); border:3px solid #202b2e; border-radius:14px; }"
        );
    auto *shbl = new QHBoxLayout(shopBox);
    shbl->setContentsMargins(12, 12, 12, 12);
    shbl->setSpacing(12);
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
    lhbl->setSpacing(12);

    // Voucher 单槽：原版下半区左侧优惠券，固定售价 $10
    auto *voucherBox = new QWidget(lowerRow);
    voucherBox->setObjectName("voucherBox");
    voucherBox->setMinimumSize(185, 275);
    voucherBox->setAttribute(Qt::WA_StyledBackground, true);
    voucherBox->setStyleSheet(
        "QWidget#voucherBox { background:rgba(57,72,76,230); border:3px solid #202b2e; border-radius:14px; }"
        );
    auto *vbl = new QVBoxLayout(voucherBox);
    vbl->setContentsMargins(10, 8, 10, 8);
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
        "QWidget#boosterBox { background:rgba(57,72,76,230); border:3px solid #202b2e; border-radius:14px; }"
        );
    auto *bhbl = new QHBoxLayout(boosterBox);
    bhbl->setContentsMargins(12, 8, 12, 8);
    bhbl->setSpacing(12);
    bhbl->setAlignment(Qt::AlignCenter);

    for (int i = 0; i < 2; ++i) {
        OfferUi ou = createOfferSlot(boosterBox, true);
        connect(ou.cardBtn, &QPushButton::clicked, this, [this, i]() { onBuyBooster(i); });
        bhbl->addWidget(ou.card);
        mBoosterUi.append(ou);
    }
    lhbl->addWidget(boosterBox, 1);

    root->addWidget(lowerRow);
}

ShopWidget::OfferUi ShopWidget::createOfferSlot(QWidget *parent, bool isBooster)
{
    OfferUi ou;
    ou.card = new QWidget(parent);
    if (isBooster) ou.card->setFixedSize(225, 318);
    else           ou.card->setFixedSize(150, 260);
    ou.card->setStyleSheet("background:transparent;");

    auto *vbl = new QVBoxLayout(ou.card);
    vbl->setContentsMargins(0, 0, 0, 0);
    vbl->setSpacing(4);
    vbl->setAlignment(Qt::AlignCenter);

    // 顶部 $X 价格标签(深底圆角)
    ou.priceLbl = new QLabel("$0", ou.card);
    ou.priceLbl->setFixedSize(64, 28);
    QFont pf = mCNFont; pf.setPixelSize(16); pf.setBold(true);
    ou.priceLbl->setFont(pf);
    ou.priceLbl->setAlignment(Qt::AlignCenter);
    ou.priceLbl->setStyleSheet(
        "color:#f3b958; background:#374244; border-radius:6px;"
        );
    vbl->addWidget(ou.priceLbl, 0, Qt::AlignCenter);

    // 卡图(整张点击)
    ou.cardBtn = new QPushButton(ou.card);
    // Booster 在原版商店区使用约 1.27×牌宽的显示区域，给它单独更宽的按钮。
    if (isBooster) ou.cardBtn->setFixedSize(190, 252);
    else           ou.cardBtn->setFixedSize(126, 174);
    ou.cardBtn->setCursor(Qt::PointingHandCursor);
    ou.cardBtn->setStyleSheet(
        "QPushButton { background:#374244; border-radius:6px; border:none; }"
        "QPushButton:hover { background:#4f6367; }"
        "QPushButton:disabled { background:#2a3035; }"
        );
    if (isBooster) {
        auto *shadow = new QGraphicsDropShadowEffect(ou.cardBtn);
        shadow->setBlurRadius(22);
        shadow->setOffset(10, 14);
        shadow->setColor(QColor(0, 0, 0, 150));
        ou.cardBtn->setGraphicsEffect(shadow);
    }

    // 图片用 QIcon，booster 贴图本身已经在 offerPixmap 里额外绘制厚度/高光。
    vbl->addWidget(ou.cardBtn, 0, Qt::AlignCenter);

    ou.imageLbl = nullptr;   // 没用,删掉

    ou.nameLbl = new QLabel("", ou.card);
    QFont nf = mCNFont; nf.setPixelSize(12);
    ou.nameLbl->setFont(nf);
    ou.nameLbl->setStyleSheet("color:white; background:transparent;");
    ou.nameLbl->setAlignment(Qt::AlignCenter);
    ou.nameLbl->setWordWrap(true);
    ou.nameLbl->setFixedHeight(38);
    vbl->addWidget(ou.nameLbl);

    return ou;
}

void ShopWidget::refresh()
{
    mLblGold->setText(QString("$%1").arg(mGS->gold()));

    auto fillSlot = [this](OfferUi &ou, const ShopOffer &o, bool canBuy, bool isBooster) {
        if (o.sold) { ou.card->setVisible(false); return; }
        ou.card->setVisible(true);

        QString name;
        if (o.kind == OfferKind::Joker) {
            Joker tmp = createJoker(o.joker);
            QString ed = editionDisplayName(o.jokerEdition);
            name = ed.isEmpty() ? tmp.name : (ed + " " + tmp.name);
            QString tip = tmp.name + "\n" + tmp.description;
            if (!ed.isEmpty()) tip += "\n" + editionDescription(o.jokerEdition);
            ou.cardBtn->setToolTip(tip);
        } else if (o.kind == OfferKind::Pack) {
            name = packDisplayName(o.pack, o.packSize);
        } else if (o.kind == OfferKind::Voucher) {
            name = voucherData(o.voucher).name;
            ou.cardBtn->setToolTip(voucherData(o.voucher).description);
        } else if (o.kind == OfferKind::PlayingCard) {
            name = o.playingCard.toString();
        } else {
            Consumable tmp = createConsumable(o.consumable);
            name = tmp.name;
        }
        ou.nameLbl->setText(name);
        ou.priceLbl->setText(QString("$%1").arg(o.cost));

        // 设按钮图片(QIcon)
        QPixmap pix = offerPixmap(o);
        if (!pix.isNull()) {
            QPixmap scaled = pix.scaled(ou.cardBtn->size() - QSize(10, 10),
                                        Qt::KeepAspectRatio, Qt::SmoothTransformation);
            ou.cardBtn->setIcon(QIcon(scaled));
            ou.cardBtn->setIconSize(scaled.size());
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
        QPixmap pix = sheet.copy(c.x() * JokerItem::WIDTH, c.y() * JokerItem::HEIGHT,
                                 JokerItem::WIDTH, JokerItem::HEIGHT);
        if (o.jokerEdition != Edition::None) {
            QPainter p(&pix);
            p.setRenderHint(QPainter::Antialiasing, true);
            BalatroShaders::paintEdition(&p, QRectF(0, 0, pix.width(), pix.height()), o.jokerEdition);
        }
        return pix;
    }

    if (o.kind == OfferKind::Tarot || o.kind == OfferKind::Planet || o.kind == OfferKind::Spectral) {
        return ConsumableItem::renderPixmap(o.consumable);
    }

    if (o.kind == OfferKind::Pack) {
        QPixmap sheet(":/textures/images/boosters.png");
        if (sheet.isNull()) return QPixmap();
        QPoint c = packSpritePos(o.pack, o.packSize);
        QPixmap base = sheet.copy(c.x() * ConsumableItem::WIDTH,
                                  c.y() * ConsumableItem::HEIGHT,
                                  ConsumableItem::WIDTH, ConsumableItem::HEIGHT);

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
            QPixmap pix = voucherSheet.copy(c.x() * ConsumableItem::WIDTH,
                                           c.y() * ConsumableItem::HEIGHT,
                                           ConsumableItem::WIDTH,
                                           ConsumableItem::HEIGHT);
            QPainter p(&pix);
            p.setRenderHint(QPainter::Antialiasing, true);
            BalatroShaders::paintVoucherShader(&p, QRectF(0, 0, pix.width(), pix.height()), 0.9);
            return pix;
        }

        QPixmap pix(ConsumableItem::WIDTH, ConsumableItem::HEIGHT);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(QColor("#2f9e44"));
        p.setPen(QPen(QColor("#e9ffd9"), 3));
        p.drawRoundedRect(4, 4, pix.width() - 8, pix.height() - 8, 10, 10);
        p.setPen(Qt::white);
        QFont f = mCNFont;
        f.setPixelSize(18);
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
    constexpr int W = ConsumableItem::WIDTH, H = ConsumableItem::HEIGHT;
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
    if (c.edition != Edition::None)
        BalatroShaders::paintEdition(&p, QRectF(0, 0, W, H), c.edition);
    if (c.seal == Seal::Gold)
        BalatroShaders::paintGoldSealGlow(&p, QRectF(0, 0, W, H));
    if (c.isDebuffed)
        BalatroShaders::paintDebuff(&p, QRectF(0, 0, W, H));
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
    // 右下角牌组要一直露出来，因此商店面板不能无限向右撑。
    int panelW = qBound(900, int(width() * 0.84), qMax(900, width() - 28));
    int panelH = qBound(650, int(height() * 0.84), 760);
    mPanel->resize(panelW, panelH);
    int x = (width()  - mPanel->width())  / 2;
    int y = (height() - mPanel->height()) / 2;
    mPanel->move(x, y);
}
