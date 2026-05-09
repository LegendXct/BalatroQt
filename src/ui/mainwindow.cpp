#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QPainter>
#include <QFontDatabase>
#include <QGraphicsProxyWidget>
#include <algorithm>
#include <QTimer>
#include <QGuiApplication>
#include <QScreen>
#include <QMenuBar>
#include <QStatusBar>

void MainWindow::loadFonts() {
    int pid = QFontDatabase::addApplicationFont(":/fonts/fonts/m6x11plus.ttf");
    int cid = QFontDatabase::addApplicationFont(":/fonts/fonts/NotoSansSC-Bold.ttf");

    QString pixelFamily = QFontDatabase::applicationFontFamilies(pid).value(0, "Arial");
    QString cnFamily = QFontDatabase::applicationFontFamilies(cid).value(0, "Arial");

    mPixelFont = QFont(pixelFamily);
    mPixelFont.setStyleStrategy(QFont::NoAntialias);
    mCNFont = QFont(cnFamily);
}

static QPushButton *makeBtn(const QString &text, const QString &bg, const QString &hover, const QFont &font, QWidget *parent, int h = 50) {
    QPushButton *btn = new QPushButton(text, parent);
    btn->setFixedHeight(h);
    btn->setFont(font);
    btn->setStyleSheet(QString(
        "QPushButton {"
        " background: %1; color: white;"
        " border: none; border-radius: 8px; font-size: 16px;"
        "}"
        "QPushButton:hover { background: %2; }"
        "QPushButton:pressed { background: %2; }"
        "QPushButton:disabled { background: #333; color: #666; }"
    ).arg(bg, hover));
    return btn;
}

static QLabel *makeLabel(const QString &text, int px, const QString &color, const QFont &font, QWidget *parent) {
    QLabel *lbl = new QLabel(text, parent);
    lbl->setAlignment(Qt::AlignCenter);
    QFont f = font; f.setPixelSize(px);
    lbl->setFont(f);
    lbl->setStyleSheet(QString("color: %1;").arg(color));
    return lbl;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , mGameState(new GameState(this))
{
    ui->setupUi(this);
    loadFonts();

    menuBar()->hide();
    statusBar()->hide();

    QSize sg = QGuiApplication::primaryScreen()->geometry().size();
    mWinW = sg.width();
    mWinH = sg.height();
    mSceneW = mWinW - mLeftW;
    mSceneH = mWinH;

    // ── 左面板（永远显示）──
    setupLeftPanel();   // 创建 mLeftPanel,parent=nullptr,layout 接管

    // ── 右半边容器:绿色牌桌永远显示 ──
    mPlayPage = new QWidget;
    mPlayPage->setAttribute(Qt::WA_StyledBackground, true);
    mPlayPage->setStyleSheet("background: #2a3144;");
    setupScene();
    setupSceneButtons();
    {
        auto *l = new QVBoxLayout(mPlayPage);
        l->setContentsMargins(0, 0, 0, 0);
        l->addWidget(mView);
    }

    // ── 整体 central:左面板 + 右半边,横向并列 ──
    auto *container = new QWidget;
    auto *cl = new QHBoxLayout(container);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(0);
    cl->addWidget(mLeftPanel);
    cl->addWidget(mPlayPage, 1);
    setCentralWidget(container);

    // ── 所有 overlay 都挂在 mPlayPage 上,默认隐藏 ──
    mBlindSelectWidget = new BlindSelectWidget(mGameState, mCNFont, mPixelFont, mPlayPage);
    mBlindSelectWidget->hide();

    mShopWidget = new ShopWidget(mGameState, mCNFont, mPixelFont, mPlayPage);
    mShopWidget->hide();

    mRoundEndOverlay = new RoundEndOverlay(mCNFont, mPixelFont, mPlayPage);
    mRoundEndOverlay->hide();
    connect(mRoundEndOverlay, &RoundEndOverlay::nextClicked,
            this, &MainWindow::onNextBlindClicked);

    mPackOpenWidget = new PackOpenWidget(mCNFont, mPixelFont, mShopWidget);
    mPackOpenWidget->hide();
    connect(mPackOpenWidget, &PackOpenWidget::choiceMade,
            this, &MainWindow::onPackChoiceMade);

    setupConnections();
    // 让所有 overlay 跟着 mPlayPage 一起 resize
    mPlayPage->installEventFilter(this);
    mGameState->startGame();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupLeftPanel() {
    mLeftPanel = new QWidget;
    mLeftPanel->setFixedWidth(mLeftW);
    mLeftPanel->setStyleSheet("background: #1e2230;");

    QVBoxLayout *layout = new QVBoxLayout(mLeftPanel);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(6);

    // ── 上下文区(固定高度 200px,3 态切换) ──
    mContextArea = new QStackedWidget(mLeftPanel);
    mContextArea->setFixedHeight(200);
    mContextArea->setStyleSheet("background:transparent;");

    // 页面 0: BlindSelect
    mCtxBlindSelect = new QWidget;
    mCtxBlindSelect->setStyleSheet("background:transparent;");
    {
        auto *vl = new QVBoxLayout(mCtxBlindSelect);
        vl->setContentsMargins(0, 16, 0, 16);
        vl->setSpacing(2);
        vl->setAlignment(Qt::AlignCenter);

        QFont t1f = mCNFont; t1f.setPixelSize(28); t1f.setBold(true);

        QLabel *l1 = new QLabel("选择你的", mCtxBlindSelect);
        l1->setFont(t1f);
        l1->setStyleSheet("color:white; background:transparent;");
        l1->setAlignment(Qt::AlignCenter);
        vl->addWidget(l1);

        QLabel *l2 = new QLabel("下一个盲注", mCtxBlindSelect);
        l2->setFont(t1f);
        l2->setStyleSheet("color:white; background:transparent;");
        l2->setAlignment(Qt::AlignCenter);
        vl->addWidget(l2);
    }
    mContextArea->addWidget(mCtxBlindSelect);

    // 页面 1: Blind (对局阶段)
    mCtxBlind = new QWidget;
    mCtxBlind->setAttribute(Qt::WA_StyledBackground, true);
    mCtxBlind->setStyleSheet("background:#161b28; border-radius:8px;");
    {
        auto *hbl = new QHBoxLayout(mCtxBlind);
        hbl->setContentsMargins(10, 8, 10, 8);
        hbl->setSpacing(10);

        mCtxBlindChipImg = new QLabel(mCtxBlind);
        mCtxBlindChipImg->setFixedSize(80, 80);
        mCtxBlindChipImg->setAlignment(Qt::AlignCenter);
        mCtxBlindChipImg->setStyleSheet("background:transparent;");
        hbl->addWidget(mCtxBlindChipImg);

        auto *vbl = new QVBoxLayout;
        vbl->setContentsMargins(0, 0, 0, 0);
        vbl->setSpacing(2);

        mLblBlind = new QLabel("小盲注", mCtxBlind);
        QFont nbf = mCNFont; nbf.setPixelSize(15); nbf.setBold(true);
        mLblBlind->setFont(nbf);
        mLblBlind->setAlignment(Qt::AlignCenter);
        mLblBlind->setStyleSheet(
            "color:white; background:#3d70b8;"
            "border-radius:6px; padding:3px 8px;");
        mLblBlind->setFixedHeight(28);
        vbl->addWidget(mLblBlind);

        QLabel *tt = new QLabel("至少得分", mCtxBlind);
        QFont ttf = mCNFont; ttf.setPixelSize(11);
        tt->setFont(ttf);
        tt->setStyleSheet("color:#888; background:transparent;");
        tt->setAlignment(Qt::AlignCenter);
        vbl->addWidget(tt);

        mLblTarget = new QLabel("✳ 300", mCtxBlind);
        QFont tf = mPixelFont; tf.setPixelSize(24);
        mLblTarget->setFont(tf);
        mLblTarget->setStyleSheet("color:#e04040; background:transparent;");
        mLblTarget->setAlignment(Qt::AlignCenter);
        vbl->addWidget(mLblTarget);

        mLblReward = new QLabel("奖励 $$$", mCtxBlind);
        QFont rf = mCNFont; rf.setPixelSize(12);
        mLblReward->setFont(rf);
        mLblReward->setStyleSheet("color:#f0c040; background:transparent;");
        mLblReward->setAlignment(Qt::AlignCenter);
        vbl->addWidget(mLblReward);

        hbl->addLayout(vbl, 1);
    }
    mContextArea->addWidget(mCtxBlind);

    // 页面 2: Shop
    mCtxShop = new QWidget;
    mCtxShop->setAttribute(Qt::WA_StyledBackground, true);
    mCtxShop->setStyleSheet("background:#161b28; border:2px solid #c03030; border-radius:8px;");
    {
        auto *vl = new QVBoxLayout(mCtxShop);
        vl->setContentsMargins(0, 0, 0, 0);
        vl->setAlignment(Qt::AlignCenter);

        QLabel *shopLbl = new QLabel("SHOP", mCtxShop);
        QFont sf = mPixelFont; sf.setPixelSize(64); sf.setBold(true);
        shopLbl->setFont(sf);
        shopLbl->setStyleSheet("color:#f0c040; background:transparent; border:none;");
        shopLbl->setAlignment(Qt::AlignCenter);
        vl->addWidget(shopLbl);

        QLabel *sub = new QLabel("来变强吧!", mCtxShop);
        QFont subf = mCNFont; subf.setPixelSize(13);
        sub->setFont(subf);
        sub->setStyleSheet("color:white; background:transparent; border:none;");
        sub->setAlignment(Qt::AlignCenter);
        vl->addWidget(sub);
    }
    mContextArea->addWidget(mCtxShop);

    layout->addWidget(mContextArea);

    // 回合得分
    QWidget *scoreBox = new QWidget(mLeftPanel);
    scoreBox->setStyleSheet("background:#161b28; border-radius:8px;");
    QHBoxLayout *sbl = new QHBoxLayout(scoreBox);
    sbl->setContentsMargins(8, 4, 8, 4);
    QLabel *sTitle = makeLabel("回合\n分数", 12, "#aaa", mCNFont, scoreBox);
    mLblScore = makeLabel("✳ 0", 20, "#ffffff", mPixelFont, scoreBox);
    sbl->addWidget(sTitle);
    sbl->addWidget(mLblScore);
    layout->addWidget(scoreBox);

    // 筹码 × 倍率
    QWidget *chipsRow = new QWidget(mLeftPanel);
    QHBoxLayout *chipsLayout = new QHBoxLayout(chipsRow);
    chipsLayout->setContentsMargins(0, 0, 0, 0);
    chipsLayout->setSpacing(4);

    mLblChips = new QLabel("0", chipsRow);
    mLblChips->setAlignment(Qt::AlignCenter);
    QFont cf = mPixelFont; cf.setPixelSize(30);
    mLblChips->setFont(cf);
    mLblChips->setStyleSheet(
        "background: #3060c0; color: white;"
        "border-radius: 8px; padding: 4px 8px;"
        );

    QLabel *lblX = new QLabel("×", chipsRow);
    lblX->setAlignment(Qt::AlignCenter);
    QFont xf = mCNFont; xf.setPixelSize(24);
    lblX->setFont(xf);
    lblX->setStyleSheet("color: white;");
    lblX->setFixedWidth(28);

    mLblMult = new QLabel("0", chipsRow);
    mLblMult->setAlignment(Qt::AlignCenter);
    mLblMult->setFont(cf);
    mLblMult->setStyleSheet(
        "background:#c03030; color:white;"
        "border-radius:8px; padding:4px 8px;"
        );

    chipsLayout->addWidget(mLblChips);
    chipsLayout->addWidget(lblX);
    chipsLayout->addWidget(mLblMult);
    layout->addWidget(chipsRow);

    // 比赛信息 + 出牌/弃牌次数
    QWidget *infoRow = new QWidget(mLeftPanel);
    QHBoxLayout *ibl = new QHBoxLayout(infoRow);
    ibl->setContentsMargins(0, 0, 0, 0);
    ibl->setSpacing(6);

    QPushButton *btnInfo = makeBtn("比赛\n信息", "#c03030", "#a02020", mCNFont, infoRow, 64);
    btnInfo->setFixedWidth(70);

    QWidget *countersBox = new QWidget(infoRow);
    QVBoxLayout *cbv = new QVBoxLayout(countersBox);
    cbv->setContentsMargins(0, 0, 0, 0);
    cbv->setSpacing(2);

    QWidget *hRow = new QWidget(countersBox);
    QHBoxLayout *hbl = new QHBoxLayout(hRow);
    hbl->setContentsMargins(0, 0, 0, 0);
    QLabel *hTitle = makeLabel("出牌", 12, "#aaa", mCNFont, hRow);
    mLblHands = makeLabel("4", 20, "#4a90d9", mPixelFont, hRow);
    hbl->addWidget(hTitle);
    hbl->addWidget(mLblHands);

    QWidget *dRow = new QWidget(countersBox);
    QHBoxLayout *dbl = new QHBoxLayout(dRow);
    dbl->setContentsMargins(0, 0, 0, 0);
    QLabel *dTitle = makeLabel("弃牌", 12, "#aaa", mCNFont, dRow);
    mLblDiscards = makeLabel("3", 20, "#e07030", mPixelFont, dRow);
    dbl->addWidget(dTitle);
    dbl->addWidget(mLblDiscards);

    cbv->addWidget(hRow);
    cbv->addWidget(dRow);

    ibl->addWidget(btnInfo);
    ibl->addWidget(countersBox);
    layout->addWidget(infoRow);

    layout->addSpacing(4);

    // ── 金币（图标 + 文字一行）──
    QWidget *goldRow = new QWidget(mLeftPanel);
    goldRow->setStyleSheet("background:#161b28; border-radius:8px;");
    auto *gbl = new QHBoxLayout(goldRow);
    gbl->addStretch();
    gbl->setContentsMargins(10, 4, 10, 4);
    gbl->setSpacing(8);

    QPixmap chipsSheet(":/textures/images/chips.png");
    if (!chipsSheet.isNull()) {
        // chips.png 第一格通常是金色筹码,如果不是请告诉我准确位置
        QPixmap goldIcon = chipsSheet.copy(0, 0, 58, 58);
        auto *iconLbl = new QLabel(goldRow);
        iconLbl->setPixmap(goldIcon.scaled(28, 28, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        iconLbl->setFixedSize(28, 28);
        iconLbl->setStyleSheet("background: transparent;");
        gbl->addWidget(iconLbl);
    }

    mLblGold = makeLabel("4", 28, "#f0c040", mPixelFont, goldRow);
    mLblGold->setStyleSheet("color:#f0c040; background:transparent;");
    gbl->addWidget(mLblGold);
    gbl->addStretch();

    layout->addWidget(goldRow);

    // 选项按钮
    auto *btnOptions = makeBtn("选项", "#c07820", "#a06010", mCNFont, mLeftPanel, 44);
    layout->addWidget(btnOptions);

    layout->addStretch();

    // 底注 / 回合
    auto *anteRow = new QWidget(mLeftPanel);
    auto *abl = new QHBoxLayout(anteRow);
    abl->setContentsMargins(0, 0, 0, 0);
    abl->setSpacing(8);

    auto *anteBox = new QWidget(anteRow);
    anteBox->setStyleSheet("background:#161b28; border-radius:6px;");
    auto *avbl = new QVBoxLayout(anteBox);
    avbl->setContentsMargins(6, 3, 6, 3);
    avbl->addWidget(makeLabel("底注", 11, "#888", mCNFont, anteBox));
    mLblAnte = makeLabel("1/8", 16, "#fff", mPixelFont, anteBox);
    avbl->addWidget(mLblAnte);

    auto *roundBox = new QWidget(anteRow);
    roundBox->setStyleSheet("background:#161b28; border-radius:6px;");
    auto *rvbl = new QVBoxLayout(roundBox);
    rvbl->setContentsMargins(6, 3, 6, 3);
    rvbl->addWidget(makeLabel("回合", 11, "#888", mCNFont, roundBox));
    mLblRound = makeLabel("1", 16, "#fff", mPixelFont, roundBox);
    rvbl->addWidget(mLblRound);

    abl->addWidget(anteBox);
    abl->addWidget(roundBox);
    layout->addWidget(anteRow);
}

void MainWindow::setupScene() {
    mView = new QGraphicsView(mScene, mPlayPage);
    mView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mView->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    mView->setFrameShape(QFrame::NoFrame);
    mView->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);

    mScene->setSceneRect(0, 0, mSceneW, mSceneH);
    mScene->setBackgroundBrush(QColor("#2b6a4a"));

    //绘制上方小丑 & 消耗牌

    mConsCountLabel = mScene->addText("0/2");
    mConsCountLabel->setDefaultTextColor(QColor("#aaddaa"));
    mConsCountLabel->setFont(mCNFont);
    mConsCountLabel->setPos(mSceneW - 52, 4);

    for (int i = 0; i < 5; ++i) {
        int x = 8 + i * (CARD_W + 14);
        auto *r = mScene->addRect(x, JOKER_Y + 18, CARD_W, CARD_H,
                                  QPen(QColor(255,255,255,60), 2, Qt::DashLine),
                                  QBrush(QColor(0,0,0,50)));
        mJokerSlotRects.append(r);
    }

    for (int i = 0; i < 2; ++i) {
        int x = mSceneW - 8 - (2 - i) * (CARD_W + 14);
        auto *r = mScene->addRect(x, JOKER_Y + 18, CARD_W, CARD_H,
                                  QPen(QColor(255,255,255,60), 2, Qt::DashLine),
                                  QBrush(QColor(0,0,0,50)));
        mConsumableSlotRects.append(r);
    }

    mPlayBgRect = mScene->addRect(10, PLAY_Y, mSceneW - 16, PLAY_H,
                                  QPen(QColor(0, 0, 0, 0)), QBrush(QColor(0, 0, 0, 25)));

    // 牌型名称标签
    mHandTypeLabel = mScene->addText("");
    QFont htf = mPixelFont; htf.setPixelSize(26);
    mHandTypeLabel->setFont(htf);
    mHandTypeLabel->setDefaultTextColor(QColor("#f0e060"));
    mHandTypeLabel->setZValue(30);

    // 本次得分标签
    mHandScoreLabel = mScene->addText("");
    QFont hsf = mCNFont; hsf.setPixelSize(14);
    mHandScoreLabel->setFont(hsf);
    mHandScoreLabel->setDefaultTextColor(QColor("#cccccc"));
    mHandScoreLabel->setZValue(30);

    // 手牌计数
    mHandCountLabel = mScene->addText("8/8");
    QFont hcf = mCNFont; hcf.setPixelSize(13);
    mHandCountLabel->setFont(hcf);
    mHandCountLabel->setDefaultTextColor(QColor("#aaddaa"));
    mHandCountLabel->setZValue(30);

    // 牌堆（右下角）
    CardData backData;
    backData.faceUp = false;
    mDeckBackCard = new CardItem(backData);
    mDeckBackCard->setPos(mSceneW - CARD_W - 10, mSceneH - CARD_H - 36);
    mDeckBackCard->setZValue(1);
    mScene->addItem(mDeckBackCard);

    mDeckLabel = mScene->addText("52/52");
    QFont df = mCNFont; df.setPixelSize(12);
    mDeckLabel->setFont(df);
    mDeckLabel->setDefaultTextColor(QColor("#cccccc"));
    mDeckLabel->setPos(mSceneW - CARD_W - 6, mSceneH - 28);
    mDeckLabel->setZValue(2);

    mBtnY  = PLAY_Y + PLAY_H + 30;        // 出牌区下方留 30px
    mHandY = mBtnY + 70;                  // 按钮下方

    mDeckLabel->setPos(mSceneW - CARD_W - 6, mSceneH - 28);
}

void MainWindow::setupSceneButtons() {
    int btnW = 160;
    int btnH = 50;
    int gap = 12;
    int totalW = btnW * 3 + gap * 2;
    int startX = (mSceneW - totalW) / 2;
    int y = mBtnY;

    // 出牌
    mBtnPlay = makeBtn("出牌", "#555", "#444", mCNFont, nullptr, btnH);
    mBtnPlay->setFixedWidth(btnW);
    auto *proxyPlay = mScene->addWidget(mBtnPlay);
    proxyPlay->setPos(startX, y);
    proxyPlay->setZValue(50);

    // 理牌（带子按钮）
    auto *sortContainer = new QWidget;
    sortContainer->setFixedSize(btnW, btnH);
    sortContainer->setStyleSheet("background:transparent;");
    auto *scbl = new QVBoxLayout(sortContainer);
    scbl->setContentsMargins(0, 0, 0, 0);
    scbl->setSpacing(3);

    mBtnSort = makeBtn("理牌", "#c07820", "#a06010", mCNFont, sortContainer, 24);
    auto *subRow = new QWidget(sortContainer);
    auto *subl = new QHBoxLayout(subRow);
    subl->setContentsMargins(0, 0, 0, 0);
    subl->setSpacing(3);
    mBtnSortNum  = makeBtn("点数", "#555", "#777", mCNFont, subRow, 22);
    mBtnSortSuit = makeBtn("花色", "#555", "#777", mCNFont, subRow, 22);
    subl->addWidget(mBtnSortNum);
    subl->addWidget(mBtnSortSuit);

    scbl->addWidget(mBtnSort);
    scbl->addWidget(subRow);

    auto *proxySort = mScene->addWidget(sortContainer);
    proxySort->setPos(startX + btnW + gap, y);
    proxySort->setZValue(50);

    // 弃牌
    mBtnDiscard = makeBtn("弃牌", "#555", "#444", mCNFont, nullptr, btnH);
    mBtnDiscard->setFixedWidth(btnW);
    auto *proxyDiscard = mScene->addWidget(mBtnDiscard);
    proxyDiscard->setPos(startX + (btnW + gap) * 2, y);
    proxyDiscard->setZValue(50);
}

void MainWindow::setupConnections() {
    connect(mBtnPlay, &QPushButton::clicked, this, &MainWindow::onPlayClicked);
    connect(mBtnDiscard, &QPushButton::clicked, this, &MainWindow::onDiscardClicked);
    connect(mBtnSortNum,  &QPushButton::clicked, this, &MainWindow::onSortByNum);
    connect(mBtnSortSuit, &QPushButton::clicked, this, &MainWindow::onSortBySuit);

    connect(mGameState, &GameState::handChanged, this, &MainWindow::refreshHand);
    connect(mGameState, &GameState::scoreChanged, this, &MainWindow::refreshScore);
    connect(mGameState, &GameState::goldChanged, this, &MainWindow::refreshGold);
    connect(mGameState, &GameState::handPlayed, this, &MainWindow::onHandPlayed);

    connect(mGameState, &GameState::roundWon, this, &MainWindow::onRoundWon);
    connect(mGameState, &GameState::gameOver, this, &MainWindow::onGameOver);
    connect(mGameState, &GameState::jokersChanged, this, &MainWindow::refreshJokerSlots);

    connect(mGameState, &GameState::consumablesChanged, this, &MainWindow::refreshConsumableSlots);

    connect(mGameState, &GameState::blindSelectEntered,
            this, &MainWindow::onBlindSelectEntered);
    connect(mGameState, &GameState::blindStarted,
            this, &MainWindow::onBlindStarted);
    connect(mBlindSelectWidget, &BlindSelectWidget::selectClicked,
            this, &MainWindow::onSelectBlindClicked);
    connect(mShopWidget, &ShopWidget::leaveClicked,
            this, &MainWindow::onLeaveShopClicked);
    connect(mShopWidget, &ShopWidget::packBuyRequested,
            this, &MainWindow::onPackBuyRequested);

    connect(mBlindSelectWidget, &BlindSelectWidget::skipClicked,
            this, &MainWindow::onSkipBlind);
}

void MainWindow::refreshHand() {
    // 移除旧手牌
    for (auto *c : mHandCards) {
        mScene->removeItem(c);
        delete c;
    }
    mHandCards.clear();
    mSelected.clear();

    const auto &hand = mGameState->hand();
    for (int i = 0; i < hand.size(); ++i) {
        auto *card = new CardItem(hand[i]);
        card->setZValue(10 + i);
        mScene->addItem(card);
        mHandCards.append(card);

        connect(card, &CardItem::clicked,
            this, &MainWindow::onCardClicked);
    }

    layoutHandCards();
    refreshCounters();
}

void MainWindow::layoutHandCards() {
    int n = mHandCards.size();
    if (n == 0) return;

    // 手牌计数
    mHandCountLabel->setPlainText(
        QString("%1/%2").arg(n).arg(Constants::HAND_SIZE));
    QRectF hcr = mHandCountLabel->boundingRect();
    mHandCountLabel->setPos((mSceneW - hcr.width()) / 2, mBtnY - 22);

    int available = mSceneW - 80;
    int step = (n > 1) ? (available - CARD_W) / (n - 1) : 0;
    step = qMin(step, CARD_W - 30);
    int totalW = (n - 1) * step + CARD_W;
    int startX = (mSceneW - totalW) / 2;

    for (int i = 0; i < n; ++i) {
        bool sel = mSelected.contains(i);
        int x = startX + i * step;
        int y = sel ? mHandY - 30 : mHandY;
        mHandCards[i]->setZValue(sel ? 50 + i : 10 + i);
        mHandCards[i]->setPos(x, y);
    }
}

// 出牌区刷新
void MainWindow::clearPlayedCards() {
    for (auto *c : mPlayedCards) {
        mScene->removeItem(c);
        delete c;
    }
    mPlayedCards.clear();
}

void MainWindow::layoutPlayedCards() {
    int n = mPlayedCards.size();
    if (n == 0) return;

    int totalW = n * CARD_W + (n - 1) * 10;
    int startX = (mSceneW - totalW) / 2;
    int y = PLAY_Y + (PLAY_H - CARD_H) / 2;

    for (int i = 0; i < n; ++i)
        mPlayedCards[i]->setPos(startX + i * (CARD_W + 10), y);
}

// 分数刷新
void MainWindow::refreshScore() {
    mLblScore->setText(QString("✳ %1").arg(mGameState->score()));
    mLblTarget->setText(QString("✳ %1").arg(mGameState->targetScore()));
}

// 金币刷新
void MainWindow::refreshGold() {
    mLblGold->setText(QString("%1").arg(mGameState->gold()));
}

// 出牌/弃牌次数刷新
void MainWindow::refreshCounters() {
    mLblHands->setText(QString::number(mGameState->handsLeft()));
    mLblDiscards->setText(QString::number(mGameState->discardLeft()));
    mLblAnte->setText(QString("%1/8").arg(mGameState->ante()));
    mLblRound->setText(QString::number(
        static_cast<int>(mGameState->blindType()) + 1));

    auto applyBlindStyle = [this](const QString &color) {
        mLblBlind->setStyleSheet(QString("color:white; background:%1; border-radius:6px; padding:3px;")
                                     .arg(color));
    };
    switch (mGameState->blindType()) {
    case BlindType::Small:
        mLblBlind->setText("小盲注");
        applyBlindStyle("#3d70b8");
        break;
    case BlindType::Big:
        mLblBlind->setText("大盲注");
        applyBlindStyle("#c07820");
        break;
    case BlindType::Boss: {
        auto info = mGameState->currentBossInfo();
        mLblBlind->setText(QString("Boss · %1").arg(info.name));
        applyBlindStyle("#a02020");
        mLblBlind->setToolTip(info.description);
        break;
    }
    }

    // 刷新左面板上下文区的芯片图
    if (mCtxBlindChipImg) {
        QPixmap sheet(":/textures/images/BlindChips.png");
        if (!sheet.isNull()) {
            int row = 0;
            switch (mGameState->blindType()) {
            case BlindType::Small: row = 0; break;
            case BlindType::Big:   row = 1; break;
            case BlindType::Boss:
                switch (mGameState->bossEffect()) {
                case BossEffect::TheHook:   row = 7;  break;
                case BossEffect::TheClub:   row = 4;  break;
                case BossEffect::TheWall:   row = 9;  break;
                case BossEffect::ThePlant:  row = 19; break;
                case BossEffect::TheNeedle: row = 20; break;
                default: row = 7; break;
                }
                break;
            }
            QPixmap pix = sheet.copy(0, row * 68, 68, 68);
            mCtxBlindChipImg->setPixmap(pix.scaled(76, 76, Qt::KeepAspectRatio,
                                                   Qt::SmoothTransformation));
        }
    }

    // 刷新左面板芯片图
    if (mBlindChipLbl) {
        QPixmap sheet(":/textures/images/BlindChips.png");
        if (!sheet.isNull()) {
            int row = 0;
            if (mGameState->blindType() == BlindType::Small) row = 0;
            else if (mGameState->blindType() == BlindType::Big) row = 1;
            else {
                switch (mGameState->bossEffect()) {
                case BossEffect::TheHook:   row = 7;  break;
                case BossEffect::TheClub:   row = 4;  break;
                case BossEffect::TheWall:   row = 9;  break;
                case BossEffect::ThePlant:  row = 19; break;
                case BossEffect::TheNeedle: row = 20; break;
                default: row = 7; break;
                }
            }
            QPixmap pix = sheet.copy(0, row * 68, 68, 68);
            mBlindChipLbl->setPixmap(pix.scaled(64, 64, Qt::KeepAspectRatio,
                                                Qt::SmoothTransformation));
        }
    }

    bool hasSelected = !mSelected.isEmpty();
    mBtnPlay->setEnabled(mGameState->handsLeft() > 0 && hasSelected);
    mBtnDiscard->setEnabled(mGameState->discardLeft() > 0 && hasSelected);

    // 更新牌堆计数
    if (mDeckLabel)
        mDeckLabel->setPlainText(
            QString("%1/52").arg(mGameState->deckRemaining()));
}

// 卡牌点击：切换选中状态
void MainWindow::onCardClicked(CardItem *card) {
    int idx = mHandCards.indexOf(card);
    if (idx < 0) return;

    if (mSelected.contains(idx)) {
        mSelected.removeAll(idx);
        card->setCardSelected(false);
    } else {
        if (mSelected.size() < 5) {  // 最多选5张
            mSelected.append(idx);
            card->setCardSelected(true);
        }
    }

    layoutHandCards();
    refreshCounters();
}

// 出牌
void MainWindow::onPlayClicked() {
    if (mSelected.isEmpty()) return;

    // 把选中的牌移到出牌区显示
    clearPlayedCards();
    QVector<int> sortedIdx = mSelected;
    std::sort(sortedIdx.begin(), sortedIdx.end());

    for (int idx : sortedIdx) {
        CardData d = mHandCards[idx]->cardData();
        auto *c = new CardItem(d);
        c->setZValue(20);
        mScene->addItem(c);
        mPlayedCards.append(c);
    }
    layoutPlayedCards();

    // 通知 GameState
    mGameState->playCards(mSelected);
    mSelected.clear();
}

// 弃牌
void MainWindow::onDiscardClicked() {
    if (mSelected.isEmpty()) return;
    clearPlayedCards();
    mGameState->discardCards(mSelected);
    mSelected.clear();
}

void MainWindow::onHandPlayed()
{
    const HandResult &r = mGameState->lastResult();

    mLblChips->setText(QString::number(r.chips));
    mLblMult->setText(QString::number(r.mult));

    mHandTypeLabel->setPlainText(QString("%1 lv.%2").arg(r.name).arg(r.level));
    QRectF tb = mHandTypeLabel->boundingRect();
    mHandTypeLabel->setPos((mSceneW - tb.width()) / 2, PLAY_Y + 10);

    int total = static_cast<int>(r.chips * r.mult * r.xmult);
    QString line = (qAbs(r.xmult - 1.0) < 1e-6)
                       ? QString("%1  ×  %2  =  %3").arg(r.chips).arg(r.mult).arg(total)
                       : QString("%1  ×  %2  ×  %3  =  %4")
                             .arg(r.chips).arg(r.mult)
                             .arg(QString::number(r.xmult, 'g', 3))
                             .arg(total);
    mHandScoreLabel->setPlainText(line);
    QRectF sb = mHandScoreLabel->boundingRect();
    mHandScoreLabel->setPos((mSceneW - sb.width()) / 2, PLAY_Y + 46);
}

void MainWindow::onSortByNum() {
    mGameState->sortHandByRank();
}

void MainWindow::onSortBySuit() {
    mGameState->sortHandBySuit();
}

void MainWindow::onGameOver(bool won)
{
    if (mGameOverHandled) return;                 // ← 新增
    mGameOverHandled = true;

    QString title = won ? "通关！" : "失败";
    QString text  = won
                       ? "恭喜！你击败了所有盲注。"
                       : QString("最终 Ante: %1\n回合得分: %2 / 目标 %3")
                             .arg(mGameState->ante())
                             .arg(mGameState->score())
                             .arg(mGameState->targetScore());

    QMessageBox box(this);
    box.setWindowTitle(title);
    box.setText(text);
    box.setInformativeText("是否重新开始？");
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::Yes);

    if (box.exec() == QMessageBox::Yes) {
        mGameState->startGame();   // 会触发 blindSelectEntered → 切到选择页
        mGameOverHandled = false;
        refreshGold();
        refreshCounters();
        refreshJokerSlots();
        refreshConsumableSlots();
    } else {
        close();
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);

    if (mPlayPage) {
        QRect r = mPlayPage->rect();
        if (mBlindSelectWidget) mBlindSelectWidget->setGeometry(r);
        if (mShopWidget)        mShopWidget       ->setGeometry(r);
        if (mRoundEndOverlay)   mRoundEndOverlay  ->setGeometry(r);
    }
    if (mPackOpenWidget && mShopWidget)
        mPackOpenWidget->setGeometry(mShopWidget->rect());
}

void MainWindow::refreshJokerSlots()
{
    // 清掉旧的视图
    for (auto *ji : mJokerItems) {
        mScene->removeItem(ji);
        delete ji;
    }
    mJokerItems.clear();

    // 用现有 jokers() 重画
    const auto &js = mGameState->jokers();
    for (int i = 0; i < js.size(); ++i) {
        int x = 8 + i * (CARD_W + 14);
        int y = JOKER_Y + 18;
        auto *ji = new JokerItem(js[i]);
        ji->setPos(x, y);
        ji->setZValue(20);
        mScene->addItem(ji);
        mJokerItems.append(ji);
    }
}

void MainWindow::refreshConsumableSlots()
{
    for (auto *ci : mConsumableItems) { mScene->removeItem(ci); delete ci; }
    mConsumableItems.clear();

    const auto &cs = mGameState->consumables();
    for (int i = 0; i < cs.size(); ++i) {
        // 消耗品在右上角，从右往左排：与 setupScene 里的虚线占位对齐
        int x = mSceneW - 8 - (Constants::MAX_CONSUMABLE_SLOTS - i) * (CARD_W + 14);
        int y = JOKER_Y + 18;
        auto *ci = new ConsumableItem(cs[i]);
        ci->setPos(x, y);
        ci->setZValue(20);
        mScene->addItem(ci);
        mConsumableItems.append(ci);

        connect(ci, &ConsumableItem::clicked,
                this, &MainWindow::onConsumableClicked);
    }
}

void MainWindow::onConsumableClicked(ConsumableItem *item, Qt::MouseButton btn)
{
    int idx = mConsumableItems.indexOf(item);
    if (idx < 0) return;

    if (btn == Qt::RightButton) {
        mGameState->sellConsumable(idx);
        return;
    }

    QVector<int> sel = mSelected;
    std::sort(sel.begin(), sel.end());

    const auto &cs = mGameState->consumables();
    if (idx >= cs.size()) return;
    if (cs[idx].needsSelection > 0 && sel.size() < cs[idx].needsSelection) {
        // 选牌不足：闪一下手牌计数提示
        mHandCountLabel->setDefaultTextColor(QColor("#ff8080"));
        QTimer::singleShot(400, this, [this]() {
            if (mHandCountLabel) mHandCountLabel->setDefaultTextColor(QColor("#aaddaa"));
        });
        return;
    }

    mGameState->useConsumable(idx, sel);
    // 后续刷新由 handChanged / consumablesChanged 信号自动驱动
}

void MainWindow::onPackChoiceMade(int chosenIdx)
{
    if (chosenIdx >= 0)
        mGameState->applyPackChoice(mPendingPack, chosenIdx);
    mShopWidget->refresh();
}

void MainWindow::onSelectBlindClicked()
{
    mGameState->selectCurrentBlind();
}

void MainWindow::onLeaveShopClicked()
{
    mShopWidget->hide();
    mGameState->leaveShop();
    refreshConsumableSlots();
}

bool MainWindow::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj == mPlayPage && ev->type() == QEvent::Resize) {
        QRect r = mPlayPage->rect();
        if (mBlindSelectWidget) mBlindSelectWidget->setGeometry(r);
        if (mShopWidget)        mShopWidget       ->setGeometry(r);
        if (mRoundEndOverlay)   mRoundEndOverlay  ->setGeometry(r);
        if (mPackOpenWidget && mShopWidget)
            mPackOpenWidget->setGeometry(mShopWidget->rect());
    }
    return QMainWindow::eventFilter(obj, ev);
}

void MainWindow::onBlindSelectEntered()
{
    setContextPage(0);   // ← 切到 BlindSelect 文案
    mBlindSelectWidget->refresh();
    mBlindSelectWidget->raise();
    mBlindSelectWidget->show();
    QTimer::singleShot(0, this, [this]() {
        if (mBlindSelectWidget && mPlayPage)
            mBlindSelectWidget->setGeometry(mPlayPage->rect());
        mBlindSelectWidget->playEnterAnimation();   // ← 浮入动画
    });
}

void MainWindow::onBlindStarted()
{
    mBlindSelectWidget->hide();
    mShopWidget->hide();
    mRoundEndOverlay->hide();
    setContextPage(1);   // ← 切到对局信息

    refreshHand();
    refreshScore();
    refreshGold();
    refreshCounters();
    refreshJokerSlots();
    refreshConsumableSlots();
    clearPlayedCards();
    if (mHandTypeLabel)  mHandTypeLabel ->setPlainText("");
    if (mHandScoreLabel) mHandScoreLabel->setPlainText("");
    mLblChips->setText("0");
    mLblMult ->setText("0");
}

void MainWindow::onNextBlindClicked()
{
    mRoundEndOverlay->hide();
    mShopWidget->refresh();
    mShopWidget->raise();
    mShopWidget->show();
    setContextPage(2);   // ← 切到 SHOP 标识
    QTimer::singleShot(0, this, [this]() {
        if (mShopWidget && mPlayPage)
            mShopWidget->setGeometry(mPlayPage->rect());
    });
}

void MainWindow::onRoundWon(int blindReward, int handBonus, int interest)
{
    QString blindName;
    switch (mGameState->blindType()) {
    case BlindType::Small: blindName = "小盲注";    break;
    case BlindType::Big:   blindName = "大盲注";    break;
    case BlindType::Boss:  blindName = "Boss 盲注"; break;
    }
    refreshGold();
    mRoundEndOverlay->setData(blindName,
                              mGameState->score(),
                              mGameState->targetScore(),
                              blindReward, handBonus, interest);
    mRoundEndOverlay->raise();
    mRoundEndOverlay->show();
    QTimer::singleShot(0, this, [this]() {
        if (mRoundEndOverlay && mPlayPage)
            mRoundEndOverlay->setGeometry(mPlayPage->rect());
    });
}

void MainWindow::onPackBuyRequested(int slot)
{
    if (!mGameState->buyPack(slot, mPendingPack)) return;
    int freeJoker = mGameState->jokerSlots() - mGameState->jokers().size();
    int freeCons  = Constants::MAX_CONSUMABLE_SLOTS - mGameState->consumables().size();
    mPackOpenWidget->open(mPendingPack, freeCons, freeJoker);
    QTimer::singleShot(0, this, [this]() {
        if (mPackOpenWidget && mShopWidget)
            mPackOpenWidget->setGeometry(mShopWidget->rect());
    });
}

void MainWindow::setContextPage(int page)
{
    if (mContextArea) mContextArea->setCurrentIndex(page);
}

void MainWindow::onSkipBlind(int /*idx*/)
{
    mGameState->skipCurrentBlind();
}
