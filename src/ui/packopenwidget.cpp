#include "packopenwidget.h"
#include "../card/carditem.h"
#include "../card/jokeritem.h"
#include "../card/consumableitem.h"
#include "../utils/shadereffects.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QLabel>
#include <QPushButton>
#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QStringList>
#include <QTimer>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QSequentialAnimationGroup>
#include <QVariantAnimation>
#include <QGraphicsPixmapItem>
#include <QGraphicsOpacityEffect>
#include <QEasingCurve>
#include <QSizePolicy>
#include <QPointer>
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static bool consumableNeedsFreeJokerSlot(ConsumableType type)
{
    // 这些牌的唯一效果是生成小丑；满槽时原版不会允许使用。
    return type == ConsumableType::Tarot_Judgement
        || type == ConsumableType::Spectral_Wraith
        || type == ConsumableType::Spectral_Soul;
}

PackOpenWidget::PackOpenWidget(const QFont &cnFont, const QFont &pixelFont,
                               QWidget *parent)
    : QWidget(parent), mCNFont(cnFont), mPixelFont(pixelFont)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet("background: transparent;");
    hide();
    buildUi();

    // 灵魂牌动效定时刷新
    mSoulAnimTimer = new QTimer(this);
    connect(mSoulAnimTimer, &QTimer::timeout, this, [this]() {
        if (!isVisible()) return;
        bool hasSoul = false;
        for (ConsumableType t : mContent.consumables)
            if (t == ConsumableType::Spectral_Soul) { hasSoul = true; break; }
        if (!hasSoul) {
            for (const Consumable &c : mInventoryConsumables)
                if (c.type == ConsumableType::Spectral_Soul) { hasSoul = true; break; }
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
    // 开包界面必须能在当前 playPage 内完整显示，不能再强制 880px 高。
    mPanel->setMinimumSize(820, 600);
    mPanel->setStyleSheet("background: transparent; border: none;");

    auto *root = new QVBoxLayout(mPanel);
    root->setContentsMargins(18, 12, 18, 12);
    root->setSpacing(8);

    mLblTitle = new QLabel("打开包", mPanel);
    QFont tf = mCNFont; tf.setPixelSize(24); tf.setBold(true);
    mLblTitle->setFont(tf);
    mLblTitle->setStyleSheet("color:#f0c040; background:transparent;");
    mLblTitle->setAlignment(Qt::AlignCenter);

    mLblChoose = new QLabel("", mPanel);
    QFont cf = mCNFont; cf.setPixelSize(15);
    mLblChoose->setFont(cf);
    mLblChoose->setStyleSheet("color:white; background:transparent;");
    mLblChoose->setAlignment(Qt::AlignCenter);

    // ── 临时手牌区:QGraphicsView + QGraphicsScene + CardItem ──
    mHandScene = new QGraphicsScene(this);
    mHandView  = new QGraphicsView(mHandScene, mPanel);
    mHandView->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    mHandView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mHandView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mHandView->setFrameShape(QFrame::NoFrame);
    mHandView->setStyleSheet("background: transparent;");
    mHandView->setAttribute(Qt::WA_TranslucentBackground);
    mHandView->viewport()->setAttribute(Qt::WA_TranslucentBackground);
    mHandView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    // 270：给下方选项卡 + 包名/跳过行留足空间，避免底部被裁。
    mHandView->setMinimumHeight(270);
    mHandView->setMouseTracking(true);
    root->addWidget(mHandView);

    // ── 中间:左侧包内牌选项,右侧仓库特殊牌 ──
    auto *midRow = new QWidget(mPanel);
    auto *midLayout = new QHBoxLayout(midRow);
    midLayout->setContentsMargins(0, 0, 0, 0);
    midLayout->setSpacing(10);

    auto *optionsBox = new QWidget(midRow);
    optionsBox->setObjectName("packOptionsBox");
    optionsBox->setAttribute(Qt::WA_StyledBackground, true);
    optionsBox->setStyleSheet("QWidget#packOptionsBox { background:transparent; border:none; }");
    auto *optionsLayout = new QHBoxLayout(optionsBox);
    optionsLayout->setContentsMargins(8, 6, 8, 6);
    optionsLayout->setSpacing(8);
    optionsLayout->setAlignment(Qt::AlignCenter);

    for (int i = 0; i < 5; ++i) {
        OptUi ou;
        ou.card = new QWidget(optionsBox);
        // 卡片图采样比例 142×190 ≈ 1:1.34；imageLbl 必须高度 ≥ 168 才能完整展示，
        // 否则像之前那样把 126×168 缩放成的 pixmap 塞进 126×148 的 QLabel，
        // 上下各被裁掉约 10px——表现就是塔罗 / 幻灵牌顶部和底部"少了一截"。
        ou.card->setFixedSize(160, 320);
        ou.card->setStyleSheet("background:transparent; border:none;");

        auto *vbl = new QVBoxLayout(ou.card);
        vbl->setContentsMargins(4, 4, 4, 4);
        vbl->setSpacing(4);

        ou.imageLbl = new QLabel(ou.card);
        ou.imageLbl->setFixedSize(140, 188);
        ou.imageLbl->setAlignment(Qt::AlignCenter);
        ou.imageLbl->setStyleSheet("background:transparent;");
        vbl->addWidget(ou.imageLbl, 0, Qt::AlignCenter);

        ou.nameLbl = new QLabel("", ou.card);
        QFont nf = mCNFont; nf.setPixelSize(13); nf.setBold(true);
        ou.nameLbl->setFont(nf);
        ou.nameLbl->setStyleSheet("color:white; background:transparent;");
        ou.nameLbl->setAlignment(Qt::AlignCenter);
        ou.nameLbl->setWordWrap(true);
        ou.nameLbl->setFixedHeight(36);
        vbl->addWidget(ou.nameLbl);

        ou.descLbl = new QLabel("", ou.card);
        QFont df = mCNFont; df.setPixelSize(10);
        ou.descLbl->setFont(df);
        ou.descLbl->setStyleSheet("color:#aab2ba; background:transparent;");
        ou.descLbl->setAlignment(Qt::AlignCenter);
        ou.descLbl->setWordWrap(true);
        ou.descLbl->setFixedHeight(44);
        vbl->addWidget(ou.descLbl);

        ou.takeBtn = new QPushButton("选择", ou.card);
        ou.takeBtn->setFixedHeight(34);
        QFont bf = mCNFont; bf.setPixelSize(13);
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
    invBox->setFixedWidth(190);
    invBox->setAttribute(Qt::WA_StyledBackground, true);
    invBox->setStyleSheet("QWidget#inventoryConsumableBox { background:#151b21; border-radius:10px; }");
    auto *ivbl = new QVBoxLayout(invBox);
    ivbl->setContentsMargins(8, 8, 8, 8);
    ivbl->setSpacing(5);

    auto *invTitle = new QLabel("仓库特殊牌", invBox);
    QFont itf = mCNFont; itf.setPixelSize(14); itf.setBold(true);
    invTitle->setFont(itf);
    invTitle->setAlignment(Qt::AlignCenter);
    invTitle->setStyleSheet("color:#f0c040; background:transparent;");
    ivbl->addWidget(invTitle);

    for (int i = 0; i < 4; ++i) {
        InvUi iu;
        iu.card = new QWidget(invBox);
        iu.card->setFixedHeight(64);
        iu.card->setStyleSheet("background:#222b33; border-radius:8px;");
        auto *hbl = new QHBoxLayout(iu.card);
        hbl->setContentsMargins(5, 5, 5, 5);
        hbl->setSpacing(5);

        iu.imageLbl = new QLabel(iu.card);
        iu.imageLbl->setFixedSize(36, 48);
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
        iu.useBtn->setFixedHeight(22);
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
    invBox->hide();

    root->addWidget(midRow, 1);

    // ── 底部:包名 + 跳过 ──
    // 用左右等长的 stretch 把"包名 + 剩余次数"夹在水平正中；跳过按钮挂在右侧但不影响中心对齐。
    auto *bottomBar = new QWidget(mPanel);
    bottomBar->setStyleSheet("background:transparent;");
    bottomBar->setFixedHeight(64);
    auto *bottomLayout = new QHBoxLayout(bottomBar);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(10);

    bottomLayout->addSpacing(96 + 10);
    bottomLayout->addStretch(1);

    auto *titleCol = new QWidget(bottomBar);
    titleCol->setStyleSheet("background:rgba(20,25,30,120); border-radius:12px;");
    auto *titleLayout = new QVBoxLayout(titleCol);
    titleLayout->setContentsMargins(14, 5, 14, 5);
    titleLayout->setSpacing(2);
    titleLayout->addWidget(mLblTitle);
    titleLayout->addWidget(mLblChoose);
    bottomLayout->addWidget(titleCol, 0, Qt::AlignCenter);

    bottomLayout->addStretch(1);

    mBtnSkip = new QPushButton("跳过", bottomBar);
    mBtnSkip->setFixedSize(96, 48);
    QFont sf = mCNFont; sf.setPixelSize(18); sf.setBold(true);
    mBtnSkip->setFont(sf);
    mBtnSkip->setCursor(Qt::PointingHandCursor);
    mBtnSkip->setStyleSheet(
        "QPushButton { background:#666; color:white; border:none; border-radius:12px; }"
        "QPushButton:hover { background:#777; }"
        );
    connect(mBtnSkip, &QPushButton::clicked, this, &PackOpenWidget::onSkip);
    bottomLayout->addWidget(mBtnSkip, 0, Qt::AlignVCenter);

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
    mLastDragTo = -1;

    show();
    raise();
    layoutPanel();
    refreshAll();
    QTimer::singleShot(0, this, [this]() {
        layoutPackHand(-1, /*instant=*/true);
        // 先把选项/手牌 hide() 等开包动画开壳后再炸出来。
        startPackReveal();
    });
}

void PackOpenWidget::setPackHand(const QVector<CardData> &packHand)
{
    mPackHand = packHand;
    mSelectedHand.erase(std::remove_if(mSelectedHand.begin(), mSelectedHand.end(),
                                       [this](int idx) { return idx < 0 || idx >= mPackHand.size(); }),
                        mSelectedHand.end());
    refreshAll();
}

void PackOpenWidget::setInventoryConsumables(const QVector<Consumable> &inv)
{
    mInventoryConsumables = inv;
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
                            .arg(mChoicesUsed).arg(mContent.choicesAllowed).arg(remain));
    if (mHandView) mHandView->setVisible(packUsesHandSelection());
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

void PackOpenWidget::refreshHandUi()
{
    // 清除旧 CardItem
    for (CardItem *c : mPackHandItems) {
        mHandScene->removeItem(c);
        c->deleteLater();
    }
    mPackHandItems.clear();

    if (!packUsesHandSelection()) return;

    // 创建新 CardItem
    for (int i = 0; i < mPackHand.size(); ++i) {
        auto *card = new CardItem(mPackHand[i]);
        mHandScene->addItem(card);
        mPackHandItems.append(card);

        connect(card, &CardItem::clicked,
                this, &PackOpenWidget::onPackCardClicked);
        connect(card, &CardItem::dragMoved,
                this, &PackOpenWidget::onPackCardDragMoved);
        connect(card, &CardItem::dragReleased,
                this, &PackOpenWidget::onPackCardDragReleased);
    }

    layoutPackHand();
}

void PackOpenWidget::layoutPackHand(int skipIdx, bool instant)
{
    if (!mHandView || !packUsesHandSelection()) return;

    int n = mPackHandItems.size();
    if (n == 0) return;

    int viewportH = qMax(1, mHandView->viewport()->height());
    // 让卡牌按视口高度自动缩放：留出 hover 抬升（26% 卡高）+ 顶部 hover 标签 (~78px) + 上下边距。
    // 单卡完整呈现需要：CardItem::HEIGHT * 1.26 + 78 + 16。把 view 整体 zoom 到这个比例。
    const double neededH = CardItem::HEIGHT * 1.26 + 78.0 + 16.0;
    const double scale = qBound(0.55, double(viewportH) / neededH, 1.0);
    mHandView->resetTransform();
    mHandView->scale(scale, scale);

    // 场景区在缩放下：以 scene 坐标计算，view 自动按 scale 收缩到 viewport 区域。
    // 用 viewport / scale 反推 scene 中可见区域大小。
    int areaW = qMax(1, int(mHandView->viewport()->width()  / scale));
    int areaH = qMax(1, int(mHandView->viewport()->height() / scale));
    mHandScene->setSceneRect(0, 0, areaW, areaH);

    int available = areaW - 80;
    int step = (n > 1) ? (available - CardItem::WIDTH) / (n - 1) : 0;
    step = qMin(step, CardItem::WIDTH - 30);
    if (step < 30) step = 30;
    int totalW = (n - 1) * step + CardItem::WIDTH;
    int startX = qMax(8, (areaW - totalW) / 2);
    // baseY 贴底布局，留出 16px 下边距；上方剩余空间够 hover 抬升 + 悬浮标签。
    int baseY = qMax(int(CardItem::HEIGHT * 0.30), areaH - CardItem::HEIGHT - 16);

    for (int i = 0; i < n; ++i) {
        if (i == skipIdx) continue;
        CardItem *c = mPackHandItems[i];
        if (!c) continue;

        bool sel = mSelectedHand.contains(i);
        double t = (-n / 2.0 - 0.5 + (i + 1)) / n;
        double angleDeg = 0.2 * t * 180.0 / M_PI;

        int x = startX + i * step;
        // 与主场景手牌一致：选中卡上提 26% 卡高（≈59px），动画感更接近原版。
        int y = baseY + (sel ? -CardItem::HEIGHT * 26 / 100 : 0);

        c->setBaseRotation(angleDeg);
        c->setZValue(i);   // ← 见下面问题 2
        c->setCardSelected(sel);

        if (instant) c->setPos(QPointF(x, y));   // ← 瞬时
        else         c->moveTo(QPointF(x, y), 180);
    }
}

void PackOpenWidget::onPackCardClicked(CardItem *card)
{
    if (!packUsesHandSelection()) return;
    int idx = mPackHandItems.indexOf(card);
    if (idx < 0 || idx >= mPackHand.size()) return;

    if (mSelectedHand.contains(idx)) {
        mSelectedHand.removeAll(idx);
    } else {
        // 和正式出牌手牌一致：选择区本身最多允许 5 张，具体塔罗/幻灵牌
        // 是否能使用由 selectionValidFor() 决定，不能在点击时强制把已选牌挤掉。
        // 否则死神、力量、世界/太阳/月亮等开包场景无法先自由选牌再决定使用哪张消耗牌。
        if (mSelectedHand.size() < qMin(5, mPackHand.size())) {
            mSelectedHand.append(idx);
            std::sort(mSelectedHand.begin(), mSelectedHand.end());
        }
    }
    layoutPackHand();
    refreshOptionUi();
    refreshInventoryUi();
}

void PackOpenWidget::onPackCardDragMoved(CardItem *card, QPointF scenePos)
{
    if (!packUsesHandSelection()) return;
    int from = mPackHandItems.indexOf(card);
    if (from < 0) return;
    int n = mPackHandItems.size();
    if (n <= 1) return;

    // 拖拽时 view 的缩放已由 layoutPackHand 设置好；这里直接用同样的 scale 反算。
    const double dragScale = mHandView->transform().m11() != 0.0 ? mHandView->transform().m11() : 1.0;
    int areaW = qMax(1, int(mHandView->viewport()->width() / dragScale));
    int available = areaW - 80;
    int step = (n > 1) ? (available - CardItem::WIDTH) / (n - 1) : 0;
    step = qMin(step, CardItem::WIDTH - 30);
    if (step < 30) step = 30;
    int totalW = (n - 1) * step + CardItem::WIDTH;
    int startX = qMax(8, (areaW - totalW) / 2);

    int to = 0;
    for (int i = 0; i < n; ++i) {
        double center = startX + i * step + CardItem::WIDTH / 2.0;
        if (scenePos.x() > center) to = i;
    }
    to = qBound(0, to, n - 1);

    if (to == mLastDragTo) {
        card->setZValue(600);
        return;
    }
    mLastDragTo = to;

    // 计算被拖卡片在视觉上"应该插入"的位置后,旁边的卡平滑滑到新位置
    QVector<CardItem*> visual = mPackHandItems;
    visual.removeAt(from);
    visual.insert(to, card);

    int areaH = qMax(1, int(mHandView->viewport()->height() / dragScale));
    int baseY = qMax(int(CardItem::HEIGHT * 0.30), areaH - CardItem::HEIGHT - 16);

    for (int vi = 0; vi < visual.size(); ++vi) {
        CardItem *ci = visual[vi];
        if (ci == card) continue;
        int realIdx = mPackHandItems.indexOf(ci);
        bool sel = mSelectedHand.contains(realIdx);
        double t = (-n / 2.0 - 0.5 + (vi + 1)) / n;
        double angleDeg = 0.2 * t * 180.0 / M_PI;
        int x = startX + vi * step;
        int y = baseY + (sel ? -CardItem::HEIGHT * 26 / 100 : 0);
        ci->setBaseRotation(angleDeg);
        ci->setZValue(vi);   // 10 + vi → vi
        ci->moveTo(QPointF(x, y), 80);
    }
    card->setZValue(600);
}

void PackOpenWidget::onPackCardDragReleased(CardItem *card, QPointF scenePos)
{
    mLastDragTo = -1;
    int from = mPackHandItems.indexOf(card);
    if (from < 0) { layoutPackHand(); return; }

    int n = mPackHandItems.size();
    if (n <= 1) { layoutPackHand(); return; }

    const double dragScale = mHandView->transform().m11() != 0.0 ? mHandView->transform().m11() : 1.0;
    int areaW = qMax(1, int(mHandView->viewport()->width() / dragScale));
    int available = areaW - 80;
    int step = (n > 1) ? (available - CardItem::WIDTH) / (n - 1) : 0;
    step = qMin(step, CardItem::WIDTH - 30);
    if (step < 30) step = 30;
    int totalW = (n - 1) * step + CardItem::WIDTH;
    int startX = qMax(8, (areaW - totalW) / 2);

    int to = 0;
    for (int i = 0; i < n; ++i) {
        double center = startX + i * step + CardItem::WIDTH / 2.0;
        if (scenePos.x() > center) to = i;
    }
    to = qBound(0, to, n - 1);

    if (from != to) {
        // 更新数据模型 + mSelectedHand 索引调整
        applyPackHandOrderMove(from, to);
        // 同步 mPackHandItems 顺序
        CardItem *moved = mPackHandItems.takeAt(from);
        mPackHandItems.insert(to, moved);
    }

    layoutPackHand();   // 所有卡(含刚拖的)动画归位
}

void PackOpenWidget::refreshOptionUi()
{
    int total = optionCount();
    for (int i = 0; i < mOptUi.size(); ++i) {
        OptUi &ou = mOptUi[i];
        if (i >= total) { ou.card->hide(); continue; }
        ou.card->show();
        ou.imageLbl->setPixmap(renderOption(i));
        ou.nameLbl->setText(optionName(i));
        ou.descLbl->setText(optionDesc(i));

        bool chosen = optionAlreadyChosen(i);
        ou.takeBtn->setText(chosen ? "已选" :
                                (mContent.kind == PackKind::Arcana || mContent.kind == PackKind::Spectral ? "使用" : "选择"));
        ou.takeBtn->setEnabled(!mFinishing && !chosen
                               && mChoicesUsed < mContent.choicesAllowed
                               && optionAvailableFor(i));
    }
}

void PackOpenWidget::refreshInventoryUi()
{
    if (mInventoryBox) mInventoryBox->hide();
    for (int i = 0; i < mInvUi.size(); ++i) {
        InvUi &iu = mInvUi[i];
        if (i >= mInventoryConsumables.size()) { iu.card->hide(); continue; }
        iu.card->show();
        const Consumable &c = mInventoryConsumables[i];
        iu.imageLbl->setPixmap(ConsumableItem::renderPixmap(c.type, c.negative)
                                   .scaled(iu.imageLbl->size(), Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation));
        iu.nameLbl->setText(c.name);
        iu.useBtn->setToolTip(c.description);
        iu.useBtn->setEnabled(!mFinishing && inventoryAvailableFor(i));
    }
}

QPixmap PackOpenWidget::renderOption(int i) const
{
    const QSize size(112, 150);

    if (mContent.kind == PackKind::Buffoon) {
        QPixmap sheet(":/textures/images/Jokers.png");
        if (sheet.isNull()) return QPixmap();
        QPoint xy = JokerItem::spritePos(mContent.jokers[i]);
        // Jokers.png 每格固定 142×190——必须使用 SRC_W / SRC_H 采样；之前用 WIDTH/HEIGHT
        // (170×228 显示尺寸) 会按错误的步长切图，导致每张小丑里粘上隔壁单元的边缘。
        QPixmap raw = sheet.copy(xy.x() * JokerItem::SRC_W, xy.y() * JokerItem::SRC_H,
                                 JokerItem::SRC_W, JokerItem::SRC_H);
        return raw.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    if (mContent.kind == PackKind::Arcana || mContent.kind == PackKind::Celestial
        || mContent.kind == PackKind::Spectral)
        return renderConsumable(mContent.consumables[i], size);
    return renderPlayingCard(mContent.standardCards[i], size);
}

QPixmap PackOpenWidget::renderConsumable(ConsumableType type, const QSize &size) const
{
    return ConsumableItem::renderPixmap(type)
    .scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QPixmap PackOpenWidget::renderPlayingCard(const CardData &c, const QSize &size) const
{
    // 按图集原始 142×190 切取，最后 scaled() 输出实际显示尺寸。
    constexpr int W = ConsumableItem::SRC_W, H = ConsumableItem::SRC_H;
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
    case PackKind::Standard:  return true;
    case PackKind::Buffoon:   return mFreeJokerSlots > 0;
    case PackKind::Celestial: return true;
    case PackKind::Arcana:
    case PackKind::Spectral: {
        Consumable c = createConsumable(mContent.consumables[i]);
        if (consumableNeedsFreeJokerSlot(c.type) && mFreeJokerSlots <= 0) return false;
        return selectionValidFor(c);
    }
    }
    return false;
}

bool PackOpenWidget::inventoryAvailableFor(int i) const
{
    if (i < 0 || i >= mInventoryConsumables.size()) return false;
    const Consumable &c = mInventoryConsumables[i];
    if (consumableNeedsFreeJokerSlot(c.type) && mFreeJokerSlots <= 0) return false;
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
    for (const Consumable &c : mInventoryConsumables)
        if (c.maxSelection > 0) limit = qMax(limit, c.maxSelection);
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
        mFinishing = true;
        refreshAll();
        mLblChoose->setText("已使用，正在收起...");
        QTimer::singleShot(
            (mContent.kind == PackKind::Arcana || mContent.kind == PackKind::Spectral) ? 1000 : 350,
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
    // 让 widget 同时滑入 + 渐入（如果它有 OpacityEffect 就一起淡入）。
    auto animateWidget = [this](QWidget *w, const QPoint &offset, int delay) {
        if (!w) return;
        QPoint end = w->pos();
        w->move(end + offset);
        QPointer<QWidget> guard(w);
        QTimer::singleShot(delay, this, [guard, end]() {
            if (!guard) return;
            QWidget *w = guard.data();
            auto *anim = new QPropertyAnimation(w, "pos", w);
            anim->setDuration(360);
            anim->setStartValue(w->pos());
            anim->setEndValue(end);
            anim->setEasingCurve(QEasingCurve::OutBack);
            anim->start(QAbstractAnimation::DeleteWhenStopped);

            if (auto *eff = qobject_cast<QGraphicsOpacityEffect*>(w->graphicsEffect())) {
                auto *fade = new QPropertyAnimation(eff, "opacity", w);
                fade->setDuration(280);
                fade->setStartValue(eff->opacity());
                fade->setEndValue(1.0);
                fade->setEasingCurve(QEasingCurve::OutCubic);
                fade->start(QAbstractAnimation::DeleteWhenStopped);
            }
        });
    };

    // 选项区：从中央向各自位置“喷射”——给每张卡一个朝向面板中心的反向位移。
    if (mPanel) {
        const QPoint panelCenter(mPanel->width() / 2, mPanel->height() / 2);
        int d = 30;
        for (const OptUi &ou : mOptUi) {
            if (!ou.card) continue;
            // ou.card 的父级未必是 mPanel；用全局坐标桥接以便算出中心方向。
            QWidget *parent = ou.card->parentWidget();
            const QPoint cardCenterInParent = ou.card->pos() + QPoint(ou.card->width()/2, ou.card->height()/2);
            const QPoint panelCenterInParent = parent ? parent->mapFrom(this, mPanel->pos() + panelCenter)
                                                       : panelCenter;
            QPoint dir = panelCenterInParent - cardCenterInParent;
            // 控制偏移强度：太大跳出可视区，太小看不到喷射。
            const double len = std::hypot(dir.x(), dir.y());
            if (len > 1.0) {
                const double k = qBound(0.45, 220.0 / len, 0.95);
                dir = QPoint(int(dir.x() * k), int(dir.y() * k));
            } else {
                dir = QPoint(0, 120);
            }
            animateWidget(ou.card, dir, d);
            d += 55;
        }
    }

    // 库存 / 跳过按钮：只渐入，不位移；位置已经由 layout 决定。
    auto fadeOnly = [this](QWidget *w, int delay) {
        if (!w) return;
        auto *eff = qobject_cast<QGraphicsOpacityEffect*>(w->graphicsEffect());
        if (!eff) return;
        QPointer<QGraphicsOpacityEffect> guard(eff);
        QTimer::singleShot(delay, this, [guard]() {
            if (!guard) return;
            auto *fade = new QPropertyAnimation(guard.data(), "opacity", guard.data());
            fade->setDuration(280);
            fade->setStartValue(guard->opacity());
            fade->setEndValue(1.0);
            fade->setEasingCurve(QEasingCurve::OutCubic);
            fade->start(QAbstractAnimation::DeleteWhenStopped);
        });
    };
    fadeOnly(mInventoryBox, 180);
    fadeOnly(mBtnSkip, 240);

    // 手牌区：CardItem 从 mPanel 中心位置朝各自目标飞，并伴随 opacity 渐入。
    int areaW = mHandView ? mHandView->viewport()->width()  : 800;
    int areaH = mHandView ? mHandView->viewport()->height() : 240;
    QPointF burstFrom(areaW / 2.0, areaH * 0.5 - 200.0);
    for (int i = 0; i < mPackHandItems.size(); ++i) {
        CardItem *c = mPackHandItems[i];
        if (!c) continue;
        QPointF target = c->pos();
        c->setPos(burstFrom);
        c->setOpacity(0.0);
        QPointer<CardItem> guard(c);
        QTimer::singleShot(60 + i * 40, this, [guard, target]() {
            if (!guard) return;
            CardItem *c = guard.data();
            c->setOpacity(1.0);
            c->moveTo(target, 360);
        });
    }
}

void PackOpenWidget::buildRevealOverlay()
{
    if (mRevealView) return;
    mRevealScene = new QGraphicsScene(this);
    mRevealView  = new QGraphicsView(mRevealScene, this);
    mRevealView->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    mRevealView->setFrameShape(QFrame::NoFrame);
    mRevealView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mRevealView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mRevealView->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    mRevealView->setAttribute(Qt::WA_TranslucentBackground, true);
    mRevealView->setStyleSheet("background: transparent; border: none;");
    mRevealView->setBackgroundBrush(Qt::NoBrush);
    mRevealView->viewport()->setAttribute(Qt::WA_TranslucentBackground, true);
    mRevealView->hide();

    mRevealDissolveTimer = new QTimer(this);
    mRevealDissolveTimer->setInterval(33); // ~30 fps
}

QPixmap PackOpenWidget::renderPackBigPixmap() const
{
    // 与 ShopWidget::offerPixmap(Pack) 同一份 booster.png 切片 + booster.fs 立体贴图。
    // 关键差异是这里要更大一些（≈ 280×376），并保留 alpha 以便后续 dissolve 平滑透。
    QPixmap sheet(":/textures/images/boosters.png");
    if (sheet.isNull()) return QPixmap();
    QPoint c = packSpritePos(mContent.kind, mContent.size);
    QPixmap base = sheet.copy(c.x() * ConsumableItem::SRC_W,
                              c.y() * ConsumableItem::SRC_H,
                              ConsumableItem::SRC_W, ConsumableItem::SRC_H);
    return BalatroShaders::makeBooster3DPixmap(base);
}

void PackOpenWidget::startPackReveal()
{
    buildRevealOverlay();

    // 让选项 / 临时手牌 / 库存 / 跳过按钮在 reveal 期间不可见，但不 hide()
    // 避免触发布局回流——只用 QGraphicsOpacityEffect 调透明度，动画结束再去除。
    auto attachFade = [](QWidget *w) {
        if (!w) return;
        auto *eff = qobject_cast<QGraphicsOpacityEffect*>(w->graphicsEffect());
        if (!eff) {
            eff = new QGraphicsOpacityEffect(w);
            w->setGraphicsEffect(eff);
        }
        eff->setOpacity(0.0);
    };
    for (auto &ou : mOptUi) attachFade(ou.card);
    attachFade(mInventoryBox);
    attachFade(mBtnSkip);
    for (CardItem *c : mPackHandItems) if (c) c->setOpacity(0.0);

    mRevealActive = true;
    mRevealPackBase = renderPackBigPixmap();
    if (mRevealPackBase.isNull()) {
        // 资源缺失，跳过动画直接进入正常状态。
        endPackReveal();
        return;
    }

    // 把 reveal view 覆盖在 mPanel 之上。
    const QRect panelRect = mPanel ? mPanel->geometry() : rect();
    mRevealView->setGeometry(panelRect);
    mRevealView->show();
    mRevealView->raise();
    mRevealScene->setSceneRect(0, 0, panelRect.width(), panelRect.height());

    // 清掉旧的 item / 动画。
    mRevealScene->clear();
    mRevealPackItem = mRevealScene->addPixmap(mRevealPackBase);

    // 设计目标尺寸：高度约占 panel 高 0.32 —— 原版 booster 包不会占满中心，
    // 之前 0.55 倍数把开包图标撑得太大，会跟选项卡 / 临时手牌位置打架。
    const double targetH = panelRect.height() * 0.32;
    const double srcH = qMax(1.0, double(mRevealPackBase.height()));
    const double targetScale = targetH / srcH;
    mRevealPackItem->setTransformationMode(Qt::SmoothTransformation);
    mRevealPackItem->setTransformOriginPoint(mRevealPackBase.width() / 2.0,
                                             mRevealPackBase.height() / 2.0);
    const QPointF center(panelRect.width() / 2.0 - mRevealPackBase.width() / 2.0,
                         panelRect.height() / 2.0 - mRevealPackBase.height() / 2.0);

    // 起始状态。
    mRevealPackItem->setScale(targetScale * 0.55);
    mRevealPackItem->setOpacity(0.0);
    mRevealPackItem->setRotation(0.0);
    mRevealPackItem->setPos(center + QPointF(0, panelRect.height() * 0.10));

    // 单根时间线：用一个 0→1 的 QVariantAnimation 推动整段开包序列。
    // QGraphicsPixmapItem 不是 QObject，没法直接 QPropertyAnimation；这里通过 valueChanged
    // 在回调里手动调 setScale/setRotation/setOpacity/setPos。
    //
    // 关键时间点（占总时长 1100ms 的比例）：
    //   0.00 – 0.30：浮现（位置 + scale + opacity）
    //   0.30 – 0.52：晃动（rotation 振幅约 ±7°）
    //   0.52 – 1.00：溶解 + 继续放大 + 渐出；同步把选项 / 手牌 / 库存渐入
    constexpr int kRevealMs = 1100;
    constexpr double kPhaseAppearEnd = 0.30;
    constexpr double kPhaseShakeEnd  = 0.52;
    bool *crackFired = new bool(false);
    QPointer<PackOpenWidget> guard(this);
    // QGraphicsPixmapItem 不是 QObject，没法直接用 QPointer 防悬空。
    // 我们靠 guard + endPackReveal() 把 mRevealPackItem 置 nullptr 来防止 use-after-free。
    auto *timeline = new QVariantAnimation(this);
    timeline->setDuration(kRevealMs);
    timeline->setStartValue(0.0);
    timeline->setEndValue(1.0);
    timeline->setEasingCurve(QEasingCurve::Linear);

    auto easeOutBack = [](double t) {
        const double s = 1.4;
        const double u = t - 1.0;
        return u * u * ((s + 1.0) * u + s) + 1.0;
    };
    auto easeOutCubic = [](double t) {
        const double u = 1.0 - t;
        return 1.0 - u * u * u;
    };
    auto easeInCubic  = [](double t) { return t * t * t; };

    mRevealDissolveT = 0.0;
    connect(timeline, &QVariantAnimation::valueChanged, this,
            [this, guard, crackFired, targetScale, center, panelRect,
             easeOutBack, easeOutCubic, easeInCubic, kPhaseAppearEnd, kPhaseShakeEnd]
            (const QVariant &v) {
        if (!guard || !mRevealPackItem) return;
        const double phase = v.toDouble();

        if (phase < kPhaseAppearEnd) {
            const double t = phase / kPhaseAppearEnd;
            const double tEase = easeOutBack(qBound(0.0, t, 1.0));
            mRevealPackItem->setScale(targetScale * (0.55 + 0.45 * tEase));
            mRevealPackItem->setOpacity(easeOutCubic(qMin(1.0, t * 1.2)));
            mRevealPackItem->setPos(center + QPointF(0, panelRect.height() * 0.10 * (1.0 - tEase)));
            mRevealPackItem->setRotation(0.0);
        } else if (phase < kPhaseShakeEnd) {
            const double t = (phase - kPhaseAppearEnd) / (kPhaseShakeEnd - kPhaseAppearEnd);
            mRevealPackItem->setScale(targetScale);
            mRevealPackItem->setOpacity(1.0);
            mRevealPackItem->setPos(center);
            // 两次正弦摆动（≈ ±7°），结束回到 0。
            mRevealPackItem->setRotation(std::sin(t * 6.28318530718) * 7.0);
        } else {
            const double t = (phase - kPhaseShakeEnd) / (1.0 - kPhaseShakeEnd);
            const double tEase = easeOutCubic(t);
            mRevealPackItem->setScale(targetScale * (1.0 + 0.22 * tEase));
            mRevealPackItem->setOpacity(1.0 - easeInCubic(t));
            mRevealPackItem->setRotation(0.0);

            // 进入阶段 C 的第一帧：启动 dissolve shader 定时器；
            // animateCardsIn() 推迟到 dissolve 接近完成时再触发，避免选项卡
            // 在包还没彻底散开就提前出现，跟用户反馈"开包动画没结束后面内容就出现了"对齐。
            if (!*crackFired) {
                *crackFired = true;
                if (mRevealDissolveTimer && !mRevealDissolveTimer->isActive()) {
                    mRevealDissolveT = 0.0;
                    mRevealDissolveTimer->start();
                }
                // dissolve 大概持续 460ms（见下方定时器），这里留 380ms 让包基本散完。
                QPointer<PackOpenWidget> g(this);
                QTimer::singleShot(380, this, [g]() {
                    if (g) g->animateCardsIn();
                });
            }
        }
    });

    // dissolve shader 定时器：每 ~33ms 把 mRevealPackBase 按当前 t 重新渲染。
    QObject::disconnect(mRevealDissolveTimer, nullptr, this, nullptr);
    connect(mRevealDissolveTimer, &QTimer::timeout, this, [this, guard]() {
        if (!guard || !mRevealPackItem || mRevealPackBase.isNull()) return;
        const double step = 33.0 / 460.0;   // dissolve 整段约 460ms
        mRevealDissolveT = qMin(1.0, mRevealDissolveT + step);
        QPixmap dp = BalatroShaders::renderDissolvePixmap(mRevealPackBase, mRevealDissolveT,
                                                          QColor(255, 222, 130, 200),
                                                          QColor(255, 110, 70, 160), 1.0);
        if (!dp.isNull()) mRevealPackItem->setPixmap(dp);
        if (mRevealDissolveT >= 1.0) mRevealDissolveTimer->stop();
    });

    connect(timeline, &QVariantAnimation::finished, this, [this, crackFired]() {
        delete crackFired;
        endPackReveal();
    });
    timeline->start(QAbstractAnimation::DeleteWhenStopped);
}

void PackOpenWidget::endPackReveal()
{
    mRevealActive = false;
    if (mRevealDissolveTimer && mRevealDissolveTimer->isActive())
        mRevealDissolveTimer->stop();
    if (mRevealView) {
        mRevealView->hide();
        if (mRevealScene) mRevealScene->clear();
        mRevealPackItem = nullptr;
    }
    // animateCardsIn 已经把 opacity 渐入到 1.0，这里把残留的 graphics effect 清掉，
    // 避免之后每次 paint 都走 effect 管线影响性能。
    auto detachFade = [](QWidget *w) {
        if (!w) return;
        if (auto *eff = qobject_cast<QGraphicsOpacityEffect*>(w->graphicsEffect())) {
            eff->setOpacity(1.0);
            w->setGraphicsEffect(nullptr);
        }
    };
    for (auto &ou : mOptUi) detachFade(ou.card);
    detachFade(mInventoryBox);
    detachFade(mBtnSkip);
    for (CardItem *c : mPackHandItems) if (c) c->setOpacity(1.0);
}

void PackOpenWidget::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    layoutPanel();
    if (mRevealView && mRevealActive && mPanel)
        mRevealView->setGeometry(mPanel->geometry());
}

void PackOpenWidget::layoutPanel()
{
    if (!mPanel) return;
    const int minW = qMin(width(),  820);
    const int minH = qMin(height(), 600);
    const int maxW = qMin(width()  - 12, 1180);
    const int maxH = qMin(height() - 12, 820);
    int panelW = qBound(minW, int(width()  * 0.96), qMax(minW, maxW));
    int panelH = qBound(minH, int(height() * 0.96), qMax(minH, maxH));
    mPanel->resize(panelW, panelH);
    int x = (width()  - mPanel->width())  / 2;
    int y = qMax(6, (height() - mPanel->height()) / 2);
    mPanel->move(x, y);
    layoutPackHand();
}
