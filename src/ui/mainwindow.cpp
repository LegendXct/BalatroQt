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
#include <QPauseAnimation>
#include <QSequentialAnimationGroup>
#include "shopsignwidget.h"

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
    container->setAttribute(Qt::WA_StyledBackground, true);
    container->setStyleSheet("background: #1a2024;");        // ← 整个窗口最外层底色
    auto *cl = new QHBoxLayout(container);
    cl->setContentsMargins(8, 8, 0, 8);                       // ← 左 8 上 8 下 8
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
    mLeftPanel->setAttribute(Qt::WA_StyledBackground, true);
    mLeftPanel->setStyleSheet(
        "background: #2a3035;"                        // 比 #2c3439 偏中性灰
        "border-radius: 12px;"
        );

    QVBoxLayout *layout = new QVBoxLayout(mLeftPanel);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(6);

    // ── 上下文区(固定高度 200px,3 态切换) ──
    mContextArea = new QStackedWidget(mLeftPanel);
    mContextArea->setFixedHeight(160);
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
    mCtxBlind->setStyleSheet("background:#374244; border-radius:8px;");
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
            "color:white; background:#1679b4;"   // ← #3d70b8 → #1679b4
            "border-radius:6px; padding:3px 8px;");
        mLblBlind->setFixedHeight(28);
        vbl->addWidget(mLblBlind);

        QLabel *tt = new QLabel("至少得分", mCtxBlind);
        QFont ttf = mCNFont; ttf.setPixelSize(11);
        tt->setFont(ttf);
        tt->setStyleSheet("color:white; background:transparent;");
        tt->setAlignment(Qt::AlignCenter);
        vbl->addWidget(tt);

        mLblTarget = new QLabel("✳ 300", mCtxBlind);
        QFont tf = mPixelFont; tf.setPixelSize(24);
        mLblTarget->setFont(tf);
        mLblTarget->setStyleSheet("color:#fe5f55; background:transparent;");
        mLblTarget->setAlignment(Qt::AlignCenter);
        vbl->addWidget(mLblTarget);

        mLblReward = new QLabel("奖励 $$$", mCtxBlind);
        QFont rf = mCNFont; rf.setPixelSize(12);
        mLblReward->setFont(rf);
        mLblReward->setStyleSheet("color:#f3b958; background:transparent;");
        mLblReward->setAlignment(Qt::AlignCenter);
        vbl->addWidget(mLblReward);

        hbl->addLayout(vbl, 1);
    }
    mContextArea->addWidget(mCtxBlind);

    // 页面 2: Shop
    mCtxShop = new QWidget;
    mCtxShop->setStyleSheet("background:transparent;");
    {
        auto *vl = new QVBoxLayout(mCtxShop);
        vl->setContentsMargins(0, 0, 0, 0);
        vl->setSpacing(0);
        vl->setAlignment(Qt::AlignCenter);

        auto *sign = new ShopSignWidget(mCtxShop);
        vl->addWidget(sign, 0, Qt::AlignCenter);

        QLabel *sub = new QLabel("来变强吧!", mCtxShop);
        QFont subf = mCNFont; subf.setPixelSize(13);
        sub->setFont(subf);
        sub->setStyleSheet("color:white; background:transparent;");
        sub->setAlignment(Qt::AlignCenter);
        vl->addWidget(sub);
    }
    mContextArea->addWidget(mCtxShop);

    layout->addWidget(mContextArea);

    // ── 回合分数(横排:标题 + 芯片 + 数字) ──
    QWidget *scoreBox = new QWidget(mLeftPanel);
    scoreBox->setFixedHeight(48);
    scoreBox->setAttribute(Qt::WA_StyledBackground, true);
    scoreBox->setStyleSheet("background:#374244; border-radius:8px;");
    auto *sbl = new QHBoxLayout(scoreBox);
    sbl->setContentsMargins(10, 6, 10, 6);
    sbl->setSpacing(6);

    QLabel *sTitle = new QLabel("回合\n分数", scoreBox);
    QFont stf = mCNFont; stf.setPixelSize(12);
    sTitle->setFont(stf);
    sTitle->setStyleSheet("color:white; background:transparent;");
    sTitle->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    sbl->addWidget(sTitle);

    sbl->addStretch();

    // 紫芯片图标
    QLabel *scoreChip = new QLabel(scoreBox);
    {
        QPixmap chipsSheet(":/textures/images/chips.png");
        if (!chipsSheet.isNull()) {
            QPixmap pix = chipsSheet.copy(0, 0, 58, 58);
            scoreChip->setPixmap(pix.scaled(28, 28, Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation));
        }
    }
    scoreChip->setFixedSize(28, 28);
    scoreChip->setStyleSheet("background:transparent;");
    sbl->addWidget(scoreChip);

    mLblScore = new QLabel("0", scoreBox);
    QFont smf = mPixelFont; smf.setPixelSize(28);
    mLblScore->setFont(smf);
    mLblScore->setStyleSheet("color:white; background:transparent;");
    sbl->addWidget(mLblScore);

    layout->addWidget(scoreBox);

    // 牌型名行
    QWidget *handNameBox = new QWidget(mLeftPanel);
    handNameBox->setFixedHeight(40);
    auto *hnl = new QHBoxLayout(handNameBox);
    hnl->setContentsMargins(0, 0, 0, 0);
    hnl->setSpacing(6);
    hnl->setAlignment(Qt::AlignCenter);

    mLblHandName = new QLabel("", handNameBox);
    QFont hnf = mCNFont; hnf.setPixelSize(22); hnf.setBold(true);
    mLblHandName->setFont(hnf);
    mLblHandName->setStyleSheet("color:white; background:transparent;");
    mLblHandName->setAlignment(Qt::AlignCenter);
    hnl->addWidget(mLblHandName);

    mLblHandLevel = new QLabel("", handNameBox);
    QFont hlf = mCNFont; hlf.setPixelSize(14);
    mLblHandLevel->setFont(hlf);
    mLblHandLevel->setStyleSheet("color:#ff9a00; background:transparent;");
    hnl->addWidget(mLblHandLevel);

    layout->addWidget(handNameBox);

    // 筹码 × 倍率
    QWidget *chipsRow = new QWidget(mLeftPanel);
    chipsRow->setFixedHeight(56);
    QHBoxLayout *chipsLayout = new QHBoxLayout(chipsRow);
    chipsLayout->setContentsMargins(0, 0, 0, 0);
    chipsLayout->setSpacing(4);

    mLblChips = new QLabel("0", chipsRow);
    mLblChips->setAlignment(Qt::AlignCenter);
    QFont cf = mPixelFont; cf.setPixelSize(30);
    mLblChips->setFont(cf);
    mLblChips->setStyleSheet(
        "background: #009dff; color: white;"
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
        "background:#fe5f55; color:white;"
        "border-radius:8px; padding:4px 8px;"
        );

    chipsLayout->addWidget(mLblChips);
    chipsLayout->addWidget(lblX);
    chipsLayout->addWidget(mLblMult);
    layout->addWidget(chipsRow);

    QWidget *bottomRow = new QWidget(mLeftPanel);
    auto *brl = new QHBoxLayout(bottomRow);
    brl->setContentsMargins(0, 0, 0, 0);
    brl->setSpacing(8);

    // 左列:两个按钮竖排
    QWidget *btnCol = new QWidget(bottomRow);
    auto *btnVbl = new QVBoxLayout(btnCol);
    btnVbl->setContentsMargins(0, 0, 0, 0);
    btnVbl->setSpacing(6);

    QPushButton *btnInfo = makeBtn("比赛\n信息", "#fe5f55", "#ff7066", mCNFont, btnCol, 70);
    btnInfo->setFixedWidth(76);
    btnVbl->addWidget(btnInfo);

    QPushButton *btnOptions = makeBtn("选项", "#fda200", "#ffb730", mCNFont, btnCol, 70);
    btnOptions->setFixedWidth(76);
    btnVbl->addWidget(btnOptions);

    brl->addWidget(btnCol);

    // 右列:出牌/弃牌、金币、底注/回合 全部堆在这里
    QWidget *rightCol = new QWidget(bottomRow);
    auto *rcvbl = new QVBoxLayout(rightCol);
    rcvbl->setContentsMargins(0, 0, 0, 0);
    rcvbl->setSpacing(6);

    // 出牌 / 弃牌 (横排)
    QWidget *handsRow = new QWidget(rightCol);
    handsRow->setFixedHeight(56);
    handsRow->setAttribute(Qt::WA_StyledBackground, true);
    handsRow->setStyleSheet("background:#374244; border-radius:8px;");
    auto *hrl = new QHBoxLayout(handsRow);
    hrl->setContentsMargins(8, 4, 8, 4);
    hrl->setSpacing(4);

    // "出牌" 标题 + 数字
    QWidget *hCell = new QWidget(handsRow);
    auto *hcv = new QVBoxLayout(hCell);
    hcv->setContentsMargins(0, 0, 0, 0);
    hcv->setSpacing(0);
    hcv->setAlignment(Qt::AlignCenter);
    hcv->addWidget(makeLabel("出牌", 11, "white", mCNFont, hCell));
    mLblHands = makeLabel("4", 22, "#009dff", mPixelFont, hCell);
    hcv->addWidget(mLblHands);
    hrl->addWidget(hCell);

    // "弃牌" 同结构
    QWidget *dCell = new QWidget(handsRow);
    auto *dcv = new QVBoxLayout(dCell);
    dcv->setContentsMargins(0, 0, 0, 0);
    dcv->setSpacing(0);
    dcv->setAlignment(Qt::AlignCenter);
    dcv->addWidget(makeLabel("弃牌", 11, "white", mCNFont, dCell));
    mLblDiscards = makeLabel("3", 22, "#fe5f55", mPixelFont, dCell);
    dcv->addWidget(mLblDiscards);
    hrl->addWidget(dCell);

    rcvbl->addWidget(handsRow);

    // 金币(长横盒)
    QWidget *goldRow = new QWidget(rightCol);
    goldRow->setFixedHeight(36);
    goldRow->setAttribute(Qt::WA_StyledBackground, true);
    goldRow->setStyleSheet("background:#374244; border-radius:8px;");
    auto *gbl = new QHBoxLayout(goldRow);
    gbl->setContentsMargins(10, 4, 10, 4);
    gbl->setSpacing(8);
    gbl->setAlignment(Qt::AlignCenter);

    mLblGold = makeLabel("$4", 24, "#f3b958", mPixelFont, goldRow);
    gbl->addWidget(mLblGold);
    rcvbl->addWidget(goldRow);

    // 底注 / 回合
    QWidget *anteRow2 = new QWidget(rightCol);
    auto *arl = new QHBoxLayout(anteRow2);
    arl->setContentsMargins(0, 0, 0, 0);
    arl->setSpacing(4);

    QWidget *anteBox = new QWidget(anteRow2);
    anteBox->setFixedHeight(44);
    anteBox->setAttribute(Qt::WA_StyledBackground, true);
    anteBox->setStyleSheet("background:#374244; border-radius:8px;");
    auto *avbl = new QVBoxLayout(anteBox);
    avbl->setContentsMargins(6, 3, 6, 3);
    avbl->setSpacing(0);
    avbl->setAlignment(Qt::AlignCenter);
    avbl->addWidget(makeLabel("底注", 11, "white", mCNFont, anteBox));
    mLblAnte = makeLabel("1<font color='white'>/8</font>", 16, "#ff9a00", mPixelFont, anteBox);
    mLblAnte->setTextFormat(Qt::RichText);     // ← 加这行,启用 HTML
    avbl->addWidget(mLblAnte);
    arl->addWidget(anteBox);

    QWidget *roundBox = new QWidget(anteRow2);
    roundBox->setFixedHeight(44);
    roundBox->setAttribute(Qt::WA_StyledBackground, true);
    roundBox->setStyleSheet("background:#374244; border-radius:8px;");
    auto *rvbl = new QVBoxLayout(roundBox);
    rvbl->setContentsMargins(6, 3, 6, 3);
    rvbl->setSpacing(0);
    rvbl->setAlignment(Qt::AlignCenter);
    rvbl->addWidget(makeLabel("回合", 11, "white", mCNFont, roundBox));
    mLblRound = makeLabel("1", 16, "#ff9a00", mPixelFont, roundBox);
    rvbl->addWidget(mLblRound);
    arl->addWidget(roundBox);

    rcvbl->addWidget(anteRow2);

    brl->addWidget(rightCol, 1);
    layout->addStretch();
    layout->addWidget(bottomRow);
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

    mHandY = PLAY_Y + PLAY_H + 30;         // 出牌区下方留 30px
    mBtnY  = mHandY + CARD_H + 24;                  // 按钮下方

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
    mBtnPlay = makeBtn("出牌", "#009dff", "#33b0ff", mCNFont, nullptr, btnH);
    mBtnPlay->setFixedWidth(btnW);
    mPlayProxy = mScene->addWidget(mBtnPlay);          // ← auto *proxyPlay = ... 改成 mPlayProxy =
    mPlayProxy->setPos(startX, y);
    mPlayProxy->setZValue(50);

    // 理牌(白色方框 + label + 点数/花色 两子按钮)
    auto *sortContainer = new QWidget;
    sortContainer->setFixedSize(btnW, btnH);
    sortContainer->setAttribute(Qt::WA_StyledBackground, true);
    sortContainer->setStyleSheet(
        "background: white;"
        "border-radius: 8px;"
        );
    auto *scbl = new QVBoxLayout(sortContainer);
    scbl->setContentsMargins(4, 2, 4, 4);
    scbl->setSpacing(2);
    scbl->setAlignment(Qt::AlignCenter);

    // "理牌" 是 label,不是 button
    QLabel *sortLbl = new QLabel("理牌", sortContainer);
    QFont slf = mCNFont; slf.setPixelSize(14); slf.setBold(true);
    sortLbl->setFont(slf);
    sortLbl->setAlignment(Qt::AlignCenter);
    sortLbl->setStyleSheet("color:#374244; background:transparent;");
    scbl->addWidget(sortLbl);

    // 子按钮行
    auto *subRow = new QWidget(sortContainer);
    subRow->setStyleSheet("background:transparent;");
    auto *subl = new QHBoxLayout(subRow);
    subl->setContentsMargins(0, 0, 0, 0);
    subl->setSpacing(4);
    mBtnSortNum  = makeBtn("点数", "#fda200", "#ffb730", mCNFont, subRow, 22);
    mBtnSortSuit = makeBtn("花色", "#fda200", "#ffb730", mCNFont, subRow, 22);
    subl->addWidget(mBtnSortNum);
    subl->addWidget(mBtnSortSuit);
    scbl->addWidget(subRow);

    mSortProxy = mScene->addWidget(sortContainer);
    mSortProxy->setPos(startX + btnW + gap, y);
    mSortProxy->setZValue(50);
    // 弃牌
    mBtnDiscard = makeBtn("弃牌", "#fe5f55", "#ff7066", mCNFont, nullptr, btnH);
    mBtnDiscard->setFixedWidth(btnW);
    mDiscardProxy = mScene->addWidget(mBtnDiscard);    // ← 同理
    mDiscardProxy->setPos(startX + (btnW + gap) * 2, y);
    mDiscardProxy->setZValue(50);
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
    const auto &hand = mGameState->hand();

    auto matches = [](const CardData &a, const CardData &b) {
        return a.rank == b.rank && a.suit == b.suit
               && a.enhancement == b.enhancement && a.seal == b.seal
               && a.edition == b.edition;
    };

    // 记录"哪些 CardData 当前是选中状态"
    QVector<CardData> selectedData;
    for (int i : mSelected)
        if (i >= 0 && i < mHandCards.size())
            selectedData.append(mHandCards[i]->cardData());

    // ...原有的删除+重排逻辑...
    for (int i = mHandCards.size() - 1; i >= 0; --i) {
        const CardData &d = mHandCards[i]->cardData();
        bool found = false;
        for (const auto &hc : hand) if (matches(hc, d)) { found = true; break; }
        if (!found) {
            mScene->removeItem(mHandCards[i]);
            mHandCards[i]->deleteLater();
            mHandCards.removeAt(i);
        }
    }

    QPointF deckPos(mSceneW - CARD_W - 10, mSceneH - CARD_H - 36);
    QVector<CardItem*> reordered;
    QVector<CardItem*> remaining = mHandCards;
    for (const auto &hc : hand) {
        CardItem *match = nullptr;
        for (int k = 0; k < remaining.size(); ++k) {
            if (matches(remaining[k]->cardData(), hc)) {
                match = remaining[k];
                remaining.removeAt(k);
                break;
            }
        }
        if (!match) {
            match = new CardItem(hc);
            match->setPos(deckPos);
            match->setZValue(10);
            mScene->addItem(match);
            connect(match, &CardItem::clicked,
                    this, &MainWindow::onCardClicked);
        }
        reordered.append(match);
    }
    mHandCards = reordered;

    // 按身份恢复选中状态
    mSelected.clear();
    for (int i = 0; i < mHandCards.size(); ++i) {
        const CardData &d = mHandCards[i]->cardData();
        bool wasSelected = false;
        for (const auto &sd : selectedData) {
            if (matches(sd, d)) { wasSelected = true; break; }
        }
        mHandCards[i]->setCardSelected(wasSelected);
        if (wasSelected) mSelected.append(i);
    }

    layoutHandCards();
    refreshCounters();
    updateHandPreview();
}

void MainWindow::layoutHandCards() {
    int n = mHandCards.size();
    if (n == 0) return;

    mHandCountLabel->setPlainText(
        QString("%1/%2").arg(n).arg(Constants::HAND_SIZE));
    QRectF hcr = mHandCountLabel->boundingRect();
    mHandCountLabel->setPos((mSceneW - hcr.width()) / 2, mHandY - 22);

    int available = mSceneW - 80;
    int step = (n > 1) ? (available - CARD_W) / (n - 1) : 0;
    step = qMin(step, CARD_W - 30);
    int totalW = (n - 1) * step + CARD_W;
    int startX = (mSceneW - totalW) / 2;

    for (int i = 0; i < n; ++i) {
        bool sel = mSelected.contains(i);

        double t = (-n / 2.0 - 0.5 + (i + 1)) / n;
        double angleDeg = 0.2 * t * 180.0 / M_PI;

        int x = startX + i * step;
        int y = mHandY + (sel ? -50 : 0);

        mHandCards[i]->setBaseRotation(angleDeg);
        mHandCards[i]->setZValue(i);
        mHandCards[i]->moveTo(QPointF(x, y), 220);
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
    mLblScore->setText(QString::number(mGameState->score()));
    mLblTarget->setText(QString::number(mGameState->targetScore()));
}

// 金币刷新
void MainWindow::refreshGold() {
    mLblGold->setText(QString("$%1").arg(mGameState->gold()));
}

// 出牌/弃牌次数刷新
void MainWindow::refreshCounters() {
    mLblHands->setText(QString::number(mGameState->handsLeft()));
    mLblDiscards->setText(QString::number(mGameState->discardLeft()));
    mLblAnte->setText(QString("%1<font color='white'>/8</font>")
                          .arg(mGameState->ante()));
    mLblRound->setText(QString::number(
        static_cast<int>(mGameState->blindType()) + 1));

    auto applyBlindStyle = [this](const QString &color) {
        mLblBlind->setStyleSheet(QString("color:white; background:%1; border-radius:6px; padding:3px;")
                                     .arg(color));
    };
    switch (mGameState->blindType()) {
    case BlindType::Small:
        mLblBlind->setText("小盲注");
        applyBlindStyle("#1679b4");
        break;
    case BlindType::Big:
        mLblBlind->setText("大盲注");
        applyBlindStyle("#ae7b1b");
        break;
    case BlindType::Boss: {
        auto info = mGameState->currentBossInfo();
        mLblBlind->setText(QString("Boss · %1").arg(info.name));
        QString col;
        switch (mGameState->bossEffect()) {
        case BossEffect::TheHook:   col = "#a84024"; break;
        case BossEffect::TheClub:   col = "#b9cb92"; break;
        case BossEffect::TheWall:   col = "#8a59a5"; break;
        case BossEffect::ThePlant:  col = "#709284"; break;
        case BossEffect::TheNeedle: col = "#5c6e31"; break;
        default:                    col = "#a84024"; break;
        }
        applyBlindStyle(col);
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
    updateHandPreview();
}

// 出牌
void MainWindow::onPlayClicked() {
    if (mSelected.isEmpty()) return;

    QVector<int> sortedIdx = mSelected;
    std::sort(sortedIdx.begin(), sortedIdx.end());

    mSelected.clear();

    clearPlayedCards();
    QVector<CardItem*> playedCards;
    for (int i = sortedIdx.size() - 1; i >= 0; --i) {
        int idx = sortedIdx[i];
        CardItem *c = mHandCards.takeAt(idx);
        c->setCardSelected(false);
        c->setZValue(20);
        playedCards.prepend(c);
    }
    mPlayedCards = playedCards;

    layoutHandCards();

    int n = mPlayedCards.size();
    int totalW = n * CARD_W + (n - 1) * 10;
    int startX = (mSceneW - totalW) / 2;
    int y = PLAY_Y + (PLAY_H - CARD_H) / 2;
    for (int i = 0; i < n; ++i) {
        QPointF target(startX + i * (CARD_W + 10), y);
        mPlayedCards[i]->moveTo(target, 280);
    }

    mGameState->playCards(sortedIdx);   // ← 改用 sortedIdx,不是 mSelected
}

// 弃牌
void MainWindow::onDiscardClicked() {
    if (mSelected.isEmpty()) return;

    QVector<int> sortedIdx = mSelected;
    std::sort(sortedIdx.begin(), sortedIdx.end());
    mSelected.clear();

    // 把选中的卡从 mHandCards 抽出来,做"飞出屏幕底 + 淡出"动画
    for (int i = sortedIdx.size() - 1; i >= 0; --i) {
        int idx = sortedIdx[i];
        CardItem *c = mHandCards.takeAt(idx);
        c->setCardSelected(false);
        c->setZValue(5);   // 低 z,新摸的牌覆盖在上面

        // 同时做两个动画:下移 + 淡出
        QPointF target(mSceneW + CARD_W, c->pos().y());   // ← 飞出屏幕右
        c->moveTo(target, 350);

        auto *fade = new QPropertyAnimation(c, "opacity", this);
        fade->setDuration(350);
        fade->setStartValue(1.0);
        fade->setEndValue(0.0);
        fade->setEasingCurve(QEasingCurve::InQuad);
        // 动画结束后销毁 item
        connect(fade, &QPropertyAnimation::finished, c, [this, c]() {
            mScene->removeItem(c);
            c->deleteLater();
        });
        fade->start(QAbstractAnimation::DeleteWhenStopped);
    }

    layoutHandCards();          // 剩余卡合拢
    mGameState->discardCards(sortedIdx);
}

void MainWindow::onHandPlayed()
{
    const HandResult &r = mGameState->lastResult();

    mLblHandName ->setText(r.name);
    mLblHandLevel->setText(QString("等级%1").arg(r.level));

    // 起步值
    mDisplayedChips = r.baseChips;
    mDisplayedMult  = r.baseMult;
    double displayedXmult = 1.0;
    mLblChips->setText(QString::number(mDisplayedChips));
    mLblMult ->setText(QString::number(mDisplayedMult));

    int delayBase = 350;
    int delayStep = 220;

    // mPlayedCards 顺序对应 scoringCards 的 played 下标(sortedIdx 顺序)
    // 但 ScoreEvent::sourceCardIdx 是 played 数组的下标,要映射到 mPlayedCards 视图

    // 简化:按事件出现顺序依次播放
    for (int ei = 0; ei < r.events.size(); ++ei) {
        const ScoreEvent &ev = r.events[ei];
        int delay = delayBase + ei * delayStep;

        QTimer::singleShot(delay, this, [this, ev]() {
            playScoreEvent(ev);
        });
    }

    // 校正到最终值
    int finalDelay = delayBase + r.events.size() * delayStep + 250;
    QTimer::singleShot(finalDelay, this, [this, r]() {
        mLblChips->setText(QString::number(r.chips));
        mLblMult ->setText(QString::number(r.mult));
    });
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
        clearObtainedTags();
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
    setContextPage(0);
    setPlayPhaseVisible(false);       // ← 隐藏对局元素
    clearPlayedCards();                // ← 清上轮出牌
    mBlindSelectWidget->refresh();
    mBlindSelectWidget->raise();
    mBlindSelectWidget->show();

    bool skipped = mGameState->justSkipped();
    QTimer::singleShot(0, this, [this, skipped]() {
        if (mBlindSelectWidget && mPlayPage
            && mBlindSelectWidget->geometry() != mPlayPage->rect())
        {
            mBlindSelectWidget->setGeometry(mPlayPage->rect());
        }
        mBlindSelectWidget->arrangeCards(!skipped);
    });
}

void MainWindow::onBlindStarted()
{
    clearFloatingScores();
    mBlindSelectWidget->hide();
    mShopWidget->hide();
    mRoundEndOverlay->hide();
    setContextPage(1);
    setPlayPhaseVisible(true);        // ← 显示对局元素

    refreshHand();
    refreshScore();
    refreshGold();
    refreshCounters();
    refreshJokerSlots();
    refreshConsumableSlots();
    clearPlayedCards();
    mLblChips->setText("0");
    mLblMult ->setText("0");
}

void MainWindow::onNextBlindClicked()
{
    clearFloatingScores();
    mRoundEndOverlay->hide();
    mShopWidget->refresh();
    mShopWidget->raise();
    mShopWidget->show();
    setContextPage(2);
    setPlayPhaseVisible(false);       // ← 隐藏对局元素
    clearPlayedCards();                // ← 清出牌
    QTimer::singleShot(0, this, [this]() {
        if (mShopWidget && mPlayPage)
            mShopWidget->setGeometry(mPlayPage->rect());
    });
}

void MainWindow::onRoundWon(int blindReward, int handBonus, int interest)
{
    refreshGold();

    // 计算 blind chip row 用于显示
    int chipRow = 0;
    switch (mGameState->blindType()) {
    case BlindType::Small: chipRow = 0; break;
    case BlindType::Big:   chipRow = 1; break;
    case BlindType::Boss:
        switch (mGameState->bossEffect()) {
        case BossEffect::TheHook:   chipRow = 7;  break;
        case BossEffect::TheClub:   chipRow = 4;  break;
        case BossEffect::TheWall:   chipRow = 9;  break;
        case BossEffect::ThePlant:  chipRow = 19; break;
        case BossEffect::TheNeedle: chipRow = 20; break;
        default: chipRow = 7; break;
        }
        break;
    }

    mRoundEndOverlay->setData(
        chipRow,
        mGameState->targetScore(),
        blindReward,
        mGameState->handsLeft(),  handBonus,
        interest
        );

    // 延迟弹出(等浮动分跑完)
    int eventCount = mGameState->lastResult().events.size();
    int delay = 350 + eventCount * 220 + 900 + 300;
    QTimer::singleShot(delay, this, [this]() {
        mRoundEndOverlay->raise();
        mRoundEndOverlay->show();
        if (mPlayPage) mRoundEndOverlay->setGeometry(mPlayPage->rect());
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
    addObtainedTag(0, 3);    // tag_skip 坐标 (col=0, row=3)
}
void MainWindow::addObtainedTag(int tagCol, int tagRow)
{
    QPixmap sheet(":/textures/images/tags.png");
    if (sheet.isNull()) return;

    QPixmap pix = sheet.copy(tagCol * 68, tagRow * 68, 68, 68)
                      .scaled(48, 48, Qt::KeepAspectRatio,
                              Qt::SmoothTransformation);

    auto *item = new QGraphicsPixmapItem(pix);
    // 排在牌堆左边,从右往左累积
    int idx = mObtainedTagIcons.size();
    int x = mSceneW - CARD_W - 10 - 60 - idx * 56;   // 牌堆 x - 间距 - 累积偏移
    int y = mSceneH - CARD_H - 36 + (CARD_H - 48) / 2;   // 跟牌堆中线对齐
    item->setPos(x, y);
    item->setZValue(5);
    mScene->addItem(item);
    mObtainedTagIcons.append(item);
}

void MainWindow::clearObtainedTags()
{
    for (auto *it : mObtainedTagIcons) {
        mScene->removeItem(it);
        delete it;
    }
    mObtainedTagIcons.clear();
}

void MainWindow::setPlayPhaseVisible(bool v)
{
    if (mPlayProxy)      mPlayProxy->setVisible(v);
    if (mSortProxy)      mSortProxy->setVisible(v);
    if (mDiscardProxy)   mDiscardProxy->setVisible(v);
    if (mHandCountLabel) mHandCountLabel->setVisible(v);
    for (auto *c : mHandCards)   c->setVisible(v);
    for (auto *c : mPlayedCards) c->setVisible(v);
}

void MainWindow::spawnFloatingText(const QPointF &nearPos, const QString &text, const QColor &color)
{
    auto *fs = new FloatingScore(text, color, mPixelFont);
    fs->setZValue(100);
    // 卡片顶部中央正上方一点点(不浮动,位置静止)
    QPointF center = nearPos + QPointF(CARD_W / 2, -20);
    fs->setPos(center);
    mScene->addItem(fs);

    // hold 600ms + fade 300ms
    auto *pause = new QPauseAnimation(600);
    auto *fade  = new QPropertyAnimation(fs, "opacity");
    fade->setDuration(300);
    fade->setStartValue(1.0);
    fade->setEndValue(0.0);
    fade->setEasingCurve(QEasingCurve::InQuad);

    auto *seq = new QSequentialAnimationGroup(this);
    seq->addAnimation(pause);
    seq->addAnimation(fade);
    pause->setParent(seq);
    fade->setParent(seq);

    connect(seq, &QAbstractAnimation::finished, fs, [this, fs]() {
        if (fs->scene()) mScene->removeItem(fs);
        fs->deleteLater();
    });
    seq->start(QAbstractAnimation::DeleteWhenStopped);

    mFloatingScores.append(fs);
    connect(fs, &QObject::destroyed, this, [this, fs]() {
        mFloatingScores.removeAll(fs);
    });
}

void MainWindow::clearFloatingScores()
{
    for (auto *fs : mFloatingScores) {
        if (fs->scene()) mScene->removeItem(fs);
        fs->deleteLater();
    }
    mFloatingScores.clear();
}

void MainWindow::updateHandPreview()
{
    if (mSelected.isEmpty()) {
        mLblHandName ->setText("");
        mLblHandLevel->setText("");
        mLblChips->setText("0");
        mLblMult ->setText("0");
        return;
    }
    HandResult r = mGameState->previewSelection(mSelected);
    mLblHandName ->setText(r.name);
    mLblHandLevel->setText(QString("等级%1").arg(r.level));
    mLblChips->setText(QString::number(r.chips));
    mLblMult ->setText(QString::number(r.mult));
}

void MainWindow::playScoreEvent(const ScoreEvent &ev)
{
    // 找到事件来源的视觉目标(卡片或小丑)
    CardItem *sourceCard = nullptr;
    JokerItem *sourceJoker = nullptr;

    if (ev.sourceCardIdx >= 0 && ev.sourceCardIdx < mPlayedCards.size())
        sourceCard = mPlayedCards[ev.sourceCardIdx];
    else if (ev.sourceHandIdx >= 0 && ev.sourceHandIdx < mHandCards.size())
        sourceCard = mHandCards[ev.sourceHandIdx];

    if (ev.sourceJokerIdx >= 0 && ev.sourceJokerIdx < mJokerItems.size())
        sourceJoker = mJokerItems[ev.sourceJokerIdx];

    // 浮动分位置
    QPointF anchorPos;
    if (sourceCard) anchorPos = sourceCard->pos();
    else if (sourceJoker) anchorPos = sourceJoker->pos();
    else anchorPos = QPointF(mSceneW / 2, mBtnY);   // 兜底:屏幕中央

    // 颜色
    QColor color;
    QString text;
    bool isXMult = false;

    switch (ev.kind) {
    case ScoreEventKind::ScoringCardChip:
    case ScoreEventKind::EditionChip:
    case ScoreEventKind::JokerChip:
        color = QColor("#009dff");   // 蓝
        text = QString("+%1").arg(ev.intValue);
        mDisplayedChips += ev.intValue;
        mLblChips->setText(QString::number(mDisplayedChips));
        break;

    case ScoreEventKind::EnhancementMult:
    case ScoreEventKind::EditionMult:
    case ScoreEventKind::JokerMult:
        color = QColor("#fe5f55");   // 红
        text = QString("+%1").arg(ev.intValue);
        mDisplayedMult += ev.intValue;
        mLblMult->setText(QString::number(mDisplayedMult));
        break;

    case ScoreEventKind::EnhancementXMult:
    case ScoreEventKind::EditionXMult:
    case ScoreEventKind::SteelXMult:
    case ScoreEventKind::JokerXMult:
        color = QColor("#fe5f55");   // 红
        text = QString("×%1").arg(QString::number(ev.xmultValue, 'g', 3));
        isXMult = true;
        // xmult 不直接累加到 mLblMult,等最终校正
        break;
    }

    // 来源 juice
    if (sourceCard) sourceCard->juiceUp(1.15, 200);
    if (sourceJoker) {
        // JokerItem 需要也有 juiceUp,见后面 §6
        sourceJoker->juiceUp(1.15, 200);
    }

    // 浮动分(目前 spawnFloatingScore 只接受 int,扩展支持文本)
    spawnFloatingText(anchorPos, text, color);
}
