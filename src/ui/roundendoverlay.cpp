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
    setStyleSheet("background: rgba(0, 0, 0, 115);");
    buildUi();
}

void RoundEndOverlay::buildUi()
{
    // 主面板：更接近 Balatro 的深色卡片 + 轻微发光边框。
    auto *panel = new QWidget(this);
    panel->setAttribute(Qt::WA_StyledBackground, true);
    panel->setObjectName("rePanel");
    panel->setStyleSheet(
        "QWidget#rePanel {"
        " background:qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #425055, stop:1 #273338);"
        " border-radius:18px; border: 3px solid #6f8589;"
        "}"
        );
    panel->setFixedSize(580, 492);
    auto *panelGlow = new QGraphicsDropShadowEffect(panel);
    panelGlow->setBlurRadius(34);
    panelGlow->setOffset(0, 0);
    panelGlow->setColor(QColor(0, 0, 0, 170));
    panel->setGraphicsEffect(panelGlow);

    // 居中 panel
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setAlignment(Qt::AlignCenter);
    outer->addWidget(panel);

    auto *vbl = new QVBoxLayout(panel);
    vbl->setContentsMargins(20, 18, 20, 18);
    vbl->setSpacing(12);

    // ── 顶部 cash out 按钮 ──
    mCashOutBtn = new QPushButton("提现: $0", panel);
    mCashOutBtn->setFixedHeight(60);
    QFont btnF = mCNFont; btnF.setPixelSize(28); btnF.setBold(true);
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
    vbl->addWidget(mCashOutBtn);

    // 一个生成"明细行"的 lambda
    auto makeRow = [&](QLabel **leftNumOut, const QString &numColor,
                       const QString &desc, QLabel **rightSymOut) {
        auto *row = new QWidget(panel);
        row->setAttribute(Qt::WA_StyledBackground, true);
        row->setStyleSheet("background:rgba(21,30,33,100); border-radius:9px;");
        auto *hbl = new QHBoxLayout(row);
        hbl->setContentsMargins(10, 2, 10, 2);
        hbl->setSpacing(8);

        // 左侧:大数字
        *leftNumOut = new QLabel("0", row);
        QFont nf = mCNFont; nf.setPixelSize(26); nf.setBold(true);
        (*leftNumOut)->setFont(nf);
        (*leftNumOut)->setStyleSheet(QString("color:%1; background:transparent;").arg(numColor));
        (*leftNumOut)->setFixedWidth(40);
        (*leftNumOut)->setAlignment(Qt::AlignCenter);
        hbl->addWidget(*leftNumOut);

        // 描述
        auto *descLbl = new QLabel(desc, row);
        QFont df = mCNFont; df.setPixelSize(14);
        descLbl->setFont(df);
        descLbl->setStyleSheet("color:white; background:transparent;");
        descLbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        hbl->addWidget(descLbl, 1);

        // 右侧 $$$$$
        *rightSymOut = new QLabel("", row);
        QFont sf = mCNFont; sf.setPixelSize(20); sf.setBold(true);
        (*rightSymOut)->setFont(sf);
        (*rightSymOut)->setStyleSheet("color:#f3b958; background:transparent;");
        (*rightSymOut)->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        hbl->addWidget(*rightSymOut);

        return row;
    };

    // ── 至少得分行(blind 主行) ──
    {
        auto *row = new QWidget(panel);
        auto *hbl = new QHBoxLayout(row);
        hbl->setContentsMargins(8, 0, 8, 0);
        hbl->setSpacing(8);

        // 盲注芯片(setData 时填充)
        mBlindChip = new QLabel(row);
        mBlindChip->setFixedSize(50, 50);
        mBlindChip->setStyleSheet("background:transparent;");
        hbl->addWidget(mBlindChip);

        // 中间:至少得分(竖排:小字+蓝芯片+红数字)
        auto *mid = new QWidget(row);
        auto *mvbl = new QVBoxLayout(mid);
        mvbl->setContentsMargins(0, 0, 0, 0);
        mvbl->setSpacing(2);
        mvbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        QLabel *small = new QLabel("至少得分", mid);
        QFont sf = mCNFont; sf.setPixelSize(13);
        small->setFont(sf);
        small->setStyleSheet("color:white; background:transparent;");
        mvbl->addWidget(small);

        // 蓝芯片 + 红数字
        auto *numRow = new QWidget(mid);
        auto *numBl = new QHBoxLayout(numRow);
        numBl->setContentsMargins(0, 0, 0, 0);
        numBl->setSpacing(4);
        numBl->setAlignment(Qt::AlignLeft);

        QLabel *chipIcon = new QLabel(numRow);
        QPixmap chipsSheet(":/textures/images/chips.png");
        if (!chipsSheet.isNull()) {
            QPixmap pix = chipsSheet.copy(0, 0, 58, 58);
            chipIcon->setPixmap(pix.scaled(20, 20, Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation));
        }
        chipIcon->setFixedSize(20, 20);
        chipIcon->setStyleSheet("background:transparent;");
        numBl->addWidget(chipIcon);

        mTargetLbl = new QLabel("0", numRow);
        QFont nf = mCNFont; nf.setPixelSize(22); nf.setBold(true);
        mTargetLbl->setFont(nf);
        mTargetLbl->setStyleSheet("color:#fe5f55; background:transparent;");
        numBl->addWidget(mTargetLbl);

        mvbl->addWidget(numRow);
        hbl->addWidget(mid, 1);

        // 右侧 $$$
        mBlindRewardSym = new QLabel("", row);
        QFont rf = mCNFont; rf.setPixelSize(20); rf.setBold(true);
        mBlindRewardSym->setFont(rf);
        mBlindRewardSym->setStyleSheet("color:#f3b958; background:transparent;");
        mBlindRewardSym->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        hbl->addWidget(mBlindRewardSym);

        vbl->addWidget(row);
    }

    // ── 虚线分隔 ──
    {
        QLabel *div = new QLabel(QString(48, '.'), panel);
        QFont dvf = mPixelFont; dvf.setPixelSize(14);
        div->setFont(dvf);
        div->setStyleSheet("color:white; background:transparent;");
        div->setAlignment(Qt::AlignCenter);
        vbl->addWidget(div);
    }

    // ── 出牌剩余 ──
    vbl->addWidget(makeRow(&mHandsNumLbl, "#009dff",
                           "剩余出牌次数(每次$1)", &mHandsRewardSym));

    // ── 利息 ──
    vbl->addWidget(makeRow(&mInterestNumLbl, "#f3b958",
                           "每$5获得1利息(最高 5)", &mInterestRewardSym));

    // ── 金牌/投资标签/回合末小丑等额外金币，通常为 0，非 0 时显示。 ──
    mExtraRow = makeRow(&mExtraNumLbl, "#d8b4ff",
                        "额外回合奖励", &mExtraRewardSym);
    mExtraRow->hide();
    vbl->addWidget(mExtraRow);

    vbl->addStretch();
}


void RoundEndOverlay::showFromBottom(const QRect &finalGeometry)
{
    if (!parentWidget()) {
        setGeometry(finalGeometry);
        raise();
        show();
        return;
    }

    QRect start = finalGeometry;
    start.moveTop(finalGeometry.bottom() + 1);
    setGeometry(start);
    raise();
    show();

    auto *anim = new QPropertyAnimation(this, "geometry", this);
    anim->setDuration(320);
    anim->setStartValue(start);
    anim->setEndValue(finalGeometry);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void RoundEndOverlay::hideToBottom(std::function<void()> after)
{
    QRect start = geometry();
    QRect end = start;
    end.moveTop(start.bottom() + 1);

    auto *anim = new QPropertyAnimation(this, "geometry", this);
    anim->setDuration(260);
    anim->setStartValue(start);
    anim->setEndValue(end);
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

    QPixmap sheet(":/textures/images/BlindChips.png");
    if (!sheet.isNull()) {
        QPixmap pix = sheet.copy(0, blindChipRow * 68, 68, 68);
        mBlindChip->setPixmap(pix.scaled(50, 50, Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation));
    }

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
