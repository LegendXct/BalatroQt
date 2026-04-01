#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QPainter>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , mScene(nullptr)
    , mView(nullptr)
{
    ui->setupUi(this);

    mScene = new QGraphicsScene(this);
    mView = new QGraphicsView(mScene, this);

    setupScene();
    addTestCards();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupScene() {
    setFixedSize(1280, 720);

    mScene->setSceneRect(0, 0, 1280, 720);
    mScene->setBackgroundBrush(QColor("#2b6a4a"));

    mView->setGeometry(0, 0, 1280, 720);
    mView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mView->setRenderHint(QPainter::Antialiasing);
    mView->setRenderHint(QPainter::SmoothPixmapTransform);
}

void MainWindow::addTestCards() {
    // ── 第一排：四种花色的 A，验证花色行号 ──────────────────────────
    const Suit suits[] = {
        Suit::Hearts, Suit::Clubs,
        Suit::Diamonds, Suit::Spades
    };
    for (int i = 0; i < 4; ++i) {
        CardData d;
        d.suit  = suits[i];
        d.rank  = Rank::Ace;
        auto *card = new CardItem(d);
        mScene->addItem(card);
        card->setPos(50 + i * 170, 40);
    }

    // ── 第二排：增强叠加层 ──────────────────────────────────────────
    const Enhancement enhs[] = {
        Enhancement::Bonus, Enhancement::Mult,
        Enhancement::Glass, Enhancement::Stone,
        Enhancement::Wild,  Enhancement::Lucky,
        Enhancement::Steel
    };
    for (int i = 0; i < 7; ++i) {
        CardData d;
        d.suit        = Suit::Hearts;
        d.rank        = Rank::King;
        d.enhancement = enhs[i];
        auto *card = new CardItem(d);
        mScene->addItem(card);
        card->setPos(50 + i * 170, 260);
    }

    // ── 第三排：印章 + 牌背 ─────────────────────────────────────────
    const Seal seals[] = {
        Seal::Red, Seal::Blue,
        Seal::Purple, Seal::Gold
    };
    for (int i = 0; i < 4; ++i) {
        CardData d;
        d.suit = Suit::Spades;
        d.rank = Rank::Queen;
        d.seal = seals[i];
        auto *card = new CardItem(d);
        mScene->addItem(card);
        card->setPos(50 + i * 170, 480);
    }

    // 牌背
    CardData back;
    back.faceUp = false;
    auto *backCard = new CardItem(back);
    mScene->addItem(backCard);
    backCard->setPos(730, 480);
}
