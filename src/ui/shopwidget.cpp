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
    setStyleSheet("background: rgba(0, 0, 0, 100);");   // ← 半透明
    buildUi();
}

void ShopWidget::buildUi()
{
    mPanel = new QWidget(this);
    mPanel->setFixedSize(720, 500);
    mPanel->setStyleSheet(
        "background: #1e2230;"
        "border: 3px solid #c07820;"
        "border-radius: 14px;"
        );

    auto *root = new QVBoxLayout(mPanel);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(12);

    // ── 顶部：标题 + 金币 ──
    auto *topRow = new QWidget(mPanel);
    auto *thbl = new QHBoxLayout(topRow);
    thbl->setContentsMargins(0, 0, 0, 0);

    auto *title = new QLabel("商店", topRow);
    QFont tf = mCNFont; tf.setPixelSize(32);
    title->setFont(tf);
    title->setStyleSheet("color:#f0c040;");

    mLblGold = new QLabel("$0", topRow);
    QFont gf = mPixelFont; gf.setPixelSize(28);
    mLblGold->setFont(gf);
    mLblGold->setStyleSheet("color:#f0c040;");
    mLblGold->setAlignment(Qt::AlignRight);

    thbl->addWidget(title);
    thbl->addStretch();
    thbl->addWidget(mLblGold);
    root->addWidget(topRow);

    // ── 商品行：2 个 offer + reroll ──
    auto *offerRow = new QWidget(mPanel);
    auto *ohbl = new QHBoxLayout(offerRow);
    ohbl->setContentsMargins(0, 0, 0, 0);
    ohbl->setSpacing(16);
    ohbl->setAlignment(Qt::AlignCenter);

    for (int i = 0; i < 2; ++i) {
        OfferUi ou;
        ou.card = new QWidget(offerRow);
        ou.card->setFixedSize(180, 340);
        ou.card->setStyleSheet("background:#161b28; border-radius:10px;");

        auto *vbl = new QVBoxLayout(ou.card);
        vbl->setContentsMargins(10, 10, 10, 10);
        vbl->setSpacing(6);

        ou.imageLbl = new QLabel(ou.card);
        ou.imageLbl->setFixedSize(142, 190);
        ou.imageLbl->setAlignment(Qt::AlignCenter);
        vbl->addWidget(ou.imageLbl, 0, Qt::AlignCenter);

        ou.nameLbl = new QLabel("", ou.card);
        QFont nf = mCNFont; nf.setPixelSize(15);
        ou.nameLbl->setFont(nf);
        ou.nameLbl->setStyleSheet("color:white;");
        ou.nameLbl->setAlignment(Qt::AlignCenter);
        ou.nameLbl->setWordWrap(true);
        vbl->addWidget(ou.nameLbl);

        ou.descLbl = new QLabel("", ou.card);
        QFont df = mCNFont; df.setPixelSize(11);
        ou.descLbl->setFont(df);
        ou.descLbl->setStyleSheet("color:#aaa;");
        ou.descLbl->setAlignment(Qt::AlignCenter);
        ou.descLbl->setWordWrap(true);
        vbl->addWidget(ou.descLbl);

        ou.buyBtn = new QPushButton("$0 购买", ou.card);
        ou.buyBtn->setFixedHeight(34);
        QFont bf = mCNFont; bf.setPixelSize(14);
        ou.buyBtn->setFont(bf);
        ou.buyBtn->setCursor(Qt::PointingHandCursor);
        ou.buyBtn->setStyleSheet(
            "QPushButton { background:#3060c0; color:white; border:none; border-radius:6px; }"
            "QPushButton:hover { background:#4070d0; }"
            "QPushButton:disabled { background:#333; color:#666; }"
            );
        connect(ou.buyBtn, &QPushButton::clicked, this, [this, i]() { onBuy(i); });

        vbl->addWidget(ou.buyBtn);
        ohbl->addWidget(ou.card);
        mOfferUi.append(ou);
    }

    // 重抽按钮
    mBtnReroll = new QPushButton("重抽\n$5", offerRow);
    mBtnReroll->setFixedSize(110, 340);
    QFont rf = mCNFont; rf.setPixelSize(18);
    mBtnReroll->setFont(rf);
    mBtnReroll->setCursor(Qt::PointingHandCursor);
    mBtnReroll->setStyleSheet(
        "QPushButton { background:#c03030; color:white; border:none; border-radius:8px; }"
        "QPushButton:hover { background:#d04040; }"
        "QPushButton:disabled { background:#333; color:#666; }"
        );
    connect(mBtnReroll, &QPushButton::clicked, this, &ShopWidget::onReroll);
    ohbl->addWidget(mBtnReroll);

    root->addWidget(offerRow);
    root->addStretch();

    // ── "前往下一盲注" ──
    mBtnLeave = new QPushButton("前往下一盲注", mPanel);
    mBtnLeave->setFixedHeight(56);
    QFont lf = mCNFont; lf.setPixelSize(20);
    mBtnLeave->setFont(lf);
    mBtnLeave->setCursor(Qt::PointingHandCursor);
    mBtnLeave->setStyleSheet(
        "QPushButton { background:#c07820; color:white; border:none; border-radius:8px; }"
        "QPushButton:hover  { background:#d08830; }"
        "QPushButton:pressed{ background:#a06010; }"
        );
    connect(mBtnLeave, &QPushButton::clicked, this, &ShopWidget::leaveClicked);
    root->addWidget(mBtnLeave);
}

void ShopWidget::refresh()
{
    mLblGold->setText(QString("$%1").arg(mGS->gold()));

    const auto &offers = mGS->shop().offers();
    for (int i = 0; i < mOfferUi.size(); ++i) {
        OfferUi &ou = mOfferUi[i];
        if (i >= offers.size()) { ou.card->hide(); continue; }
        const ShopOffer &o = offers[i];
        if (o.sold) { ou.card->setVisible(false); continue; }
        ou.card->setVisible(true);

        QString name, desc;
        bool slotOk = true;
        if (o.kind == OfferKind::Joker) {
            Joker tmp = createJoker(o.joker);
            name = tmp.name; desc = tmp.description;
            slotOk = mGS->canAddJoker();
        } else if (o.kind == OfferKind::Pack) {
            name = packDisplayName(o.pack);
            desc = "打开后从中选 1 个";
            slotOk = true;
        } else {
            Consumable tmp = createConsumable(o.consumable);
            name = tmp.name; desc = tmp.description;
            slotOk = mGS->canAddConsumable();
        }

        ou.nameLbl->setText(name);
        ou.descLbl->setText(desc);
        ou.imageLbl->setPixmap(offerPixmap(o));
        ou.buyBtn->setText(QString("$%1 购买").arg(o.cost));
        ou.buyBtn->setEnabled(mGS->shop().canBuy(i, mGS->gold()) && slotOk);
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

void ShopWidget::onBuy(int slot) {
    const auto &offers = mGS->shop().offers();
    if (slot >= offers.size()) return;
    if (offers[slot].kind == OfferKind::Pack) {
        emit packBuyRequested(slot);    // 交给 MainWindow 唤起开包界面
    } else {
        if (mGS->buyOffer(slot)) refresh();
    }
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
