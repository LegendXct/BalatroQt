#include "packopenwidget.h"
#include "../card/carditem.h"
#include "../card/jokeritem.h"
#include "../card/consumableitem.h"
#include "../utils/shadereffects.h"
#include "balatroinfopanel.h"
#include "cardtooltipformat.h"
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
#include <QEvent>
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
    // 整体上移并压缩纵向间距：让放大后的塔罗牌选项卡（含“选择”按钮）完整落在可见区域内。
    root->setContentsMargins(18, 8, 18, 10);
    root->setSpacing(4);

    mLblTitle = new QLabel("打开包", mPanel);
    QFont tf = mCNFont; tf.setPixelSize(24); tf.setBold(true);
    mLblTitle->setFont(tf);
    mLblTitle->setStyleSheet("color:#f0c040; background:transparent;");
    mLblTitle->setAlignment(Qt::AlignCenter);

    mLblChoose = new QLabel("", mPanel);
    QFont cf = mCNFont; cf.setPixelSize(22);
    mLblChoose->setFont(cf);
    mLblChoose->setStyleSheet("color:white; background:transparent;");
    mLblChoose->setAlignment(Qt::AlignCenter);

    // ── 临时手牌区:QGraphicsView + QGraphicsScene + CardItem ──
    // 关键：mHandView 不再放进 VBoxLayout 的 240px 槽里——而是作为 mPanel 的子部件
    // 全尺寸覆盖整个面板，这样拖牌时卡片可以在面板任意位置可见，不再被原本的小条
    // 剪裁（对齐用户反馈3）。其它子部件（标题/选项/按钮）位于 mPanel 内并依靠
    // raise() 排在 mHandView 之上。透明 spacer 占位以维持原 VBox 布局高度。
    mHandScene = new QGraphicsScene(this);
    mHandView  = new QGraphicsView(mHandScene, mPanel);
    mHandView->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    mHandView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mHandView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mHandView->setFrameShape(QFrame::NoFrame);
    mHandView->setStyleSheet("background: transparent;");
    mHandView->setAttribute(Qt::WA_TranslucentBackground);
    mHandView->viewport()->setAttribute(Qt::WA_TranslucentBackground);
    mHandView->setMouseTracking(true);
    // 占位：让 VBoxLayout 仍然在中间留出 240px 高度。spacer 设为 WA_TransparentForMouseEvents
    // 让点击直接穿透到下方（mHandView 上层 raise 时为它，非 raise 时为 mPanel 自身/无）。
    auto *handSlotSpacer = new QWidget(mPanel);
    handSlotSpacer->setObjectName("handSlotSpacer");
    handSlotSpacer->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    handSlotSpacer->setStyleSheet("background: transparent;");
    handSlotSpacer->setFixedHeight(240);
    handSlotSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    root->addWidget(handSlotSpacer);

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
    optionsLayout->setContentsMargins(8, 2, 8, 2);
    optionsLayout->setSpacing(8);
    // 选项卡靠上对齐：紧贴上方手牌，富余的空间留到卡片下方，确保“选择”按钮可见可点。
    optionsLayout->setAlignment(Qt::AlignHCenter | Qt::AlignTop);

    for (int i = 0; i < 5; ++i) {
        OptUi ou;
        ou.card = new QWidget(optionsBox);
        // 卡下方原本有一行 nameLbl 文本（"黑桃J / 太空人 / 木星"）——按用户反馈去掉，
        // 卡片整体下移，让默认状态看起来更贴近原版 booster 包内"卡居中略偏下"的布局。
        // 用户反馈：148×198 还是显得偏大；再缩一档到接近图集原始 142×190 的尺寸 134×180，
        // 容器按比例 148×226。
        ou.card->setFixedSize(148, 226);
        ou.card->setStyleSheet("background:transparent; border:none;");
        ou.card->setAttribute(Qt::WA_Hover, true);
        ou.card->installEventFilter(this);

        // 用绝对定位：默认状态 card 图垂直略偏下（restImageY > 居中），
        // 点击聚焦时整体上移让出底部空间给"选择/使用"小按钮——对齐原版
        // card_focus_button 的 align="bm" 视觉。
        constexpr int kImageW = 134, kImageH = 180;
        constexpr int kBtnH   = 26;
        constexpr int kBtnW   = 88;
        // restImageY：让卡更靠下，留出顶部空间给 info 浮窗 hover 锚定。
        const int restImageY = ou.card->height() - kImageH - 6;
        // liftImageY：上移 28 px 让出底部空间给按钮。
        const int liftImageY = qMax(2, restImageY - 28);

        ou.imageLbl = new QLabel(ou.card);
        ou.imageLbl->setAlignment(Qt::AlignCenter);
        ou.imageLbl->setStyleSheet("background:transparent;");
        ou.imageLbl->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        ou.card->setCursor(Qt::PointingHandCursor);
        ou.imageRestRect = QRect((ou.card->width() - kImageW) / 2, restImageY, kImageW, kImageH);
        ou.imageLiftRect = QRect((ou.card->width() - kImageW) / 2, liftImageY, kImageW, kImageH);
        ou.imageLbl->setGeometry(ou.imageRestRect);

        // nameLbl 保留但永远 hide()——原本的字号 / 几何字段都不再使用，
        // 留着是为了让 OptUi 的字段保持原结构，省掉 refresh 处的 if 判空。
        ou.nameLbl = new QLabel("", ou.card);
        ou.nameLbl->hide();
        ou.nameRestRect = QRect();
        ou.nameLiftRect = QRect();

        ou.takeBtn = new QPushButton("选择", ou.card);
        QFont bf = mCNFont; bf.setPixelSize(14); bf.setBold(true);
        ou.takeBtn->setFont(bf);
        ou.takeBtn->setCursor(Qt::PointingHandCursor);
        // 对齐原版 button_callbacks.lua: can_use_consumeable / can_select_card——
        //   使用（消耗类）= G.C.RED  #fe5f55
        //   选择（小丑 / 标准包） = G.C.GREEN #4BC292
        // 禁用统一是 G.C.UI.BACKGROUND_INACTIVE = #666666。
        // 具体颜色在 refreshOptionUi / setOptionFocused 根据 PackKind 切换。默认用 GREEN，
        // 这里只设字体 / 圆角 / emboss 风格，颜色由 styleSheet 动态注入。
        ou.takeBtn->setObjectName("packTakeBtn");
        ou.takeBtn->setGeometry((ou.card->width() - kBtnW) / 2,
                                liftImageY + kImageH - 2, kBtnW, kBtnH);
        ou.takeBtn->hide();
        connect(ou.takeBtn, &QPushButton::clicked, this, [this, i]() { onChoose(i); });

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
    // 之前 64 太矮——包名 + "选择 N/M 剩余 X 次"挤在一起。给到 88 让两行有呼吸。
    bottomBar->setFixedHeight(88);
    auto *bottomLayout = new QHBoxLayout(bottomBar);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(10);

    bottomLayout->addSpacing(96 + 10);
    bottomLayout->addStretch(1);

    auto *titleCol = new QWidget(bottomBar);
    titleCol->setStyleSheet("background:rgba(20,25,30,120); border-radius:12px;");
    auto *titleLayout = new QVBoxLayout(titleCol);
    // padding 拉大让两行字距离舒展些，对齐原版底部 "Open this pack" + "Select 1 of 5" 的视觉重量。
    titleLayout->setContentsMargins(20, 10, 20, 10);
    titleLayout->setSpacing(4);
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
    if (mFocusedOptIdx >= 0) {
        setOptionFocused(mFocusedOptIdx, false, false);
        mFocusedOptIdx = -1;
    }
    // 关键修复（问题4）：之前如果上一次开包走 finalChoice 把 mBtnSkip 禁用，
    // 重新打开时没有重置，遇到小丑槽满的小丑包就出现"选项灰、跳过也灰"，
    // 用户只能卖小丑才能继续。这里强制把所有按钮恢复可用状态。
    if (mBtnSkip) mBtnSkip->setEnabled(true);
    for (OptUi &ou : mOptUi)
        if (ou.takeBtn) ou.takeBtn->setEnabled(true);

    show();
    raise();
    layoutPanel();
    refreshAll();
    layoutPackHand(-1, /*instant=*/true);
    startPackReveal();
}

void PackOpenWidget::setPackHand(const QVector<CardData> &packHand)
{
    mPackHand = packHand;
    mSelectedHand.erase(std::remove_if(mSelectedHand.begin(), mSelectedHand.end(),
                                       [this](int idx) { return idx < 0 || idx >= mPackHand.size(); }),
                        mSelectedHand.end());
    if (mFinishing && (mContent.kind == PackKind::Buffoon || mContent.kind == PackKind::Standard))
        return;
    refreshAll();
}

void PackOpenWidget::setInventoryConsumables(const QVector<Consumable> &inv)
{
    mInventoryConsumables = inv;
    if (mFinishing && (mContent.kind == PackKind::Buffoon || mContent.kind == PackKind::Standard))
        return;
    refreshInventoryUi();
    refreshOptionUi();
}

void PackOpenWidget::setFreeJokerSlots(int freeSlots)
{
    mFreeJokerSlots = qMax(0, freeSlots);
    if (mFinishing && mContent.kind == PackKind::Buffoon)
        return;
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
    if (!packUsesHandSelection()) {
        // 该包不使用临时手牌（如小丑包 / 行星包），清掉残留 CardItem 即可。
        for (CardItem *c : mPackHandItems) {
            mHandScene->removeItem(c);
            c->deleteLater();
        }
        mPackHandItems.clear();
        return;
    }

    // 按 uid 匹配已有 CardItem：让"塔罗/幻灵打增强"走 flip 翻面动画而不是 destroy+recreate；
    // 之前每次 setPackHand 都会把整列 CardItem 删掉重建，看起来就是"闪一下然后从远处飞回来"。
    // 主场景 mainwindow.cpp::refreshHand() 也走同一种 uid 匹配，这里和它对齐。
    QHash<int, CardItem*> existingByUid;
    for (CardItem *c : mPackHandItems)
        existingByUid.insert(c->cardData().uid, c);

    QVector<CardItem*> reordered;
    QVector<CardItem*>  toFlip;
    QVector<CardData>   flipNewData;

    for (int i = 0; i < mPackHand.size(); ++i) {
        const CardData &nc = mPackHand[i];
        auto it = existingByUid.find(nc.uid);
        if (it != existingByUid.end()) {
            CardItem *item = it.value();
            existingByUid.erase(it);

            // 视觉相关字段任一变化都触发 flip。
            const CardData &oc = item->cardData();
            const bool visualChanged = (oc.enhancement != nc.enhancement)
                                    || (oc.edition     != nc.edition)
                                    || (oc.seal        != nc.seal)
                                    || (oc.suit        != nc.suit)
                                    || (oc.rank        != nc.rank)
                                    || (oc.isDebuffed  != nc.isDebuffed)
                                    || (oc.permanentBonusChips != nc.permanentBonusChips);
            if (visualChanged) {
                toFlip.append(item);
                flipNewData.append(nc);
            }
            reordered.append(item);
        } else {
            auto *card = new CardItem(nc);
            mHandScene->addItem(card);
            connect(card, &CardItem::clicked,
                    this, &PackOpenWidget::onPackCardClicked);
            connect(card, &CardItem::dragMoved,
                    this, &PackOpenWidget::onPackCardDragMoved);
            connect(card, &CardItem::dragReleased,
                    this, &PackOpenWidget::onPackCardDragReleased);
            connect(card, &CardItem::hoverChanged,
                    this, &PackOpenWidget::onPackCardHoverChanged);
            reordered.append(card);
        }
    }

    // 旧手牌里现在没有的 uid（被摧毁的牌 / 倒吊人 / Death 等）：从场景里移除。
    for (auto it = existingByUid.begin(); it != existingByUid.end(); ++it) {
        mHandScene->removeItem(it.value());
        it.value()->deleteLater();
    }
    mPackHandItems = reordered;

    // 触发"翻到背面 → 换数据 → 翻回正面"的双段翻面，对齐 mainwindow.cpp
    // 的塔罗/幻灵节奏：单次 flip() 只是 1→0→1 的 240ms 收缩+扩张，并且在中点
    // 把 faceUp 切换到背面。如果只调一次，setCardData() 会因为 keepFaceUp
    // 保留住"背面"状态，导致永远翻不回来——正是用户反馈的问题1。
    for (int k = 0; k < toFlip.size(); ++k) {
        CardItem *target = toFlip[k];
        const CardData newData = flipNewData[k];
        target->flip();
        QPointer<CardItem> guard(target);
        // 1) 翻到背面的中点（≈120ms）切换数据，setCardData 保留 faceUp=false。
        QTimer::singleShot(120, this, [guard, newData]() {
            if (guard) guard->setCardData(newData);
        });
        // 2) 等首次 flip 整段（240ms）+ 短暂停顿后再翻回正面。
        QTimer::singleShot(300, this, [guard]() {
            if (guard) guard->flip();
        });
    }

    layoutPackHand();
}

void PackOpenWidget::onPackCardHoverChanged(CardItem *card, bool hovered)
{
    if (hovered) showHandCardTooltip(card);
    else hideTooltip();
}

bool PackOpenWidget::eventFilter(QObject *obj, QEvent *e)
{
    // 选项卡（QWidget）上 Enter/Leave 进出时显隐描述浮窗；点击则切换聚焦态。
    if (e->type() == QEvent::Enter || e->type() == QEvent::Leave
        || e->type() == QEvent::MouseButtonPress) {
        for (int i = 0; i < mOptUi.size(); ++i) {
            if (mOptUi[i].card == obj) {
                if (e->type() == QEvent::Enter) {
                    showOptionTooltip(i);
                    // 与槽位 / 商店里的 JokerItem hover 一致的"弹一下"feedback——给开包里的
                    // 选项卡也加上视觉抖动。focus 中的选项跳过，避免几何动画冲突。
                    if (mFocusedOptIdx != i && i < optionCount() && !optionAlreadyChosen(i)
                        && mOptUi[i].imageLbl) {
                        QLabel *img = mOptUi[i].imageLbl;
                        const QRect base = img->geometry();
                        const QRect up   = base.translated(0, -5);
                        auto *seq = new QSequentialAnimationGroup(img);
                        auto *go = new QPropertyAnimation(img, "geometry", seq);
                        go->setDuration(70);
                        go->setStartValue(base);
                        go->setEndValue(up);
                        go->setEasingCurve(QEasingCurve::OutQuad);
                        auto *back = new QPropertyAnimation(img, "geometry", seq);
                        back->setDuration(150);
                        back->setStartValue(up);
                        back->setEndValue(base);
                        back->setEasingCurve(QEasingCurve::OutBack);
                        seq->addAnimation(go);
                        seq->addAnimation(back);
                        seq->start(QAbstractAnimation::DeleteWhenStopped);
                    }
                }
                else if (e->type() == QEvent::Leave) hideTooltip();
                else if (e->type() == QEvent::MouseButtonPress) {
                    if (i < optionCount() && !optionAlreadyChosen(i) && !mFinishing) {
                        if (mFocusedOptIdx == i) {
                            setOptionFocused(i, false, true);
                            mFocusedOptIdx = -1;
                        } else {
                            if (mFocusedOptIdx >= 0)
                                setOptionFocused(mFocusedOptIdx, false, true);
                            mFocusedOptIdx = i;
                            setOptionFocused(i, true, true);
                        }
                    }
                }
                break;
            }
        }
    }
    return QWidget::eventFilter(obj, e);
}

void PackOpenWidget::setOptionFocused(int idx, bool focused, bool animate)
{
    if (idx < 0 || idx >= mOptUi.size()) return;
    OptUi &ou = mOptUi[idx];
    if (!ou.card) return;

    auto animateLabel = [animate](QLabel *lbl, const QRect &target) {
        if (!lbl) return;
        if (!animate) { lbl->setGeometry(target); return; }
        auto *anim = new QPropertyAnimation(lbl, "geometry", lbl);
        anim->setDuration(160);
        anim->setStartValue(lbl->geometry());
        anim->setEndValue(target);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    };

    animateLabel(ou.imageLbl, focused ? ou.imageLiftRect : ou.imageRestRect);
    // nameLbl 现在常 hide()——不再动画它的位置，省掉一段 animation。

    if (focused) {
        // 只在仍可选时才弹按钮；不可选状态下保持按钮隐藏，避免误点。
        bool enabled = !mFinishing
                       && mChoicesUsed < mContent.choicesAllowed
                       && optionAvailableFor(idx);
        ou.takeBtn->setEnabled(enabled);
        ou.takeBtn->setText((mContent.kind == PackKind::Arcana
                              || mContent.kind == PackKind::Spectral
                              || mContent.kind == PackKind::Celestial)
                                ? QStringLiteral("使用")
                                : QStringLiteral("选择"));
        // 按按钮类型挑色：使用类（消耗品）= G.C.RED，选择类（小丑/扑克）= G.C.GREEN。
        const bool isUseBtn = (mContent.kind == PackKind::Arcana
                               || mContent.kind == PackKind::Spectral
                               || mContent.kind == PackKind::Celestial);
        const QString bg = isUseBtn ? "#fe5f55" : "#4BC292";   // G.C.RED / G.C.GREEN
        const QString bgHover = isUseBtn ? "#ff7065" : "#5fd1a0";
        const QString bgDown  = isUseBtn ? "#c64a42" : "#3a9870";
        ou.takeBtn->setStyleSheet(QString(
            "QPushButton { background:%1; color:white;"
            "  border: 1px solid %3;"
            "  border-bottom: 3px solid %3;"
            "  border-radius: 6px; padding: 3px 12px 1px 12px; }"
            "QPushButton:hover { background:%2; }"
            "QPushButton:disabled { background:#666666; color:#cccccc; border-color:#4f4f4f; }"
        ).arg(bg, bgHover, bgDown));
        ou.takeBtn->show();
        ou.takeBtn->raise();
    } else {
        ou.takeBtn->hide();
    }
}

void PackOpenWidget::showOptionTooltip(int idx)
{
    if (idx < 0 || idx >= optionCount()) return;
    if (idx >= mOptUi.size() || !mOptUi[idx].card) return;
    mHoveredOptIdx = idx;

    if (!mInfoTooltip) {
        mInfoTooltip = new BalatroInfoPanel(mCNFont, this);
        mInfoTooltip->hide();
        mInfoTooltip->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }

    QVector<BalatroInfoPanel::Badge> badges;
    QString name;
    QString bodyHtml;
    bool playingCardStyle = false;
    int tooltipW = 175;

    switch (mContent.kind) {
    case PackKind::Arcana:
        name = optionName(idx);
        bodyHtml = CardTooltipFormat::fromLuaMarkup(optionDesc(idx));
        badges.append({QStringLiteral("塔罗牌"), BalatroInfoPanel::tarotPillColor()});
        break;
    case PackKind::Celestial:
        name = optionName(idx);
        bodyHtml = CardTooltipFormat::fromLuaMarkup(optionDesc(idx));
        badges.append({QStringLiteral("行星牌"), BalatroInfoPanel::planetPillColor()});
        break;
    case PackKind::Spectral:
        name = optionName(idx);
        bodyHtml = CardTooltipFormat::fromLuaMarkup(optionDesc(idx));
        badges.append({QStringLiteral("幻灵牌"), BalatroInfoPanel::spectralPillColor()});
        break;
    case PackKind::Buffoon:
        if (idx < mContent.jokers.size()) {
            Joker tmp = createJoker(mContent.jokers[idx]);
            name = tmp.name;
            bodyHtml = CardTooltipFormat::fromLuaMarkup(tmp.description);
            const JokerRarity rr = jokerRarity(mContent.jokers[idx]);
            badges.append({CardTooltipFormat::rarityName(rr),
                           CardTooltipFormat::rarityColor(rr)});
        }
        break;
    case PackKind::Standard:
        // Standard pack：与主场景手牌悬浮相同——标题只放花色+点数，描述用 cardBodyHtml
        // 内联（基础筹码 / 增强加成 / edition / 蜡封 全部带色），不再走 optionDesc 的
        // "Foil/Holo/Poly" 缩写。
        if (idx < mContent.standardCards.size()) {
            const CardData &c = mContent.standardCards[idx];
            name = CardTooltipFormat::cardTitleHtml(c);
            bodyHtml = CardTooltipFormat::cardBodyHtml(c);
            playingCardStyle = true;
            tooltipW = 160;
        }
        break;
    }

    mInfoTooltip->setContent(name, bodyHtml, badges, tooltipW, playingCardStyle);

    // 定位到选项卡顶部上方居中。坐标需要从 ou.card 的 parent 转到本 widget。
    QWidget *card = mOptUi[idx].card;
    QPoint topLeftInThis = card->mapTo(this, QPoint(0, 0));
    int x = topLeftInThis.x() + (card->width() - mInfoTooltip->width()) / 2;
    int y = topLeftInThis.y() - mInfoTooltip->height() - 8;
    // 越界保护
    if (x < 6) x = 6;
    if (x + mInfoTooltip->width() > width() - 6) x = width() - mInfoTooltip->width() - 6;
    // 之前 y < 6 会把信息框翻到卡牌下方——超级塔罗包 5 选 2 时第 4 张就会撞上这条边界
    // 出现"上下不齐"。改成 clamp 到顶部，所有 hover 信息一律在卡牌上方，UI 统一。
    if (y < 6) y = 6;
    mInfoTooltip->move(x, y);
    mInfoTooltip->raise();
    mInfoTooltip->show();
}

void PackOpenWidget::showHandCardTooltip(CardItem *card)
{
    if (!card || !mHandView) return;
    int idx = mPackHandItems.indexOf(card);
    if (idx < 0 || idx >= mPackHand.size()) return;

    if (!mInfoTooltip) {
        mInfoTooltip = new BalatroInfoPanel(mCNFont, this);
        mInfoTooltip->hide();
        mInfoTooltip->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }

    const CardData &c = mPackHand[idx];

    QVector<BalatroInfoPanel::Badge> badges;
    switch (c.edition) {
    case Edition::Foil:        badges.append({QStringLiteral("镀膜"), BalatroInfoPanel::editionPillColor()}); break;
    case Edition::Holographic: badges.append({QStringLiteral("全息"), BalatroInfoPanel::editionPillColor()}); break;
    case Edition::Polychrome:  badges.append({QStringLiteral("多彩"), BalatroInfoPanel::editionPillColor()}); break;
    default: break;
    }
    switch (c.seal) {
    case Seal::Gold:   badges.append({QStringLiteral("金印章"), BalatroInfoPanel::sealPillColor(0)}); break;
    case Seal::Red:    badges.append({QStringLiteral("红印章"), BalatroInfoPanel::sealPillColor(1)}); break;
    case Seal::Blue:   badges.append({QStringLiteral("蓝印章"), BalatroInfoPanel::sealPillColor(2)}); break;
    case Seal::Purple: badges.append({QStringLiteral("紫印章"), BalatroInfoPanel::sealPillColor(3)}); break;
    default: break;
    }

    // 与主场景 / 牌组查看 hover 共用同一份 helper——确保塔罗/幻灵包临时手牌
    // 上的牌与主场景手牌呈现完全一致。
    mInfoTooltip->setContent(CardTooltipFormat::cardTitleHtml(c),
                             CardTooltipFormat::cardBodyHtml(c),
                             badges, 160, /*nameHasWhiteBox=*/true);

    // 锚到卡片头部正上方：把 scene 坐标映射到 view，再映射到本 widget。
    QPointF scenePos = card->scenePos();
    QPoint viewPt = mHandView->mapFromScene(scenePos);
    QPoint topLeftInThis = mHandView->mapTo(this, viewPt);
    int x = topLeftInThis.x() + (CardItem::WIDTH - mInfoTooltip->width()) / 2;
    int y = topLeftInThis.y() - mInfoTooltip->height() - 6;
    if (x < 6) x = 6;
    if (x + mInfoTooltip->width() > width() - 6) x = width() - mInfoTooltip->width() - 6;
    if (y < 6) y = topLeftInThis.y() + CardItem::HEIGHT + 6;
    mInfoTooltip->move(x, y);
    mInfoTooltip->raise();
    mInfoTooltip->show();
}

void PackOpenWidget::hideTooltip()
{
    if (mInfoTooltip) mInfoTooltip->hide();
    mHoveredOptIdx = -1;
}

void PackOpenWidget::layoutPackHand(int skipIdx, bool instant)
{
    if (!mHandView || !packUsesHandSelection()) return;

    int n = mPackHandItems.size();
    if (n == 0) return;

    // 视图整体缩放：开包临时手牌在视觉上比主场景再小一档，避免开包面板里的卡片
    // 看起来"太大"挤压下方的选项区。布局计算仍在场景坐标里做，sceneRect 取 viewport / scale。
    constexpr double kPackHandViewScale = 0.78;
    mHandView->resetTransform();
    mHandView->scale(kPackHandViewScale, kPackHandViewScale);
    int viewW = qMax(1, mHandView->viewport()->width());
    int viewH = qMax(1, mHandView->viewport()->height());
    int areaW = int(viewW / kPackHandViewScale);
    int areaH = int(viewH / kPackHandViewScale);
    mHandScene->setSceneRect(0, 0, areaW, areaH);

    int available = areaW - 80;
    int step = (n > 1) ? (available - CardItem::WIDTH) / (n - 1) : 0;
    step = qMin(step, CardItem::WIDTH - 30);
    if (step < 30) step = 30;
    int totalW = (n - 1) * step + CardItem::WIDTH;
    int startX = qMax(8, (areaW - totalW) / 2);
    // mHandView 现在覆盖整个 mPanel；卡片视觉位置仍要落在原"上方第二行"的
    // 旧手牌槽位置——把 mPanel 坐标系下槽位中心换算到场景坐标。
    // 槽位 = panel 内 (titleH 8+24=32 ~ +240)，中心约 32 + 120 = 152。
    int slotCenterPanelY = 32 + 120;
    int slotCenterSceneY = int(slotCenterPanelY / kPackHandViewScale);
    int baseY = qMax(int(CardItem::HEIGHT * 0.30), slotCenterSceneY - CardItem::HEIGHT / 2);

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

    // 拖拽期间把 mHandView 提到 mPanel 顶层，让卡片在整个面板范围内都可见、
    // 不再被原手牌槽下方的选项卡 /底部栏视觉遮挡。释放时再 lower 回去恢复点击。
    if (mHandView) mHandView->raise();

    // 拖拽布局需要在场景坐标系算，因此沿用 layoutPackHand 的缩放后逻辑宽。
    constexpr double kPackHandViewScale = 0.78;
    int areaW = int(qMax(1, mHandView->viewport()->width()) / kPackHandViewScale);
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

    // baseY 与 layoutPackHand 一致：定位到 mPanel 上"原手牌槽"的中心。
    int slotCenterPanelY = 32 + 120;
    int slotCenterSceneY = int(slotCenterPanelY / kPackHandViewScale);
    int baseY = qMax(int(CardItem::HEIGHT * 0.30), slotCenterSceneY - CardItem::HEIGHT / 2);

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

    constexpr double kPackHandViewScale = 0.78;
    int areaW = int(qMax(1, mHandView->viewport()->width()) / kPackHandViewScale);
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
    // 释放后把 mHandView 退回 mPanel 底层，让选项 / 跳过按钮恢复可点击。
    if (mHandView) mHandView->lower();
}

void PackOpenWidget::refreshOptionUi()
{
    int total = optionCount();
    for (int i = 0; i < mOptUi.size(); ++i) {
        OptUi &ou = mOptUi[i];
        if (i >= total) { ou.card->hide(); continue; }

        bool chosen = optionAlreadyChosen(i);
        // 原版开包时，被选择的那张牌本体飞走，原位置变空。
        // 之前 refreshAll() 会把 onChoose() 里 hide() 掉的源卡重新 show()，
        // 导致"小丑包/标准包原地留一张，另一个贴图飞走"的双贴图问题。
        if (chosen) {
            ou.card->hide();
            if (mFocusedOptIdx == i) mFocusedOptIdx = -1;
            continue;
        }

        ou.card->show();
        ou.imageLbl->setPixmap(renderOption(i));
        ou.nameLbl->setText(optionName(i));
        // 描述不再直接显示在卡上——hover 时通过 BalatroInfoPanel 浮窗给出，对齐原版样式。

        // 默认按钮隐藏（只在被点击聚焦时弹出）；如果当前是聚焦选项，根据可用性刷新按钮。
        const bool isFocused = (mFocusedOptIdx == i);
        ou.takeBtn->setText((mContent.kind == PackKind::Arcana
                              || mContent.kind == PackKind::Spectral
                              || mContent.kind == PackKind::Celestial)
                                ? QStringLiteral("使用")
                                : QStringLiteral("选择"));
        ou.takeBtn->setEnabled(!mFinishing
                               && mChoicesUsed < mContent.choicesAllowed
                               && optionAvailableFor(i));
        ou.takeBtn->setVisible(isFocused);
        if (isFocused) ou.takeBtn->raise();
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
    // 与 imageLbl 实际尺寸（134×180）一致——比槽位略小一档，对齐用户对开包
    // 卡片"还是偏大"的反馈。比例仍接近图集原始 142×190 = ~0.747 长宽比。
    const QSize size(134, 180);

    if (mContent.kind == PackKind::Buffoon) {
        QPixmap sheet(":/textures/images/Jokers.png");
        if (sheet.isNull()) return QPixmap();
        QPoint xy = JokerItem::spritePos(mContent.jokers[i]);
        // Jokers.png 每格固定 142×190——必须使用 SRC_W / SRC_H 采样；之前用 WIDTH/HEIGHT
        // (170×228 显示尺寸) 会按错误的步长切图，导致每张小丑里粘上隔壁单元的边缘。
        QPixmap raw = sheet.copy(xy.x() * JokerItem::SRC_W, xy.y() * JokerItem::SRC_H,
                                 JokerItem::SRC_W, JokerItem::SRC_H);
        // 演示模式可以通过 pack.jokerEditions 给单张小丑挂版本（多彩/闪箔/镭射/负片）——
        // 包内 hover 时就应该看到 shader 效果，不能等买入后才显示。
        Edition ed = Edition::None;
        if (i < mContent.jokerEditions.size()) ed = mContent.jokerEditions[i];
        if (ed != Edition::None) raw = BalatroShaders::renderEditionPixmap(raw, ed);
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

    const QVector<int> selectedHandBeforeChoice = mSelectedHand;

    int animationTarget = 0;
    if (mContent.kind == PackKind::Buffoon) animationTarget = 1;       // 飞入小丑槽
    else if (mContent.kind == PackKind::Standard) animationTarget = 3; // 翻成背面飞入右下牌堆

    QPixmap flyPixmap;
    QPoint flyCenter;
    if (animationTarget != 0 && idx < mOptUi.size() && mOptUi[idx].imageLbl) {
        flyPixmap = renderOption(idx);
        flyCenter = mOptUi[idx].imageLbl->mapToGlobal(
            QPoint(mOptUi[idx].imageLbl->width() / 2, mOptUi[idx].imageLbl->height() / 2));
        // 先隐藏源卡，再启动顶层飞行动画和数据刷新；这样不会出现源卡原地重绘一帧，
        // 小丑包选择时也少一次视觉卡顿。
        if (mOptUi[idx].card) mOptUi[idx].card->hide();
    }
    // 已被选择的卡片不再保持聚焦态，其它卡片在 refreshOptionUi() 后保持当前聚焦不动。
    if (mFocusedOptIdx == idx) {
        setOptionFocused(idx, false, false);
        mFocusedOptIdx = -1;
    }

    mChosenOptions.append(idx);
    ++mChoicesUsed;
    if (mContent.kind == PackKind::Buffoon && mFreeJokerSlots > 0) --mFreeJokerSlots;

    const bool finalChoice = (mChoicesUsed >= mContent.choicesAllowed);
    if (finalChoice) mFinishing = true;

    if (animationTarget != 0 && !flyPixmap.isNull())
        emit choiceAnimationRequested(flyPixmap, flyCenter, animationTarget);

    emit choiceMade(idx, selectedHandBeforeChoice);

    mSelectedHand.clear();

    if (finalChoice) {
        mLblChoose->setText("已使用，正在收起...");
        for (OptUi &ou : mOptUi) {
            if (ou.takeBtn) ou.takeBtn->setEnabled(false);
        }
        if (mBtnSkip) mBtnSkip->setEnabled(false);

        // 小丑包/标准包选择后正在播放飞入动画，避免立刻 refreshAll() 重绘整组选项，
        // 否则低配机器会在动画第一帧出现卡顿；Arcana/Spectral 仍保留原刷新逻辑。
        if (mContent.kind != PackKind::Buffoon && mContent.kind != PackKind::Standard)
            refreshAll();

        int closeDelay = 350;
        if (mContent.kind == PackKind::Arcana || mContent.kind == PackKind::Spectral)
            closeDelay = 1000;
        else if (mContent.kind == PackKind::Standard)
            closeDelay = 640;   // 标准包翻成背面飞往牌堆，动画更长
        else if (mContent.kind == PackKind::Buffoon)
            closeDelay = 720;   // 等小丑飞入槽位后再关包，避免关闭时打断动画
        QTimer::singleShot(closeDelay, this, &PackOpenWidget::finishAndClose);
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
    hideTooltip();
    // 先通知 MainWindow 把商店 / 盲注选择界面抬起来，再隐藏开包层。
    // 之前先 hide() 会露出一帧底层暗背景，小丑包选择后看起来像黑屏卡顿。
    emit packFinished();
    hide();
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

    // 手牌区：CardItem 从手牌槽位中心稍上方飞向各自目标，并伴随 opacity 渐入。
    constexpr double kPackHandViewScale = 0.78;
    int areaW = mHandView ? int(mHandView->viewport()->width()  / kPackHandViewScale) : 1024;
    int slotCenterSceneY = int((32 + 120) / kPackHandViewScale);
    QPointF burstFrom(areaW / 2.0, slotCenterSceneY - 200.0);
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
    QPoint c = packSpritePos(mContent.kind, mContent.size, mContent.spriteVariant);
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
        animateCardsIn();
        QPointer<PackOpenWidget> g(this);
        QTimer::singleShot(520, this, [g]() {
            if (g) g->endPackReveal();
        });
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

    // mHandView 现在是 mPanel 的全尺寸子级；默认 lower 到 mPanel 内最底层，
    // 这样选项/标题/按钮（依然在 mPanel 内）显示在 mHandView 之上、能够正常点击；
    // 拖牌时再 raise() 到顶（见 onPackCardDragMoved/onPackCardDragReleased）。
    if (mHandView) {
        mHandView->setGeometry(0, 0, mPanel->width(), mPanel->height());
        mHandView->lower();
    }
    if (mInfoTooltip) mInfoTooltip->raise();

    layoutPackHand();
}
