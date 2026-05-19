#include "roundendoverlay.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPixmap>
#include <QPropertyAnimation>
#include <cmath>
#include <algorithm>
#include <QEasingCurve>
#include <QGraphicsDropShadowEffect>
#include <QColor>
#include <QResizeEvent>
#include <QPainter>
#include <QPen>
#include <QPaintEvent>
#include "animatedblindchip.h"


static QString formatLargeNumber(double num)
{
    if (std::isnan(num)) return QStringLiteral("NaNeInf");
    if (std::isinf(num)) return QStringLiteral("Inf");
    const bool neg = num < 0.0;
    num = std::abs(num);
    if (num >= 100000000000.0) {
        int exp = int(std::floor(std::log10(std::max(num, 1.0))));
        double mantissa = num / std::pow(10.0, exp);
        return QString("%1%2e%3").arg(neg ? "-" : "")
                                  .arg(QString::number(mantissa, 'f', 3))
                                  .arg(exp);
    }
    qint64 n = qRound64(num);
    QString raw = QString::number(n);
    QString out;
    int count = 0;
    for (int i = raw.size() - 1; i >= 0; --i) {
        out.prepend(raw[i]);
        ++count;
        if (count == 3 && i > 0) { out.prepend(','); count = 0; }
    }
    if (neg && out != "0") out.prepend('-');
    return out;
}

RoundEndOverlay::RoundEndOverlay(const QFont &cnFont, const QFont &pixelFont, QWidget *parent)
    : QWidget(parent), mCNFont(cnFont), mPixelFont(pixelFont)
{
    setAttribute(Qt::WA_StyledBackground, true);
    // 取消全屏蒙层 —— overlay 自身就只覆盖面板区域，外部的小丑/牌堆区域不再被吃掉 hover。
    setStyleSheet("background: transparent;");
    buildUi();
}

void RoundEndOverlay::buildUi()
{
    // ── 主面板（外层 slate；用原版 G.C.BLACK = HEX("374244")，与 dyn_container 主体一致）──
    auto *panel = new QWidget(this);
    panel->setAttribute(Qt::WA_StyledBackground, true);
    panel->setObjectName("rePanel");
    panel->setStyleSheet(
        "QWidget#rePanel {"
        " background:#374244;"          // BLACK
        " border-top:3px solid #4f6367;"// L_BLACK 描边
        " border-left:3px solid #4f6367;"
        " border-right:3px solid #4f6367;"
        " border-bottom:0px;"
        " border-top-left-radius:18px;"
        " border-top-right-radius:18px;"
        " border-bottom-left-radius:0px;"
        " border-bottom-right-radius:0px;"
        "}"
        );
    // 之前 820×620 太宽；缩到 560 × 580 让面板比例更接近原版 round_eval。
    panel->setFixedSize(560, 580);
    mPanel = panel;

    auto *vbl = new QVBoxLayout(panel);
    vbl->setContentsMargins(20, 18, 20, 18);
    vbl->setSpacing(10);

    // ── 内层更深的圆角框（在 BLACK 基础上再压暗一档；原版视觉上内层比 dyn_container
    //    主体更"凹"一点，由 emboss/阴影叠出来——这里用 #1f2729 直接还原那种"很黑的 slate"）。
    //    一框装下所有内容：提现按钮、至少得分、分割点、各项奖励。
    auto *innerBox = new QWidget(panel);
    innerBox->setObjectName("reInnerBox");
    innerBox->setAttribute(Qt::WA_StyledBackground, true);
    innerBox->setStyleSheet(
        "QWidget#reInnerBox {"
        " background:#1f2729;"          // 比 BLACK 更暗的 slate
        " border:2px solid #141a1c;"
        " border-radius:14px;"
        "}"
        );
    auto *innerVbl = new QVBoxLayout(innerBox);
    innerVbl->setContentsMargins(18, 16, 18, 18);
    innerVbl->setSpacing(12);

    // Cash-out 按钮（撑满内层框宽度）
    mCashOutBtn = new QPushButton("提现: $0", innerBox);
    mCashOutBtn->setFixedHeight(64);
    QFont btnF = mCNFont; btnF.setPixelSize(30); btnF.setBold(true);
    mCashOutBtn->setFont(btnF);
    mCashOutBtn->setCursor(Qt::PointingHandCursor);
    mCashOutBtn->setStyleSheet(
        "QPushButton {"
        " background:qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #ffc756, stop:0.5 #fda200, stop:1 #d77b00);"
        " color:white; border:3px solid #ffe49a; border-radius: 13px; padding: 4px;"
        "}"
        "QPushButton:hover { background:qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #ffe18a, stop:1 #ffa817); }"
        "QPushButton:pressed { background:#cf7100; }"
        );
    connect(mCashOutBtn, &QPushButton::clicked, this, &RoundEndOverlay::nextClicked);
    innerVbl->addWidget(mCashOutBtn);

    // ── 至少得分行（chip + 文字 + 分数；无背景色 / 无右侧 $$$）──
    {
        auto *row = new QWidget(innerBox);
        row->setStyleSheet("background:transparent;");
        auto *hbl = new QHBoxLayout(row);
        hbl->setContentsMargins(6, 4, 6, 4);
        hbl->setSpacing(12);

        mBlindChip = new AnimatedBlindChip(row);
        mBlindChip->setDisplaySize(64);
        hbl->addWidget(mBlindChip);

        auto *mid = new QWidget(row);
        mid->setStyleSheet("background:transparent;");
        auto *mvbl = new QVBoxLayout(mid);
        mvbl->setContentsMargins(0, 0, 0, 0);
        mvbl->setSpacing(2);
        mvbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        QLabel *small = new QLabel("至少得分", mid);
        QFont sf = mCNFont; sf.setPixelSize(15);
        small->setFont(sf);
        small->setStyleSheet("color:white; background:transparent;");
        mvbl->addWidget(small);

        auto *numRow = new QWidget(mid);
        numRow->setStyleSheet("background:transparent;");
        auto *numBl = new QHBoxLayout(numRow);
        numBl->setContentsMargins(0, 0, 0, 0);
        numBl->setSpacing(6);
        numBl->setAlignment(Qt::AlignLeft);

        QLabel *chipIcon = new QLabel(numRow);
        QPixmap chipsSheet(":/textures/images/chips.png");
        if (!chipsSheet.isNull()) {
            QPixmap pix = chipsSheet.copy(0, 0, 58, 58);
            chipIcon->setPixmap(pix.scaled(28, 28, Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation));
        }
        chipIcon->setFixedSize(28, 28);
        chipIcon->setStyleSheet("background:transparent;");
        numBl->addWidget(chipIcon);

        mTargetLbl = new QLabel("0", numRow);
        QFont nf = mCNFont; nf.setPixelSize(26); nf.setBold(true);
        mTargetLbl->setFont(nf);
        mTargetLbl->setStyleSheet("color:#fe5f55; background:transparent;");
        numBl->addWidget(mTargetLbl);
        mvbl->addWidget(numRow);

        hbl->addWidget(mid, 1);
        // 不放右侧 $$$；保留指针避免空指针访问。
        mBlindRewardSym = new QLabel("", row);
        mBlindRewardSym->hide();
        innerVbl->addWidget(row);
    }

    // ── 分隔点：水平方向上一排离散圆点 ──
    {
        class DotDivider : public QWidget {
        public:
            using QWidget::QWidget;
        protected:
            void paintEvent(QPaintEvent *) override {
                QPainter p(this);
                p.setRenderHint(QPainter::Antialiasing, true);
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(255, 255, 255, 230));
                const int y = height() / 2;
                const int margin = 12;
                const int spacing = 18;
                for (int x = margin; x <= width() - margin; x += spacing) {
                    p.drawEllipse(QPointF(x, y), 2.5, 2.5);
                }
            }
        };
        auto *div = new DotDivider(innerBox);
        div->setFixedHeight(10);
        innerVbl->addWidget(div);
    }

    // ── 明细行（无背景；左数字 + 描述 + 右 $$$）──
    auto makeRow = [&](QLabel **leftNumOut, const QString &numColor,
                       const QString &desc, QLabel **rightSymOut) {
        auto *row = new QWidget(innerBox);
        row->setStyleSheet("background:transparent;");
        auto *hbl = new QHBoxLayout(row);
        hbl->setContentsMargins(14, 2, 14, 2);
        hbl->setSpacing(10);

        *leftNumOut = new QLabel("0", row);
        QFont nf = mCNFont; nf.setPixelSize(26); nf.setBold(true);
        (*leftNumOut)->setFont(nf);
        (*leftNumOut)->setStyleSheet(QString("color:%1; background:transparent;").arg(numColor));
        (*leftNumOut)->setFixedWidth(40);
        (*leftNumOut)->setAlignment(Qt::AlignCenter);
        hbl->addWidget(*leftNumOut);

        auto *descLbl = new QLabel(desc, row);
        QFont df = mCNFont; df.setPixelSize(15);
        descLbl->setFont(df);
        descLbl->setStyleSheet("color:white; background:transparent;");
        descLbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        hbl->addWidget(descLbl, 1);

        *rightSymOut = new QLabel("", row);
        QFont sf = mCNFont; sf.setPixelSize(20); sf.setBold(true);
        (*rightSymOut)->setFont(sf);
        (*rightSymOut)->setStyleSheet("color:#f3b958; background:transparent;");
        (*rightSymOut)->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        hbl->addWidget(*rightSymOut);

        return row;
    };

    // 顺序对齐原版 round_eval：出牌奖励 → 利息 → 额外奖励。
    innerVbl->addWidget(makeRow(&mHandsNumLbl, "#009dff",
                                "剩余出牌次数(每次$1)", &mHandsRewardSym));
    innerVbl->addWidget(makeRow(&mInterestNumLbl, "#f3b958",
                                "每$5获得1利息(最高 5)", &mInterestRewardSym));
    mExtraRow = makeRow(&mExtraNumLbl, "#d8b4ff",
                        "额外回合奖励", &mExtraRewardSym);
    mExtraRow->hide();
    innerVbl->addWidget(mExtraRow);

    // 内框不要拉伸，仅按内容高度撑起；剩余空间留给外层（外层下方加 stretch）。
    vbl->addWidget(innerBox, 0, Qt::AlignTop);
    vbl->addStretch();
}


void RoundEndOverlay::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    relayoutPanel();
}

void RoundEndOverlay::relayoutPanel()
{
    if (!mPanel) return;
    // overlay 现在覆盖 mPlayPage 整块，但面板尺寸固定。把 panel 放在 (avail-W, bottom)：
    // 水平按"可用区域"减去 mRightReserve 后居中，垂直贴底。
    const int availW = qMax(0, width() - mRightReserve);
    int x = (availW - mPanel->width()) / 2;
    if (x < 0) x = 0;
    int y = qMax(0, height() - mPanel->height());
    mPanel->move(x, y);
}

void RoundEndOverlay::showFromBottom(const QRect &finalGeometry)
{
    if (!mPanel) {
        setGeometry(finalGeometry);
        raise();
        show();
        return;
    }
    // overlay 现在只覆盖 panel 自身那块矩形，外部的小丑 / 牌堆区域因此不会被
    // RoundEndOverlay 截走鼠标，可以正常 hover / click。
    const int parentW = finalGeometry.width();
    const int parentH = finalGeometry.height();
    const int availW  = qMax(0, parentW - mRightReserve);
    const int panelW  = mPanel->width();
    const int panelH  = mPanel->height();
    int overlayX = (availW - panelW) / 2;
    if (overlayX < 0) overlayX = 0;
    // 终点 Y：与 parent 底贴齐。
    const int overlayEndY   = qMax(0, parentH - panelH);
    const int overlayStartY = parentH;   // 起点：刚好从下方屏外滑入

    // 让 overlay widget 直接代替 mPanel 进行 slide：geometry 动画整体上滑 / 下滑。
    mPanel->move(0, 0);
    setGeometry(overlayX, overlayStartY, panelW, panelH);
    raise();
    show();

    auto *anim = new QPropertyAnimation(this, "pos", this);
    anim->setDuration(320);
    anim->setStartValue(QPoint(overlayX, overlayStartY));
    anim->setEndValue(QPoint(overlayX, overlayEndY));
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void RoundEndOverlay::hideToBottom(std::function<void()> after)
{
    if (!mPanel || !parentWidget()) {
        hide();
        if (after) after();
        return;
    }
    const QPoint startPos = pos();
    const int parentH = parentWidget()->height();
    const QPoint endPos(startPos.x(), parentH);

    auto *anim = new QPropertyAnimation(this, "pos", this);
    anim->setDuration(260);
    anim->setStartValue(startPos);
    anim->setEndValue(endPos);
    anim->setEasingCurve(QEasingCurve::InCubic);
    connect(anim, &QPropertyAnimation::finished, this, [this, after]() {
        hide();
        if (after) after();
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void RoundEndOverlay::setData(int blindChipRow, double targetScore, int blindReward,
                              int handsLeft, int handBonus, int interest,
                              int extraBonus, int totalPayout)
{
    int total = (totalPayout >= 0) ? totalPayout : (blindReward + handBonus + interest + extraBonus);
    mCashOutBtn->setText(QString("提现: $%1").arg(total));

    if (mBlindChip) mBlindChip->setBlindRow(blindChipRow);

    mTargetLbl->setText(formatLargeNumber(targetScore));
    mBlindRewardSym->setText(QString(blindReward, '$'));

    mHandsNumLbl->setText(QString::number(handsLeft));
    mHandsRewardSym->setText(QString(handBonus, '$'));

    mInterestNumLbl->setText(QString::number(interest));
    mInterestRewardSym->setText(QString(qMax(0, interest), '$'));

    if (mExtraRow && mExtraNumLbl && mExtraRewardSym) {
        if (extraBonus != 0) {
            mExtraRow->show();
            mExtraNumLbl->setText(QString::number(extraBonus));
            const QString sign = extraBonus < 0 ? "-" : "";
            mExtraRewardSym->setText(sign + QString(qAbs(extraBonus), '$'));
        } else {
            mExtraRow->hide();
            mExtraNumLbl->clear();
            mExtraRewardSym->clear();
        }
    }
}
