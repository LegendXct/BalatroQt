#include "packopenwidget.h"
#include "../card/jokeritem.h"
#include "../card/consumable.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QLabel>
#include <QPushButton>
#include <QPainter>
#include "../card/consumableitem.h"

PackOpenWidget::PackOpenWidget(const QFont &cnFont, const QFont &pixelFont,
                               QWidget *parent)
    : QWidget(parent), mCNFont(cnFont), mPixelFont(pixelFont)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet("background: rgba(0, 0, 0, 180);");
    hide();
    buildUi();
}

void PackOpenWidget::buildUi()
{
    mPanel = new QWidget(this);
    mPanel->setFixedSize(820, 540);
    mPanel->setStyleSheet(
        "background: #1e2230;"
        "border: 3px solid #f0c040;"
        "border-radius: 14px;"
        );
    auto *root = new QVBoxLayout(mPanel);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(12);

    mLblTitle = new QLabel("打开包", mPanel);
    QFont tf = mCNFont; tf.setPixelSize(32);
    mLblTitle->setFont(tf);
    mLblTitle->setStyleSheet("color:#f0c040;");
    mLblTitle->setAlignment(Qt::AlignCenter);
    root->addWidget(mLblTitle);

    auto *optionsRow = new QWidget(mPanel);
    auto *ohbl = new QHBoxLayout(optionsRow);
    ohbl->setContentsMargins(0, 0, 0, 0);
    ohbl->setSpacing(16);
    ohbl->setAlignment(Qt::AlignCenter);

    for (int i = 0; i < 3; ++i) {        // 最多 3 个选项位
        OptUi ou;
        ou.card = new QWidget(optionsRow);
        ou.card->setFixedSize(190, 380);
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

        ou.takeBtn = new QPushButton("选择", ou.card);
        ou.takeBtn->setFixedHeight(34);
        QFont bf = mCNFont; bf.setPixelSize(14);
        ou.takeBtn->setFont(bf);
        ou.takeBtn->setCursor(Qt::PointingHandCursor);
        ou.takeBtn->setStyleSheet(
            "QPushButton { background:#3060c0; color:white; border:none; border-radius:6px; }"
            "QPushButton:hover { background:#4070d0; }"
            "QPushButton:disabled { background:#333; color:#666; }"
            );
        connect(ou.takeBtn, &QPushButton::clicked, this, [this, i]() { onChoose(i); });
        vbl->addWidget(ou.takeBtn);

        ohbl->addWidget(ou.card);
        mOptUi.append(ou);
    }
    root->addWidget(optionsRow);
    root->addStretch();

    mBtnSkip = new QPushButton("跳过", mPanel);
    mBtnSkip->setFixedHeight(46);
    QFont sf = mCNFont; sf.setPixelSize(18);
    mBtnSkip->setFont(sf);
    mBtnSkip->setCursor(Qt::PointingHandCursor);
    mBtnSkip->setStyleSheet(
        "QPushButton { background:#555; color:white; border:none; border-radius:8px; }"
        "QPushButton:hover { background:#666; }"
        );
    connect(mBtnSkip, &QPushButton::clicked, this, &PackOpenWidget::onSkip);
    root->addWidget(mBtnSkip);
}

int PackOpenWidget::optionCount() const {
    switch (mContent.kind) {
    case PackKind::Standard:  return mContent.standardCards.size();
    case PackKind::Buffoon:   return mContent.jokers.size();
    case PackKind::Arcana:
    case PackKind::Celestial: return mContent.consumables.size();
    }
    return 0;
}

void PackOpenWidget::open(const PackContent &content,
                          int freeConsumableSlots, int freeJokerSlots)
{
    mContent = content;
    mFreeConsumableSlots = freeConsumableSlots;
    mFreeJokerSlots = freeJokerSlots;

    mLblTitle->setText(packDisplayName(content.kind));

    int total = optionCount();
    for (int i = 0; i < mOptUi.size(); ++i) {
        OptUi &ou = mOptUi[i];
        if (i >= total) { ou.card->setVisible(false); continue; }
        ou.card->setVisible(true);
        ou.imageLbl->setPixmap(renderOption(i));
        ou.nameLbl->setText(optionName(i));
        ou.descLbl->setText(optionDesc(i));
        ou.takeBtn->setEnabled(slotAvailableFor(i));
    }
    show();
    raise();
}

QPixmap PackOpenWidget::renderOption(int i) const
{
    constexpr int W = 142, H = 190;

    if (mContent.kind == PackKind::Buffoon) {
        QPixmap sheet(":/textures/images/Jokers.png");
        if (sheet.isNull()) return QPixmap();
        QPoint xy = JokerItem::spritePos(mContent.jokers[i]);
        return sheet.copy(xy.x() * W, xy.y() * H, W, H);
    }

    if (mContent.kind == PackKind::Arcana || mContent.kind == PackKind::Celestial) {
        QPixmap sheet(":/textures/images/Tarots.png");
        if (sheet.isNull()) return QPixmap();
        QPoint xy = ConsumableItem::spritePos(mContent.consumables[i]);
        return sheet.copy(xy.x() * W, xy.y() * H, W, H);
    }

    // Standard：用资源图叠出扑克牌
    const CardData &c = mContent.standardCards[i];
    QPixmap deckSheet(":/textures/images/8BitDeck.png");
    QPixmap enhSheet (":/textures/images/Enhancers.png");
    QPixmap pix(W, H); pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    // 底色
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

    // 牌面
    if (c.enhancement != Enhancement::Stone && !deckSheet.isNull()) {
        int col = static_cast<int>(c.rank) - 2;
        int row = 0;
        switch (c.suit) {
        case Suit::Hearts: row = 0; break;
        case Suit::Clubs:  row = 1; break;
        case Suit::Diamonds:row = 2;break;
        case Suit::Spades: row = 3; break;
        }
        p.drawPixmap(QRect(0, 0, W, H), deckSheet, QRect(col*W, row*H, W, H));
    }

    // seal
    int sCol = -1, sRow = 0;
    switch (c.seal) {
    case Seal::Gold:   sCol = 2; sRow = 0; break;
    case Seal::Purple: sCol = 4; sRow = 4; break;
    case Seal::Red:    sCol = 5; sRow = 4; break;
    case Seal::Blue:   sCol = 6; sRow = 4; break;
    default: break;
    }
    if (sCol >= 0 && !enhSheet.isNull())
        p.drawPixmap(QRect(0, 0, W, H), enhSheet, QRect(sCol*W, sRow*H, W, H));

    // edition 边框
    p.setBrush(Qt::NoBrush);
    switch (c.edition) {
    case Edition::Foil:
        p.setPen(QPen(QColor(120, 200, 255, 200), 4));
        p.drawRoundedRect(2, 2, W-4, H-4, 8, 8); break;
    case Edition::Holographic:
        p.setPen(QPen(QColor(255, 100, 200, 200), 4));
        p.drawRoundedRect(2, 2, W-4, H-4, 8, 8); break;
    case Edition::Polychrome: {
        QLinearGradient g(0, 0, W, H);
        g.setColorAt(0,   QColor(255, 100, 100, 220));
        g.setColorAt(0.5, QColor(100, 255, 100, 220));
        g.setColorAt(1,   QColor(100, 100, 255, 220));
        p.setPen(QPen(QBrush(g), 4));
        p.drawRoundedRect(2, 2, W-4, H-4, 8, 8); break;
    }
    default: break;
    }
    return pix;
}

QString PackOpenWidget::optionName(int i) const {
    switch (mContent.kind) {
    case PackKind::Standard:  return mContent.standardCards[i].toString();
    case PackKind::Buffoon:   return createJoker(mContent.jokers[i]).name;
    case PackKind::Arcana:
    case PackKind::Celestial: return createConsumable(mContent.consumables[i]).name;
    }
    return "";
}

QString PackOpenWidget::optionDesc(int i) const {
    if (mContent.kind == PackKind::Standard) {
        const CardData &c = mContent.standardCards[i];
        QStringList parts;
        switch (c.enhancement) {
        case Enhancement::Bonus: parts << "Bonus +30 筹码"; break;
        case Enhancement::Mult:  parts << "Mult +4 倍率"; break;
        case Enhancement::Wild:  parts << "Wild 任意花色"; break;
        case Enhancement::Glass: parts << "Glass ×2 (1/4 破)"; break;
        case Enhancement::Steel: parts << "Steel 持有 ×1.5"; break;
        case Enhancement::Stone: parts << "Stone +50 无花色"; break;
        case Enhancement::Gold:  parts << "Gold 通关 +$3"; break;
        case Enhancement::Lucky: parts << "Lucky 概率"; break;
        default: break;
        }
        switch (c.edition) {
        case Edition::Foil:        parts << "Foil"; break;
        case Edition::Holographic: parts << "Holo"; break;
        case Edition::Polychrome:  parts << "Polychrome"; break;
        default: break;
        }
        switch (c.seal) {
        case Seal::Gold:   parts << "金印章"; break;
        case Seal::Red:    parts << "红印章"; break;
        case Seal::Blue:   parts << "蓝印章"; break;
        case Seal::Purple: parts << "紫印章"; break;
        default: break;
        }
        return parts.isEmpty() ? "普通牌" : parts.join("\n");
    }
    if (mContent.kind == PackKind::Buffoon)
        return createJoker(mContent.jokers[i]).description;
    return createConsumable(mContent.consumables[i]).description;
}

bool PackOpenWidget::slotAvailableFor(int i) const {
    Q_UNUSED(i);
    switch (mContent.kind) {
    case PackKind::Standard:  return true;
    case PackKind::Buffoon:   return mFreeJokerSlots > 0;
    case PackKind::Arcana:
    case PackKind::Celestial: return mFreeConsumableSlots > 0;
    }
    return false;
}

void PackOpenWidget::onChoose(int idx) { hide(); emit choiceMade(idx); }
void PackOpenWidget::onSkip()          { hide(); emit choiceMade(-1); }

void PackOpenWidget::resizeEvent(QResizeEvent *e) {
    QWidget::resizeEvent(e);
    layoutPanel();
}

void PackOpenWidget::layoutPanel() {
    if (!mPanel) return;
    int x = (width()  - mPanel->width())  / 2;
    int y = (height() - mPanel->height()) / 2;
    mPanel->move(x, y);
}
