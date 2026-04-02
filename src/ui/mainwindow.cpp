#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QPainter>
#include <QHBoxLayout>
#include <QGraphicsDropShadowEffect>

static QPushButton *makeBtn(const QString &text, const QString &bg, const QString &hover, QWidget *parent) {
    QPushButton *btn = new QPushButton(text, parent);
    btn->setFixedHeight(52);
    btn->setStyleSheet(QString(
        "QPushButton {"
        " background: %1; color: white;"
        " border: none; border-radius: 8px;"
        " font-size: 16px; font-weight: bold;"
        "}"
        "QPushButton:hover { background: %2; }"
        "QPushButton:pressed { background: %2; }"
        "QPushButtton:disabled { background: #444; color: #888; }"
    ).arg(bg, hover));
    return btn;
}

static QLabel *makeInfoLabel(const QString &text, int fontSize, const QString &color, QWidget *parent) {
    QLabel *lbl = new QLabel(text, parent);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold;").arg(color).arg(fontSize));
    return lbl;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , mGameState(new GameState(this))
{
    ui->setupUi(this);
    setFixedSize(WIN_W, WIN_H);
    setWindowTitle("BalatroQt");

    mScene = new QGraphicsScene(this);
    mView = new QGraphicsView(mScene, this);

    setupLeftPanel();
    setupScene();
    setupConnections();

    refreshHand();
    refreshScore();
    refreshGold();
    refreshCounters();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupLeftPanel() {
    mLeftPanel = new QWidget(this);
    mLeftPanel->setGeometry(0, 0, LEFT_W, WIN_H);
    mLeftPanel->setStyleSheet("background: #1a1f2e;");

    auto *layout = new QVBoxLayout(mLeftPanel);
    layout->setContentsMargins(12, 16, 12, 16);
    layout->setSpacing(8);

    // 目标分数
    QLabel *lblTargetTitle = new QLabel("目标分数", mLeftPanel);
    lblTargetTitle->setStyleSheet("color: #888; font-size: 12px;");
    lblTargetTitle->setAlignment(Qt::AlignCenter);
    layout->addWidget(lblTargetTitle);

    mLblTarget = makeInfoLabel("✳ 300", 18, "#ffffff", mLeftPanel);
    layout->addWidget(mLblTarget);

    layout->addSpacing(4);

    // 回合得分
    QLabel *lblScoreTitle = new QLabel("回合分数", mLeftPanel);
    lblScoreTitle->setStyleSheet("color: #888; font-size: 12px;");
    lblScoreTitle->setAlignment(Qt::AlignCenter);
    layout->addWidget(lblScoreTitle);

    mLblScore = makeInfoLabel("✳ 0", 18, "#ffffff", mLeftPanel);
    layout->addWidget(mLblScore);

    layout->addSpacing(4);

    // 筹码 × 倍率
    QWidget *chipsRow = new QWidget(mLeftPanel);
    QHBoxLayout *chipsLayout = new QHBoxLayout(chipsRow);
    chipsLayout->setContentsMargins(0, 0, 0, 0);
    chipsLayout->setSpacing(4);

    mLblChips = new QLabel("0", chipsRow);
    mLblChips->setAlignment(Qt::AlignCenter);
    mLblChips->setStyleSheet(
        "background: #4a90d9; color: white;"
        "font-size: 28px; font-weight: bold;"
        "border-radius: 6px; padding: 4px;"
        );

    auto *lblX = new QLabel("×", chipsRow);
    lblX->setAlignment(Qt::AlignCenter);
    lblX->setStyleSheet("color: white; font-size: 22px; font-weight: bold;");
    lblX->setFixedWidth(24);

    mLblMult = new QLabel("0", chipsRow);
    mLblMult->setAlignment(Qt::AlignCenter);
    mLblMult->setStyleSheet(
        "background: #d94a4a; color: white;"
        "font-size: 28px; font-weight: bold;"
        "border-radius: 6px; padding: 4px;"
        );

    chipsLayout->addWidget(mLblChips);
    chipsLayout->addWidget(lblX);
    chipsLayout->addWidget(mLblMult);
    layout->addWidget(chipsRow);

    layout->addSpacing(8);

    // 出牌/弃牌次数
    QWidget *countersRow = new QWidget(mLeftPanel);
    QHBoxLayout *countersLayout = new QHBoxLayout(countersRow);
    countersLayout->setContentsMargins(0, 0, 0, 0);
    countersLayout->setSpacing(8);

    QWidget *handsBox = new QWidget(countersRow);
    QVBoxLayout *handsLayout = new QVBoxLayout(handsBox);
    handsLayout->setContentsMargins(0, 0, 0, 0);
    handsLayout->setSpacing(2);
    QLabel *handsTitle = new QLabel("出牌", handsBox);
    handsTitle->setStyleSheet("color: #888; font-size: 12px;");
    handsTitle->setAlignment(Qt::AlignCenter);
    mLblHands = makeInfoLabel("4", 22, "#4a90d9", handsBox);
    handsLayout->addWidget(handsTitle);
    handsLayout->addWidget(mLblHands);

    QWidget *discBox = new QWidget(countersRow);
    QVBoxLayout *discLayout = new QVBoxLayout(discBox);
    discLayout->setContentsMargins(0, 0, 0, 0);
    discLayout->setSpacing(2);
    QLabel *discTitle = new QLabel("弃牌", discBox);
    discTitle->setStyleSheet("color: #888; font-size: 12px;");
    discTitle->setAlignment(Qt::AlignCenter);
    mLblDiscards = makeInfoLabel("3", 22, "#e07030", discBox);
    discLayout->addWidget(discTitle);
    discLayout->addWidget(mLblDiscards);

    countersLayout->addWidget(handsBox);
    countersLayout->addWidget(discBox);
    layout->addWidget(countersRow);

    layout->addSpacing(4);

    // 金币
    mLblGold = makeInfoLabel("$4", 26, "#f0c040", mLeftPanel);
    layout->addWidget(mLblGold);

    layout->addSpacing(8);

    // 出牌按钮
    mBtnPlay = makeBtn("出牌", "#c03030", "#a02020", mLeftPanel);
    layout->addWidget(mBtnPlay);

    // ── 弃牌按钮 ──
    mBtnDiscard = makeBtn("弃牌", "#c07820", "#a06010", mLeftPanel);
    layout->addWidget(mBtnDiscard);

    layout->addStretch();

    // 底注/回合
    mLblAnte = makeInfoLabel("底注 1/8  回合 1", 13, "#aaaaaa", mLeftPanel);
    layout->addWidget(mLblAnte);
}

void MainWindow::setupScene() {
    mView->setGeometry(LEFT_W, 0, SCENE_W, SCENE_H);
    mView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mView->setRenderHint(QPainter::Antialiasing);
    mView->setRenderHint(QPainter::SmoothPixmapTransform);
    mView->setFrameShape(QFrame::NoFrame);
    mView->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);

    mScene->setSceneRect(0, 0, SCENE_W, SCENE_H);
    mScene->setBackgroundBrush(QColor("#2b6a4a"));

    // 小丑槽位占位框
    for (int i = 0; i < 5; ++i) {
        int x = 20 + i * (CARD_W + 12);
        auto *slot = mScene->addRect(x, JOKER_Y, CARD_W, CARD_H,
            QPen(QColor(255, 255, 255, 50), 2, Qt::DashLine),
            QBrush(QColor(0, 0, 0, 40)));
        slot->setZValue(0);
    }

    // 小丑槽位计数
    auto *jokerCount = mScene->addText("0/5");
    jokerCount->setDefaultTextColor(QColor(180, 220, 180));
    jokerCount->setPos(12, JOKER_Y - 2);
    jokerCount->setFont(QFont("Arial", 11));

    // 消耗牌槽位占位框
    for (int i = 0; i < 2; ++i) {
        int x = SCENE_W - 20 - (2 - i) * (CARD_W + 12);
        mScene->addRect(x, JOKER_Y, CARD_W, CARD_H,
            QPen(QColor(255, 255, 255, 50), 2, Qt::DashLine),
            QBrush(QColor(0, 0, 0, 40)));
    }

    // 消耗牌槽位计数
    auto *consCount = mScene->addText("0/2");
    consCount->setDefaultTextColor(QColor(180, 220, 180));
    consCount->setPos(SCENE_W - 50, JOKER_Y - 2);
    consCount->setFont(QFont("Arial", 11));

    // 出牌区背景
    mScene->addRect(10, PLAY_Y, SCENE_W - 20, PLAY_H,
        QPen(QColor(255, 255, 255, 30), 1),
        QBrush(QColor(0, 0, 0, 30)));

    // 牌堆（用牌背展示）
    CardData backData;
    backData.faceUp = false;
    CardItem *deckCard = new CardItem(backData);
    deckCard->setPos(SCENE_W - CARD_W - 15, SCENE_H - CARD_H - 30);
    deckCard->setZValue(1);
    mScene->addItem(deckCard);

    auto *deckLabel = mScene->addText("52/52");
    deckLabel->setDefaultTextColor(QColor(200, 200, 200));
    deckLabel->setPos(SCENE_W - CARD_W - 10, SCENE_H - 28);
    deckLabel->setFont(QFont("Arial", 10));
}

void MainWindow::setupConnections() {
    connect(mBtnPlay, &QPushButton::clicked, this, &MainWindow::onPlayClicked);
    connect(mBtnDiscard, &QPushButton::clicked, this, &MainWindow::onDiscardClicked);

    connect(mGameState, &GameState::handChanged, this, &MainWindow::refreshHand);
    connect(mGameState, &GameState::scoreChanged, this, &MainWindow::refreshScore);
    connect(mGameState, &GameState::goldChanged, this, &MainWindow::refreshGold);
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

    // 保留左右各 20px 边距，最后一张牌完整显示
    int available = SCENE_W - 40;
    int step = (n > 1) ? (available - CARD_W) / (n - 1) : 0;

    // 所有牌的总占宽
    int totalW = (n - 1) * step + CARD_W;
    int startX = (SCENE_W - totalW) / 2;

    for (int i = 0; i < n; ++i) {
        bool sel = mSelected.contains(i);
        mHandCards[i]->setZValue(sel ? 50 + i : 10 + i);
        mHandCards[i]->setPos(startX + i * step,
            sel ? HAND_Y - 30 : HAND_Y);
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

    int totalW = n * CARD_W + (n - 1) * 8;
    int startX = (SCENE_W - totalW) / 2;
    int y = PLAY_Y + (PLAY_H - CARD_H) / 2;

    for (int i = 0; i < n; ++i)
        mPlayedCards[i]->setPos(startX + i * (CARD_W + 8), y);
}

// 分数刷新
void MainWindow::refreshScore() {
    mLblScore->setText(QString("✳ %1").arg(mGameState->score()));
    mLblTarget->setText(QString("✳ %1").arg(mGameState->targetScore()));
    // 筹码和倍率暂时先用当前分数简单显示，等 HandResult 暴露接口后再细化
    mLblChips->setText(QString::number(mGameState->score()));
    mLblMult->setText("×1");
}

// 金币刷新
void MainWindow::refreshGold() {
    mLblGold->setText(QString("$%1").arg(mGameState->gold()));
}

// 出牌/弃牌次数刷新
void MainWindow::refreshCounters() {
    mLblHands->setText(QString::number(mGameState->handsLeft()));
    mLblDiscards->setText(QString::number(mGameState->discardLeft()));
    mLblAnte->setText(QString("底注 %1/8  回合 %2")
        .arg(mGameState->ante())
        .arg(static_cast<int>(mGameState->blindType()) + 1));

    mBtnPlay->setEnabled(mGameState->handsLeft() > 0 && !mSelected.isEmpty());
    mBtnDiscard->setEnabled(mGameState->discardLeft() > 0 && !mSelected.isEmpty());
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
