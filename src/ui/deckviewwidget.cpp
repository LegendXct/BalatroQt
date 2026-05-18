#include "deckviewwidget.h"
#include "../card/consumableitem.h"
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

void drawDeckHoverTag(QPainter *painter, const CardData &d, const QRectF &cardRect, const QFont &baseFont)
{
    const QString title = deckHoverTitle(d);
    const QString desc = deckHoverDesc(d);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, false);

    QFont titleFont = baseFont;
    titleFont.setPixelSize(13);
    titleFont.setBold(true);
    QFont descFont = baseFont;
    descFont.setPixelSize(13);
    descFont.setBold(true);

    QFontMetrics titleFm(titleFont);
    QFontMetrics descFm(descFont);
    const int w = qMax(58, qMax(titleFm.horizontalAdvance(title), descFm.horizontalAdvance(desc)) + 18);
    const int titleH = 23;
    const int descH = 23;
    const qreal x = cardRect.center().x() - w / 2.0;
    const qreal y = 1.0;

    QRectF outer(x, y, w, titleH + descH + 3);
    QPainterPath shadowPath;
    shadowPath.addRoundedRect(outer.adjusted(1.8, 2.2, 1.8, 2.2), 6, 6);
    painter->fillPath(shadowPath, QColor(0, 0, 0, 70));

    QRectF titleRect(x, y, w, titleH + 1);
    QRectF descRect(x, y + titleH - 1, w, descH + 2);
    QColor bg(248, 255, 250);
    QColor edge(34, 45, 48);

    QPainterPath titlePath;
    titlePath.addRoundedRect(titleRect, 6, 6);
    painter->fillPath(titlePath, bg);
    painter->setPen(QPen(edge, 1.6));
    painter->drawPath(titlePath);

    QPainterPath descPath;
    descPath.addRoundedRect(descRect, 6, 6);
    painter->fillPath(descPath, bg);
    painter->setPen(QPen(edge, 1.6));
    painter->drawPath(descPath);
    painter->fillRect(QRectF(x + 2, y + titleH - 2, w - 4, 4), bg);
    painter->setPen(QPen(edge, 1.4));
    painter->drawLine(QPointF(x + 2, y + titleH), QPointF(x + w - 2, y + titleH));

    painter->setFont(titleFont);
    painter->setPen(QColor(31, 45, 48));
    painter->drawText(titleRect, Qt::AlignCenter, title);
    painter->setFont(descFont);
    painter->setPen(QColor(42, 132, 205));
    painter->drawText(descRect, Qt::AlignCenter, desc);
    painter->restore();
}

class DeckCardPreviewLabel : public QLabel
{
public:
    DeckCardPreviewLabel(const CardData &card, const QPixmap &pixmap, const QFont &font, QWidget *parent = nullptr)
        : QLabel(parent), mCard(card), mPixmap(pixmap), mFont(font)
    {
        setMouseTracking(true);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setStyleSheet("background:transparent; border:none;");
        setCursor(Qt::PointingHandCursor);
    }

protected:
    bool event(QEvent *event) override
    {
        if (event->type() == QEvent::Enter) {
            mHovered = true;
            raise();
            update();
        } else if (event->type() == QEvent::Leave) {
            mHovered = false;
            update();
        }
        return QLabel::event(event);
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const int cardX = (width() - mPixmap.width()) / 2;
        const int cardY = 48;
        QRectF cardRect(cardX, cardY, mPixmap.width(), mPixmap.height());

        painter.drawPixmap(cardRect.topLeft(), mPixmap);

        if (mHovered) {
            QColor blue(31, 183, 255, 245);
            QColor glow = blue;
            glow.setAlpha(90);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(glow, 3.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawRoundedRect(cardRect.adjusted(-1.1, -1.1, 1.1, 1.1), 7, 7);
            painter.setPen(QPen(blue, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawRoundedRect(cardRect.adjusted(1.4, 1.4, -1.4, -1.4), 6, 6);
            drawDeckHoverTag(&painter, mCard, cardRect, mFont);
        }
    }

private:
    CardData mCard;
    QPixmap mPixmap;
    QFont mFont;
    bool mHovered = false;
};
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
    auto *tabLayout = new QHBoxLayout(tabs);
    tabLayout->setContentsMargins(0,0,0,0);
    tabLayout->setSpacing(8);

    auto makeTab = [this](const QString &text) {
        auto *btn = new QPushButton(text);
        QFont f = mCNFont; f.setPixelSize(deckUiPx(16)); f.setBold(true);
        btn->setFont(f);
        btn->setFixedHeight(42);
        btn->setCursor(Qt::PointingHandCursor);
        return btn;
    };
    mBtnRemaining = makeTab("剩余牌组");
    mBtnFull = makeTab("完整牌组");
    connect(mBtnRemaining, &QPushButton::clicked, this, &DeckViewWidget::showRemaining);
    connect(mBtnFull, &QPushButton::clicked, this, &DeckViewWidget::showFull);
    tabLayout->addWidget(mBtnRemaining);
    tabLayout->addWidget(mBtnFull);
    root->addWidget(tabs);

    mScroll = new QScrollArea(mPanel);
    mScroll->setWidgetResizable(true);
    mScroll->setFrameShape(QFrame::NoFrame);
    mScroll->setStyleSheet(
        "QScrollArea { background:#151b21; border-radius:12px; }"
        "QScrollBar:vertical { background:#283238; width:12px; margin:0; }"
        "QScrollBar::handle:vertical { background:#fda200; border-radius:6px; min-height:30px; }"
        );
    mGridHost = new QWidget;
    mGridHost->setStyleSheet("background:#151b21;");
    mGrid = new QGridLayout(mGridHost);
    mGrid->setContentsMargins(14, 14, 14, 14);
    mGrid->setHorizontalSpacing(10);
    mGrid->setVerticalSpacing(12);
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
    hide();
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

    auto addRow = [&](const QString &title, QVector<CardData> rowCards) {
        std::sort(rowCards.begin(), rowCards.end(), rankDesc);
        if (rowCards.isEmpty()) return;

        auto *row = new QWidget(mGridHost);
        row->setMinimumHeight(154);
        row->setStyleSheet("background:#222b33; border-radius:12px;");

        auto *titleLbl = new QLabel(title, row);
        QFont tf = mCNFont; tf.setPixelSize(deckUiPx(16)); tf.setBold(true);
        titleLbl->setFont(tf);
        titleLbl->setAlignment(Qt::AlignCenter);
        titleLbl->setStyleSheet("color:#f3b958; background:transparent;");
        titleLbl->setGeometry(10, 12, 84, 28);

        auto *countLbl = new QLabel(QString::number(rowCards.size()), row);
        QFont cf = mPixelFont; cf.setPixelSize(deckUiPx(22)); cf.setBold(true);
        countLbl->setFont(cf);
        countLbl->setAlignment(Qt::AlignCenter);
        countLbl->setStyleSheet("color:white; background:#374244; border-radius:8px;");
        countLbl->setGeometry(24, 52, 52, 34);

        const QSize cardSize(74, 99);
        const int previewW = cardSize.width() + 20;
        const int previewH = cardSize.height() + 52;
        const int left = 108;
        const int top = 0;
        int avail = qMax(620, mPanel ? mPanel->width() - 180 : 760);
        int step = (rowCards.size() <= 1) ? 0 : (avail - cardSize.width()) / (rowCards.size() - 1);
        step = qBound(24, step, 58); // 牌多时重叠，牌少时展开，接近原版 Deck Info 的行展示

        for (int i = 0; i < rowCards.size(); ++i) {
            auto *img = new DeckCardPreviewLabel(rowCards[i], renderCard(rowCards[i], cardSize), mCNFont, row);
            img->setFixedSize(previewW, previewH);
            img->setGeometry(left + i * step - 10, top, previewW, previewH);
            img->raise();
        }

        int rowW = left + qMax(1, rowCards.size()) * step + previewW + 18;
        row->setMinimumWidth(qMax(rowW, 760));
        mGrid->addWidget(row, gridRow++, 0);
    };

    for (Suit s : suits) {
        QVector<CardData> rowCards;
        for (const CardData &c : cards) {
            if (c.suit == s) rowCards.append(c);
        }
        addRow(suitTitle(s), rowCards);
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
}

void DeckViewWidget::layoutPanel()
{
    if (!mPanel) return;
    int panelW = qBound(900, int(width() * 0.78), 1120);
    int panelH = qBound(640, int(height() * 0.82), 820);
    mPanel->resize(panelW, panelH);
    mPanel->move((width() - panelW) / 2, (height() - panelH) / 2);
}
