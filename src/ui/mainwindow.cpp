#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QPainter>
#include <QFontDatabase>
#include <QGraphicsProxyWidget>
#include <algorithm>

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

    QScreen *screen = QGuiApplication::primaryScreen();
    QRect screenRect = screen->geometry();
    int winW = screenRect.width();
    int winH = screenRect.height();
    mWinW = winW;
    mWinH = winH;
    mSceneW = winW - mLeftW;
    mSceneH = winH;
    mBtnY = mSceneH - 80;
    mHandY = mSceneH - CARD_H - 90;

    loadFonts();

    mScene = new QGraphicsScene(this);
    mView = new QGraphicsView(mScene, this);

    setupLeftPanel();
    setupScene();
    setupSceneButtons();
    setupConnections();

    refreshHand();
    refreshScore();
    refreshGold();
    refreshCounters();

    // ── 创建过关结算 overlay ──
    mRoundEndOverlay = new RoundEndOverlay(mCNFont, mPixelFont, this);
    mRoundEndOverlay->setGeometry(rect());
    connect(mRoundEndOverlay, &RoundEndOverlay::nextClicked,
            this, &MainWindow::onNextBlindClicked);

    // 创建商店 overlay
    mShopOverlay = new ShopWidget(mGameState, mCNFont, mPixelFont, this);
    mShopOverlay->setGeometry(rect());
    connect(mShopOverlay, &ShopWidget::leaveClicked,
            this, &MainWindow::onLeaveShopClicked);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupLeftPanel() {
    mLeftPanel = new QWidget(this);
    mLeftPanel->setGeometry(0, 0, mLeftW, mWinH);
    mLeftPanel->setStyleSheet(
        "background: #1e2230;"
    );

    QVBoxLayout *layout = new QVBoxLayout(mLeftPanel);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(6);

    // 盲注名称
    mLblBlind = makeLabel("小盲注", 20, "#f0c040", mCNFont, mLeftPanel);
    mLblBlind->setStyleSheet(
        "color:#1a1000; background:#c07820;"
        "border-radius:8px; padding:4px;");
    mLblBlind->setFixedHeight(38);
    layout->addWidget(mLblBlind);

    // 目标分数框
    QWidget *targetBox = new QWidget(mLeftPanel);
    targetBox->setStyleSheet("background:#161b28; border-radius:8px;");
    QVBoxLayout *tbl = new QVBoxLayout(targetBox);
    tbl->setContentsMargins(8, 6, 8, 6);
    tbl->setSpacing(2);
    QLabel *tTitle = makeLabel("至少得分", 12, "#888", mCNFont, targetBox);
    mLblTarget  = makeLabel("✳ 300", 22, "#e04040", mPixelFont, targetBox);
    mLblReward  = makeLabel("奖励: $$$", 13, "#f0c040", mCNFont, targetBox);
    tbl->addWidget(tTitle);
    tbl->addWidget(mLblTarget);
    tbl->addWidget(mLblReward);
    layout->addWidget(targetBox);

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

    // 金币
    mLblGold = makeLabel("$4", 28, "#f0c040", mPixelFont, mLeftPanel);
    mLblGold->setStyleSheet(
        "color:#f0c040; background:#161b28;"
        "border-radius:8px; padding:4px;");
    layout->addWidget(mLblGold);

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
    mView->setGeometry(mLeftW, 0, mSceneW, mSceneH);
    mView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mView->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    mView->setFrameShape(QFrame::NoFrame);
    mView->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);

    mScene->setSceneRect(0, 0, mSceneW, mSceneH);
    mScene->setBackgroundBrush(QColor("#2b6a4a"));

    // 小丑槽位（左上）
    auto *jokerCountItem = mScene->addText("0/5");
    jokerCountItem->setDefaultTextColor(QColor("#aaddaa"));
    jokerCountItem->setFont(mCNFont);
    jokerCountItem->setPos(8, 4);

    for (int i = 0; i < 5; ++i) {
        int x = 8 + i * (CARD_W + 14);
        mScene->addRect(x, JOKER_Y + 18, CARD_W, CARD_H,
            QPen(QColor(255,255,255,60), 2, Qt::DashLine),
            QBrush(QColor(0,0,0,50)));
    }

    // 消耗品槽位（右上）
    auto *consCountItem = mScene->addText("0/2");
    consCountItem->setDefaultTextColor(QColor("#aaddaa"));
    consCountItem->setFont(mCNFont);
    consCountItem->setPos(mSceneW - 52, 4);

    for (int i = 0; i < 2; ++i) {
        int x = mSceneW - 8 - (2 - i) * (CARD_W + 14);
        mScene->addRect(x, JOKER_Y + 18, CARD_W, CARD_H,
            QPen(QColor(255,255,255,60), 2, Qt::DashLine),
            QBrush(QColor(0,0,0,50)));
    }

    // 出牌区背景
    mScene->addRect(10, PLAY_Y, mSceneW - 16, PLAY_H,
        QPen(QColor(0, 0, 0, 0)),
        QBrush(QColor(0, 0, 0, 25)));

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
    auto *deckCard = new CardItem(backData);
    deckCard->setPos(mSceneW - CARD_W - 10,
        mSceneH - CARD_H - 36);
    deckCard->setZValue(1);
    mScene->addItem(deckCard);

    mDeckLabel = mScene->addText("52/52");
    QFont df = mCNFont; df.setPixelSize(12);
    mDeckLabel->setFont(df);
    mDeckLabel->setDefaultTextColor(QColor("#cccccc"));
    mDeckLabel->setPos(mSceneW - CARD_W - 6, mSceneH - 28);
    mDeckLabel->setZValue(2);
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

    connect(mGameState, &GameState::roundWon, this, &MainWindow::onRoundWon);
    connect(mGameState, &GameState::gameOver, this, &MainWindow::onGameOver);
    connect(mGameState, &GameState::jokersChanged, this, &MainWindow::refreshJokerSlots);
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
    mLblGold->setText(QString("$%1").arg(mGameState->gold()));
}

// 出牌/弃牌次数刷新
void MainWindow::refreshCounters() {
    mLblHands->setText(QString::number(mGameState->handsLeft()));
    mLblDiscards->setText(QString::number(mGameState->discardLeft()));
    mLblAnte->setText(QString("%1/8").arg(mGameState->ante()));
    mLblRound->setText(QString::number(
        static_cast<int>(mGameState->blindType()) + 1));

    // 盲注名称
    switch (mGameState->blindType()) {
    case BlindType::Small: mLblBlind->setText("小盲注"); break;
    case BlindType::Big: mLblBlind->setText("大盲注"); break;
    case BlindType::Boss: mLblBlind->setText("Boss 盲注"); break;
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

void MainWindow::onHandPlayed() {
    const HandResult &r = mGameState->lastResult();

    // 更新筹码×倍率
    mLblChips->setText(QString::number(r.chips));
    mLblMult->setText(QString::number(r.mult));

    // 显示牌型名称，居中
    mHandTypeLabel->setPlainText(r.name);
    QRectF tb = mHandTypeLabel->boundingRect();
    mHandTypeLabel->setPos((mSceneW - tb.width()) / 2, PLAY_Y + 10);

    // 显示分数
    mHandScoreLabel->setPlainText(QString("%1  ×  %2  =  %3").arg(r.chips).arg(r.mult).arg(r.chips * r.mult));
    QRectF sb = mHandScoreLabel->boundingRect();
    mHandScoreLabel->setPos((mSceneW - sb.width()) / 2,PLAY_Y + 46);
}

void MainWindow::onSortByNum() {
    mGameState->sortHandByRank();
}

void MainWindow::onSortBySuit() {
    mGameState->sortHandBySuit();
}

void MainWindow::onRoundWon(int blindReward, int handBonus, int interest)
{
    QString blindName;
    switch (mGameState->blindType()) {
    case BlindType::Small: blindName = "小盲注";    break;
    case BlindType::Big:   blindName = "大盲注";    break;
    case BlindType::Boss:  blindName = "Boss 盲注"; break;
    }

    // 刷新左面板金币（GameState 在发 roundWon 之前已经加过钱了）
    refreshGold();

    // 填数据并显示
    mRoundEndOverlay->setData(blindName,
                              mGameState->score(),
                              mGameState->targetScore(),
                              blindReward, handBonus, interest);
    mRoundEndOverlay->setGeometry(rect());
    mRoundEndOverlay->raise();
    mRoundEndOverlay->show();
}

void MainWindow::onNextBlindClicked()
{
    mRoundEndOverlay->hide();

    // 弹出商店
    mShopOverlay->refresh();
    mShopOverlay->setGeometry(rect());
    mShopOverlay->raise();
    mShopOverlay->show();
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
        mGameState->startGame();
        mGameOverHandled = false;

        clearPlayedCards();
        refreshHand();
        refreshScore();
        refreshGold();
        refreshCounters();
        if (mHandTypeLabel)  mHandTypeLabel ->setPlainText("");
        if (mHandScoreLabel) mHandScoreLabel->setPlainText("");
        mLblChips->setText("0");
        mLblMult ->setText("0");
    } else {
        close();
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (mRoundEndOverlay) mRoundEndOverlay->setGeometry(rect());
    if (mShopOverlay)     mShopOverlay    ->setGeometry(rect());
}

void MainWindow::onLeaveShopClicked()
{
    mShopOverlay->hide();

    mGameState->leaveShop();   // → nextBlind → startBlind → handChanged

    refreshHand();
    refreshScore();
    refreshGold();
    refreshCounters();
    refreshJokerSlots();

    clearPlayedCards();
    if (mHandTypeLabel)  mHandTypeLabel ->setPlainText("");
    if (mHandScoreLabel) mHandScoreLabel->setPlainText("");
    mLblChips->setText("0");
    mLblMult ->setText("0");
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
