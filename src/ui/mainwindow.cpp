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
#include <QDialog>
#include <QTabWidget>
#include <QTextEdit>
#include <QTableWidget>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QScrollArea>
#include <QGridLayout>
#include <QFrame>
#include <QAbstractItemView>
#include <cmath>

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


static QString formatScoreNumber(double num)
{
    // 对齐原版 functions/misc_functions.lua:number_format：
    // 低于 1e11 使用千分位；达到 1e11 切换成 1.234e11 形式。
    if (num >= 100000000000.0) {
        int exp = int(std::floor(std::log10(num)));
        double mantissa = num / std::pow(10.0, exp);
        return QString::number(mantissa, 'f', 3) + "e" + QString::number(exp);
    }
    qint64 n = qRound64(num);
    QString raw = QString::number(n);
    QString out;
    int count = 0;
    for (int i = raw.size() - 1; i >= 0; --i) {
        out.prepend(raw[i]);
        ++count;
        if (count == 3 && i > 0) {
            out.prepend(',');
            count = 0;
        }
    }
    return out;
}

static QWidget *makeInfoCard(const QString &title, const QString &body, const QFont &cnFont, QWidget *parent = nullptr,
                             const QString &accent = "#fe5f55")
{
    auto *box = new QWidget(parent);
    box->setAttribute(Qt::WA_StyledBackground, true);
    box->setStyleSheet(QString(
        "background:rgba(42,57,60,235); border:2px solid %1; border-radius:12px;"
    ).arg(accent));
    auto *v = new QVBoxLayout(box);
    v->setContentsMargins(12, 8, 12, 8);
    v->setSpacing(4);
    auto *t = new QLabel(title, box);
    QFont tf = cnFont; tf.setPixelSize(17); tf.setBold(true);
    t->setFont(tf);
    t->setAlignment(Qt::AlignCenter);
    t->setStyleSheet("color:white; background:transparent; border:none;");
    v->addWidget(t);
    auto *b = new QLabel(body, box);
    QFont bf = cnFont; bf.setPixelSize(14); bf.setBold(true);
    b->setFont(bf);
    b->setAlignment(Qt::AlignCenter);
    b->setWordWrap(true);
    b->setStyleSheet("color:#e7f5f2; background:transparent; border:none;");
    v->addWidget(b, 1);
    return box;
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
        if (mDynamicBg) {
            mDynamicBg->setGeometry(mPlayPage->rect());
            mDynamicBg->lower();
        }
        if (mView) mView->raise();
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

    // 等窗口和布局进入事件循环后再触发第一帧盲注选择，
    // 避免 overlay 在默认 geometry 下先闪一下，再被 resize/动画拉走。
    QTimer::singleShot(0, this, [this]() {
        if (mBlindSelectWidget) mBlindSelectWidget->hide();
        mGameState->startGame();
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupLeftPanel() {
    mLeftPanel = new QWidget;
    mLeftPanel->setObjectName("LeftPanel");
    mLeftPanel->setFixedWidth(mLeftW);
    mLeftPanel->setAttribute(Qt::WA_StyledBackground, true);
    mLeftPanel->setStyleSheet(
        "QWidget#LeftPanel { background: rgba(31,39,42,238); border: none; border-radius: 0px; }"
        "QWidget#LeftPanel QWidget { border: none; }"
        "QWidget#LeftPanel QLabel { border: none; }"
        "QWidget#LeftPanel QPushButton { border: none; }"
        );

    QVBoxLayout *layout = new QVBoxLayout(mLeftPanel);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    // ── 上下文区(固定高度 200px,3 态切换) ──
    mContextArea = new QStackedWidget(mLeftPanel);
    mContextArea->setFixedHeight(215);
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
    mCtxBlind->setStyleSheet("background:#374244; border:none; border-radius:8px;");
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
    scoreBox->setFixedHeight(66);
    scoreBox->setAttribute(Qt::WA_StyledBackground, true);
    scoreBox->setStyleSheet("background:#374244; border:none; border-radius:8px;");
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
    QFont smf = mPixelFont; smf.setPixelSize(36);
    mLblScore->setFont(smf);
    mLblScore->setStyleSheet("color:white; background:transparent;");
    sbl->addWidget(mLblScore);

    layout->addWidget(scoreBox);

    // 牌型名行
    QWidget *handNameBox = new QWidget(mLeftPanel);
    handNameBox->setFixedHeight(54);
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
    chipsRow->setFixedHeight(74);
    QHBoxLayout *chipsLayout = new QHBoxLayout(chipsRow);
    chipsLayout->setContentsMargins(0, 0, 0, 0);
    chipsLayout->setSpacing(4);

    mLblChips = new QLabel("0", chipsRow);
    mLblChips->setAlignment(Qt::AlignCenter);
    QFont cf = mPixelFont; cf.setPixelSize(38);
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

        QDialog dlg(this);
        dlg.setModal(true);
        dlg.setWindowTitle("比赛信息");
        dlg.resize(qMin(960, int(mWinW * 0.60)), qMin(690, int(mWinH * 0.68)));
        dlg.setStyleSheet(
            "QDialog { background:rgba(27,43,45,246); border:4px solid #dbe9e7; border-radius:18px; }"
            "QLabel { color:white; background:transparent; border:none; }"
            "QPushButton { font-weight:bold; border:none; }"
        );

        auto *root = new QVBoxLayout(&dlg);
        root->setContentsMargins(18, 12, 18, 12);
        root->setSpacing(8);

        QWidget *top = new QWidget(&dlg);
        auto *topL = new QVBoxLayout(top);
        topL->setContentsMargins(0,0,0,0);
        topL->setSpacing(3);
        QLabel *arrow = new QLabel("▼", top);
        QFont af = mCNFont; af.setPixelSize(24); af.setBold(true);
        arrow->setFont(af);
        arrow->setAlignment(Qt::AlignCenter);
        arrow->setStyleSheet("color:#ff5f55;");
        topL->addWidget(arrow);
        QWidget *tabRow = new QWidget(top);
        auto *tabL = new QHBoxLayout(tabRow);
        tabL->setContentsMargins(70,0,70,0);
        tabL->setSpacing(12);
        topL->addWidget(tabRow);
        root->addWidget(top);

        QStackedWidget *pages = new QStackedWidget(&dlg);
        pages->setStyleSheet("background:transparent; border:none;");
        root->addWidget(pages, 1);

        auto makeTab = [&](const QString &txt, int pageIdx) {
            auto *b = new QPushButton(txt, tabRow);
            QFont f = mCNFont; f.setPixelSize(19); f.setBold(true);
            b->setFont(f);
            b->setFixedHeight(50);
            b->setStyleSheet(
                "QPushButton { background:#f04f47; color:white; border-radius:9px; padding:8px 24px; }"
                "QPushButton:hover { background:#ff665e; }"
                "QPushButton:pressed { background:#c63f38; }"
            );
            connect(b, &QPushButton::clicked, this, [pages, pageIdx, arrow, b]() {
                pages->setCurrentIndex(pageIdx);
                arrow->move(b->x() + b->width()/2 - arrow->width()/2, arrow->y());
            });
            tabL->addWidget(b, 1);
            return b;
        };

        auto makeDarkPage = [&](QWidget *parent) {
            auto *w = new QWidget(parent);
            w->setAttribute(Qt::WA_StyledBackground, true);
            w->setStyleSheet("background:rgba(31,48,50,230); border:3px solid #132023; border-radius:14px;");
            return w;
        };
        auto makeHandRow = [&](QWidget *parent, const QString &level, const QString &name,
                               const QString &chips, const QString &mult, const QString &played) {
            QWidget *row = new QWidget(parent);
            row->setFixedHeight(48);
            row->setAttribute(Qt::WA_StyledBackground, true);
            row->setStyleSheet("background:rgba(226,232,226,235); border:2px solid #bcd3cc; border-radius:11px;");
            auto *h = new QHBoxLayout(row);
            h->setContentsMargins(8,4,8,4);
            h->setSpacing(8);
            auto addPill = [&](const QString &txt, const QString &bg, int w) {
                QLabel *l = new QLabel(txt, row);
                QFont f = mCNFont; f.setPixelSize(16); f.setBold(true);
                l->setFont(f); l->setAlignment(Qt::AlignCenter);
                l->setFixedWidth(w);
                l->setStyleSheet(QString("background:%1; color:white; border-radius:10px; padding:3px 8px;").arg(bg));
                h->addWidget(l);
                return l;
            };
            addPill(level, "#eef7f4", 92)->setStyleSheet("background:#eef7f4; color:#23584f; border-radius:10px; padding:3px 8px;");
            QLabel *n = new QLabel(name, row);
            QFont nf = mCNFont; nf.setPixelSize(17); nf.setBold(true);
            n->setFont(nf); n->setAlignment(Qt::AlignCenter); n->setStyleSheet("color:white; background:transparent;");
            h->addWidget(n, 1);
            addPill(chips, "#009dff", 86);
            QLabel *x = new QLabel("X", row);
            QFont xf = mCNFont; xf.setPixelSize(18); xf.setBold(true); x->setFont(xf); x->setStyleSheet("color:#fe5f55; background:transparent;");
            h->addWidget(x);
            addPill(mult, "#fe5f55", 72);
            QLabel *hash = new QLabel("#", row); hash->setFont(xf); hash->setStyleSheet("color:white; background:transparent;"); h->addWidget(hash);
            addPill(played, "#3b4d50", 56);
            return row;
        };
        auto makeInfoTile = [&](QWidget *parent, const QString &title, const QString &body, const QString &accent) {
            QWidget *tile = new QWidget(parent);
            tile->setAttribute(Qt::WA_StyledBackground, true);
            tile->setStyleSheet(QString("background:rgba(14,23,25,215); border:3px solid %1; border-radius:13px;").arg(accent));
            auto *v = new QVBoxLayout(tile); v->setContentsMargins(12,8,12,8); v->setSpacing(5);
            QLabel *t = new QLabel(title, tile); QFont tf2=mCNFont; tf2.setPixelSize(19); tf2.setBold(true); t->setFont(tf2); t->setAlignment(Qt::AlignCenter); t->setStyleSheet(QString("color:%1;").arg(accent)); v->addWidget(t);
            QLabel *b = new QLabel(body, tile); QFont bf=mCNFont; bf.setPixelSize(15); bf.setBold(true); b->setFont(bf); b->setAlignment(Qt::AlignCenter); b->setWordWrap(true); b->setStyleSheet("color:#f4fbfb;"); v->addWidget(b,1);
            return tile;
        };

        // 牌型页：按原版一行一行显示，右侧是筹码 × 倍率与打出次数。
        QWidget *handPage = makeDarkPage(pages);
        auto *handV = new QVBoxLayout(handPage);
        handV->setContentsMargins(78, 18, 78, 18);
        handV->setSpacing(5);
        QVector<HandType> order = {
            HandType::FlushFive, HandType::FiveOfAKind, HandType::FlushHouse, HandType::Flush,
            HandType::Straight, HandType::ThreeOfAKind, HandType::TwoPair, HandType::Pair, HandType::HighCard
        };
        const auto &levels = mGameState->handLevels();
        for (HandType t : order) {
            HandLevel lv = levels.value(t);
            auto b = baseScore(t);
            handV->addWidget(makeHandRow(handPage,
                                         QString("等级%1").arg(lv.level),
                                         handName(t),
                                         formatScoreNumber(b.first + lv.chipsBonus),
                                         formatScoreNumber(b.second + lv.multBonus),
                                         QString::number(lv.played)));
        }
        pages->addWidget(handPage);

        // 盲注页：三列 Small/Big/Boss 样式。
        QWidget *blindPage = makeDarkPage(pages);
        auto *blindH = new QHBoxLayout(blindPage); blindH->setContentsMargins(50, 22, 50, 22); blindH->setSpacing(16);
        BossInfo bi = mGameState->currentBossInfo();
        BossInfo nextBi = bossInfo(mGameState->pendingBossEffect());
        blindH->addWidget(makeInfoTile(blindPage, "小盲注", QString("至少得分\n%1\n奖励：$$$+").arg(formatScoreNumber(qMax(1, mGameState->targetScore()*2/3))), "#009dff"), 1);
        blindH->addWidget(makeInfoTile(blindPage, "当前盲注", QString("至少得分\n%1\n奖励：$$$$+").arg(formatScoreNumber(mGameState->targetScore())), "#fda200"), 1);
        blindH->addWidget(makeInfoTile(blindPage, nextBi.name.isEmpty()?bi.name:nextBi.name,
                                       (nextBi.description.isEmpty()?bi.description:nextBi.description) + QString("\n\n至少得分\n%1\n奖励：$$$$$+").arg(formatScoreNumber(mGameState->targetScore()*4/3)), "#fe5f55"), 1);
        pages->addWidget(blindPage);

        // 优惠券页：空时显示原版式提示和票券图标；有券时显示图标网格。
        QWidget *voucherPage = makeDarkPage(pages);
        auto *voucherV = new QVBoxLayout(voucherPage); voucherV->setContentsMargins(70, 34, 70, 34); voucherV->setSpacing(18); voucherV->setAlignment(Qt::AlignCenter);
        QLabel *voucherTitle = new QLabel("本赛局兑换的优惠券", voucherPage); QFont vtf=mCNFont; vtf.setPixelSize(25); vtf.setBold(true); voucherTitle->setFont(vtf); voucherTitle->setAlignment(Qt::AlignCenter); voucherV->addWidget(voucherTitle);
        if (mGameState->redeemedVouchers().isEmpty()) {
            QLabel *empty = new QLabel("本赛局尚未兑换任何优惠券", voucherPage); QFont ef=mCNFont; ef.setPixelSize(22); ef.setBold(true); empty->setFont(ef); empty->setAlignment(Qt::AlignCenter); voucherV->addWidget(empty, 1);
        } else {
            QWidget *gridW = new QWidget(voucherPage); auto *grid = new QGridLayout(gridW); grid->setSpacing(12); grid->setContentsMargins(0,0,0,0);
            int vi = 0;
            QPixmap voucherSheet(":/textures/images/Vouchers.png");
            for (VoucherType v : mGameState->redeemedVouchers()) {
                const VoucherData vd = voucherData(v);
                QWidget *card = new QWidget(gridW); card->setFixedSize(205, 92); card->setAttribute(Qt::WA_StyledBackground,true);
                card->setStyleSheet("background:rgba(11,20,22,215); border:2px solid #51696d; border-radius:10px;");
                auto *h = new QHBoxLayout(card); h->setContentsMargins(8,6,8,6); h->setSpacing(8);
                QLabel *img = new QLabel(card); img->setFixedSize(54,72); img->setAlignment(Qt::AlignCenter);
                if (!voucherSheet.isNull()) {
                    QPoint c = vd.spritePos;
                    QPixmap pm = voucherSheet.copy(c.x()*ConsumableItem::WIDTH, c.y()*ConsumableItem::HEIGHT, ConsumableItem::WIDTH, ConsumableItem::HEIGHT);
                    img->setPixmap(pm.scaled(img->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
                }
                h->addWidget(img);
                QLabel *txt = new QLabel(vd.name + "\n" + vd.description, card); QFont txf=mCNFont; txf.setPixelSize(13); txf.setBold(true); txt->setFont(txf); txt->setWordWrap(true); txt->setStyleSheet("color:white;"); h->addWidget(txt,1);
                grid->addWidget(card, vi/2, vi%2); ++vi;
            }
            voucherV->addWidget(gridW, 1, Qt::AlignCenter);
        }
        pages->addWidget(voucherPage);

        // 赌注页：本阶段还未做白注/牌组限制，先按原版格式展示占位和当前数据。
        QWidget *stakePage = makeDarkPage(pages);
        auto *stakeV = new QVBoxLayout(stakePage); stakeV->setContentsMargins(110, 42, 110, 42); stakeV->setSpacing(14);
        stakeV->addWidget(makeInfoTile(stakePage, "蓝注", "弃牌次数 -1", "#009dff"));
        stakeV->addWidget(makeInfoTile(stakePage, "同样起效", "商店可能会出现永恒小丑牌\n底注提升时，过关需求分数的增速更快\n小盲注没有奖励金", "#4bc292"));
        stakeV->addWidget(makeInfoTile(stakePage, "当前赌注", QString("底注 %1/8　金钱 $%2\n小丑 %3/%4　消耗牌 %5/%6\n牌组 %7/%8")
                                             .arg(mGameState->ante()).arg(mGameState->gold())
                                             .arg(mGameState->jokers().size()).arg(mGameState->jokerSlots())
                                             .arg(mGameState->consumables().size()).arg(mGameState->consumableSlots())
                                             .arg(mGameState->deckRemaining()).arg(mGameState->deckTotal()), "#fe5f55"));
        pages->addWidget(stakePage);

        QVector<QPushButton*> tabButtons;
        tabButtons << makeTab("牌型", 0) << makeTab("盲注", 1) << makeTab("优惠券", 2) << makeTab("赌注", 3);
        pages->setCurrentIndex(0);

        auto *back = new QPushButton("返回", &dlg);
        QFont bf = mCNFont; bf.setPixelSize(21); bf.setBold(true); back->setFont(bf);
        back->setFixedHeight(46);
        back->setStyleSheet("QPushButton { background:#fda200; color:white; border:none; border-radius:9px; } QPushButton:hover { background:#ffb730; }");
        connect(back, &QPushButton::clicked, &dlg, &QDialog::accept);
        root->addWidget(back);
        dlg.exec();
    });

    QPushButton *btnOptions = makeBtn("选项", "#fda200", "#ffb730", mCNFont, btnCol, 70);
    btnOptions->setFixedWidth(76);
    btnVbl->addWidget(btnOptions);
    connect(btnOptions, &QPushButton::clicked, this, [this]() {
        QDialog dlg(this);
        dlg.setWindowTitle("选项");
        dlg.setModal(true);
        dlg.resize(520, 560);
        dlg.setStyleSheet(
            "QDialog { background:rgba(36,51,54,245); border:3px solid #dce9e9; border-radius:18px; }"
            "QPushButton { background:#fe5f55; color:white; border:none; border-radius:10px; padding:13px 24px; font-size:20px; font-weight:bold; }"
            "QPushButton:hover { background:#ff7066; }"
            "QPushButton#back { background:#fda200; }"
            "QLabel { color:white; background:transparent; }"
        );
        auto *v = new QVBoxLayout(&dlg);
        v->setContentsMargins(34, 30, 34, 30);
        v->setSpacing(14);
        QFont titleFont = mCNFont; titleFont.setPixelSize(25); titleFont.setBold(true);
        auto *title = new QLabel("选项", &dlg);
        title->setFont(titleFont);
        title->setAlignment(Qt::AlignCenter);
        v->addWidget(title);

        QStringList items = {"设置", "开始新的一局", "主菜单", "统计数据", "收藏", "定制牌组"};
        for (const QString &txt : items) {
            auto *b = new QPushButton(txt, &dlg);
            b->setFont(titleFont);
            if (txt == "开始新的一局") {
                connect(b, &QPushButton::clicked, this, [this, &dlg]() {
                    dlg.accept();
                    mGameState->startGame();
                    refreshHand(); refreshJokerSlots(); refreshConsumableSlots(); refreshCounters(); refreshScore(); refreshGold();
                });
            } else {
                b->setEnabled(false);
            }
            v->addWidget(b);
        }
        auto *back = new QPushButton("返回", &dlg);
        back->setObjectName("back");
        connect(back, &QPushButton::clicked, &dlg, &QDialog::accept);
        v->addWidget(back);
        dlg.exec();
    });

    brl->addWidget(btnCol);

    // 右列:出牌/弃牌、金币、底注/回合 全部堆在这里
    QWidget *rightCol = new QWidget(bottomRow);
    auto *rcvbl = new QVBoxLayout(rightCol);
    rcvbl->setContentsMargins(0, 0, 0, 0);
    rcvbl->setSpacing(6);

    // 出牌 / 弃牌 (横排)
    QWidget *handsRow = new QWidget(rightCol);
    handsRow->setFixedHeight(66);
    handsRow->setAttribute(Qt::WA_StyledBackground, true);
    handsRow->setStyleSheet("background:#374244; border:none; border-radius:8px;");
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
    goldRow->setFixedHeight(48);
    goldRow->setAttribute(Qt::WA_StyledBackground, true);
    goldRow->setStyleSheet("background:#374244; border:none; border-radius:8px;");
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
    anteBox->setFixedHeight(54);
    anteBox->setAttribute(Qt::WA_StyledBackground, true);
    anteBox->setStyleSheet("background:#374244; border:none; border-radius:8px;");
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
    roundBox->setFixedHeight(54);
    roundBox->setAttribute(Qt::WA_StyledBackground, true);
    roundBox->setStyleSheet("background:#374244; border:none; border-radius:8px;");
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
    // 原游戏把背景当成单独的 shader 全屏层来画；不要把动态背景塞进 QGraphicsScene。
    // 否则背景每 16ms update 一次，就会把所有卡牌、按钮、文字一起拖着重绘。
    mDynamicBg = new DynamicBackgroundItem(mPlayPage);
    mDynamicBg->setGeometry(mPlayPage ? mPlayPage->rect() : QRect(0, 0, mSceneW, mSceneH));
    mDynamicBg->setSceneSize(mDynamicBg->width() > 0 ? mDynamicBg->width() : mSceneW,
                             mDynamicBg->height() > 0 ? mDynamicBg->height() : mSceneH);
    mDynamicBg->setMood(DynamicBackgroundItem::Mood::Default);
    mDynamicBg->show();
    mDynamicBg->lower();

    mView = new QGraphicsView(mScene, mPlayPage);
    mView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mView->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    mView->setFrameShape(QFrame::NoFrame);
    mView->setStyleSheet("background: transparent; border: none;");
    mView->setAttribute(Qt::WA_TranslucentBackground, true);
    mView->setAutoFillBackground(false);
    mView->viewport()->setAttribute(Qt::WA_TranslucentBackground, true);
    mView->viewport()->setAutoFillBackground(false);
    mView->setBackgroundBrush(QBrush(Qt::NoBrush));

    // 前景场景只在牌/按钮变化时刷新；背景动画在下层 QWidget 自己刷新。
    mView->setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    mView->setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing, true);
    mView->setOptimizationFlag(QGraphicsView::DontSavePainterState, true);

    mScene->setSceneRect(0, 0, mSceneW, mSceneH);
    mScene->setBackgroundBrush(QBrush(Qt::NoBrush));

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
        // DNA、标准包、死神等会制造“牌面完全相同”的牌。
        // 只按花色/点数匹配会把新牌和旧牌混成同一张，导致选中状态、拖拽状态错乱。
        if (a.uid > 0 && b.uid > 0) return a.uid == b.uid;
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
            connect(match, &CardItem::dragMoved,
                    this, &MainWindow::onHandCardDragMoved);
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
    mLblScore->setText(formatScoreNumber(mGameState->score()));
    mLblTarget->setText(formatScoreNumber(mGameState->targetScore()));
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
            QString("%1/%2").arg(formatScoreNumber(mGameState->deckRemaining())).arg(formatScoreNumber(mGameState->deckTotal())));
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
        int visualSlots = Constants::MAX_CONSUMABLE_SLOTS;
        int totalW = CARD_W + qMax(0, visualSlots - 1) * (CARD_W + 14);
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


void MainWindow::onHandCardDragMoved(CardItem *card, QPointF scenePos)
{
    int from = mHandCards.indexOf(card);
    if (from < 0) return;
    int n = mHandCards.size();
    if (n <= 1) return;

    int available = mSceneW - 80;
    int step = (available - CARD_W) / qMax(1, n - 1);
    step = qMin(step, CARD_W - 30);
    int totalW = (n - 1) * step + CARD_W;
    int startX = (mSceneW - totalW) / 2;

    int to = 0;
    for (int i = 0; i < n; ++i) {
        double center = startX + i * step + CARD_W / 2.0;
        if (scenePos.x() > center) to = i;
    }
    to = qBound(0, to, n - 1);

    QVector<CardItem*> visual = mHandCards;
    visual.removeAt(from);
    visual.insert(to, card);

    for (int vi = 0; vi < visual.size(); ++vi) {
        CardItem *ci = visual[vi];
        if (ci == card) continue;
        int realIdx = mHandCards.indexOf(ci);
        bool sel = mSelected.contains(realIdx);
        double t = (-n / 2.0 - 0.5 + (vi + 1)) / n;
        double angleDeg = 0.2 * t * 180.0 / M_PI;
        int x = startX + vi * step;
        int y = mHandY + (sel ? -50 : 0);
        ci->setBaseRotation(angleDeg);
        ci->setZValue(10 + vi);
        ci->moveTo(QPointF(x, y), 120);
    }
    card->setZValue(600);
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
    mLblChips->setText(formatScoreNumber(mDisplayedChips));
    mLblMult ->setText(formatScoreNumber(mDisplayedMult));

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
        mLblChips->setText(formatScoreNumber(r.chips));
        mLblMult ->setText(formatScoreNumber(mDisplayedMult));
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
        if (mDynamicBg) {
            mDynamicBg->setGeometry(r);
            mDynamicBg->setSceneSize(r.width(), r.height());
            mDynamicBg->lower();
        }
        if (mBlindSelectWidget) { mBlindSelectWidget->setGeometry(lowerOverlayRect()); if (mBlindSelectWidget->isVisible()) mBlindSelectWidget->raise(); }
        if (mRoundEndOverlay)   { mRoundEndOverlay  ->setGeometry(r);                 if (mRoundEndOverlay->isVisible())   mRoundEndOverlay->raise(); }
        if (mShopWidget)        { mShopWidget       ->setGeometry(lowerOverlayRect()); if (mShopWidget->isVisible())        mShopWidget->raise(); }
        if (mPackOpenWidget)    { mPackOpenWidget   ->setGeometry(lowerOverlayRect()); if (mPackOpenWidget->isVisible())    mPackOpenWidget->raise(); }
        if (mDeckViewWidget)    { mDeckViewWidget   ->setGeometry(r);                 if (mDeckViewWidget->isVisible())    mDeckViewWidget->raise(); }
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
        connect(ji, &JokerItem::dragMoved, this, &MainWindow::onJokerDragMoved);
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
    // 售出面板打开后，鼠标经过旁边小丑不能抢焦点，否则售出按钮会消失。
    if (!showSellButton && mSelectedJokerIdx >= 0 && mSelectedJokerIdx != idx) return;
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

        // 不能再把小丑信息框放进 QGraphicsScene：商店/开包是 QWidget overlay，
        // QGraphicsProxyWidget 会被 overlay 挡住。改成挂在 mPlayPage 上并 raise，
        // 这样在商店、盲注选择、开包时都能显示在最上层。
        mJokerInfoPanel->setParent(mPlayPage);
        mJokerInfoPanel->hide();
        mJokerInfoProxy = nullptr;
    }

    mJokerInfoName->setText(j.name);
    QString editionText = editionName(j.edition);
    if (editionText.isEmpty()) editionText = "普通";
    QString editionEffect = editionDesc(j.edition);
    QString meta = QString("%1小丑　出售 $%2").arg(editionText).arg(qMax(1, j.sellValue));
    if (!editionEffect.isEmpty()) meta += QString("　%1").arg(editionEffect);
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
    mJokerInfoName->setVisible(!showSellButton);
    mJokerInfoDesc->setVisible(!showSellButton);
    mJokerInfoDesc->setText(desc);

    if (showSellButton) {
        // 原版点击牌后只出现一个很小的操作牌片：上方价格、下方售出。
        // 详细说明仍由 hover 面板负责，这里不要再塞大段文字。
        if (auto *lay = mJokerInfoPanel->layout()) {
            lay->setContentsMargins(6, 5, 6, 6);
            lay->setSpacing(3);
        }
        mJokerInfoPanel->setStyleSheet(
            "background:transparent;"
            "border:none;"
        );
        mJokerInfoPanel->setFixedSize(90, 54);

        QFont mf = mCNFont; mf.setPixelSize(12); mf.setBold(true);
        mJokerInfoMeta->setFont(mf);
        mJokerInfoMeta->setAlignment(Qt::AlignCenter);
        mJokerInfoMeta->setStyleSheet(
            "color:#ffd05a; background:rgba(8,11,12,220);"
            "border:2px solid #101719; border-radius:8px; padding:0px 4px;"
        );
        mJokerInfoMeta->setText(QString("$%1").arg(qMax(1, j.sellValue)));
        mJokerInfoMeta->setFixedHeight(20);

        QFont sf = mCNFont; sf.setPixelSize(13); sf.setBold(true);
        mJokerSellButton->setFont(sf);
        mJokerSellButton->setText(QString("售出 +$%1").arg(qMax(1, j.sellValue)));
        mJokerSellButton->setFixedSize(82, 25);
        mJokerSellButton->setStyleSheet(
            "QPushButton { background:#ff5b4f; color:white; border:0px;"
            "border-radius:7px; padding:0px; }"
            "QPushButton:hover { background:#ff7468; }"
            "QPushButton:pressed { background:#d94b42; }"
        );
    } else {
        if (auto *lay = mJokerInfoPanel->layout()) {
            lay->setContentsMargins(12, 10, 12, 10);
            lay->setSpacing(6);
        }
        mJokerInfoPanel->setMinimumSize(0, 0);
        mJokerInfoPanel->setMaximumSize(16777215, 16777215);
        mJokerInfoPanel->setStyleSheet(
            "background:rgba(31,37,42,235);"
            "border:2px solid #fda200;"
            "border-radius:12px;"
        );
        QFont mf = mCNFont; mf.setPixelSize(13); mf.setBold(false);
        mJokerInfoMeta->setFont(mf);
        mJokerInfoMeta->setStyleSheet("color:#cbd6dc; background:transparent; border:none;");
        mJokerInfoMeta->setMinimumHeight(0);
        mJokerInfoMeta->setMaximumHeight(16777215);
        mJokerInfoMeta->setText(meta);
        mJokerSellButton->setMinimumSize(0, 0);
        mJokerSellButton->setMaximumSize(16777215, 16777215);
    }
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
        // 单击后，紧贴小丑右侧出现小型售出牌片；右侧空间不够时放到左侧。
        x = jp.x() + CARD_W + 8;
        y = jp.y() + CARD_H * 0.5 - 31;
        if (x + 90 > mSceneW - 6) x = jp.x() - 98;
        x = qBound<qreal>(6, x, mSceneW - 96);
        y = qBound<qreal>(6, y, mSceneH - 60);
    } else {
        // 悬浮说明默认放在小丑下方；如果商店面板挡住，就放到小丑上方。
        x = jp.x() + CARD_W / 2.0 - 140;
        x = qBound<qreal>(8, x, mSceneW - 285);
        y = jp.y() + CARD_H + 10;
        if (mShopWidget && mShopWidget->isVisible()) {
            qreal shopTopSceneY = lowerOverlayRect().y() + 18;
            if (y + mJokerInfoPanel->height() > shopTopSceneY)
                y = jp.y() - mJokerInfoPanel->height() - 12;
        }
    }
    if (mJokerInfoPanel->parentWidget() != mPlayPage) mJokerInfoPanel->setParent(mPlayPage);
    QPoint viewPoint = mView->mapFromScene(QPointF(x, y));
    QPoint pagePoint = mView->mapTo(mPlayPage, viewPoint);
    pagePoint.setX(qBound(6, pagePoint.x(), qMax(6, mPlayPage->width() - mJokerInfoPanel->width() - 6)));
    pagePoint.setY(qBound(6, pagePoint.y(), qMax(6, mPlayPage->height() - mJokerInfoPanel->height() - 6)));
    mJokerInfoPanel->move(pagePoint);
    mJokerInfoPanel->raise();
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


void MainWindow::onJokerDragMoved(JokerItem *item, QPointF scenePos)
{
    int from = mJokerItems.indexOf(item);
    if (from < 0) return;
    int n = mJokerItems.size();
    if (n <= 1) return;

    int visualSlots = Constants::MAX_JOKER_SLOTS;
    int visualStep = CARD_W + 14;
    int visualW = CARD_W + qMax(0, visualSlots - 1) * visualStep;
    int available = qMin(mSceneW - 430, 780);
    int startX = 8;
    if (visualW < available) startX = 8 + (available - visualW) / 2;
    int step = (n > 1) ? (visualW - CARD_W) / qMax(1, n - 1) : visualStep;
    step = qBound(42, step, visualStep);

    int to = 0;
    for (int i = 0; i < n; ++i) {
        double center = startX + i * step + CARD_W / 2.0;
        if (scenePos.x() > center) to = i;
    }
    to = qBound(0, to, n - 1);

    QVector<JokerItem*> visual = mJokerItems;
    visual.removeAt(from);
    visual.insert(to, item);
    for (int vi = 0; vi < visual.size(); ++vi) {
        JokerItem *ji = visual[vi];
        if (ji == item) continue;
        int x = startX + vi * step;
        int y = JOKER_Y + 18;
        ji->setZValue(20 + vi);
        ji->moveTo(QPointF(x, y), 120);
    }
    item->setZValue(650);
}

void MainWindow::onJokerDragReleased(JokerItem *item, QPointF scenePos)
{
    int from = mJokerItems.indexOf(item);
    if (from < 0) { refreshJokerSlots(); return; }
    int n = mJokerItems.size();
    if (n <= 1) { refreshJokerSlots(); return; }

    int visualSlots = Constants::MAX_JOKER_SLOTS;
    int visualStep = CARD_W + 14;
    int visualW = CARD_W + qMax(0, visualSlots - 1) * visualStep;
    int available = qMin(mSceneW - 430, 780);
    int startX = 8;
    if (visualW < available) startX = 8 + (available - visualW) / 2;
    int step = (n > 1) ? (visualW - CARD_W) / qMax(1, n - 1) : visualStep;
    step = qBound(42, step, visualStep);

    int to = 0;
    for (int i = 0; i < n; ++i) {
        double center = startX + i * step + CARD_W / 2.0;
        if (scenePos.x() > center) to = i;
    }
    to = qBound(0, to, n - 1);

    if (from != to) mGameState->moveJoker(from, to);
    else refreshJokerSlots();
}


void MainWindow::showConsumableAction(int idx)
{
    const auto &cs = mGameState->consumables();
    if (idx < 0 || idx >= cs.size() || idx >= mConsumableItems.size()) return;
    mSelectedConsumableIdx = idx;
    const Consumable &c = cs[idx];

    if (!mConsumableActionPanel) {
        mConsumableActionPanel = new QWidget;
        mConsumableActionPanel->setAttribute(Qt::WA_StyledBackground, true);
        mConsumableActionPanel->setStyleSheet(
            "background:rgba(18,23,26,230); border:2px solid #2b3135; border-radius:8px;"
        );
        auto *v = new QVBoxLayout(mConsumableActionPanel);
        v->setContentsMargins(6, 5, 6, 6);
        v->setSpacing(3);

        mConsumableActionPrice = new QLabel(mConsumableActionPanel);
        QFont pf = mCNFont; pf.setPixelSize(12); pf.setBold(true);
        mConsumableActionPrice->setFont(pf);
        mConsumableActionPrice->setAlignment(Qt::AlignCenter);
        mConsumableActionPrice->setStyleSheet(
            "color:#ffd05a; background:rgba(0,0,0,150); border:1px solid #0d1114; border-radius:7px; padding:1px 4px;"
        );
        v->addWidget(mConsumableActionPrice);

        auto *row = new QWidget(mConsumableActionPanel);
        auto *h = new QHBoxLayout(row);
        h->setContentsMargins(0,0,0,0);
        h->setSpacing(4);
        QFont bf = mCNFont; bf.setPixelSize(12); bf.setBold(true);
        mConsumableUseButton = new QPushButton("使用", row);
        mConsumableSellButton = new QPushButton("售出", row);
        for (QPushButton *b : {mConsumableUseButton, mConsumableSellButton}) {
            b->setFont(bf);
            b->setFixedSize(48, 26);
            b->setStyleSheet(
                "QPushButton { background:#ff5b4f; color:white; border:0px; border-radius:7px; padding:0px; }"
                "QPushButton:hover { background:#ff7468; }"
                "QPushButton:pressed { background:#d94b42; }"
            );
            h->addWidget(b);
        }
        mConsumableUseButton->setStyleSheet(
            "QPushButton { background:#009dff; color:white; border:0px; border-radius:7px; padding:0px; }"
            "QPushButton:hover { background:#33b0ff; }"
            "QPushButton:pressed { background:#007acc; }"
        );
        v->addWidget(row);

        mConsumableActionPanel->setFixedSize(110, 62);
        mConsumableActionProxy = mScene->addWidget(mConsumableActionPanel);
        mConsumableActionProxy->setZValue(910);

        connect(mConsumableUseButton, &QPushButton::clicked, this, [this]() {
            if (mSelectedConsumableIdx < 0) return;
            QVector<int> sel = mSelected;
            std::sort(sel.begin(), sel.end());
            if (mGameState->useConsumable(mSelectedConsumableIdx, sel)) {
                mSelectedConsumableIdx = -1;
                hideConsumableAction();
                mSelected.clear();
                refreshHand();
                refreshGold();
                refreshScore();
                refreshCounters();
                if (mShopWidget && mShopWidget->isVisible()) mShopWidget->refresh();
            } else {
                mConsumableActionPanel->setStyleSheet(
                    "background:rgba(42,18,20,235); border:2px solid #ff6a6a; border-radius:8px;"
                );
                QTimer::singleShot(260, this, [this]() {
                    if (mConsumableActionPanel)
                        mConsumableActionPanel->setStyleSheet(
                            "background:rgba(18,23,26,230); border:2px solid #2b3135; border-radius:8px;"
                        );
                });
            }
        });
        connect(mConsumableSellButton, &QPushButton::clicked, this, [this]() {
            if (mSelectedConsumableIdx < 0) return;
            if (mGameState->sellConsumable(mSelectedConsumableIdx)) {
                mSelectedConsumableIdx = -1;
                hideConsumableAction();
                refreshConsumableSlots();
                refreshGold();
                refreshCounters();
                if (mShopWidget && mShopWidget->isVisible()) mShopWidget->refresh();
            }
        });
    }

    mConsumableActionPrice->setText(QString("$%1").arg(qMax(1, c.sellValue)));
    mConsumableSellButton->setText(QString("售出"));
    QPointF cp = mConsumableItems[idx]->pos();
    qreal x = cp.x() + CARD_W + 8;
    qreal y = cp.y() + CARD_H * 0.5 - 31;
    if (x + 110 > mSceneW - 6) x = cp.x() - 118;
    x = qBound<qreal>(6, x, mSceneW - 116);
    y = qBound<qreal>(6, y, mSceneH - 68);
    mConsumableActionProxy->setPos(x, y);
    mConsumableActionPanel->show();
}

void MainWindow::hideConsumableAction()
{
    if (mConsumableActionPanel) mConsumableActionPanel->hide();
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
    if (mSelectedConsumableIdx >= cs.size()) {
        mSelectedConsumableIdx = -1;
        hideConsumableAction();
    }
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
        hideConsumableAction();
        if (mShopWidget && mShopWidget->isVisible()) mShopWidget->refresh();
        return;
    }

    // 开塔罗/幻灵包时，点击右上角仓库特殊牌仍然直接使用，目标是包界面的临时手牌。
    // 平时左键点击则像原版一样选中消耗牌，并在右侧弹出小型“使用/售出”牌片。
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

    if (btn == Qt::LeftButton) {
        showConsumableAction(idx);
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
            mBlindSelectWidget->setGeometry(lowerOverlayRect());
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
    // 商店/开包/盲注选择只覆盖中下区，保留上方小丑槽、右上消耗槽、右下牌组可点击。
    const int y = JOKER_Y + JOKER_H + 10;
    const int rightDeckReserve = CARD_W + 150;
    return QRect(0, y, qMax(600, mPlayPage->width() - rightDeckReserve), qMax(0, mPlayPage->height() - y));
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
        if (mDynamicBg) {
            mDynamicBg->setGeometry(r);
            mDynamicBg->setSceneSize(r.width(), r.height());
            mDynamicBg->lower();
        }
        if (mBlindSelectWidget) { mBlindSelectWidget->setGeometry(lowerOverlayRect()); if (mBlindSelectWidget->isVisible()) mBlindSelectWidget->raise(); }
        if (mRoundEndOverlay)   { mRoundEndOverlay  ->setGeometry(r);                 if (mRoundEndOverlay->isVisible())   mRoundEndOverlay->raise(); }
        if (mShopWidget)        { mShopWidget       ->setGeometry(lowerOverlayRect()); if (mShopWidget->isVisible())        mShopWidget->raise(); }
        if (mPackOpenWidget)    { mPackOpenWidget   ->setGeometry(lowerOverlayRect()); if (mPackOpenWidget->isVisible())    mPackOpenWidget->raise(); }
        if (mDeckViewWidget)    { mDeckViewWidget   ->setGeometry(r);                 if (mDeckViewWidget->isVisible())    mDeckViewWidget->raise(); }
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
    if (!mBlindSelectWidget || !mPlayPage) return;

    const bool skipped = mGameState->justSkipped();
    mBlindSelectWidget->hide();
    mBlindSelectWidget->setGeometry(lowerOverlayRect());
    mBlindSelectWidget->refresh();

    // 首次进入时，先把三张盲注卡放到屏幕下方，再 show。
    // 这样不会出现“先完整显示一帧 -> 消失 -> 再滑入”的闪屏。
    if (!skipped) {
        mBlindSelectWidget->prepareEntrancePositions();
    } else {
        mBlindSelectWidget->arrangeCards(false);
    }

    mBlindSelectWidget->raise();
    mBlindSelectWidget->show();

    QTimer::singleShot(0, this, [this, skipped]() {
        if (!mBlindSelectWidget || !mPlayPage) return;
        if (mBlindSelectWidget->geometry() != lowerOverlayRect())
            mBlindSelectWidget->setGeometry(lowerOverlayRect());
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
        mLblScore->setText(formatScoreNumber(v.toDouble()));
        QFont f = mLblScore->font();
        f.setPixelSize(30 + (v.toInt() % 2));
        mLblScore->setFont(f);
    });
    connect(anim, &QVariantAnimation::finished, this, [this, after, gained]() {
        mLblScore->setText(formatScoreNumber(after));
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
    mLblChips->setText(formatScoreNumber(r.chips));
    mLblMult ->setText(formatScoreNumber(r.mult));
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
        text = QString("+%1").arg(formatScoreNumber(ev.intValue));
        mDisplayedChips += ev.intValue;
        mLblChips->setText(formatScoreNumber(mDisplayedChips));
        break;

    case ScoreEventKind::EnhancementMult:
    case ScoreEventKind::EditionMult:
    case ScoreEventKind::JokerMult:
        color = QColor("#fe5f55");   // 红
        text = QString("+%1").arg(formatScoreNumber(ev.intValue));
        mDisplayedMult += ev.intValue;
        mLblMult->setText(formatScoreNumber(mDisplayedMult));
        break;

    case ScoreEventKind::EnhancementXMult:
    case ScoreEventKind::EditionXMult:
    case ScoreEventKind::SteelXMult:
    case ScoreEventKind::JokerXMult:
        color = QColor("#fe5f55");   // 红
        text = QString("×%1").arg(QString::number(ev.xmultValue, 'g', 3));
        isXMult = true;
        mDisplayedMult = qMax(1, qRound(mDisplayedMult * ev.xmultValue));
        mLblMult->setText(formatScoreNumber(mDisplayedMult));
        break;
    }

    // 原版计分时只有真正参与本次事件的牌/小丑会 juice_up。
    // 计分牌向上弹一下再回位；非计分牌不会跟着动。
    if (sourceCard) {
        QPointF base = sourceCard->pos();
        auto *seq = new QSequentialAnimationGroup(sourceCard);
        auto *up = new QPropertyAnimation(sourceCard, "pos");
        up->setDuration(90);
        up->setStartValue(base);
        up->setEndValue(base + QPointF(0, -26));
        up->setEasingCurve(QEasingCurve::OutCubic);
        auto *down = new QPropertyAnimation(sourceCard, "pos");
        down->setDuration(140);
        down->setStartValue(base + QPointF(0, -26));
        down->setEndValue(base);
        down->setEasingCurve(QEasingCurve::OutBounce);
        seq->addAnimation(up);
        seq->addAnimation(down);
        up->setParent(seq); down->setParent(seq);
        seq->start(QAbstractAnimation::DeleteWhenStopped);
        sourceCard->juiceUp(1.18, 210);
    }
    if (sourceJoker) {
        sourceJoker->juiceUp(1.15, 200);
    }

    spawnFloatingText(anchorPos, text, color);
}
