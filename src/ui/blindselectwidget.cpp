#include "blindselectwidget.h"
#include "../game/bossblind.h"
#include "../utils/constants.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPropertyAnimation>
#include <QResizeEvent>

BlindSelectWidget::BlindSelectWidget(GameState *gs, const QFont &cnFont,
                                     const QFont &pixelFont, QWidget *parent)
    : QWidget(parent), mGS(gs), mCNFont(cnFont), mPixelFont(pixelFont)
{
    // 半透明遮罩,可看到下面的牌桌
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet("background: rgba(0, 0, 0, 60);");
    buildUi();
}

void BlindSelectWidget::buildUi()
{
    mContainer = new QWidget(this);
    mContainer->setStyleSheet("background:transparent;");

    auto *hbl = new QHBoxLayout(mContainer);
    hbl->setContentsMargins(0, 0, 0, 0);
    hbl->setSpacing(GAP);
    hbl->setAlignment(Qt::AlignBottom | Qt::AlignHCenter);

    QString labels[3]       = { "小盲注", "大盲注", "Boss 盲注" };
    QString bannerColors[3] = { "#3d70b8", "#c07820", "#a02020" };
    QString outlineColors[3]= { "#3d70b8", "#c07820", "#a02020" };

    for (int i = 0; i < 3; ++i) {
        BlindCard &b = mCards[i];

        b.card = new QWidget(mContainer);
        b.card->setFixedSize(CARD_W, CARD_H);
        b.card->setAttribute(Qt::WA_StyledBackground, true);
        // 关键:只有顶部圆角,卡片向下"长"到屏幕底
        b.card->setStyleSheet(QString(
                                  "background: #161b28;"
                                  "border: 2px solid %1;"
                                  "border-bottom: none;"
                                  "border-top-left-radius: 14px;"
                                  "border-top-right-radius: 14px;"
                                  ).arg(outlineColors[i]));

        auto *vbl = new QVBoxLayout(b.card);
        vbl->setContentsMargins(14, 12, 14, 0);
        vbl->setSpacing(10);

        // 顶部状态按钮
        b.actionBtn = new QPushButton("", b.card);
        b.actionBtn->setFixedHeight(44);
        QFont aabf = mCNFont; aabf.setPixelSize(18); aabf.setBold(true);
        b.actionBtn->setFont(aabf);
        b.actionBtn->setCursor(Qt::PointingHandCursor);
        connect(b.actionBtn, &QPushButton::clicked, this, [this, i]() {
            if (mGS->blindState(i) == BlindState::Current)
                emit selectClicked();
        });
        vbl->addWidget(b.actionBtn);

        // 名字横幅
        b.banner = new QLabel(labels[i], b.card);
        QFont bf = mCNFont; bf.setPixelSize(20); bf.setBold(true);
        b.banner->setFont(bf);
        b.banner->setAlignment(Qt::AlignCenter);
        b.banner->setStyleSheet(QString(
                                    "color:white; background:%1;"
                                    "border-radius:6px; padding:4px;"
                                    ).arg(bannerColors[i]));
        b.banner->setFixedHeight(36);
        vbl->addWidget(b.banner);

        // 芯片图
        b.chipImg = new QLabel(b.card);
        b.chipImg->setFixedSize(150, 150);
        b.chipImg->setAlignment(Qt::AlignCenter);
        b.chipImg->setStyleSheet("background:transparent; border:none;");
        vbl->addWidget(b.chipImg, 0, Qt::AlignHCenter);

        // Boss 描述(占位,Boss 才显示)
        b.bossDescLbl = new QLabel("", b.card);
        QFont df = mCNFont; df.setPixelSize(13);
        b.bossDescLbl->setFont(df);
        b.bossDescLbl->setAlignment(Qt::AlignCenter);
        b.bossDescLbl->setStyleSheet("color:#ff8888; background:transparent; border:none;");
        b.bossDescLbl->setWordWrap(true);
        b.bossDescLbl->setFixedHeight(34);
        vbl->addWidget(b.bossDescLbl);

        // 至少得分标题
        QLabel *targetTitle = new QLabel("至少得分", b.card);
        QFont tt2f = mCNFont; tt2f.setPixelSize(13);
        targetTitle->setFont(tt2f);
        targetTitle->setAlignment(Qt::AlignCenter);
        targetTitle->setStyleSheet("color:#aaa; background:transparent; border:none;");
        vbl->addWidget(targetTitle);

        // 至少得分数字(大像素字体红色)
        b.targetLbl = new QLabel("✳ 300", b.card);
        QFont t2f = mPixelFont; t2f.setPixelSize(36);
        b.targetLbl->setFont(t2f);
        b.targetLbl->setAlignment(Qt::AlignCenter);
        b.targetLbl->setStyleSheet("color:#e04040; background:transparent; border:none;");
        vbl->addWidget(b.targetLbl);

        // 奖励
        b.rewardLbl = new QLabel("奖励 $$$", b.card);
        QFont rf = mPixelFont; rf.setPixelSize(15);
        b.rewardLbl->setFont(rf);
        b.rewardLbl->setAlignment(Qt::AlignCenter);
        b.rewardLbl->setStyleSheet("color:#f0c040; background:transparent; border:none;");
        vbl->addWidget(b.rewardLbl);

        vbl->addStretch();

        // "或" + 跳过按钮 (Boss 隐藏)
        b.skipBox = new QWidget(b.card);
        b.skipBox->setStyleSheet("background:transparent; border:none;");
        auto *svbl = new QVBoxLayout(b.skipBox);
        svbl->setContentsMargins(0, 0, 0, 16);
        svbl->setSpacing(6);
        svbl->setAlignment(Qt::AlignHCenter);

        QLabel *orLbl = new QLabel("或", b.skipBox);
        QFont of = mCNFont; of.setPixelSize(15);
        orLbl->setFont(of);
        orLbl->setAlignment(Qt::AlignCenter);
        orLbl->setStyleSheet("color:white; background:transparent; border:none;");
        svbl->addWidget(orLbl);

        b.skipBtn = new QPushButton("跳过盲注", b.skipBox);
        b.skipBtn->setFixedSize(140, 36);
        QFont skf = mCNFont; skf.setPixelSize(15); skf.setBold(true);
        b.skipBtn->setFont(skf);
        b.skipBtn->setCursor(Qt::PointingHandCursor);
        b.skipBtn->setStyleSheet(
            "QPushButton { background:#e04050; color:white; border:none;"
            "              border-radius:6px; padding:4px 12px; }"
            "QPushButton:hover    { background:#f05060; }"
            "QPushButton:disabled { background:#444; color:#888; }"
            );
        connect(b.skipBtn, &QPushButton::clicked, this, [this, i]() {
            emit skipClicked(i);
        });
        svbl->addWidget(b.skipBtn, 0, Qt::AlignHCenter);
        vbl->addWidget(b.skipBox, 0, Qt::AlignHCenter);

        if (i == 2) b.skipBox->hide();
        hbl->addWidget(b.card);
    }
}

void BlindSelectWidget::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    if (!mContainer) return;
    int totalW = CARD_W * 3 + GAP * 2;
    int x = (width() - totalW) / 2;
    int y = height() - CARD_H + 40;   // +40 让卡片底部超出屏幕一截
    mContainer->setGeometry(x, y, totalW, CARD_H);
}

void BlindSelectWidget::playEnterAnimation()
{
    if (!mContainer) return;
    QPoint endPos = mContainer->pos();
    QPoint startPos(endPos.x(), height());
    mContainer->move(startPos);
    auto *anim = new QPropertyAnimation(mContainer, "pos", this);
    anim->setDuration(450);
    anim->setStartValue(startPos);
    anim->setEndValue(endPos);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

QPixmap BlindSelectWidget::chipPixmap(int blindIdx) const
{
    QPixmap sheet(":/textures/images/BlindChips.png");
    if (sheet.isNull()) return QPixmap();
    constexpr int FW = 68, FH = 68;
    int row = 0;
    if (blindIdx == 0) row = 0;
    else if (blindIdx == 1) row = 1;
    else {
        switch (mGS->pendingBossEffect()) {
        case BossEffect::TheHook:   row = 7;  break;
        case BossEffect::TheClub:   row = 4;  break;
        case BossEffect::TheWall:   row = 9;  break;
        case BossEffect::ThePlant:  row = 19; break;
        case BossEffect::TheNeedle: row = 20; break;
        default: row = 7; break;
        }
    }
    return sheet.copy(0, row * FH, FW, FH);
}

int BlindSelectWidget::targetFor(int blindIdx) const
{
    static const int base[] = { 0, 300, 800, 2000, 5000, 11000, 20000, 35000, 50000 };
    double m = 1.0;
    switch (blindIdx) {
    case 0: m = Constants::SMALL_BLIND_MULT; break;
    case 1: m = Constants::BIG_BLIND_MULT;   break;
    case 2: m = Constants::BOSS_BLIND_MULT;  break;
    }
    int t = static_cast<int>(base[mGS->ante()] * m);
    if (blindIdx == 2 && mGS->pendingBossEffect() == BossEffect::TheWall) t *= 2;
    return t;
}

int BlindSelectWidget::rewardFor(int blindIdx) const
{
    return blindIdx == 0 ? 3 : blindIdx == 1 ? 4 : 5;
}

void BlindSelectWidget::refresh()
{
    BossInfo bi = bossInfo(mGS->pendingBossEffect());
    QString bossName = bi.name.isEmpty() ? "Boss 盲注" : bi.name;
    QString names[3] = { "小盲注", "大盲注", bossName };
    QString descs[3] = { "", "", bi.description };

    for (int i = 0; i < 3; ++i) {
        BlindCard &b = mCards[i];
        b.banner->setText(names[i]);

        QPixmap pix = chipPixmap(i);
        if (!pix.isNull())
            b.chipImg->setPixmap(pix.scaled(150, 150, Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation));
        else
            b.chipImg->clear();

        b.targetLbl->setText(QString("✳ %1").arg(targetFor(i)));
        b.rewardLbl->setText(QString("奖励 $%1").arg(rewardFor(i)));
        b.bossDescLbl->setText(i == 2 ? descs[2] : "");
        b.bossDescLbl->setVisible(i == 2);

        BlindState state = mGS->blindState(i);
        QString text, btnColor, hoverColor;
        bool enabled = false;
        switch (state) {
        case BlindState::Current:
            text = "选择"; btnColor = "#c07820"; hoverColor = "#d08830"; enabled = true; break;
        case BlindState::Defeated:
            text = "已被击败"; btnColor = "#3a3f4a"; hoverColor = btnColor; break;
        case BlindState::Skipped:
            text = "已跳过"; btnColor = "#3060a0"; hoverColor = btnColor; break;
        case BlindState::Upcoming:
            text = "下一回合"; btnColor = "#3a3f4a"; hoverColor = btnColor; break;
        }
        b.actionBtn->setText(text);
        b.actionBtn->setEnabled(enabled);
        b.actionBtn->setStyleSheet(QString(
                                       "QPushButton { background:%1; color:white; border:none;"
                                       "              border-radius:6px; padding:4px; }"
                                       "QPushButton:hover    { background:%2; }"
                                       "QPushButton:disabled { background:%1; color:#bbb; }"
                                       ).arg(btnColor, hoverColor));

        if (i < 2) {
            bool show = (state == BlindState::Current);
            b.skipBox->setVisible(show);
        }
    }
}
