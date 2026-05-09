#include "roundendoverlay.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QResizeEvent>

// 创建一个带颜色和字号的 QLabel
static QLabel *makeRowLabel(const QString &text, int px, const QString &color,
                            const QFont &font, QWidget *parent,
                            Qt::Alignment align = Qt::AlignCenter) {
    QLabel *l = new QLabel(text, parent);
    l->setAlignment(align);
    QFont f = font; f.setPixelSize(px);
    l->setFont(f);
    l->setStyleSheet(QString("color: %1;").arg(color));
    return l;
}

RoundEndOverlay::RoundEndOverlay(const QFont &cnFont,
                                 const QFont &pixelFont,
                                 QWidget *parent)
    : QWidget(parent)
    , mCNFont(cnFont)
    , mPixelFont(pixelFont)
{
    // 半透明黑色蒙层
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet("background: rgba(0, 0, 0, 160);");
    hide();   // 默认不显示，只在过关时调用 show()
    buildUi();
}

void RoundEndOverlay::buildUi()
{
    // ── 中央结算面板 ──
    mPanel = new QWidget(this);
    mPanel->setFixedSize(520, 460);
    mPanel->setStyleSheet(
        "background: #1e2230;"
        "border: 3px solid #f0c040;"
        "border-radius: 14px;"
        );

    auto *root = new QVBoxLayout(mPanel);
    root->setContentsMargins(28, 24, 28, 24);
    root->setSpacing(10);

    // ── 标题 ──
    mLblTitle = makeRowLabel("胜利！", 36, "#f0c040", mCNFont, mPanel);
    root->addWidget(mLblTitle);

    mLblBlind = makeRowLabel("击败 小盲注", 18, "#cccccc", mCNFont, mPanel);
    root->addWidget(mLblBlind);

    root->addSpacing(8);

    // ── 得分 / 目标对比 ──
    auto *scoreBox = new QWidget(mPanel);
    scoreBox->setStyleSheet("background:#161b28; border-radius:8px;");
    auto *sbl = new QVBoxLayout(scoreBox);
    sbl->setContentsMargins(12, 10, 12, 10);
    sbl->setSpacing(4);

    mLblScoreLine  = makeRowLabel("得分  ✳ 0",   24, "#ffffff", mPixelFont, scoreBox);
    mLblTargetLine = makeRowLabel("目标  ✳ 300", 16, "#888888", mPixelFont, scoreBox);
    sbl->addWidget(mLblScoreLine);
    sbl->addWidget(mLblTargetLine);
    root->addWidget(scoreBox);

    root->addSpacing(6);

    // ── 金币奖励明细 ──
    auto *rewardBox = new QWidget(mPanel);
    rewardBox->setStyleSheet("background:#161b28; border-radius:8px;");
    auto *rbl = new QVBoxLayout(rewardBox);
    rbl->setContentsMargins(12, 10, 12, 10);
    rbl->setSpacing(4);

    auto *rewardTitle = makeRowLabel("奖励", 14, "#888888", mCNFont,
                                     rewardBox, Qt::AlignLeft);
    rbl->addWidget(rewardTitle);

    // 创建一行"标签 + 金币值"的辅助函数
    auto makeRow = [&](const QString &label, QLabel *&valueLbl) {
        auto *row  = new QWidget(rewardBox);
        auto *hbl  = new QHBoxLayout(row);
        hbl->setContentsMargins(0, 0, 0, 0);
        auto *l    = makeRowLabel(label, 16, "#cccccc", mCNFont,
                               row, Qt::AlignLeft);
        valueLbl   = makeRowLabel("$0", 18, "#f0c040", mPixelFont,
                                row, Qt::AlignRight);
        hbl->addWidget(l, 1);
        hbl->addWidget(valueLbl);
        return row;
    };

    rbl->addWidget(makeRow("击败盲注奖励",          mLblBlindReward));
    rbl->addWidget(makeRow("剩余出牌奖励",          mLblHandBonus));
    rbl->addWidget(makeRow("利息（每5金 +1，封顶 5）", mLblInterest));

    // 分隔线
    auto *sep = new QWidget(rewardBox);
    sep->setFixedHeight(1);
    sep->setStyleSheet("background: #444;");
    rbl->addWidget(sep);

    // 合计
    auto *totalRow = new QWidget(rewardBox);
    auto *thbl = new QHBoxLayout(totalRow);
    thbl->setContentsMargins(0, 0, 0, 0);
    auto *tl = makeRowLabel("合计", 16, "#ffffff", mCNFont,
                            totalRow, Qt::AlignLeft);
    mLblTotal = makeRowLabel("$0", 22, "#f0c040", mPixelFont,
                             totalRow, Qt::AlignRight);
    thbl->addWidget(tl, 1);
    thbl->addWidget(mLblTotal);
    rbl->addWidget(totalRow);

    root->addWidget(rewardBox);

    root->addStretch();

    // ── 按钮 ──
    mBtnNext = new QPushButton("前往商店", mPanel);
    mBtnNext->setFixedHeight(56);
    QFont bf = mCNFont; bf.setPixelSize(20);
    mBtnNext->setFont(bf);
    mBtnNext->setCursor(Qt::PointingHandCursor);
    mBtnNext->setStyleSheet(
        "QPushButton {"
        "  background: #c07820; color: white;"
        "  border: none; border-radius: 8px;"
        "}"
        "QPushButton:hover  { background: #d08830; }"
        "QPushButton:pressed{ background: #a06010; }"
        );
    connect(mBtnNext, &QPushButton::clicked,
            this, &RoundEndOverlay::nextClicked);
    root->addWidget(mBtnNext);
}

void RoundEndOverlay::setData(const QString &blindName,
                              int score, int target,
                              int blindReward, int handBonus, int interest)
{
    mLblBlind      ->setText(QString("击败 %1").arg(blindName));
    mLblScoreLine  ->setText(QString("得分  ✳ %1").arg(score));
    mLblTargetLine ->setText(QString("目标  ✳ %1").arg(target));
    mLblBlindReward->setText(QString("$%1").arg(blindReward));
    mLblHandBonus  ->setText(QString("$%1").arg(handBonus));
    mLblInterest   ->setText(QString("$%1").arg(interest));
    mLblTotal      ->setText(QString("$%1").arg(blindReward + handBonus + interest));
}

void RoundEndOverlay::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    layoutPanel();
}

void RoundEndOverlay::layoutPanel()
{
    if (!mPanel) return;
    int x = (width()  - mPanel->width())  / 2;
    int y = (height() - mPanel->height()) / 2;
    mPanel->move(x, y);
}
