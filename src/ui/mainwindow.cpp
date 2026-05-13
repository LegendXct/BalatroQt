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
#include <QMenu>
#include <QPropertyAnimation>
#include <QCursor>
#include <QStringList>
#include "shopsignwidget.h"
#include <QParallelAnimationGroup>
#include <QVariantAnimation>

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


static QString enhancementName(Enhancement e) {
    switch (e) {
    case Enhancement::Bonus: return "奖励牌";
    case Enhancement::Mult: return "倍率牌";
    case Enhancement::Wild: return "万能牌";
    case Enhancement::Glass: return "玻璃牌";
    case Enhancement::Steel: return "钢铁牌";
    case Enhancement::Stone: return "石头牌";
    case Enhancement::Gold: return "黄金牌";
    case Enhancement::Lucky: return "幸运牌";
    default: return "普通牌";
    }
}

static QString enhancementDesc(Enhancement e) {
    switch (e) {
    case Enhancement::Bonus: return "+30 筹码";
    case Enhancement::Mult: return "+4 倍率";
    case Enhancement::Wild: return "可视作任意花色";
    case Enhancement::Glass: return "计分时 ×2 倍率，之后有概率破碎";
    case Enhancement::Steel: return "留在手牌中时 ×1.5 倍率";
    case Enhancement::Stone: return "+50 筹码，没有点数与花色";
    case Enhancement::Gold: return "回合结束若仍在手牌中，获得 $3";
    case Enhancement::Lucky: return "概率获得 +20 倍率或 $20";
    default: return "基础牌面筹码";
    }
}

static QString editionName(Edition e) {
    switch (e) {
    case Edition::Foil: return "闪箔";
    case Edition::Holographic: return "镭射";
    case Edition::Polychrome: return "多彩";
    case Edition::Negative: return "负片";
    default: return "";
    }
}

static QString editionDesc(Edition e) {
    switch (e) {
    case Edition::Foil: return "+50 筹码";
    case Edition::Holographic: return "+10 倍率";
    case Edition::Polychrome: return "×1.5 倍率";
    case Edition::Negative: return "+1 持有槽位";
    default: return "";
    }
}

static QString sealName(Seal s) {
    switch (s) {
    case Seal::Gold: return "金色蜡封";
    case Seal::Red: return "红色蜡封";
    case Seal::Blue: return "蓝色蜡封";
    case Seal::Purple: return "紫色蜡封";
    default: return "";
    }
}

static QString sealDesc(Seal s) {
    switch (s) {
    case Seal::Gold: return "打出并计分后获得 $3";
    case Seal::Red: return "重新触发这张牌 1 次";
    case Seal::Blue: return "回合结束时生成对应星球牌";
    case Seal::Purple: return "弃掉时生成一张塔罗牌";
    default: return "";
    }
}

static QString cardTooltipTitle(const CardData &c) {
    if (c.enhancement == Enhancement::Stone) return "石头牌";
    QString title = c.toString();
    if (c.enhancement != Enhancement::None) title += " · " + enhancementName(c.enhancement);
    return title;
}

static QString cardTooltipBody(const CardData &c) {
    QStringList lines;
    lines << enhancementDesc(c.enhancement);
    if (c.edition != Edition::None)
        lines << editionName(c.edition) + "：" + editionDesc(c.edition);
    if (c.seal != Seal::None)
        lines << sealName(c.seal) + "：" + sealDesc(c.seal);
    if (c.isDebuffed)
        lines << "被 Boss 盲注禁用：本张牌不会触发效果";
    return lines.join("\n");
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

    mPackOpenWidget = new PackOpenWidget(mCNFont, mPixelFont, mPlayPage);
    mPackOpenWidget->hide();
    connect(mPackOpenWidget, &PackOpenWidget::choiceMade,
            this, &MainWindow::onPackChoiceMade);
    connect(mPackOpenWidget, &PackOpenWidget::inventoryConsumableRequested,
            this, &MainWindow::onInventoryConsumableUseRequested);
    connect(mPackOpenWidget, &PackOpenWidget::packFinished,
            this, &MainWindow::onPackFinished);

    mDeckViewWidget = new DeckViewWidget(mCNFont, mPixelFont, mPlayPage);
    mDeckViewWidget->hide();

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
    connect(btnInfo, &QPushButton::clicked, this, [this]() {
        auto handName = [](HandType t) {
            switch (t) {
            case HandType::HighCard: return QString("高牌");
            case HandType::Pair: return QString("对子");
            case HandType::TwoPair: return QString("两对");
            case HandType::ThreeOfAKind: return QString("三条");
            case HandType::Straight: return QString("顺子");
            case HandType::Flush: return QString("同花");
            case HandType::FullHouse: return QString("葫芦");
            case HandType::FourOfAKind: return QString("四条");
            case HandType::StraightFlush: return QString("同花顺");
            case HandType::RoyalFlush: return QString("皇家同花顺");
            case HandType::FiveOfAKind: return QString("五条");
            case HandType::FlushHouse: return QString("同花葫芦");
            case HandType::FlushFive: return QString("同花五条");
            }
            return QString("未知");
        };

        auto baseScore = [](HandType t) -> QPair<int,int> {
            switch (t) {
            case HandType::HighCard: return {Constants::BASE_HIGH_CARD_CHIPS, Constants::BASE_HIGH_CARD_MULT};
            case HandType::Pair: return {Constants::BASE_PAIR_CHIPS, Constants::BASE_PAIR_MULT};
            case HandType::TwoPair: return {Constants::BASE_TWO_PAIR_CHIPS, Constants::BASE_TWO_PAIR_MULT};
            case HandType::ThreeOfAKind: return {Constants::BASE_THREE_CHIPS, Constants::BASE_THREE_MULT};
            case HandType::Straight: return {Constants::BASE_STRAIGHT_CHIPS, Constants::BASE_STRAIGHT_MULT};
            case HandType::Flush: return {Constants::BASE_FLUSH_CHIPS, Constants::BASE_FLUSH_MULT};
            case HandType::FullHouse: return {Constants::BASE_FULL_HOUSE_CHIPS, Constants::BASE_FULL_HOUSE_MULT};
            case HandType::FourOfAKind: return {Constants::BASE_FOUR_CHIPS, Constants::BASE_FOUR_MULT};
            case HandType::StraightFlush: return {Constants::BASE_STRAIGHT_FLUSH_CHIPS, Constants::BASE_STRAIGHT_FLUSH_MULT};
            case HandType::RoyalFlush: return {Constants::BASE_ROYAL_FLUSH_CHIPS, Constants::BASE_ROYAL_FLUSH_MULT};
            case HandType::FiveOfAKind: return {Constants::BASE_FIVE_CHIPS, Constants::BASE_FIVE_MULT};
            case HandType::FlushHouse: return {Constants::BASE_FLUSH_HOUSE_CHIPS, Constants::BASE_FLUSH_HOUSE_MULT};
            case HandType::FlushFive: return {Constants::BASE_FLUSH_FIVE_CHIPS, Constants::BASE_FLUSH_FIVE_MULT};
            }
            return {0,0};
        };

        QString text;
        text += QString("当前盲注目标：%1\n当前分数：%2\n底注：%3/8\n剩余出牌：%4　剩余弃牌：%5\n\n")
                    .arg(mGameState->targetScore())
                    .arg(mGameState->score())
                    .arg(mGameState->ante())
                    .arg(mGameState->handsLeft())
                    .arg(mGameState->discardLeft());

        text += "牌型等级：\n";
        QVector<HandType> order = {
            HandType::HighCard, HandType::Pair, HandType::TwoPair, HandType::ThreeOfAKind,
            HandType::Straight, HandType::Flush, HandType::FullHouse, HandType::FourOfAKind,
            HandType::StraightFlush, HandType::FiveOfAKind, HandType::FlushHouse, HandType::FlushFive
        };
        const auto &levels = mGameState->handLevels();
        for (HandType t : order) {
            const HandLevel lv = levels.value(t);
            text += QString("  等级%1　%2　%3×%4　#%5\n")
                        .arg(lv.level).arg(handName(t))
                        .arg(baseScore(t).first + lv.chipsBonus)
                        .arg(baseScore(t).second + lv.multBonus)
                        .arg(lv.played);
        }

        text += "\n已兑换优惠券：";
        if (mGameState->redeemedVouchers().isEmpty()) {
            text += "无";
        } else {
            text += "\n";
            for (VoucherType v : mGameState->redeemedVouchers())
                text += "  · " + voucherData(v).name + "：" + voucherData(v).description + "\n";
        }

        QMessageBox::information(this, "比赛信息", text);
    });

    QPushButton *btnOptions = makeBtn("选项", "#fda200", "#ffb730", mCNFont, btnCol, 70);
    btnOptions->setFixedWidth(76);
    btnVbl->addWidget(btnOptions);
    connect(btnOptions, &QPushButton::clicked, this, [this]() {
        QMessageBox::information(this, "选项",
            "操作：\n"
            "  · 点数 / 花色：切换自动理牌方式，之后每次补牌都会自动按该方式排序\n"
            "  · 拖动手牌：只调整当前顺序，不会永久关闭自动理牌\n"
            "  · 点击小丑：展开说明与售出按钮\n"
            "  · 右下角牌组：查看剩余牌组 / 完整牌组\n"
            "  · Esc：关闭当前窗口\n\n"
            "调试提示：当前版本负片小丑概率仍保持 20%，方便测试槽位和重叠 UI。");
    });

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
    mScene->setBackgroundBrush(QBrush(Qt::NoBrush));

    mDynamicBg = new DynamicBackgroundItem();
    mDynamicBg->setSceneSize(mSceneW, mSceneH);
    mDynamicBg->setMood(DynamicBackgroundItem::Mood::Default);
    mScene->addItem(mDynamicBg);

    //绘制上方小丑 & 消耗牌

    mJokerCountLabel = mScene->addText("0/5");
    mJokerCountLabel->setDefaultTextColor(QColor("#d7e7d2"));
    mJokerCountLabel->setFont(mCNFont);
    mJokerCountLabel->setZValue(30);

    mConsCountLabel = mScene->addText("0/2");
    mConsCountLabel->setDefaultTextColor(QColor("#d7e7d2"));
    mConsCountLabel->setFont(mCNFont);
    mConsCountLabel->setZValue(30);

    // 原版不是一个个虚线槽，而是一块柔和的半透明持有区。
    refreshJokerSlotFrames();
    refreshConsumableSlotFrames();

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
    connect(mDeckBackCard, &CardItem::clicked, this, &MainWindow::onDeckClicked);

    mDeckLabel = mScene->addText("52/52");
    QFont df = mCNFont; df.setPixelSize(12);
    mDeckLabel->setFont(df);
    mDeckLabel->setDefaultTextColor(QColor("#cccccc"));
    mDeckLabel->setPos(mSceneW - CARD_W - 4, mSceneH - 34);
    mDeckLabel->setZValue(2);

    // 原版出牌区在中部，手牌/按钮贴近屏幕底部；避免底部出现大片空白。
    mHandY = mSceneH - CARD_H - 150;
    mBtnY  = mSceneH - 88;

    mDeckLabel->setPos(mSceneW - CARD_W - 4, mSceneH - 34);
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
    connect(mGameState, &GameState::shopChanged, this, [this]() {
        refreshCounters();
        refreshGold();
        if (mShopWidget && mShopWidget->isVisible()) mShopWidget->refresh();
        if (mPackOpenWidget && mPackOpenWidget->isVisible())
            mPackOpenWidget->setFreeJokerSlots(mGameState->jokerSlots() - mGameState->jokers().size());
    });

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
            connect(match, &CardItem::dragReleased,
                    this, &MainWindow::onHandCardDragReleased);
            connect(match, &CardItem::hoverChanged,
                    this, [this](CardItem *c, bool hovered) {
                        if (hovered) showCardInfo(c);
                        else hideCardInfo();
                    });
        } else {
            // Boss debuff、塔罗/幻灵增强等会改变同一张牌的数据；复用旧 CardItem 时也必须刷新画面。
            match->setCardData(hc);
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
        QString("%1/%2").arg(n).arg(mGameState->handSize()));
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
            case BlindType::Boss:  row = bossChipRow(mGameState->bossEffect()); break;
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

    // 更新右下牌堆计数。放在牌背正下方，避免贴到窗口边缘。
    if (mDeckLabel) {
        mDeckLabel->setPlainText(
            QString("%1/%2").arg(mGameState->deckRemaining()).arg(mGameState->deckTotal()));
        QRectF br = mDeckLabel->boundingRect();
        mDeckLabel->setPos(mSceneW - CARD_W - 10 + (CARD_W - br.width()) / 2.0,
                           mSceneH - 34);
    }
    if (mJokerCountLabel) {
        mJokerCountLabel->setPlainText(QString("%1/%2")
            .arg(mGameState->jokers().size()).arg(mGameState->jokerSlots()));
        QRectF br = mJokerCountLabel->boundingRect();
        mJokerCountLabel->setPos(22, JOKER_Y + CARD_H + 24);
    }
    if (mConsCountLabel) {
        QRectF br = mConsCountLabel->boundingRect();
        int slotCount = mGameState->consumableSlots();
        int totalW = CARD_W + qMax(0, slotCount - 1) * (CARD_W + 14);
        int startX = mSceneW - 8 - totalW;
        mConsCountLabel->setPos(startX + totalW - br.width() - 2, JOKER_Y + CARD_H + 24);
    }
}

void MainWindow::onDeckClicked(CardItem *)
{
    if (!mDeckViewWidget || !mPlayPage) return;
    mDeckViewWidget->setGeometry(mPlayPage->rect());
    mDeckViewWidget->open(mGameState->remainingDeckCards(), mGameState->fullDeckCards());
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


void MainWindow::showCardInfo(CardItem *card)
{
    if (!card) return;
    const CardData &d = card->cardData();

    if (!mCardInfoPanel) {
        mCardInfoPanel = new QWidget;
        mCardInfoPanel->setAttribute(Qt::WA_StyledBackground, true);
        mCardInfoPanel->setStyleSheet(
            "background:rgba(26,31,35,238);"
            "border:2px solid #6fd3ff;"
            "border-radius:10px;"
        );
        auto *v = new QVBoxLayout(mCardInfoPanel);
        v->setContentsMargins(12, 10, 12, 10);
        v->setSpacing(6);

        mCardInfoName = new QLabel(mCardInfoPanel);
        QFont nf = mCNFont; nf.setPixelSize(18); nf.setBold(true);
        mCardInfoName->setFont(nf);
        mCardInfoName->setStyleSheet("color:white; background:transparent;");
        mCardInfoName->setAlignment(Qt::AlignCenter);
        v->addWidget(mCardInfoName);

        mCardInfoDesc = new QLabel(mCardInfoPanel);
        QFont df = mCNFont; df.setPixelSize(13);
        mCardInfoDesc->setFont(df);
        mCardInfoDesc->setStyleSheet("color:#d6edf7; background:transparent;");
        mCardInfoDesc->setWordWrap(true);
        mCardInfoDesc->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        v->addWidget(mCardInfoDesc);

        mCardInfoPanel->setFixedWidth(260);
        mCardInfoProxy = mScene->addWidget(mCardInfoPanel);
        mCardInfoProxy->setZValue(900);
    }

    mCardInfoName->setText(cardTooltipTitle(d));
    mCardInfoDesc->setText(cardTooltipBody(d));
    mCardInfoPanel->adjustSize();

    // 原版扑克牌 tooltip 在牌上方，不在右侧；只有空间不足时才压回下方。
    QPointF p(card->scenePos().x() + CardItem::WIDTH / 2.0 - mCardInfoPanel->width() / 2.0,
              card->scenePos().y() - mCardInfoPanel->height() - 12);
    if (p.x() < 8) p.setX(8);
    if (p.x() + mCardInfoPanel->width() > mSceneW - 8)
        p.setX(mSceneW - mCardInfoPanel->width() - 8);
    if (p.y() < 8)
        p.setY(card->scenePos().y() + CardItem::HEIGHT + 10);
    mCardInfoProxy->setPos(p);
    mCardInfoProxy->show();
}

void MainWindow::hideCardInfo()
{
    if (mCardInfoProxy) mCardInfoProxy->hide();
}

void MainWindow::onHandCardDragReleased(CardItem *card, QPointF scenePos)
{
    int from = mHandCards.indexOf(card);
    if (from < 0) { layoutHandCards(); return; }

    int n = mHandCards.size();
    if (n <= 1) { layoutHandCards(); return; }

    int available = mSceneW - 80;
    int step = (available - CARD_W) / qMax(1, n - 1);
    step = qMin(step, CARD_W - 30);
    int totalW = (n - 1) * step + CARD_W;
    int startX = (mSceneW - totalW) / 2;

    int to = 0;
    double x = scenePos.x();
    for (int i = 0; i < n; ++i) {
        double center = startX + i * step + CARD_W / 2.0;
        if (x > center) to = i;
    }
    to = qBound(0, to, n - 1);

    if (from != to) {
        QVector<int> newSelected;
        for (int s : mSelected) {
            int ns = s;
            if (s == from) {
                ns = to;
            } else if (from < to && s > from && s <= to) {
                ns = s - 1;
            } else if (from > to && s >= to && s < from) {
                ns = s + 1;
            }
            if (!newSelected.contains(ns)) newSelected.append(ns);
        }

        mGameState->moveHandCard(from, to);
        // GameState 会同步触发 refreshHand。这里再按“拖拽前选中的实体”修正下标，
        // 防止手动排序后所有牌被取消选中，导致不能出牌/弃牌。
        mSelected = newSelected;
        for (int i = 0; i < mHandCards.size(); ++i)
            mHandCards[i]->setCardSelected(mSelected.contains(i));
        layoutHandCards();
    } else {
        layoutHandCards();
    }
    refreshCounters();
    updateHandPreview();
}

// 出牌
void MainWindow::onPlayClicked() {
    if (mScoringInProgress) return;
    if (mSelected.isEmpty()) return;
    mScoringInProgress = true;
    if (mBtnPlay) mBtnPlay->setEnabled(false);
    if (mBtnDiscard) mBtnDiscard->setEnabled(false);

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
    if (mScoringInProgress) return;
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

    // 原版先亮出牌型的基础筹码/倍率，再逐张牌、小丑实时累加。
    mDisplayedChips = r.baseChips;
    mDisplayedMult  = r.baseMult;
    mLblChips->setText(QString::number(mDisplayedChips));
    mLblMult ->setText(QString::number(mDisplayedMult));

    // 总分本手结算结束前不立刻更新。
    int gained = static_cast<int>(r.chips * r.mult * r.xmult);

    int delayBase = 420;
    int delayStep = 230;
    for (int ei = 0; ei < r.events.size(); ++ei) {
        const ScoreEvent ev = r.events[ei];
        int delay = delayBase + ei * delayStep;
        QTimer::singleShot(delay, this, [this, ev]() {
            playScoreEvent(ev);
        });
    }

    int finalDelay = delayBase + r.events.size() * delayStep + 260;
    QTimer::singleShot(finalDelay, this, [this, r, gained, finalDelay]() {
        mDisplayedChips = r.chips;
        mDisplayedMult  = qRound(r.mult * r.xmult);
        mLblChips->setText(QString::number(r.chips));
        mLblMult ->setText(QString::number(mDisplayedMult));
        animateScoreTotalThenFinalize(gained, finalDelay);
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
    if (mGameOverHandled) return;
    mGameOverHandled = true;
    mScoringInProgress = false;
    if (mBtnPlay) mBtnPlay->setEnabled(false);
    if (mBtnDiscard) mBtnDiscard->setEnabled(false);
    showGameOverOverlay(won);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);

    if (mPlayPage) {
        QRect r = mPlayPage->rect();
        if (mDynamicBg) { mDynamicBg->setSceneSize(mSceneW, mSceneH); }
        if (mBlindSelectWidget) mBlindSelectWidget->setGeometry(r);
        if (mRoundEndOverlay)   mRoundEndOverlay  ->setGeometry(r);
        if (mShopWidget)        mShopWidget       ->setGeometry(lowerOverlayRect());
        if (mPackOpenWidget)    mPackOpenWidget   ->setGeometry(lowerOverlayRect());
        if (mDeckViewWidget)    mDeckViewWidget   ->setGeometry(r);
    }
}

void MainWindow::refreshJokerSlotFrames()
{
    for (auto *r : mJokerSlotRects) {
        mScene->removeItem(r);
        delete r;
    }
    mJokerSlotRects.clear();

    // 原版槽位区域不会因为负片/反物质变宽；多出来的牌在同一区域内重叠挤入。
    int visualSlots = Constants::MAX_JOKER_SLOTS;
    int step = CARD_W + 14;
    int totalW = CARD_W + qMax(0, visualSlots - 1) * step;
    int available = qMin(mSceneW - 430, 780);
    int startX = 8;
    if (totalW < available) startX = 8 + (available - totalW) / 2;

    QRectF bg(startX - 12, JOKER_Y + 8, totalW + 24, CARD_H + 32);
    auto *r = mScene->addRect(bg,
                              QPen(Qt::NoPen),
                              QBrush(QColor(0, 0, 0, 44)));
    r->setZValue(0.5);
    mJokerSlotRects.append(r);
}

void MainWindow::refreshJokerSlots()
{
    refreshJokerSlotFrames();
    hideJokerInfo();

    for (auto *ji : mJokerItems) {
        mScene->removeItem(ji);
        delete ji;
    }
    mJokerItems.clear();

    const auto &js = mGameState->jokers();
    int n = js.size();
    if (mJokerCountLabel) {
        mJokerCountLabel->setPlainText(QString("%1/%2").arg(n).arg(mGameState->jokerSlots()));
        mJokerCountLabel->setPos(22, JOKER_Y + CARD_H + 24);
    }
    if (mPackOpenWidget && mPackOpenWidget->isVisible())
        mPackOpenWidget->setFreeJokerSlots(mGameState->jokerSlots() - n);
    int visualSlots = Constants::MAX_JOKER_SLOTS;
    int visualStep = CARD_W + 14;
    int visualW = CARD_W + qMax(0, visualSlots - 1) * visualStep;
    int available = qMin(mSceneW - 430, 780);
    int startX = 8;
    if (visualW < available) startX = 8 + (available - visualW) / 2;
    int step = (n > 1) ? (visualW - CARD_W) / qMax(1, n - 1) : visualStep;
    step = qBound(42, step, visualStep); // 多出槽位时重叠进原区域

    for (int i = 0; i < js.size(); ++i) {
        int x = startX + i * step;
        int y = JOKER_Y + 18;
        auto *ji = new JokerItem(js[i]);
        ji->setPos(x, y);
        ji->setZValue(20 + i);
        mScene->addItem(ji);
        mJokerItems.append(ji);
        connect(ji, &JokerItem::pressed, this, &MainWindow::onJokerPressed);
        connect(ji, &JokerItem::dragReleased, this, &MainWindow::onJokerDragReleased);
        connect(ji, &JokerItem::hoverChanged, this, [this, ji](JokerItem *, bool hovered) {
            int idx = mJokerItems.indexOf(ji);
            if (hovered && idx >= 0) showJokerInfo(idx, false);
            else if (!hovered && mSelectedJokerIdx < 0) hideJokerInfo();
        });
    }
}

void MainWindow::showJokerInfo(int idx, bool showSellButton)
{
    const auto &js = mGameState->jokers();
    if (idx < 0 || idx >= js.size() || idx >= mJokerItems.size()) return;
    if (showSellButton) mSelectedJokerIdx = idx;
    else if (mSelectedJokerIdx != idx) mSelectedJokerIdx = -1;
    const Joker &j = js[idx];

    if (!mJokerInfoPanel) {
        mJokerInfoPanel = new QWidget;
        mJokerInfoPanel->setAttribute(Qt::WA_StyledBackground, true);
        mJokerInfoPanel->setStyleSheet(
            "background:rgba(31,37,42,235);"
            "border:2px solid #fda200;"
            "border-radius:12px;"
        );
        auto *vbl = new QVBoxLayout(mJokerInfoPanel);
        vbl->setContentsMargins(12, 10, 12, 10);
        vbl->setSpacing(6);

        mJokerInfoName = new QLabel(mJokerInfoPanel);
        QFont nf = mCNFont; nf.setPixelSize(18); nf.setBold(true);
        mJokerInfoName->setFont(nf);
        mJokerInfoName->setStyleSheet("color:#ffe9a8; background:transparent; border:none;");
        mJokerInfoName->setAlignment(Qt::AlignCenter);
        vbl->addWidget(mJokerInfoName);

        mJokerInfoMeta = new QLabel(mJokerInfoPanel);
        QFont mf = mCNFont; mf.setPixelSize(13);
        mJokerInfoMeta->setFont(mf);
        mJokerInfoMeta->setStyleSheet("color:#cbd6dc; background:transparent; border:none;");
        mJokerInfoMeta->setAlignment(Qt::AlignCenter);
        vbl->addWidget(mJokerInfoMeta);

        mJokerInfoDesc = new QLabel(mJokerInfoPanel);
        QFont df = mCNFont; df.setPixelSize(13);
        mJokerInfoDesc->setFont(df);
        mJokerInfoDesc->setWordWrap(true);
        mJokerInfoDesc->setAlignment(Qt::AlignCenter);
        mJokerInfoDesc->setStyleSheet("color:white; background:transparent; border:none;");
        mJokerInfoDesc->setFixedWidth(240);
        vbl->addWidget(mJokerInfoDesc);

        mJokerSellButton = new QPushButton(mJokerInfoPanel);
        QFont sf = mCNFont; sf.setPixelSize(16); sf.setBold(true);
        mJokerSellButton->setFont(sf);
        mJokerSellButton->setCursor(Qt::PointingHandCursor);
        mJokerSellButton->setStyleSheet(
            "QPushButton { background:#fe5f55; color:white; border:none; border-radius:10px; padding:7px 14px; }"
            "QPushButton:hover { background:#ff7066; }"
        );
        vbl->addWidget(mJokerSellButton);

        mJokerInfoProxy = mScene->addWidget(mJokerInfoPanel);
        mJokerInfoProxy->setZValue(900);
    }

    mJokerInfoName->setText(j.name);
    QString editionText = editionName(j.edition);
    if (editionText.isEmpty()) editionText = "普通";
    QString editionEffect = editionDesc(j.edition);
    QString meta = QString("%1小丑　出售 $%2").arg(editionText).arg(qMax(1, j.sellValue));
    if (!editionEffect.isEmpty()) meta += QString("　%1").arg(editionEffect);
    mJokerInfoMeta->setText(meta);

    QString desc = j.description;
    if (j.type == JokerType::Yorick) {
        desc += QString("\n当前：X%1 倍率\n还需要弃牌 [%2/23]")
                    .arg(mGameState->yorickXMult(), 0, 'f', 1)
                    .arg(mGameState->yorickDiscardsRemaining());
    } else if (j.type == JokerType::Caino) {
        desc += QString("\n当前：X%1 倍率")
                    .arg(mGameState->cainoXMult(), 0, 'f', 1);
    } else if (j.type == JokerType::DriversLicense) {
        int enhanced = 0;
        for (const CardData &c : mGameState->fullDeckCards())
            if (c.enhancement != Enhancement::None) ++enhanced;
        desc += QString("\n当前增强牌 [%1/16] %2")
                    .arg(enhanced)
                    .arg(enhanced >= 16 ? "已生效：X3" : "未生效");
    } else if (j.type == JokerType::IceCream) {
        desc += QString("\n当前：+%1 筹码\n每次出牌后 -5 筹码").arg(qMax(0, j.counter));
    } else if (j.type == JokerType::Stuntman) {
        desc += "\n当前：+250 筹码；手牌上限 -2";
    } else if (j.type == JokerType::DNA) {
        desc += QString("\n状态：%1")
                    .arg(mGameState->dnaCanTriggerThisPlay() ? "本次出 1 张可触发" : "仅本盲注第一次出牌且只出 1 张时触发");
    } else if (j.type == JokerType::Blueprint) {
        if (idx + 1 < mGameState->jokers().size())
            desc += QString("\n当前指向右侧：%1").arg(mGameState->jokers()[idx + 1].name);
        else
            desc += "\n右侧没有可复制小丑";
    } else if (j.type == JokerType::Brainstorm) {
        if (!mGameState->jokers().isEmpty() && idx != 0)
            desc += QString("\n当前指向最左侧：%1").arg(mGameState->jokers().first().name);
        else
            desc += "\n当前没有可复制小丑";
    }
    mJokerInfoDesc->setText(desc);
    mJokerSellButton->setText(QString("售出　+$%1").arg(qMax(1, j.sellValue)));
    mJokerSellButton->setVisible(showSellButton);

    disconnect(mJokerSellButton, nullptr, this, nullptr);
    connect(mJokerSellButton, &QPushButton::clicked, this, [this]() {
        if (mSelectedJokerIdx < 0) return;
        if (mGameState->sellJoker(mSelectedJokerIdx)) {
            mSelectedJokerIdx = -1;
            hideJokerInfo();
            if (mPackOpenWidget && mPackOpenWidget->isVisible())
                mPackOpenWidget->setFreeJokerSlots(mGameState->jokerSlots() - mGameState->jokers().size());
            if (mShopWidget && mShopWidget->isVisible()) mShopWidget->refresh();
            refreshCounters();
        }
    });

    QPointF jp = mJokerItems[idx]->pos();
    qreal x;
    qreal y;
    if (showSellButton) {
        // 单击后，售出按钮出现在小丑右侧。
        x = qMin<qreal>(jp.x() + CARD_W + 12, mSceneW - 285);
        y = jp.y() + 14;
        if (x <= jp.x()) {
            x = qBound<qreal>(8, jp.x(), mSceneW - 285);
            y = jp.y() + CARD_H + 8;
        }
    } else {
        // 悬浮说明放在小丑下方，不直接显示售出按钮。
        x = jp.x() + CARD_W / 2.0 - 140;
        x = qBound<qreal>(8, x, mSceneW - 285);
        y = jp.y() + CARD_H + 10;
    }
    mJokerInfoProxy->setPos(x, y);
    mJokerInfoPanel->show();
}

void MainWindow::hideJokerInfo()
{
    if (mJokerInfoPanel) mJokerInfoPanel->hide();
}

void MainWindow::onJokerPressed(JokerItem *item, Qt::MouseButton btn)
{
    int idx = mJokerItems.indexOf(item);
    if (idx < 0) return;

    // 兼容：右键仍可打开同一个售卖面板；正版主要是悬停/左键后出现售出按钮。
    if (btn == Qt::LeftButton || btn == Qt::RightButton) {
        showJokerInfo(idx, true);
        item->juiceUp(1.08, 140);
    }
}

void MainWindow::onJokerDragReleased(JokerItem *item, QPointF scenePos)
{
    int from = mJokerItems.indexOf(item);
    if (from < 0) { refreshJokerSlots(); return; }
    int n = mJokerItems.size();
    if (n <= 1) { refreshJokerSlots(); return; }

    int available = qMin(mSceneW - 430, 780);
    int step = (n > 1) ? (available - CARD_W) / qMax(1, n - 1) : CARD_W + 14;
    step = qBound(64, step, CARD_W + 14);
    int totalW = CARD_W + (n - 1) * step;
    int startX = 8;
    if (totalW < available) startX = 8 + (available - totalW) / 2;

    int to = 0;
    for (int i = 0; i < n; ++i) {
        double center = startX + i * step + CARD_W / 2.0;
        if (scenePos.x() > center) to = i;
    }
    to = qBound(0, to, n - 1);

    if (from != to) mGameState->moveJoker(from, to);
    else refreshJokerSlots();
}

void MainWindow::refreshConsumableSlotFrames()
{
    for (auto *r : mConsumableSlotRects) {
        mScene->removeItem(r);
        delete r;
    }
    mConsumableSlotRects.clear();

    int visualSlots = Constants::MAX_CONSUMABLE_SLOTS;
    int step = CARD_W + 14;
    int totalW = CARD_W + qMax(0, visualSlots - 1) * step;
    int startX = mSceneW - 8 - totalW;
    QRectF bg(startX - 12, JOKER_Y + 8, totalW + 24, CARD_H + 32);
    auto *r = mScene->addRect(bg,
                              QPen(Qt::NoPen),
                              QBrush(QColor(0, 0, 0, 44)));
    r->setZValue(0.5);
    mConsumableSlotRects.append(r);
}

void MainWindow::refreshConsumableSlots()
{
    refreshConsumableSlotFrames();

    for (auto *ci : mConsumableItems) { mScene->removeItem(ci); delete ci; }
    mConsumableItems.clear();

    const auto &cs = mGameState->consumables();
    int slotCount = mGameState->consumableSlots();
    if (mConsCountLabel) {
        mConsCountLabel->setPlainText(QString("%1/%2").arg(cs.size()).arg(slotCount));
        QRectF br = mConsCountLabel->boundingRect();
        int visualSlots = Constants::MAX_CONSUMABLE_SLOTS;
        int totalW = CARD_W + qMax(0, visualSlots - 1) * (CARD_W + 14);
        int startX = mSceneW - 8 - totalW;
        mConsCountLabel->setPos(startX + totalW - br.width() - 2, JOKER_Y + CARD_H + 24);
    }

    int visualSlots = Constants::MAX_CONSUMABLE_SLOTS;
    int totalW = CARD_W + qMax(0, visualSlots - 1) * (CARD_W + 14);
    int startX = mSceneW - 8 - totalW;
    int step = (cs.size() > 1) ? (totalW - CARD_W) / qMax(1, cs.size() - 1) : (CARD_W + 14);
    step = qBound(42, step, CARD_W + 14);
    for (int i = 0; i < cs.size(); ++i) {
        int x = startX + i * step;
        int y = JOKER_Y + 18;
        auto *ci = new ConsumableItem(cs[i]);
        ci->setPos(x, y);
        ci->setZValue(30 + i);
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
        if (mShopWidget && mShopWidget->isVisible()) mShopWidget->refresh();
        return;
    }

    // 开奥秘/幻灵包时，原版仍然点击右上角仓库特殊牌使用，目标是包界面的临时手牌。
    if (mPackOpenWidget && mPackOpenWidget->isVisible() && !mPendingPackHand.isEmpty()) {
        QVector<int> packSel = mPackOpenWidget->selectedHandIndices();
        if (mGameState->useConsumableOnPackHand(idx, packSel, mPendingPackHand)) {
            mPackOpenWidget->setPackHand(mPendingPackHand);
            mPackOpenWidget->setInventoryConsumables(mGameState->consumables());
            refreshConsumableSlots();
            refreshGold();
            if (mShopWidget && mShopWidget->isVisible()) mShopWidget->refresh();
        } else {
            mConsCountLabel->setDefaultTextColor(QColor("#ff8080"));
            QTimer::singleShot(400, this, [this]() {
                if (mConsCountLabel) mConsCountLabel->setDefaultTextColor(QColor("#aaddaa"));
            });
        }
        return;
    }

    QVector<int> sel = mSelected;
    std::sort(sel.begin(), sel.end());

    const auto &cs = mGameState->consumables();
    if (idx >= cs.size()) return;
    if (cs[idx].needsSelection > 0 && sel.size() < cs[idx].needsSelection) {
        mHandCountLabel->setDefaultTextColor(QColor("#ff8080"));
        QTimer::singleShot(400, this, [this]() {
            if (mHandCountLabel) mHandCountLabel->setDefaultTextColor(QColor("#aaddaa"));
        });
        return;
    }

    if (mGameState->useConsumable(idx, sel)) {
        refreshGold();
        if (mShopWidget && mShopWidget->isVisible()) mShopWidget->refresh();
    }
}

void MainWindow::onPackChoiceMade(int chosenIdx, QVector<int> selectedPackHandIdx)
{
    if (chosenIdx >= 0) {
        mGameState->applyPackChoice(mPendingPack, chosenIdx, selectedPackHandIdx, mPendingPackHand);
        mPackOpenWidget->setPackHand(mPendingPackHand);
        mPackOpenWidget->setInventoryConsumables(mGameState->consumables());
    }
    refreshConsumableSlots();
    refreshJokerSlots();
    refreshGold();
    refreshCounters();
    if (mPackOpenWidget && mPackOpenWidget->isVisible())
        mPackOpenWidget->setFreeJokerSlots(mGameState->jokerSlots() - mGameState->jokers().size());
    if (mShopWidget) mShopWidget->refresh();
}

void MainWindow::onInventoryConsumableUseRequested(int inventoryIdx, QVector<int> selectedPackHandIdx)
{
    if (mGameState->useConsumableOnPackHand(inventoryIdx, selectedPackHandIdx, mPendingPackHand)) {
        mPackOpenWidget->setPackHand(mPendingPackHand);
        mPackOpenWidget->setInventoryConsumables(mGameState->consumables());
        refreshConsumableSlots();
        refreshGold();
        refreshCounters();
    }
}

void MainWindow::onPackFinished()
{
    if (!mPendingPackHand.isEmpty())
        mGameState->returnPackHand(mPendingPackHand);
    mPendingPackHand.clear();
    refreshCounters();
    refreshGold();

    if (mPackFromTag) {
        mPackFromTag = false;
        if (mDynamicBg) mDynamicBg->setMood(DynamicBackgroundItem::Mood::BlindSelect);
        if (mBlindSelectWidget && mPlayPage) {
            mBlindSelectWidget->refresh();
            mBlindSelectWidget->setGeometry(mPlayPage->rect());
            mBlindSelectWidget->show();
            mBlindSelectWidget->raise();
            mBlindSelectWidget->arrangeCards(false);
        }
        return;
    }

    if (mShopWidget) {
        if (mDynamicBg) mDynamicBg->setMood(DynamicBackgroundItem::Mood::Shop);
        mShopWidget->refresh();
        mShopWidget->setGeometry(lowerOverlayRect());
        QPoint end = mShopWidget->pos();
        mShopWidget->move(end.x(), mPlayPage ? mPlayPage->height() + 20 : end.y() + 500);
        mShopWidget->show();
        mShopWidget->raise();
        auto *anim = new QPropertyAnimation(mShopWidget, "pos", this);
        anim->setDuration(260);
        anim->setStartValue(mShopWidget->pos());
        anim->setEndValue(end);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }
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

QRect MainWindow::lowerOverlayRect() const
{
    if (!mPlayPage) return QRect();
    // 商店/开包只覆盖中下区，保留原版上方小丑槽和右上角消耗牌槽可点击。
    const int y = JOKER_Y + JOKER_H + 10;
    return QRect(0, y, mPlayPage->width(), qMax(0, mPlayPage->height() - y));
}

void MainWindow::showShopOverlay()
{
    if (mDynamicBg) mDynamicBg->setMood(DynamicBackgroundItem::Mood::Shop);
    if (!mShopWidget || !mPlayPage) return;
    mShopWidget->refresh();
    mShopWidget->setGeometry(lowerOverlayRect());
    mShopWidget->raise();
    mShopWidget->show();
}

bool MainWindow::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj == mPlayPage && ev->type() == QEvent::Resize) {
        QRect r = mPlayPage->rect();
        if (mDynamicBg) { mDynamicBg->setSceneSize(mSceneW, mSceneH); }
        if (mBlindSelectWidget) mBlindSelectWidget->setGeometry(r);
        if (mRoundEndOverlay)   mRoundEndOverlay  ->setGeometry(r);
        if (mShopWidget)        mShopWidget       ->setGeometry(lowerOverlayRect());
        if (mPackOpenWidget)    mPackOpenWidget   ->setGeometry(lowerOverlayRect());
        if (mDeckViewWidget)    mDeckViewWidget   ->setGeometry(r);
    }
    return QMainWindow::eventFilter(obj, ev);
}


void MainWindow::setBackgroundMoodForPhase()
{
    if (!mDynamicBg || !mGameState) return;
    switch (mGameState->phase()) {
    case GamePhase::BlindSelect:
        mDynamicBg->setMood(DynamicBackgroundItem::Mood::BlindSelect);
        break;
    case GamePhase::Shop:
        mDynamicBg->setMood(DynamicBackgroundItem::Mood::Shop);
        break;
    case GamePhase::Blind:
        mDynamicBg->setMood(DynamicBackgroundItem::Mood::Default);
        break;
    default:
        mDynamicBg->setMood(DynamicBackgroundItem::Mood::Default);
        break;
    }
}

void MainWindow::setBackgroundMoodForPack(PackKind kind)
{
    if (!mDynamicBg) return;
    switch (kind) {
    case PackKind::Arcana:    mDynamicBg->setMood(DynamicBackgroundItem::Mood::Tarot); break;
    case PackKind::Spectral:  mDynamicBg->setMood(DynamicBackgroundItem::Mood::Spectral); break;
    case PackKind::Celestial: mDynamicBg->setMood(DynamicBackgroundItem::Mood::Celestial); break;
    case PackKind::Buffoon:   mDynamicBg->setMood(DynamicBackgroundItem::Mood::Buffoon); break;
    case PackKind::Standard:  mDynamicBg->setMood(DynamicBackgroundItem::Mood::Standard); break;
    }
}

void MainWindow::openImmediateTagPack(PackKind kind)
{
    PackSize size = PackSize::Mega;
    if (kind == PackKind::Spectral) size = PackSize::Normal;

    QVector<JokerType> owned;
    for (const Joker &j : mGameState->jokers()) owned.append(j.type);

    mPendingPack = generatePackContent(kind, size,
                                       mGameState->hasVoucher(VoucherType::OmenGlobe),
                                       mGameState->hasVoucher(VoucherType::Telescope),
                                       ConsumableType::Planet_Pluto,
                                       owned,
                                       false);
    mPendingPackHand.clear();
    if (kind == PackKind::Arcana || kind == PackKind::Spectral) {
        mPendingPackHand = mGameState->drawPackHand();
        refreshCounters();
    }

    mPackFromTag = true;
    setBackgroundMoodForPack(kind);
    if (mBlindSelectWidget) mBlindSelectWidget->hide();
    if (mShopWidget) mShopWidget->hide();

    int freeJoker = mGameState->jokerSlots() - mGameState->jokers().size();
    mPackOpenWidget->open(mPendingPack, mPendingPackHand,
                          mGameState->consumables(), freeJoker);
    mPackOpenWidget->setGeometry(lowerOverlayRect());
}

void MainWindow::onBlindSelectEntered()
{
    if (mDynamicBg) mDynamicBg->setMood(DynamicBackgroundItem::Mood::BlindSelect);
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
    if (mDynamicBg) mDynamicBg->setMood(DynamicBackgroundItem::Mood::Default);
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
    mScoringInProgress = false;
    if (mBtnPlay) mBtnPlay->setEnabled(true);
    if (mBtnDiscard) mBtnDiscard->setEnabled(true);
}

void MainWindow::animateCollectRoundCardsThen(std::function<void()> after)
{
    QVector<CardItem*> cards;
    for (auto *c : mHandCards) if (c) cards.append(c);
    for (auto *c : mPlayedCards) if (c) cards.append(c);

    QPointF deckPos(mSceneW - CARD_W - 10, mSceneH - CARD_H - 36);
    const int duration = cards.isEmpty() ? 0 : 420;

    for (int i = 0; i < cards.size(); ++i) {
        CardItem *c = cards[i];
        c->setZValue(80 + i);
        c->moveTo(deckPos, duration);
        auto *fade = new QPropertyAnimation(c, "opacity", this);
        fade->setDuration(duration);
        fade->setStartValue(c->opacity());
        fade->setEndValue(0.0);
        fade->setEasingCurve(QEasingCurve::InQuad);
        fade->start(QAbstractAnimation::DeleteWhenStopped);
    }

    QTimer::singleShot(duration + 40, this, [this, after]() {
        clearPlayedCards();
        mGameState->collectRoundCardsToDeck();
        for (auto *c : mHandCards) c->setOpacity(1.0);
        refreshHand();
        refreshCounters();
        if (after) after();
    });
}

void MainWindow::onNextBlindClicked()
{
    clearFloatingScores();

    // 原版 cash_out 只负责把结算窗口收走并进入商店；
    // 手牌/弃牌在 ROUND_EVAL 弹出之前就已经自动收回牌组了。
    auto enterShop = [this]() {
        setContextPage(2);
        setPlayPhaseVisible(false);
        showShopOverlay();
    };

    if (mRoundEndOverlay && mRoundEndOverlay->isVisible()) {
        mRoundEndOverlay->hideToBottom(enterShop);
    } else {
        enterShop();
    }
}

void MainWindow::onRoundWon(int blindReward, int handBonus, int interest)
{
    refreshGold();

    // 计算 blind chip row 用于显示
    int chipRow = 0;
    switch (mGameState->blindType()) {
    case BlindType::Small: chipRow = 0; break;
    case BlindType::Big:   chipRow = 1; break;
    case BlindType::Boss:  chipRow = bossChipRow(mGameState->bossEffect()); break;
    }

    mRoundEndOverlay->setData(
        chipRow,
        mGameState->targetScore(),
        blindReward,
        mGameState->handsLeft(),  handBonus,
        interest
        );

    // 本手计分动画已经结束并加分；胜利后先自动收回剩余手牌，再从下方弹出提现面板。
    QTimer::singleShot(260, this, [this]() {
        animateCollectRoundCardsThen([this]() {
            if (!mRoundEndOverlay || !mPlayPage) return;
            mRoundEndOverlay->showFromBottom(mPlayPage->rect());
        });
    });
}

void MainWindow::onPackBuyRequested(int slot)
{
    if (!mGameState->buyPack(slot, mPendingPack)) return;

    // 原版开包：商店牌区先向下收起，背景露出并按包类型变色，然后包内容出现。
    if (mShopWidget) {
        auto *anim = new QPropertyAnimation(mShopWidget, "pos", this);
        anim->setDuration(220);
        anim->setStartValue(mShopWidget->pos());
        anim->setEndValue(QPoint(mShopWidget->x(), mPlayPage ? mPlayPage->height() + 20 : mShopWidget->y() + 500));
        anim->setEasingCurve(QEasingCurve::InCubic);
        connect(anim, &QPropertyAnimation::finished, mShopWidget, &QWidget::hide);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }
    mPackFromTag = false;
    setBackgroundMoodForPack(mPendingPack.kind);

    // 原版只有塔罗包/幻灵包需要在上方显示临时手牌供塔罗/幻灵作用；
    // 标准包、天体包、小丑包不需要抽这排牌。
    mPendingPackHand.clear();
    if (mPendingPack.kind == PackKind::Arcana || mPendingPack.kind == PackKind::Spectral) {
        mPendingPackHand = mGameState->drawPackHand();
        refreshCounters();
    }

    int freeJoker = mGameState->jokerSlots() - mGameState->jokers().size();
    mPackOpenWidget->open(mPendingPack, mPendingPackHand,
                          mGameState->consumables(), freeJoker);
    QTimer::singleShot(0, this, [this]() {
        if (mPackOpenWidget)
            mPackOpenWidget->setGeometry(lowerOverlayRect());
        if (mDeckViewWidget && mPlayPage)
            mDeckViewWidget->setGeometry(mPlayPage->rect());
    });
}

void MainWindow::setContextPage(int page)
{
    if (mContextArea) mContextArea->setCurrentIndex(page);
}

void MainWindow::onSkipBlind(int /*idx*/)
{
    mGameState->skipCurrentBlind();
    TagType gained = mGameState->lastSkippedTag();
    TagData td = tagData(gained);
    addObtainedTag(td.spritePos.x(), td.spritePos.y());
    refreshGold();
    refreshConsumableSlots();
    refreshJokerSlots();

    switch (gained) {
    case TagType::Standard: openImmediateTagPack(PackKind::Standard); break;
    case TagType::Charm:    openImmediateTagPack(PackKind::Arcana); break;
    case TagType::Meteor:   openImmediateTagPack(PackKind::Celestial); break;
    case TagType::Buffoon:  openImmediateTagPack(PackKind::Buffoon); break;
    case TagType::Ethereal: openImmediateTagPack(PackKind::Spectral); break;
    default: break;
    }
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

    // 原版标签触发后会播放 yep/消散，不会永久挂在右下角。
    // 这里统一延迟清空，避免单个图标被其它流程提前清除后再次 delete。
    QTimer::singleShot(1400, this, [this]() {
        clearObtainedTags();
    });
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


void MainWindow::animateScoreTotalThenFinalize(int gained, int /*delayAfterEvents*/)
{
    int before = mGameState->score();
    int after = before + gained;

    auto *anim = new QVariantAnimation(this);
    anim->setDuration(520);
    anim->setStartValue(before);
    anim->setEndValue(after);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        mLblScore->setText(QString::number(v.toInt()));
        QFont f = mLblScore->font();
        f.setPixelSize(30 + (v.toInt() % 2));
        mLblScore->setFont(f);
    });
    connect(anim, &QVariantAnimation::finished, this, [this, after, gained]() {
        mLblScore->setText(QString::number(after));
        if (gained >= mGameState->targetScore()) {
            // 原版是“本手得分本身超过盲注目标”时，筹码×倍率框开始燃烧；
            // 不是累计总分达到目标就燃烧。
            const QString chipBase = "background:#009dff; color:white; border-radius:8px; padding:4px 8px;";
            const QString multBase = "background:#fe5f55; color:white; border-radius:8px; padding:4px 8px;";
            mLblChips->setStyleSheet(chipBase + " border:3px solid #ffb000;");
            mLblMult ->setStyleSheet(multBase + " border:3px solid #ffb000;");
            for (int i = 0; i < 8; ++i) {
                auto *flame = new FloatingScore("🔥", QColor("#ff9a00"), mPixelFont);
                flame->setZValue(120);
                flame->setPos(QPointF(18 + i * 34, 298 - (i % 3) * 10));
                mScene->addItem(flame);
                auto *move = new QPropertyAnimation(flame, "pos", this);
                move->setDuration(760);
                move->setStartValue(flame->pos());
                move->setEndValue(flame->pos() + QPointF(0, -62));
                auto *fade = new QPropertyAnimation(flame, "opacity", this);
                fade->setDuration(760);
                fade->setStartValue(1.0);
                fade->setEndValue(0.0);
                auto *group = new QParallelAnimationGroup(this);
                group->addAnimation(move);
                group->addAnimation(fade);
                connect(group, &QParallelAnimationGroup::finished, flame, [this, flame]() {
                    if (flame->scene()) mScene->removeItem(flame);
                    flame->deleteLater();
                });
                group->start(QAbstractAnimation::DeleteWhenStopped);
            }
            QTimer::singleShot(900, this, [this, chipBase, multBase]() {
                if (mLblChips) mLblChips->setStyleSheet(chipBase);
                if (mLblMult)  mLblMult ->setStyleSheet(multBase);
            });
        }
        animatePlayedCardsToDiscardThen([this]() {
            mGameState->finalizePlayedHand();
            mScoringInProgress = false;
            if (mGameState->phase() == GamePhase::Blind) {
                if (mBtnPlay) mBtnPlay->setEnabled(true);
                if (mBtnDiscard) mBtnDiscard->setEnabled(true);
            }
        });
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::animatePlayedCardsToDiscardThen(std::function<void()> after)
{
    QVector<CardItem*> cards = mPlayedCards;
    QPointF deckPos(mSceneW - CARD_W - 10, mSceneH - CARD_H - 36);
    int duration = cards.isEmpty() ? 0 : 420;
    for (int i = 0; i < cards.size(); ++i) {
        CardItem *c = cards[i];
        if (!c) continue;
        c->setZValue(90 + i);
        c->moveTo(deckPos, duration);
        auto *fade = new QPropertyAnimation(c, "opacity", this);
        fade->setDuration(duration);
        fade->setStartValue(c->opacity());
        fade->setEndValue(0.0);
        fade->setEasingCurve(QEasingCurve::InQuad);
        fade->start(QAbstractAnimation::DeleteWhenStopped);
    }
    QTimer::singleShot(duration + 40, this, [this, after]() {
        clearPlayedCards();
        if (after) after();
    });
}

void MainWindow::showGameOverOverlay(bool won)
{
    if (!mGameOverPanel) {
        mGameOverPanel = new QWidget;
        mGameOverPanel->setAttribute(Qt::WA_StyledBackground, true);
        mGameOverPanel->setStyleSheet(
            "background:rgba(18,18,24,235); border:3px solid #fe5f55; border-radius:24px;"
        );
        auto *vl = new QVBoxLayout(mGameOverPanel);
        vl->setContentsMargins(34, 28, 34, 28);
        vl->setSpacing(14);
        auto *title = new QLabel(mGameOverPanel);
        title->setObjectName("gameOverTitle");
        QFont tf = mCNFont; tf.setPixelSize(42); tf.setBold(true);
        title->setFont(tf);
        title->setAlignment(Qt::AlignCenter);
        title->setStyleSheet("color:#fe5f55; background:transparent; border:none;");
        vl->addWidget(title);
        auto *body = new QLabel(mGameOverPanel);
        body->setObjectName("gameOverBody");
        QFont bf = mCNFont; bf.setPixelSize(18);
        body->setFont(bf);
        body->setAlignment(Qt::AlignCenter);
        body->setWordWrap(true);
        body->setStyleSheet("color:white; background:transparent; border:none;");
        vl->addWidget(body);
        auto *row = new QWidget(mGameOverPanel);
        row->setStyleSheet("background:transparent; border:none;");
        auto *hl = new QHBoxLayout(row);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(12);
        auto *restart = makeBtn("重新开始", "#009dff", "#33b0ff", mCNFont, row, 48);
        auto *quit = makeBtn("退出", "#fe5f55", "#ff7066", mCNFont, row, 48);
        hl->addWidget(restart);
        hl->addWidget(quit);
        vl->addWidget(row);
        connect(restart, &QPushButton::clicked, this, [this]() {
            hideGameOverOverlay();
            mGameOverHandled = false;
            mScoringInProgress = false;
            clearObtainedTags();
            clearPlayedCards();
            for (auto *c : mHandCards) { if (c->scene()) mScene->removeItem(c); c->deleteLater(); }
            mHandCards.clear();
            mSelected.clear();
            mGameState->startGame();
            refreshGold();
            refreshCounters();
            refreshJokerSlots();
            refreshConsumableSlots();
            refreshScore();
        });
        connect(quit, &QPushButton::clicked, this, &MainWindow::close);
        mGameOverProxy = mScene->addWidget(mGameOverPanel);
        mGameOverProxy->setZValue(1500);
    }

    auto *title = mGameOverPanel->findChild<QLabel*>("gameOverTitle");
    auto *body = mGameOverPanel->findChild<QLabel*>("gameOverBody");
    if (title) title->setText(won ? "胜利" : "游戏结束");
    if (body) body->setText(won
        ? "你击败了所有盲注。"
        : QString("未达到盲注要求\n分数：%1 / %2\n底注：%3")
            .arg(mGameState->score()).arg(mGameState->targetScore()).arg(mGameState->ante()));
    mGameOverPanel->adjustSize();
    mGameOverProxy->setPos((mSceneW - mGameOverPanel->width()) / 2.0,
                           mSceneH + 40);
    mGameOverProxy->show();
    auto *anim = new QPropertyAnimation(mGameOverProxy, "pos", this);
    anim->setDuration(360);
    anim->setStartValue(mGameOverProxy->pos());
    anim->setEndValue(QPointF((mSceneW - mGameOverPanel->width()) / 2.0,
                              (mSceneH - mGameOverPanel->height()) / 2.0));
    anim->setEasingCurve(QEasingCurve::OutBack);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::hideGameOverOverlay()
{
    if (mGameOverProxy) mGameOverProxy->hide();
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
        mDisplayedMult = qMax(1, qRound(mDisplayedMult * ev.xmultValue));
        mLblMult->setText(QString::number(mDisplayedMult));
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
