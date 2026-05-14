#include "packopenwidget.h"
#include "../card/jokeritem.h"
#include "../card/consumableitem.h"
#include "../utils/shadereffects.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QResizeEvent>
#include <QLabel>
#include <QPushButton>
#include <QPainter>
#include <QIcon>
#include <algorithm>
#include <QStringList>
#include <QTimer>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QSizePolicy>
#include <QMouseEvent>
#include <QLineF>
#include <QEnterEvent>
#include <QTransform>
#include <cmath>

class PackHandCardWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PackHandCardWidget(int index, QWidget *parent = nullptr)
        : QWidget(parent), mIndex(index)
    {
        setMouseTracking(true);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_TranslucentBackground, true);
    }

    void setIndex(int index) { mIndex = index; }
    void setPixmap(const QPixmap &pix) { mPixmap = pix; update(); }
    void setSelected(bool s) { mSelected = s; update(); }
    void setBaseRotation(qreal deg) { mBaseRotation = deg; update(); }

signals:
    void clicked(int index);
    void dragged(int index, QPoint parentPos);
    void released(int index, QPoint parentPos);

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        const QRectF r = rect().adjusted(3, 3, -3, -3);
        const qreal cx = width() / 2.0;
        const qreal cy = height() / 2.0;
        QTransform t;
        t.translate(cx, cy);
        t.rotate(mBaseRotation + mHoverTiltY * 0.18);
        t.shear(mHoverTiltY / 260.0, -mHoverTiltX / 280.0);
        t.translate(-cx, -cy);
        p.setTransform(t, true);
        if (!mPixmap.isNull()) p.drawPixmap(r, mPixmap, QRectF(mPixmap.rect()));
        if (mSelected || mHovered) {
            p.setPen(QPen(mSelected ? QColor(255, 230, 109, 230) : QColor(255, 230, 109, 140), mSelected ? 4 : 2));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(r, 9, 9);
        }
    }

    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() != Qt::LeftButton) return QWidget::mousePressEvent(e);
        mPressed = true;
        mDragging = false;
        mPressPos = e->pos();
        mStartPos = pos();
        raise();
        e->accept();
    }

    void mouseMoveEvent(QMouseEvent *e) override
    {
        const qreal nx = (e->pos().x() / qMax(1.0, double(width()))) - 0.5;
        const qreal ny = (e->pos().y() / qMax(1.0, double(height()))) - 0.5;
        mHoverTiltY = nx * 18.0;
        mHoverTiltX = ny * 18.0;
        update();

        if (!mPressed) return QWidget::mouseMoveEvent(e);
        if (!mDragging && QLineF(e->pos(), mPressPos).length() > 7.0) mDragging = true;
        if (mDragging) {
            QPoint parentP = mapToParent(e->pos());
            move(parentP - mPressPos);
            emit dragged(mIndex, parentP);
            e->accept();
            return;
        }
        QWidget::mouseMoveEvent(e);
    }

    void mouseReleaseEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton && mPressed) {
            QPoint parentP = mapToParent(e->pos());
            mPressed = false;
            if (mDragging) {
                mDragging = false;
                emit released(mIndex, parentP);
            } else {
                emit clicked(mIndex);
            }
            e->accept();
            return;
        }
        QWidget::mouseReleaseEvent(e);
    }

    void enterEvent(QEnterEvent *e) override
    {
        mHovered = true; update(); QWidget::enterEvent(e);
    }

    void leaveEvent(QEvent *e) override
    {
        if (!mPressed) { mHovered = false; mHoverTiltX = mHoverTiltY = 0; update(); }
        QWidget::leaveEvent(e);
    }

private:
    int mIndex = 0;
    QPixmap mPixmap;
    bool mSelected = false;
    bool mHovered = false;
    bool mPressed = false;
    bool mDragging = false;
    QPoint mPressPos;
    QPoint mStartPos;
    qreal mBaseRotation = 0;
    qreal mHoverTiltX = 0;
    qreal mHoverTiltY = 0;
};

PackOpenWidget::PackOpenWidget(const QFont &cnFont, const QFont &pixelFont,
                               QWidget *parent)
    : QWidget(parent), mCNFont(cnFont), mPixelFont(pixelFont)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet("background: transparent;");
    hide();
    buildUi();

    // 灵魂牌的白水晶是动态前景层。开包界面使用 QLabel 贴图，
    // 所以需要定时重新渲染 Pixmap，否则只会停在第一帧。
    mSoulAnimTimer = new QTimer(this);
    connect(mSoulAnimTimer, &QTimer::timeout, this, [this]() {
        if (!isVisible()) return;
        bool hasSoul = false;
        for (ConsumableType t : mContent.consumables) {
            if (t == ConsumableType::Spectral_Soul) { hasSoul = true; break; }
        }
        if (!hasSoul) {
            for (const Consumable &c : mInventoryConsumables) {
                if (c.type == ConsumableType::Spectral_Soul) { hasSoul = true; break; }
            }
        }
        if (hasSoul) {
            refreshOptionUi();
            refreshInventoryUi();
        }
    });
    mSoulAnimTimer->start(100);
}

void PackOpenWidget::buildUi()
{
    mPanel = new QWidget(this);
    mPanel->setMinimumSize(860, 540);
    mPanel->setStyleSheet(
        "background: transparent;"
        "border: none;"
        );

    auto *root = new QVBoxLayout(mPanel);
    root->setContentsMargins(22, 16, 22, 16);
    root->setSpacing(10);

    mLblTitle = new QLabel("打开包", mPanel);
    QFont tf = mCNFont; tf.setPixelSize(30); tf.setBold(true);
    mLblTitle->setFont(tf);
    mLblTitle->setStyleSheet("color:#f0c040; background:transparent;");
    mLblTitle->setAlignment(Qt::AlignCenter);

    mLblChoose = new QLabel("", mPanel);
    QFont cf = mCNFont; cf.setPixelSize(18);
    mLblChoose->setFont(cf);
    mLblChoose->setStyleSheet("color:white; background:transparent;");
    mLblChoose->setAlignment(Qt::AlignCenter);

    // 上方临时手牌区：改成原版手牌式重叠排布，避免十张牌硬塞成一排导致越界/裁切。
    mHandBox = new QWidget(mPanel);
    auto *handBox = mHandBox;
    handBox->setObjectName("packHandBox");
    handBox->setAttribute(Qt::WA_StyledBackground, true);
    handBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    handBox->setMinimumHeight(172);
    handBox->setStyleSheet("QWidget#packHandBox { background:transparent; border:none; }");

    for (int i = 0; i < 10; ++i) {
        HandUi hu;
        hu.btn = new PackHandCardWidget(i, handBox);
        connect(hu.btn, &PackHandCardWidget::clicked, this, &PackOpenWidget::onHandCardClicked);
        connect(hu.btn, &PackHandCardWidget::dragged, this, &PackOpenWidget::onHandCardDragged);
        connect(hu.btn, &PackHandCardWidget::released, this, &PackOpenWidget::onHandCardReleased);

        hu.nameLbl = new QLabel("", handBox);
        hu.nameLbl->hide();
        mHandUi.append(hu);
    }
    root->addWidget(handBox);

    // 中间：左侧包内牌，右侧仓库特殊牌。
    auto *midRow = new QWidget(mPanel);
    auto *midLayout = new QHBoxLayout(midRow);
    midLayout->setContentsMargins(0, 0, 0, 0);
    midLayout->setSpacing(12);

    auto *optionsBox = new QWidget(midRow);
    optionsBox->setObjectName("packOptionsBox");
    optionsBox->setAttribute(Qt::WA_StyledBackground, true);
    optionsBox->setStyleSheet("QWidget#packOptionsBox { background:transparent; border:none; }");
    auto *optionsLayout = new QHBoxLayout(optionsBox);
    optionsLayout->setContentsMargins(12, 10, 12, 10);
    optionsLayout->setSpacing(10);
    optionsLayout->setAlignment(Qt::AlignCenter);

    for (int i = 0; i < 5; ++i) {        // 原版普通包 3，超级/巨型包最多 5；幻灵/小丑超级最多 4
        OptUi ou;
        ou.card = new QWidget(optionsBox);
        ou.card->setFixedSize(154, 270);
        ou.card->setStyleSheet("background:transparent; border:none;");

        auto *vbl = new QVBoxLayout(ou.card);
        vbl->setContentsMargins(6, 6, 6, 6);
        vbl->setSpacing(4);

        ou.imageLbl = new QLabel(ou.card);
        ou.imageLbl->setFixedSize(126, 168);
        ou.imageLbl->setAlignment(Qt::AlignCenter);
        ou.imageLbl->setStyleSheet("background:transparent;");
        vbl->addWidget(ou.imageLbl, 0, Qt::AlignCenter);

        ou.nameLbl = new QLabel("", ou.card);
        QFont nf = mCNFont; nf.setPixelSize(12); nf.setBold(true);
        ou.nameLbl->setFont(nf);
        ou.nameLbl->setStyleSheet("color:white; background:transparent;");
        ou.nameLbl->setAlignment(Qt::AlignCenter);
        ou.nameLbl->setWordWrap(true);
        ou.nameLbl->setFixedHeight(34);
        vbl->addWidget(ou.nameLbl);

        ou.descLbl = new QLabel("", ou.card);
        QFont df = mCNFont; df.setPixelSize(9);
        ou.descLbl->setFont(df);
        ou.descLbl->setStyleSheet("color:#aab2ba; background:transparent;");
        ou.descLbl->setAlignment(Qt::AlignCenter);
        ou.descLbl->setWordWrap(true);
        ou.descLbl->setFixedHeight(44);
        vbl->addWidget(ou.descLbl);

        ou.takeBtn = new QPushButton("选择", ou.card);
        ou.takeBtn->setFixedHeight(30);
        QFont bf = mCNFont; bf.setPixelSize(12);
        ou.takeBtn->setFont(bf);
        ou.takeBtn->setCursor(Qt::PointingHandCursor);
        ou.takeBtn->setStyleSheet(
            "QPushButton { background:#3060c0; color:white; border:none; border-radius:6px; }"
            "QPushButton:hover { background:#4070d0; }"
            "QPushButton:disabled { background:#333; color:#777; }"
            );
        connect(ou.takeBtn, &QPushButton::clicked, this, [this, i]() { onChoose(i); });
        vbl->addWidget(ou.takeBtn);

        optionsLayout->addWidget(ou.card);
        mOptUi.append(ou);
    }
    midLayout->addWidget(optionsBox, 1);

    auto *invBox = new QWidget(midRow);
    mInventoryBox = invBox;
    invBox->setObjectName("inventoryConsumableBox");
    invBox->setFixedWidth(210);
    invBox->setAttribute(Qt::WA_StyledBackground, true);
    invBox->setStyleSheet("QWidget#inventoryConsumableBox { background:#151b21; border-radius:10px; }");
    auto *ivbl = new QVBoxLayout(invBox);
    ivbl->setContentsMargins(10, 10, 10, 10);
    ivbl->setSpacing(6);

    auto *invTitle = new QLabel("仓库特殊牌", invBox);
    QFont itf = mCNFont; itf.setPixelSize(14); itf.setBold(true);
    invTitle->setFont(itf);
    invTitle->setAlignment(Qt::AlignCenter);
    invTitle->setStyleSheet("color:#f0c040; background:transparent;");
    ivbl->addWidget(invTitle);

    for (int i = 0; i < 4; ++i) {
        InvUi iu;
        iu.card = new QWidget(invBox);
        iu.card->setFixedHeight(74);
        iu.card->setStyleSheet("background:#222b33; border-radius:8px;");
        auto *hbl = new QHBoxLayout(iu.card);
        hbl->setContentsMargins(6, 6, 6, 6);
        hbl->setSpacing(6);

        iu.imageLbl = new QLabel(iu.card);
        iu.imageLbl->setFixedSize(42, 56);
        iu.imageLbl->setStyleSheet("background:transparent;");
        iu.imageLbl->setAlignment(Qt::AlignCenter);
        hbl->addWidget(iu.imageLbl);

        auto *textCol = new QWidget(iu.card);
        auto *tc = new QVBoxLayout(textCol);
        tc->setContentsMargins(0, 0, 0, 0);
        tc->setSpacing(3);

        iu.nameLbl = new QLabel("", textCol);
        QFont inf = mCNFont; inf.setPixelSize(11);
        iu.nameLbl->setFont(inf);
        iu.nameLbl->setStyleSheet("color:white; background:transparent;");
        iu.nameLbl->setWordWrap(true);
        tc->addWidget(iu.nameLbl);

        iu.useBtn = new QPushButton("使用", textCol);
        iu.useBtn->setFixedHeight(24);
        QFont ubf = mCNFont; ubf.setPixelSize(10);
        iu.useBtn->setFont(ubf);
        iu.useBtn->setCursor(Qt::PointingHandCursor);
        iu.useBtn->setStyleSheet(
            "QPushButton { background:#8a4fd3; color:white; border:none; border-radius:5px; }"
            "QPushButton:hover { background:#9b60e8; }"
            "QPushButton:disabled { background:#333; color:#777; }"
            );
        connect(iu.useBtn, &QPushButton::clicked, this, [this, i]() { onUseInventory(i); });
        tc->addWidget(iu.useBtn);

        hbl->addWidget(textCol, 1);
        ivbl->addWidget(iu.card);
        mInvUi.append(iu);
    }
    ivbl->addStretch();
    midLayout->addWidget(invBox);
    // 原版开包时仍然使用右上角仓库区；这里不再在包界面右侧重复做仓库侧栏。
    invBox->hide();

    root->addWidget(midRow, 1);

    // 原版开包 UI：包名/选择数在下方中央，跳过按钮在右侧。
    auto *bottomBar = new QWidget(mPanel);
    bottomBar->setStyleSheet("background:transparent;");
    auto *bottomLayout = new QHBoxLayout(bottomBar);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(12);
    bottomLayout->addStretch(1);

    auto *titleCol = new QWidget(bottomBar);
    titleCol->setStyleSheet("background:rgba(20,25,30,120); border-radius:12px;");
    auto *titleLayout = new QVBoxLayout(titleCol);
    titleLayout->setContentsMargins(18, 8, 18, 8);
    titleLayout->setSpacing(2);
    titleLayout->addWidget(mLblTitle);
    titleLayout->addWidget(mLblChoose);
    bottomLayout->addWidget(titleCol, 0, Qt::AlignCenter);

    mBtnSkip = new QPushButton("跳过", bottomBar);
    mBtnSkip->setFixedSize(110, 56);
    QFont sf = mCNFont; sf.setPixelSize(20); sf.setBold(true);
    mBtnSkip->setFont(sf);
    mBtnSkip->setCursor(Qt::PointingHandCursor);
    mBtnSkip->setStyleSheet(
        "QPushButton { background:#666; color:white; border:none; border-radius:12px; }"
        "QPushButton:hover { background:#777; }"
        );
    connect(mBtnSkip, &QPushButton::clicked, this, &PackOpenWidget::onSkip);
    bottomLayout->addWidget(mBtnSkip, 0, Qt::AlignRight | Qt::AlignVCenter);

    root->addWidget(bottomBar);
}

void PackOpenWidget::open(const PackContent &content,
                          const QVector<CardData> &packHand,
                          const QVector<Consumable> &inventoryConsumables,
                          int freeJokerSlots)
{
    mContent = content;
    mPackHand = packHand;
    mInventoryConsumables = inventoryConsumables;
    mFreeJokerSlots = freeJokerSlots;
    mChoicesUsed = 0;
    mChosenOptions.clear();
    mSelectedHand.clear();
    mFinishing = false;

    show();
    raise();
    layoutPanel();
    refreshAll();
    QTimer::singleShot(0, this, &PackOpenWidget::animateCardsIn);
}

void PackOpenWidget::setPackHand(const QVector<CardData> &packHand)
{
    mPackHand = packHand;
    mSelectedHand.erase(std::remove_if(mSelectedHand.begin(), mSelectedHand.end(), [this](int idx) {
        return idx < 0 || idx >= mPackHand.size();
    }), mSelectedHand.end());
    refreshAll();
}

void PackOpenWidget::setInventoryConsumables(const QVector<Consumable> &inventoryConsumables)
{
    mInventoryConsumables = inventoryConsumables;
    refreshInventoryUi();
    refreshOptionUi();
}

void PackOpenWidget::setFreeJokerSlots(int freeSlots)
{
    mFreeJokerSlots = qMax(0, freeSlots);
    refreshOptionUi();
}

void PackOpenWidget::refreshAll()
{
    int remain = qMax(0, mContent.choicesAllowed - mChoicesUsed);
    mLblTitle->setText(packDisplayName(mContent.kind, mContent.size));
    mLblChoose->setText(QString("选择 %1 / %2　　剩余 %3 次")
                            .arg(mChoicesUsed)
                            .arg(mContent.choicesAllowed)
                            .arg(remain));
    if (mHandBox) mHandBox->setVisible(packUsesHandSelection());
    refreshHandUi();
    refreshOptionUi();
    refreshInventoryUi();
}

int PackOpenWidget::optionCount() const
{
    switch (mContent.kind) {
    case PackKind::Standard:  return mContent.standardCards.size();
    case PackKind::Buffoon:   return mContent.jokers.size();
    case PackKind::Arcana:
    case PackKind::Celestial:
    case PackKind::Spectral:  return mContent.consumables.size();
    }
    return 0;
}

void PackOpenWidget::layoutPackHand()
{
    if (!mHandBox) return;
    if (!packUsesHandSelection()) return;

    const int n = qMin(mPackHand.size(), mHandUi.size());
    const int areaW = qMax(1, mHandBox->width() > 0 ? mHandBox->width() : mPanel->width() - 44);
    const int maxTotal = qMax(120, areaW - 24);

    int cardW = 116;
    int cardH = 156;
    int step = (n > 1) ? qMin(68, (maxTotal - cardW) / qMax(1, n - 1)) : 0;
    if (n > 1 && step < 42) {
        // 在窄屏/十张临时手牌时，按原版手牌区缩小并增加重叠，保证整排不出界。
        cardW = qBound(82, int(maxTotal / (1.0 + 0.52 * (n - 1))), 116);
        cardH = qMax(112, int(cardW * 190.0 / 142.0));
        step = qMin(int(cardW * 0.58), (maxTotal - cardW) / qMax(1, n - 1));
        step = qMax(32, step);
    }

    const int totalW = n <= 0 ? 0 : cardW + qMax(0, n - 1) * step;
    const int startX = qMax(8, (areaW - totalW) / 2);
    const int baseY = 18;
    mHandBox->setFixedHeight(qBound(148, cardH + 34, 190));

    for (int i = 0; i < mHandUi.size(); ++i) {
        HandUi &hu = mHandUi[i];
        if (!hu.btn) continue;
        if (i >= n) {
            hu.btn->hide();
            if (hu.nameLbl) hu.nameLbl->hide();
            continue;
        }
        const bool selected = mSelectedHand.contains(i);
        const int lift = selected ? 22 : 0;
        hu.btn->setIndex(i);
        hu.btn->setFixedSize(cardW, cardH);
        hu.btn->move(startX + i * step, baseY - lift);
        hu.btn->setPixmap(renderPlayingCard(mPackHand[i], QSize(cardW - 4, cardH - 4)));
        hu.btn->setSelected(selected);
        double t = n > 0 ? (-n / 2.0 - 0.5 + (i + 1)) / double(n) : 0.0;
        hu.btn->setBaseRotation(0.2 * t * 180.0 / 3.14159265358979323846);
        hu.btn->show();
        hu.btn->raise();
        if (hu.nameLbl) hu.nameLbl->hide();
    }
}

void PackOpenWidget::refreshHandUi()
{
    if (!packUsesHandSelection()) {
        for (HandUi &hu : mHandUi) {
            if (hu.btn) hu.btn->hide();
            if (hu.nameLbl) hu.nameLbl->hide();
        }
        return;
    }
    layoutPackHand();
}

void PackOpenWidget::refreshOptionUi()
{
    int total = optionCount();
    for (int i = 0; i < mOptUi.size(); ++i) {
        OptUi &ou = mOptUi[i];
        if (i >= total) {
            ou.card->hide();
            continue;
        }
        ou.card->show();
        ou.imageLbl->setPixmap(renderOption(i));
        ou.nameLbl->setText(optionName(i));
        ou.descLbl->setText(optionDesc(i));

        bool chosen = optionAlreadyChosen(i);
        ou.takeBtn->setText(chosen ? "已选" : (mContent.kind == PackKind::Arcana || mContent.kind == PackKind::Spectral ? "使用" : "选择"));
        ou.takeBtn->setEnabled(!mFinishing && !chosen && mChoicesUsed < mContent.choicesAllowed && optionAvailableFor(i));
    }
}

void PackOpenWidget::refreshInventoryUi()
{
    if (mInventoryBox) mInventoryBox->hide();
    for (int i = 0; i < mInvUi.size(); ++i) {
        InvUi &iu = mInvUi[i];
        if (i >= mInventoryConsumables.size()) {
            iu.card->hide();
            continue;
        }
        iu.card->show();
        const Consumable &c = mInventoryConsumables[i];
        iu.imageLbl->setPixmap(ConsumableItem::renderPixmap(c.type, c.negative).scaled(iu.imageLbl->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        iu.nameLbl->setText(c.name);
        iu.useBtn->setToolTip(c.description);
        iu.useBtn->setEnabled(!mFinishing && inventoryAvailableFor(i));
    }
}

QPixmap PackOpenWidget::renderOption(int i) const
{
    const QSize size(126, 168);

    if (mContent.kind == PackKind::Buffoon) {
        QPixmap sheet(":/textures/images/Jokers.png");
        if (sheet.isNull()) return QPixmap();
        QPoint xy = JokerItem::spritePos(mContent.jokers[i]);
        QPixmap raw = sheet.copy(xy.x() * JokerItem::WIDTH, xy.y() * JokerItem::HEIGHT,
                                 JokerItem::WIDTH, JokerItem::HEIGHT);
        return raw.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    if (mContent.kind == PackKind::Arcana || mContent.kind == PackKind::Celestial || mContent.kind == PackKind::Spectral) {
        return renderConsumable(mContent.consumables[i], size);
    }

    return renderPlayingCard(mContent.standardCards[i], size);
}

QPixmap PackOpenWidget::renderConsumable(ConsumableType type, const QSize &size) const
{
    return ConsumableItem::renderPixmap(type).scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QPixmap PackOpenWidget::renderPlayingCard(const CardData &c, const QSize &size) const
{
    constexpr int W = ConsumableItem::WIDTH, H = ConsumableItem::HEIGHT;
    QPixmap deckSheet(":/textures/images/8BitDeck.png");
    QPixmap enhSheet (":/textures/images/Enhancers.png");
    QPixmap pix(W, H); pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.setRenderHint(QPainter::Antialiasing, true);

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
    p.end();

    if (c.edition != Edition::None)
        pix = BalatroShaders::renderEditionPixmap(pix, c.edition);

    int sCol = -1, sRow = 0;
    switch (c.seal) {
    case Seal::Gold:   sCol = 2; sRow = 0; break;
    case Seal::Purple: sCol = 4; sRow = 4; break;
    case Seal::Red:    sCol = 5; sRow = 4; break;
    case Seal::Blue:   sCol = 6; sRow = 4; break;
    default: break;
    }
    if (sCol >= 0 && !enhSheet.isNull()) {
        QPixmap seal = enhSheet.copy(sCol*W, sRow*H, W, H);
        if (c.seal == Seal::Gold) seal = BalatroShaders::renderGoldSealPixmap(seal, 0.95);
        QPainter sealPainter(&pix);
        sealPainter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        sealPainter.drawPixmap(QRect(0, 0, W, H), seal);
    }

    if (c.isDebuffed)
        pix = BalatroShaders::renderDebuffedPixmap(pix, 0.95);

    return pix.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QString PackOpenWidget::optionName(int i) const
{
    switch (mContent.kind) {
    case PackKind::Standard:  return mContent.standardCards[i].toString();
    case PackKind::Buffoon:   return createJoker(mContent.jokers[i]).name;
    case PackKind::Arcana:
    case PackKind::Celestial:
    case PackKind::Spectral:  return createConsumable(mContent.consumables[i]).name;
    }
    return "";
}

QString PackOpenWidget::optionDesc(int i) const
{
    if (mContent.kind == PackKind::Standard) {
        const CardData &c = mContent.standardCards[i];
        QStringList parts;
        switch (c.enhancement) {
        case Enhancement::Bonus: parts << "Bonus +30 筹码"; break;
        case Enhancement::Mult:  parts << "Mult +4 倍率"; break;
        case Enhancement::Wild:  parts << "Wild 任意花色"; break;
        case Enhancement::Glass: parts << "Glass ×2"; break;
        case Enhancement::Steel: parts << "Steel 持有 ×1.5"; break;
        case Enhancement::Stone: parts << "Stone +50"; break;
        case Enhancement::Gold:  parts << "Gold 通关 +$3"; break;
        case Enhancement::Lucky: parts << "Lucky 概率"; break;
        default: break;
        }
        switch (c.edition) {
        case Edition::Foil:        parts << "Foil"; break;
        case Edition::Holographic: parts << "Holo"; break;
        case Edition::Polychrome:  parts << "Poly"; break;
        default: break;
        }
        switch (c.seal) {
        case Seal::Gold:   parts << "金印章"; break;
        case Seal::Red:    parts << "红印章"; break;
        case Seal::Blue:   parts << "蓝印章"; break;
        case Seal::Purple: parts << "紫印章"; break;
        default: break;
        }
        return parts.isEmpty() ? "加入牌组" : parts.join("\n");
    }
    if (mContent.kind == PackKind::Buffoon)
        return createJoker(mContent.jokers[i]).description;
    return createConsumable(mContent.consumables[i]).description;
}

bool PackOpenWidget::packUsesHandSelection() const
{
    return mContent.kind == PackKind::Arcana || mContent.kind == PackKind::Spectral;
}

bool PackOpenWidget::selectionValidFor(const Consumable &c) const
{
    int n = packUsesHandSelection() ? mSelectedHand.size() : 0;
    if (n < c.needsSelection) return false;
    if (c.maxSelection > 0 && n > c.maxSelection) return false;
    return true;
}

bool PackOpenWidget::optionAvailableFor(int i) const
{
    if (i < 0 || i >= optionCount()) return false;
    switch (mContent.kind) {
    case PackKind::Standard:
        return true;
    case PackKind::Buffoon:
        return mFreeJokerSlots > 0;
    case PackKind::Celestial:
        return true;    // 行星牌从包里直接使用，不占仓库槽，也不显示临时手牌
    case PackKind::Arcana:
    case PackKind::Spectral: {
        Consumable c = createConsumable(mContent.consumables[i]);
        return selectionValidFor(c);
    }
    }
    return false;
}

bool PackOpenWidget::inventoryAvailableFor(int i) const
{
    if (i < 0 || i >= mInventoryConsumables.size()) return false;
    const Consumable &c = mInventoryConsumables[i];
    return selectionValidFor(c);
}

int PackOpenWidget::maxCurrentSelectionLimit() const
{
    if (!packUsesHandSelection()) return 0;
    int limit = 0;
    for (int i = 0; i < optionCount(); ++i) {
        if (optionAlreadyChosen(i)) continue;
        if (mContent.kind == PackKind::Arcana || mContent.kind == PackKind::Spectral) {
            Consumable c = createConsumable(mContent.consumables[i]);
            if (c.maxSelection > 0) limit = qMax(limit, c.maxSelection);
        }
    }
    for (const Consumable &c : mInventoryConsumables) {
        if (c.maxSelection > 0) limit = qMax(limit, c.maxSelection);
    }
    return qMax(1, qMin(5, limit));
}

void PackOpenWidget::applyPackHandOrderMove(int from, int to)
{
    if (from < 0 || from >= mPackHand.size() || to < 0 || to >= mPackHand.size() || from == to) return;
    CardData moved = mPackHand.takeAt(from);
    mPackHand.insert(to, moved);

    QVector<int> newSel;
    for (int s : mSelectedHand) {
        int ns = s;
        if (s == from) ns = to;
        else if (from < to && s > from && s <= to) ns = s - 1;
        else if (from > to && s >= to && s < from) ns = s + 1;
        if (!newSel.contains(ns)) newSel.append(ns);
    }
    std::sort(newSel.begin(), newSel.end());
    mSelectedHand = newSel;
    emit packHandReordered(mPackHand);
}

void PackOpenWidget::onHandCardDragged(int idx, QPoint localPos)
{
    if (!packUsesHandSelection() || idx < 0 || idx >= mPackHand.size()) return;
    const int n = qMin(mPackHand.size(), mHandUi.size());
    if (n <= 1) return;
    int to = 0;
    for (int i = 0; i < n; ++i) {
        if (!mHandUi[i].btn || !mHandUi[i].btn->isVisible()) continue;
        double center = mHandUi[i].btn->geometry().center().x();
        if (localPos.x() > center) to = i;
    }
    to = qBound(0, to, n - 1);
    if (to == idx) return;
    applyPackHandOrderMove(idx, to);
    layoutPackHand();
}

void PackOpenWidget::onHandCardReleased(int, QPoint)
{
    layoutPackHand();
}

void PackOpenWidget::onHandCardClicked(int idx)
{
    if (!packUsesHandSelection()) return;
    if (idx < 0 || idx >= mPackHand.size()) return;
    if (mSelectedHand.contains(idx)) {
        mSelectedHand.removeAll(idx);
    } else {
        int limit = qMax(1, maxCurrentSelectionLimit());
        if (mSelectedHand.size() >= limit) mSelectedHand.removeFirst();
        mSelectedHand.append(idx);
        std::sort(mSelectedHand.begin(), mSelectedHand.end());
    }
    refreshAll();
}

void PackOpenWidget::onChoose(int idx)
{
    if (mFinishing) return;
    if (idx < 0 || idx >= optionCount()) return;
    if (optionAlreadyChosen(idx)) return;
    if (!optionAvailableFor(idx)) return;

    emit choiceMade(idx, mSelectedHand);

    mChosenOptions.append(idx);
    ++mChoicesUsed;
    if (mContent.kind == PackKind::Buffoon && mFreeJokerSlots > 0) --mFreeJokerSlots;

    mSelectedHand.clear();

    if (mChoicesUsed >= mContent.choicesAllowed) {
        // 塔罗/幻灵包使用后先让玩家看到牌面增强、印章或版本变化，再退出二级界面。
        mFinishing = true;
        refreshAll();
        mLblChoose->setText("已使用，正在收起...");
        QTimer::singleShot((mContent.kind == PackKind::Arcana || mContent.kind == PackKind::Spectral) ? 1000 : 350,
                           this, &PackOpenWidget::finishAndClose);
    } else {
        refreshAll();
    }
}

void PackOpenWidget::onUseInventory(int idx)
{
    if (mFinishing) return;
    if (!inventoryAvailableFor(idx)) return;
    emit inventoryConsumableRequested(idx, mSelectedHand);
    mSelectedHand.clear();
    refreshAll();
}

void PackOpenWidget::onSkip()
{
    finishAndClose();
}

void PackOpenWidget::finishAndClose()
{
    mFinishing = true;
    hide();
    emit packFinished();
}

void PackOpenWidget::animateCardsIn()
{
    auto animateWidget = [this](QWidget *w, const QPoint &offset, int delay) {
        if (!w || !w->isVisible()) return;
        QPoint end = w->pos();
        w->move(end + offset);
        QTimer::singleShot(delay, this, [w, end]() {
            if (!w) return;
            auto *anim = new QPropertyAnimation(w, "pos", w);
            anim->setDuration(320);
            anim->setStartValue(w->pos());
            anim->setEndValue(end);
            anim->setEasingCurve(QEasingCurve::OutCubic);
            anim->start(QAbstractAnimation::DeleteWhenStopped);
        });
    };

    int d = 0;
    for (const HandUi &hu : mHandUi) {
        if (hu.btn && hu.btn->isVisible()) {
            animateWidget(hu.btn, QPoint(420, 120), d);
            d += 35;
        }
    }
    d = 60;
    for (const OptUi &ou : mOptUi) {
        if (ou.card && ou.card->isVisible()) {
            animateWidget(ou.card, QPoint(0, 120), d);
            d += 45;
        }
    }
}

void PackOpenWidget::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    layoutPanel();
}

void PackOpenWidget::layoutPanel()
{
    if (!mPanel) return;
    const int maxW = qMax(860, width() - 18);
    const int maxH = qMax(520, height() - 24);
    int panelW = qBound(860, int(width() * 0.88), qMin(1180, maxW));
    int panelH = qBound(520, int(height() * 0.80), qMin(740, maxH));
    mPanel->resize(panelW, panelH);
    int x = (width()  - mPanel->width())  / 2;
    int y = qMax(8, (height() - mPanel->height()) / 2);
    mPanel->move(x, y);
    layoutPackHand();
}
#include "packopenwidget.moc"
