#include "shopwidget.h"
#include "../card/consumableitem.h"
#include "../card/jokeritem.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QResizeEvent>

ShopWidget::ShopWidget(GameState *gs,
                       const QFont &cnFont, const QFont &pixelFont,
                       QWidget *parent)
    : QWidget(parent), mGS(gs), mCNFont(cnFont), mPixelFont(pixelFont)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet("background: rgba(0, 0, 0, 180);");   // ← 半透明
    buildUi();
}

void ShopWidget::buildUi()
{
    mPanel = new QWidget(this);
    mPanel->setObjectName("shopPanel");
    mPanel->setFixedSize(900, 520);   // 800×420 → 900×520
    mPanel->setAttribute(Qt::WA_StyledBackground, true);
    mPanel->setStyleSheet(
        "QWidget#shopPanel {"
        "  background:#374244;"
        "  border: 3px solid #fda200;"
        "  border-radius: 14px;"
        "}"
        );

    auto *root = new QVBoxLayout(mPanel);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(14);

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
        "QWidget#shopBox { background:#4f6367; border-radius:10px; }"
        );
    auto *shbl = new QHBoxLayout(shopBox);
    shbl->setContentsMargins(12, 12, 12, 12);
    shbl->setSpacing(12);
    shbl->setAlignment(Qt::AlignCenter);

    for (int i = 0; i < 2; ++i) {
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

    // Voucher 单槽(暂时占位,不实现购买)
    auto *voucherBox = new QWidget(lowerRow);
    voucherBox->setObjectName("voucherBox");
    voucherBox->setFixedSize(160, 260);     // ← 加宽到 160 高度 260,跟 booster 槽对齐
    voucherBox->setAttribute(Qt::WA_StyledBackground, true);
    voucherBox->setStyleSheet(
        "QWidget#voucherBox { background:#4f6367; border-radius:10px; }"
        );
    auto *vbl = new QVBoxLayout(voucherBox);
    vbl->setContentsMargins(8, 8, 8, 8);
    vbl->setAlignment(Qt::AlignCenter);
    QLabel *voucherLbl = new QLabel("底注券", voucherBox);
    QFont vlf = mCNFont; vlf.setPixelSize(12);
    voucherLbl->setFont(vlf);
    voucherLbl->setStyleSheet("color:#888; background:transparent;");
    voucherLbl->setAlignment(Qt::AlignCenter);
    vbl->addWidget(voucherLbl);
    lhbl->addWidget(voucherBox);

    // Booster 区
    auto *boosterBox = new QWidget(lowerRow);
    boosterBox->setObjectName("boosterBox");
    boosterBox->setAttribute(Qt::WA_StyledBackground, true);
    boosterBox->setStyleSheet(
        "QWidget#boosterBox { background:#4f6367; border-radius:10px; }"
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
    ou.card->setFixedSize(160, 280);
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
    ou.cardBtn->setFixedSize(140, 190);
    ou.cardBtn->setCursor(Qt::PointingHandCursor);
    ou.cardBtn->setStyleSheet(
        "QPushButton { background:#374244; border-radius:6px; border:none; }"
        "QPushButton:hover { background:#4f6367; }"
        "QPushButton:disabled { background:#2a3035; }"
        );
    // 图片用 QIcon
    vbl->addWidget(ou.cardBtn, 0, Qt::AlignCenter);

    ou.imageLbl = nullptr;   // 没用,删掉

    ou.nameLbl = new QLabel("", ou.card);
    QFont nf = mCNFont; nf.setPixelSize(12);
    ou.nameLbl->setFont(nf);
    ou.nameLbl->setStyleSheet("color:white; background:transparent;");
    ou.nameLbl->setAlignment(Qt::AlignCenter);
    ou.nameLbl->setWordWrap(true);
    ou.nameLbl->setFixedHeight(36);
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
            name = tmp.name;
        } else if (o.kind == OfferKind::Pack) {
            name = packDisplayName(o.pack);
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
    for (int i = 0; i < mShopUi.size() && i < shopOffers.size(); ++i) {
        bool slotOk = true;
        const ShopOffer &o = shopOffers[i];
        if (o.kind == OfferKind::Joker)
            slotOk = mGS->canAddJoker();
        else if (o.kind == OfferKind::Tarot || o.kind == OfferKind::Planet)
            slotOk = mGS->canAddConsumable();
        fillSlot(mShopUi[i], o,
                 mGS->shop().canBuyShop(i, mGS->gold()) && slotOk, false);
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
        return sheet.copy(c.x() * JokerItem::WIDTH, c.y() * JokerItem::HEIGHT,
                          JokerItem::WIDTH, JokerItem::HEIGHT);
    }

    if (o.kind == OfferKind::Tarot || o.kind == OfferKind::Planet) {
        QPixmap sheet(":/textures/images/Tarots.png");
        if (sheet.isNull()) return QPixmap();
        QPoint c = ConsumableItem::spritePos(o.consumable);
        return sheet.copy(c.x() * ConsumableItem::WIDTH,
                          c.y() * ConsumableItem::HEIGHT,
                          ConsumableItem::WIDTH, ConsumableItem::HEIGHT);
    }

    if (o.kind == OfferKind::Pack) {
        QPixmap sheet(":/textures/images/boosters.png");
        if (sheet.isNull()) return QPixmap();
        // 我们目前只用 normal 版,固定 x=0
        // 行号: Standard=6, Arcana=0, Celestial=1, Buffoon=8
        int row = 0;
        switch (o.pack) {
        case PackKind::Standard:  row = 6; break;
        case PackKind::Arcana:    row = 0; break;
        case PackKind::Celestial: row = 1; break;
        case PackKind::Buffoon:   row = 8; break;
        }
        return sheet.copy(0, row * ConsumableItem::HEIGHT,
                          ConsumableItem::WIDTH, ConsumableItem::HEIGHT);
    }
    return QPixmap();
}

void ShopWidget::onBuyShop(int slot) {
    if (mGS->buyShopOffer(slot)) refresh();
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
    int x = (width()  - mPanel->width())  / 2;
    int y = (height() - mPanel->height()) / 2;
    mPanel->move(x, y);
}
