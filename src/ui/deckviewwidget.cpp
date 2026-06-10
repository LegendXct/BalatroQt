#include "deckviewwidget.h"
#include "balatroinfopanel.h"
#include "cardtooltipformat.h"
#include "../card/consumableitem.h"
#include "../card/deckskin.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QPainter>
#include <QResizeEvent>
#include <QMap>
#include <QStringList>
#include <QFrame>
#include <QEvent>
#include <QPainterPath>
#include <QFontMetrics>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include "../utils/shadereffects.h"


namespace {
static int deckUiPx(int px) { return qMax(px + 2, int(std::round(px * 1.35))); }

QString deckRankLabel(Rank r)
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

QString deckSuitLabel(Suit s)
{
    switch (s) {
    case Suit::Spades: return QStringLiteral("黑桃");
    case Suit::Hearts: return QStringLiteral("红桃");
    case Suit::Diamonds: return QStringLiteral("方块");
    case Suit::Clubs: return QStringLiteral("梅花");
    }
    return QString();
}

QString deckHoverTitle(const CardData &d)
{
    if (d.enhancement == Enhancement::Stone)
        return QStringLiteral("石头牌");
    return deckSuitLabel(d.suit) + deckRankLabel(d.rank);
}

QString deckHoverDesc(const CardData &d)
{
    int chips = d.chipValue() + d.permanentBonusChips;
    if (d.enhancement == Enhancement::Bonus) chips += 30;
    if (d.enhancement == Enhancement::Stone) chips = 50 + d.permanentBonusChips;
    return QStringLiteral("+%1筹码").arg(chips);
}

// 注意：不要在 anonymous namespace 内 forward-declare DeckViewWidget——会遮蔽 deckviewwidget.h
// 里的真正类型，造成 mDeckView->showHoverInfo() 找不到方法。直接用 ::DeckViewWidget 即可。

class DeckCardPreviewLabel : public QLabel
{
public:
    DeckCardPreviewLabel(const CardData &card, const QPixmap &pixmap, const QFont &font,
                         qreal angleDeg, ::DeckViewWidget *deckView, QWidget *parent = nullptr);

    const CardData &cardData() const { return mCard; }

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *) override;

private:
    CardData mCard;
    QPixmap mPixmap;
    QFont mFont;
    qreal mAngleDeg = 0.0;
    bool mHovered = false;
    ::DeckViewWidget *mDeckView = nullptr;
};
} // namespace

// ── DeckCardPreviewLabel 实现 ───────────────────────────────────
namespace {
DeckCardPreviewLabel::DeckCardPreviewLabel(const CardData &card, const QPixmap &pixmap,
                                           const QFont &font, qreal angleDeg,
                                           ::DeckViewWidget *deckView, QWidget *parent)
    : QLabel(parent), mCard(card), mPixmap(pixmap), mFont(font),
      mAngleDeg(angleDeg), mDeckView(deckView)
{
    setMouseTracking(true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setStyleSheet("background:transparent; border:none;");
    setCursor(Qt::PointingHandCursor);
}

bool DeckCardPreviewLabel::event(QEvent *event)
{
    if (event->type() == QEvent::Enter) {
        mHovered = true;
        // 注意：不再 raise()——之前会把悬浮的卡片抬到当前行最上层，破坏从左到右
        // 自然的 z 序，让被悬停的牌看起来"跳出"行外。保持创建顺序的层级即可。
        update();
        if (mDeckView) mDeckView->showHoverInfo(this, mCard);
    } else if (event->type() == QEvent::Leave) {
        mHovered = false;
        update();
        if (mDeckView) mDeckView->hideHoverInfo();
    }
    return QLabel::event(event);
}

void DeckCardPreviewLabel::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const int cardX = (width() - mPixmap.width()) / 2;
    const int cardY = (height() - mPixmap.height()) / 2 + (mHovered ? -2 : 0);
    QRectF cardRect(cardX, cardY, mPixmap.width(), mPixmap.height());

    painter.save();
    painter.translate(cardRect.center());
    painter.rotate(mAngleDeg);
    QRectF localRect(-mPixmap.width() / 2.0, -mPixmap.height() / 2.0,
                     mPixmap.width(), mPixmap.height());
    painter.drawPixmap(localRect.topLeft(), mPixmap);

    if (mHovered) {
        QColor blue(31, 183, 255, 245);
        QColor glow = blue;
        glow.setAlpha(90);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(glow, 3.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawRoundedRect(localRect.adjusted(-1.1, -1.1, 1.1, 1.1), 7, 7);
        painter.setPen(QPen(blue, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawRoundedRect(localRect.adjusted(1.4, 1.4, -1.4, -1.4), 6, 6);
    }
    painter.restore();
    // hover info 由 DeckViewWidget::showHoverInfo() 统一弹出，对齐主场景 BalatroInfoPanel 样式。
}

// 牌组查看里的卡描述与主场景共用 CardTooltipFormat（cardtooltipformat.h）。
} // namespace

DeckViewWidget::DeckViewWidget(const QFont &cnFont, const QFont &pixelFont,
                               QWidget *parent)
    : QWidget(parent), mCNFont(cnFont), mPixelFont(pixelFont)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet("background: rgba(0, 0, 0, 190);");
    hide();
    buildUi();
}

void DeckViewWidget::showHoverInfo(QWidget *anchor, const CardData &card)
{
    if (!anchor) return;
    if (!mHoverTooltip) {
        mHoverTooltip = new BalatroInfoPanel(mCNFont, this);
        mHoverTooltip->hide();
        mHoverTooltip->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }

    QVector<BalatroInfoPanel::Badge> badges;
    switch (card.edition) {
    case Edition::Foil:        badges.append({QStringLiteral("镀膜"), BalatroInfoPanel::editionPillColor()}); break;
    case Edition::Holographic: badges.append({QStringLiteral("全息"), BalatroInfoPanel::editionPillColor()}); break;
    case Edition::Polychrome:  badges.append({QStringLiteral("多彩"), BalatroInfoPanel::editionPillColor()}); break;
    case Edition::Negative:    badges.append({QStringLiteral("负片"), BalatroInfoPanel::editionPillColor()}); break;
    default: break;
    }
    switch (card.seal) {
    case Seal::Gold:   badges.append({QStringLiteral("金印章"), BalatroInfoPanel::sealPillColor(0)}); break;
    case Seal::Red:    badges.append({QStringLiteral("红印章"), BalatroInfoPanel::sealPillColor(1)}); break;
    case Seal::Blue:   badges.append({QStringLiteral("蓝印章"), BalatroInfoPanel::sealPillColor(2)}); break;
    case Seal::Purple: badges.append({QStringLiteral("紫印章"), BalatroInfoPanel::sealPillColor(3)}); break;
    default: break;
    }
    if (card.isDebuffed)
        badges.append({QStringLiteral("被禁用"), QColor("#9b3a3a")});

    // 牌组里的卡都是 playing card——名字也用白盒。
    mHoverTooltip->setContent(CardTooltipFormat::cardTitleHtml(card),
                              CardTooltipFormat::cardBodyHtml(card), badges, 160,
                              /*nameHasWhiteBox=*/true);

    // 锚到卡片正上方：把 anchor 的全局位置转换到本 widget 坐标。
    QPoint topLeftInThis = anchor->mapTo(this, QPoint(0, 0));
    int x = topLeftInThis.x() + (anchor->width() - mHoverTooltip->width()) / 2;
    int y = topLeftInThis.y() - mHoverTooltip->height() - 6;
    if (x < 6) x = 6;
    if (x + mHoverTooltip->width() > width() - 6) x = width() - mHoverTooltip->width() - 6;
    if (y < 6) y = topLeftInThis.y() + anchor->height() + 6;
    mHoverTooltip->move(x, y);
    mHoverTooltip->raise();
    mHoverTooltip->show();
}

void DeckViewWidget::hideHoverInfo()
{
    if (mHoverTooltip) mHoverTooltip->hide();
}

void DeckViewWidget::buildUi()
{
    mPanel = new QWidget(this);
    mPanel->setObjectName("deckPanel");
    mPanel->setAttribute(Qt::WA_StyledBackground, true);
    mPanel->setStyleSheet(
        "QWidget#deckPanel { background:#374244; border:3px solid #fda200; border-radius:16px; }"
        );

    auto *root = new QVBoxLayout(mPanel);
    root->setContentsMargins(18, 14, 18, 16);
    root->setSpacing(10);

    auto *top = new QWidget(mPanel);
    top->setStyleSheet("background:transparent;");
    auto *topLayout = new QHBoxLayout(top);
    topLayout->setContentsMargins(0,0,0,0);
    topLayout->setSpacing(10);

    mTitle = new QLabel("牌组", top);
    QFont tf = mCNFont; tf.setPixelSize(deckUiPx(28)); tf.setBold(true);
    mTitle->setFont(tf);
    mTitle->setStyleSheet("color:#f3b958; background:transparent;");
    topLayout->addWidget(mTitle);

    mSubtitle = new QLabel("", top);
    QFont sf = mCNFont; sf.setPixelSize(deckUiPx(15));
    mSubtitle->setFont(sf);
    mSubtitle->setStyleSheet("color:#dce3e6; background:transparent;");
    topLayout->addWidget(mSubtitle, 1);

    mBtnClose = new QPushButton("关闭", top);
    mBtnClose->setFixedSize(90, 36);
    QFont bf = mCNFont; bf.setPixelSize(deckUiPx(14)); bf.setBold(true);
    mBtnClose->setFont(bf);
    mBtnClose->setCursor(Qt::PointingHandCursor);
    mBtnClose->setStyleSheet(
        "QPushButton { background:#fe5f55; color:white; border:none; border-radius:8px; }"
        "QPushButton:hover { background:#ff7066; }"
        );
    connect(mBtnClose, &QPushButton::clicked, this, &DeckViewWidget::closeView);
    topLayout->addWidget(mBtnClose);
    root->addWidget(top);

    auto *tabs = new QWidget(mPanel);
    tabs->setStyleSheet("background:transparent;");
    auto *tabsRoot = new QVBoxLayout(tabs);
    tabsRoot->setContentsMargins(0,0,0,0);
    tabsRoot->setSpacing(2);
    mTabArrow = new QLabel("▼", tabs);
    QFont af = mCNFont; af.setPixelSize(deckUiPx(20)); af.setBold(true);
    mTabArrow->setFont(af);
    mTabArrow->setAlignment(Qt::AlignCenter);
    mTabArrow->setStyleSheet("color:#fe5f55; background:transparent; border:none;");
    mTabArrow->setFixedHeight(24);
    mTabArrow->move(0, 0);
    tabsRoot->addSpacing(24);

    auto *tabRow = new QWidget(tabs);
    tabRow->setStyleSheet("background:transparent;");
    auto *tabLayout = new QHBoxLayout(tabRow);
    tabLayout->setContentsMargins(0,0,0,0);
    tabLayout->setSpacing(8);

    auto makeTab = [this](const QString &text) {
        auto *btn = new QPushButton(text);
        QFont f = mCNFont; f.setPixelSize(deckUiPx(16)); f.setBold(true);
        btn->setFont(f);
        btn->setFixedHeight(38);
        btn->setCursor(Qt::PointingHandCursor);
        return btn;
    };
    mBtnRemaining = makeTab("剩余牌组");
    mBtnFull = makeTab("完整牌组");
    connect(mBtnRemaining, &QPushButton::clicked, this, &DeckViewWidget::showRemaining);
    connect(mBtnFull, &QPushButton::clicked, this, &DeckViewWidget::showFull);
    tabLayout->addWidget(mBtnRemaining);
    tabLayout->addWidget(mBtnFull);
    tabsRoot->addWidget(tabRow);
    root->addWidget(tabs);

    mScroll = new QScrollArea(mPanel);
    mScroll->setWidgetResizable(true);
    mScroll->setFrameShape(QFrame::NoFrame);
    mScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mScroll->setStyleSheet(
        "QScrollArea { background:#151b21; border-radius:12px; }"
        );
    mGridHost = new QWidget;
    mGridHost->setStyleSheet("background:#151b21;");
    mGrid = new QGridLayout(mGridHost);
    mGrid->setContentsMargins(12, 10, 12, 10);
    mGrid->setHorizontalSpacing(0);
    mGrid->setVerticalSpacing(8);
    mScroll->setWidget(mGridHost);
    root->addWidget(mScroll, 1);
}

void DeckViewWidget::open(const QVector<CardData> &remainingCards,
                          const QVector<CardData> &fullDeckCards)
{
    mRemainingCards = remainingCards;
    mFullDeckCards = fullDeckCards;
    mShowingFull = false;
    show();
    raise();
    refreshTabs();
    refreshGrid();
}

void DeckViewWidget::showRemaining()
{
    mShowingFull = false;
    refreshTabs();
    refreshGrid();
}

void DeckViewWidget::showFull()
{
    mShowingFull = true;
    refreshTabs();
    refreshGrid();
}

void DeckViewWidget::closeView()
{
    // 关闭前先把 hover 浮窗收掉，避免它在主场景上空闲挂着。
    hideHoverInfo();
    hide();
    emit closed();
}

void DeckViewWidget::refreshTabs()
{
    const QString active = "QPushButton { background:#fda200; color:white; border:none; border-radius:8px; }";
    const QString inactive = "QPushButton { background:#4f6367; color:#dce3e6; border:none; border-radius:8px; }"
                             "QPushButton:hover { background:#60777c; }";
    mBtnRemaining->setStyleSheet(mShowingFull ? inactive : active);
    mBtnFull->setStyleSheet(mShowingFull ? active : inactive);

    mTitle->setText(mShowingFull ? "完整牌组" : "剩余牌组");
    mSubtitle->setText(QString("剩余 %1 张 / 完整 %2 张")
                           .arg(mRemainingCards.size())
                           .arg(mFullDeckCards.size()));

    QTimer::singleShot(0, this, [this]() {
        if (!mTabArrow || !mBtnRemaining || !mBtnFull) return;
        QPushButton *target = mShowingFull ? mBtnFull : mBtnRemaining;
        QWidget *parent = mTabArrow->parentWidget();
        if (!target || !parent) return;
        const QPoint topLeft = target->mapTo(parent, QPoint(0, 0));
        mTabArrow->setGeometry(topLeft.x(), 0, qMax(1, target->width()), mTabArrow->height());
        mTabArrow->raise();
    });
}

static int suitOrder(Suit s)
{
    switch (s) {
    case Suit::Spades: return 0;
    case Suit::Hearts: return 1;
    case Suit::Clubs: return 2;
    case Suit::Diamonds: return 3;
    }
    return 0;
}

static QString suitTitle(Suit s)
{
    switch (s) {
    case Suit::Spades:   return "♠ 黑桃";
    case Suit::Hearts:   return "♥ 红心";
    case Suit::Clubs:    return "♣ 梅花";
    case Suit::Diamonds: return "♦ 方块";
    }
    return "";
}

void DeckViewWidget::refreshGrid()
{
    while (auto *item = mGrid->takeAt(0)) {
        if (auto *w = item->widget()) w->deleteLater();
        delete item;
    }

    QVector<CardData> cards = mShowingFull ? mFullDeckCards : mRemainingCards;
    if (cards.isEmpty()) {
        auto *empty = new QLabel("没有牌", mGridHost);
        QFont f = mCNFont; f.setPixelSize(deckUiPx(22)); f.setBold(true);
        empty->setFont(f);
        empty->setAlignment(Qt::AlignCenter);
        empty->setStyleSheet("color:#dce3e6; background:transparent;");
        mGrid->addWidget(empty, 0, 0);
        return;
    }

    auto rankDesc = [](const CardData &a, const CardData &b) {
        // 原版牌组查看里石头牌仍保留原来的 suit/rank 归属，排在该花色行最后，
        // 不是单独开一行。
        bool as = a.enhancement == Enhancement::Stone;
        bool bs = b.enhancement == Enhancement::Stone;
        if (as != bs) return !as;
        if (a.rank != b.rank) return static_cast<int>(a.rank) > static_cast<int>(b.rank);
        return static_cast<int>(a.suit) < static_cast<int>(b.suit);
    };

    const Suit suits[4] = { Suit::Spades, Suit::Hearts, Suit::Clubs, Suit::Diamonds };
    int gridRow = 0;

    QMargins gridMargins = mGrid->contentsMargins();
    const int viewportH = mScroll && mScroll->viewport() ? mScroll->viewport()->height() : 520;
    const int rowH = qBound(96,
                            (viewportH - gridMargins.top() - gridMargins.bottom()
                             - 3 * mGrid->verticalSpacing()) / 4,
                            124);
    const int cardH = qBound(70, rowH - 30, 94);
    const int cardW = qMax(1, int(std::round(cardH * (142.0 / 190.0))));
    const QSize cardSize(cardW, cardH);
    const int previewW = cardSize.width() + 24;
    const int previewH = rowH;
    const int viewportW = mScroll && mScroll->viewport() ? mScroll->viewport()->width() : 860;
    const int rowContentW = qMax(360, viewportW - gridMargins.left() - gridMargins.right());

    auto addRow = [&](QVector<CardData> rowCards) {
        std::sort(rowCards.begin(), rowCards.end(), rankDesc);

        auto *row = new QWidget(mGridHost);
        row->setFixedHeight(rowH);
        row->setStyleSheet("background:#222b33; border-radius:12px;");

        // 左侧只保留与“比赛信息”页一致的红色三角提示，不再写“黑桃/红心/梅花/方块”。
        auto *marker = new QLabel("▶", row);
        QFont mf = mCNFont; mf.setPixelSize(deckUiPx(18)); mf.setBold(true);
        marker->setFont(mf);
        marker->setAlignment(Qt::AlignCenter);
        marker->setStyleSheet("color:#fe5f55; background:transparent; border:none;");
        marker->setGeometry(8, rowH / 2 - 13, 24, 26);

        const int left = 34;
        const int rightPad = 12;
        const int maxSpan = qMax(0, rowContentW - left - rightPad - previewW + 10);
        int step = (rowCards.size() <= 1) ? 0 : maxSpan / qMax(1, rowCards.size() - 1);
        step = qBound(30, step, 58); // 牌多时按原版 CardArea 重叠，始终留在单页行宽内。

        for (int i = 0; i < rowCards.size(); ++i) {
            const qreal angle = qBound(-5.0, (i - (rowCards.size() - 1) / 2.0) * 0.75, 5.0);
            auto *img = new DeckCardPreviewLabel(rowCards[i], renderCard(rowCards[i], cardSize),
                                                 mCNFont, angle, this, row);
            img->setFixedSize(previewW, previewH);
            img->setGeometry(left + i * step - 10, 0, previewW, previewH);
            img->raise();
        }

        row->setMinimumWidth(qMax(rowContentW, 760));
        mGrid->addWidget(row, gridRow++, 0);
    };

    for (Suit s : suits) {
        QVector<CardData> rowCards;
        for (const CardData &c : cards) {
            if (c.suit == s) rowCards.append(c);
        }
        addRow(rowCards);
    }
}

QString DeckViewWidget::cardExtraText(const CardData &c) const
{
    QStringList parts;
    switch (c.enhancement) {
    case Enhancement::Bonus: parts << "Bonus"; break;
    case Enhancement::Mult: parts << "Mult"; break;
    case Enhancement::Wild: parts << "Wild"; break;
    case Enhancement::Glass: parts << "Glass"; break;
    case Enhancement::Steel: parts << "Steel"; break;
    case Enhancement::Stone: parts << "Stone"; break;
    case Enhancement::Gold: parts << "Gold"; break;
    case Enhancement::Lucky: parts << "Lucky"; break;
    default: break;
    }
    switch (c.edition) {
    case Edition::Foil: parts << "Foil"; break;
    case Edition::Holographic: parts << "Holo"; break;
    case Edition::Polychrome: parts << "Poly"; break;
    case Edition::Negative: parts << "Negative"; break;
    default: break;
    }
    switch (c.seal) {
    case Seal::Gold: parts << "金印"; break;
    case Seal::Red: parts << "红印"; break;
    case Seal::Blue: parts << "蓝印"; break;
    case Seal::Purple: parts << "紫印"; break;
    default: break;
    }
    return parts.isEmpty() ? "普通" : parts.join(" · ");
}

QPixmap DeckViewWidget::renderCard(const CardData &c, const QSize &size) const
{
    // 这里只是按图集每格 142×190 切取原始纹理；最后通过 scaled() 输出 size。
    constexpr int W = ConsumableItem::SRC_W, H = ConsumableItem::SRC_H;
    QPixmap deckSheet = DeckSkin::deckSheet();   // 跟随定制牌组换肤

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
        case Suit::Hearts:   row = 0; break;
        case Suit::Clubs:    row = 1; break;
        case Suit::Diamonds: row = 2; break;
        case Suit::Spades:   row = 3; break;
        }
        p.drawPixmap(QRect(0, 0, W, H), deckSheet, QRect(col*W, row*H, W, H));
    }

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

    // 后续可能重新赋值 pix；必须先结束 QPainter，否则完整牌组里遇到 debuff
    // 卡时会出现 QPixmap 正在被绘制又被替换，导致闪退。
    p.end();

    {
        QPainter ep(&pix);
        ep.setRenderHint(QPainter::SmoothPixmapTransform, true);
        switch (c.edition) {
        case Edition::Foil:
            ep.setPen(QPen(QColor(120, 200, 255, 200), 4));
            ep.drawRoundedRect(2, 2, W-4, H-4, 8, 8); break;
        case Edition::Holographic:
            ep.setPen(QPen(QColor(255, 100, 200, 200), 4));
            ep.drawRoundedRect(2, 2, W-4, H-4, 8, 8); break;
        case Edition::Polychrome: {
            QLinearGradient g(0, 0, W, H);
            g.setColorAt(0,   QColor(255, 100, 100, 220));
            g.setColorAt(0.5, QColor(100, 255, 100, 220));
            g.setColorAt(1,   QColor(100, 100, 255, 220));
            ep.setPen(QPen(QBrush(g), 4));
            ep.drawRoundedRect(2, 2, W-4, H-4, 8, 8); break;
        }
        case Edition::Negative:
            ep.fillRect(0, 0, W, H, QColor(40, 0, 60, 120));
            ep.setPen(QPen(QColor(180, 100, 255, 200), 4));
            ep.drawRoundedRect(2, 2, W-4, H-4, 8, 8); break;
        default: break;
        }
    }

    if (c.isDebuffed) {
        pix = BalatroShaders::renderDebuffedPixmap(pix, 1.0);
    }

    return pix.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

void DeckViewWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    layoutPanel();
    if (isVisible()) {
        QTimer::singleShot(0, this, [this]() {
            if (isVisible()) refreshGrid();
        });
    }
}

void DeckViewWidget::layoutPanel()
{
    if (!mPanel) return;
    int panelW = qBound(900, int(width() * 0.88), 1260);
    int panelH = qBound(600, int(height() * 0.82), 760);
    mPanel->resize(panelW, panelH);
    mPanel->move((width() - panelW) / 2, (height() - panelH) / 2);
}
