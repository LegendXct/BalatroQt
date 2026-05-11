#include "blindselectwidget.h"
#include "../game/bossblind.h"
#include "../utils/constants.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QColor>
#include <QStyle>

// 把 #rrggbb 字符串解析成 RGB
static QColor hexToColor(const QString &hex) { return QColor(hex); }

// mix(c1, c2, p) = c1*p + c2*(1-p)  跟原版公式一致
static QString mixHex(const QString &c1, const QString &c2, double p)
{
    QColor a = hexToColor(c1), b = hexToColor(c2);
    int r = qBound(0, int(a.red()  *p + b.red()  *(1-p)), 255);
    int g = qBound(0, int(a.green()*p + b.green()*(1-p)), 255);
    int bl= qBound(0, int(a.blue() *p + b.blue() *(1-p)), 255);
    return QString::asprintf("#%02x%02x%02x", r, g, bl);
}

static QString darkenHex(const QString &c, double p)
{
    QColor a = hexToColor(c);
    int r = qBound(0, int(a.red()  *(1-p)), 255);
    int g = qBound(0, int(a.green()*(1-p)), 255);
    int b = qBound(0, int(a.blue() *(1-p)), 255);
    return QString::asprintf("#%02x%02x%02x", r, g, b);
}

// 每个 Boss 的 boss_colour(原版 game.lua 数据)
static QString bossColourRaw(BossEffect e)
{
    switch (e) {
    case BossEffect::TheHook:   return "#a84024";
    case BossEffect::TheClub:   return "#b9cb92";
    case BossEffect::TheWall:   return "#8a59a5";
    case BossEffect::ThePlant:  return "#709284";
    case BossEffect::TheNeedle: return "#5c6e31";
    default:                    return "#a84024";   // 钩子作 fallback
    }
}

// blind_main_colour(等价于原版 get_blind_main_colour)
// idx: 0=Small 1=Big 2=Boss; state: 已击败/已跳过强制变 BLACK
static QString blindMainColour(int idx, BlindState state, BossEffect bossEff)
{
    if (state == BlindState::Defeated || state == BlindState::Skipped)
        return "#374244";   // BLACK
    if (idx == 0) return mixHex("#009dff", "#374244", 0.6);   // 小盲深蓝
    if (idx == 1) return mixHex("#fda200", "#374244", 0.6);   // 大盲深橙
    return bossColourRaw(bossEff);                             // Boss 各色
}

BlindSelectWidget::BlindSelectWidget(GameState *gs, const QFont &cnFont,
                                     const QFont &pixelFont, QWidget *parent)
    : QWidget(parent), mGS(gs), mCNFont(cnFont), mPixelFont(pixelFont)
{
    // 半透明遮罩,可看到下面的牌桌
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet("background: rgba(0, 0, 0, 30);");
    buildUi();
}

void BlindSelectWidget::buildUi()
{
    QString labels[3]       = { "小盲注", "大盲注", "Boss 盲注" };
    QString bannerColors[3] = { "#3d70b8", "#c07820", "#a02020" };

    for (int i = 0; i < 3; ++i) {
        BlindCard &b = mCards[i];

        // ===== 卡片本身完全透明 =====
        b.card = new QWidget(this);
        b.card->setObjectName(QString("blindCard%1").arg(i));
        b.card->setFixedSize(CARD_W, CARD_H);
        b.card->setAttribute(Qt::WA_StyledBackground, true);
        b.card->setStyleSheet(QString(
                                  "QWidget#%1 {"
                                  "  background: #374244;"
                                  "  border: 3px solid #4f6367;"
                                  "  border-radius: 14px;"
                                  "}"
                                  ).arg(b.card->objectName()));

        auto *vbl = new QVBoxLayout(b.card);
        vbl->setContentsMargins(14, 16, 14, 0);   // ← 周围留 14px 给 upperBox 不贴卡片边
        vbl->setSpacing(10);

        // ===== 上半部分 upperBox(无边框,深一点的灰) =====
        b.upperBox = new QWidget(b.card);
        b.upperBox->setObjectName(QString("upperBox%1").arg(i));
        b.upperBox->setAttribute(Qt::WA_StyledBackground, true);
        b.upperBox->setStyleSheet(QString(
                                      "QWidget#%1 {"
                                      "  background: transparent;"
                                      "  border: 2px solid #4f6367;"
                                      "  border-radius: 12px;"
                                      "}"
                                      ).arg(b.upperBox->objectName()));

        auto *uvbl = new QVBoxLayout(b.upperBox);
        uvbl->setContentsMargins(12, 12, 12, 12);
        uvbl->setSpacing(8);
        uvbl->setAlignment(Qt::AlignHCenter);

        // actionBtn 现在放在 upperBox 顶部
        b.actionBtn = new QPushButton("", b.upperBox);
        b.actionBtn->setFixedSize(170, 44);
        QFont aabf = mCNFont; aabf.setPixelSize(20); aabf.setBold(true);
        b.actionBtn->setFont(aabf);
        b.actionBtn->setCursor(Qt::PointingHandCursor);
        connect(b.actionBtn, &QPushButton::clicked, this, [this, i]() {
            if (mGS->blindState(i) == BlindState::Current)
                emit selectClicked();
        });
        uvbl->addWidget(b.actionBtn, 0, Qt::AlignHCenter);

        // 名字横幅(parent 改成 upperBox)
        b.banner = new QLabel(labels[i], b.upperBox);
        b.banner->setFixedSize(190, 40);
        QFont bf = mCNFont; bf.setPixelSize(20); bf.setBold(true);
        b.banner->setFont(bf);
        b.banner->setAlignment(Qt::AlignCenter);
        b.banner->setObjectName(QString("blindBanner%1").arg(i));   // ← 加
        b.banner->setStyleSheet(
            "QLabel#blindBanner0 {"   // 占位,refresh 会动态写
            "  color:white; background:#374244;"
            "  border: 2px solid #4f6367;"
            "  border-radius: 10px;"
            "}"
            );
        uvbl->addWidget(b.banner, 0, Qt::AlignHCenter);

        // 芯片图
        b.chipImg = new QLabel(b.upperBox);
        b.chipImg->setFixedSize(95, 95);
        b.chipImg->setAlignment(Qt::AlignCenter);
        b.chipImg->setStyleSheet("background:transparent; border:none;");
        uvbl->addWidget(b.chipImg, 0, Qt::AlignHCenter);

        // Boss 描述
        b.bossDescLbl = new QLabel("", b.upperBox);
        QFont df = mCNFont; df.setPixelSize(15);
        b.bossDescLbl->setFont(df);
        b.bossDescLbl->setAlignment(Qt::AlignCenter);
        b.bossDescLbl->setStyleSheet("color:#ffffff; background:transparent; border:none;");
        b.bossDescLbl->setWordWrap(true);
        b.bossDescLbl->setFixedHeight(34);
        uvbl->addWidget(b.bossDescLbl);

        // ===== scoreFrame(更暗透明圆角) =====
        QWidget *scoreFrame = new QWidget(b.upperBox);
        scoreFrame->setAttribute(Qt::WA_StyledBackground, true);
        scoreFrame->setStyleSheet(
            "background: #374244;"
            "border-radius: 8px;"
            );
        auto *sfvbl = new QVBoxLayout(scoreFrame);
        sfvbl->setContentsMargins(8, 6, 8, 6);
        sfvbl->setSpacing(2);
        sfvbl->setAlignment(Qt::AlignHCenter);

        QLabel *targetTitle = new QLabel("至少得分", scoreFrame);
        QFont tt2f = mCNFont; tt2f.setPixelSize(14);
        targetTitle->setFont(tt2f);
        targetTitle->setAlignment(Qt::AlignCenter);
        targetTitle->setStyleSheet("color:white; background:transparent; border:none;");
        sfvbl->addWidget(targetTitle);

        // 数字行(芯片+大数字)
        QWidget *targetRow = new QWidget(scoreFrame);
        targetRow->setStyleSheet("background:transparent; border:none;");
        auto *trbl = new QHBoxLayout(targetRow);
        trbl->setContentsMargins(0, 0, 0, 0);
        trbl->setSpacing(6);
        trbl->setAlignment(Qt::AlignCenter);

        QLabel *chipIcon = new QLabel(targetRow);
        {
            QPixmap chipsSheet(":/textures/images/chips.png");
            if (!chipsSheet.isNull()) {
                QPixmap pix = chipsSheet.copy(0, 0, 58, 58);
                chipIcon->setPixmap(pix.scaled(36, 36, Qt::KeepAspectRatio,
                                               Qt::SmoothTransformation));
            }
        }
        chipIcon->setFixedSize(36, 36);
        chipIcon->setStyleSheet("background:transparent; border:none;");
        trbl->addWidget(chipIcon);

        b.targetLbl = new QLabel("300", targetRow);
        QFont t2f = mCNFont; t2f.setPixelSize(38); t2f.setBold(true);
        b.targetLbl->setFont(t2f);
        b.targetLbl->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        b.targetLbl->setStyleSheet("color:#fe5f55; background:transparent; border:none;");
        trbl->addWidget(b.targetLbl);

        sfvbl->addWidget(targetRow);

        // 奖励行
        QWidget *rewardRow = new QWidget(scoreFrame);
        rewardRow->setStyleSheet("background:transparent; border:none;");
        auto *rrbl = new QHBoxLayout(rewardRow);
        rrbl->setContentsMargins(0, 0, 0, 0);
        rrbl->setSpacing(4);
        rrbl->setAlignment(Qt::AlignCenter);

        b.rewardTextLbl = new QLabel("奖励:", rewardRow);
        QFont rtf = mCNFont; rtf.setPixelSize(16);
        b.rewardTextLbl->setFont(rtf);
        b.rewardTextLbl->setStyleSheet("color:white; background:transparent; border:none;");
        rrbl->addWidget(b.rewardTextLbl);

        b.rewardSymLbl = new QLabel("$$$+", rewardRow);
        QFont rsf = mCNFont; rsf.setPixelSize(20);
        b.rewardSymLbl->setFont(rsf);
        b.rewardSymLbl->setStyleSheet("color:#eac058; background:transparent; border:none;");
        rrbl->addWidget(b.rewardSymLbl);

        sfvbl->addWidget(rewardRow);

        uvbl->addWidget(scoreFrame);
        vbl->addWidget(b.upperBox);

        // ===== "或" =====
        b.orLbl = new QLabel("或", b.card);
        QFont of = mCNFont; of.setPixelSize(20); of.setBold(true);
        b.orLbl->setFont(of);
        b.orLbl->setAlignment(Qt::AlignCenter);
        b.orLbl->setStyleSheet("color:white; background:transparent; border:none;");
        vbl->addWidget(b.orLbl, 0, Qt::AlignHCenter);

        // ===== Boss 提示框(i==2 显示,其他卡 hide) =====
        b.bossPromptBox = new QWidget(b.card);
        b.bossPromptBox->setStyleSheet("background: transparent; border: none;");
        b.bossPromptBox->setMinimumWidth(CARD_W - 40);   // ← 撑宽

        auto *bpvbl = new QVBoxLayout(b.bossPromptBox);
        bpvbl->setContentsMargins(20, 10, 20, 10);       // ← 左右更大边距
        // ... 后面不变 ...
        bpvbl->setSpacing(2);
        bpvbl->setAlignment(Qt::AlignCenter);

        QLabel *p1 = new QLabel("提高底注", b.bossPromptBox);
        QFont p1f = mCNFont; p1f.setPixelSize(17); p1f.setBold(true);
        p1->setFont(p1f);
        p1->setAlignment(Qt::AlignCenter);
        p1->setStyleSheet("color:#eab93c; background:transparent; border:none;");
        //                       ↑ 之前 #f0c040
        bpvbl->addWidget(p1);

        QFont p2f = mCNFont; p2f.setPixelSize(13);

        QLabel *p2 = new QLabel("加注所有盲注", b.bossPromptBox);
        p2->setFont(p2f);
        p2->setAlignment(Qt::AlignCenter);
        p2->setStyleSheet("color:white; background:transparent; border:none;");
        bpvbl->addWidget(p2);

        QLabel *p3 = new QLabel("刷新盲注", b.bossPromptBox);
        p3->setFont(p2f);
        p3->setAlignment(Qt::AlignCenter);
        p3->setStyleSheet("color:white; background:transparent; border:none;");
        bpvbl->addWidget(p3);

        vbl->addWidget(b.bossPromptBox, 0, Qt::AlignHCenter);

        // ===== skipBox(透明灰圆角,横排:tag图标 + 按钮) =====
        b.skipBox = new QWidget(b.card);
        b.skipBox->setStyleSheet("background: transparent; border: none;");
        auto *shbl = new QHBoxLayout(b.skipBox);
        shbl->setContentsMargins(8, 8, 8, 8);
        shbl->setSpacing(8);
        shbl->setAlignment(Qt::AlignCenter);

        // 跳过 tag 图标(从 tags.png 切片,Skip Tag 在第 4 行第 1 列)
        QLabel *skipTagIcon = new QLabel(b.skipBox);
        {
            QPixmap tagSheet(":/textures/images/tags.png");
            if (!tagSheet.isNull()) {
                // tag_skip 坐标 (col=0, row=3),单格 68×68
                QPixmap pix = tagSheet.copy(0, 3 * 68, 68, 68);
                skipTagIcon->setPixmap(pix.scaled(48, 48, Qt::KeepAspectRatio,
                                                  Qt::SmoothTransformation));
            }
        }
        skipTagIcon->setFixedSize(48, 48);
        skipTagIcon->setStyleSheet("background:transparent; border:none;");
        shbl->addWidget(skipTagIcon);

        b.skipBtn = new QPushButton("跳过盲注", b.skipBox);
        b.skipBtn->setFixedSize(120, 48);
        QFont skf = mCNFont; skf.setPixelSize(16); skf.setBold(true);
        b.skipBtn->setFont(skf);
        b.skipBtn->setCursor(Qt::PointingHandCursor);
        b.skipBtn->setStyleSheet(
            "QPushButton { background:#fe5f55; color:white; border:none;"   // ← 之前 #e04050
            "              border-radius: 12px; padding:4px 12px; }"
            "QPushButton:hover    { background:#ff7066; }"                  // ← 之前 #f05060
            "QPushButton:disabled { background:#4f6367; color:#888; }"      // ← 之前 #444
            );
        connect(b.skipBtn, &QPushButton::clicked, this, [this, i]() {
            emit skipClicked(i);
        });
        shbl->addWidget(b.skipBtn);

        vbl->addWidget(b.skipBox, 0, Qt::AlignHCenter);
        vbl->addStretch();

        if (i == 2) {
            b.orLbl->hide();
            b.skipBox->hide();
        } else {
            b.bossPromptBox->hide();   // ← 只 Boss 显示提示框
        }
    }
}

void BlindSelectWidget::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    // 立即把卡片摆到目标位置(没有动画,用于初次出现/窗口尺寸变化)
    for (int i = 0; i < 3; ++i)
        if (mCards[i].card) mCards[i].card->move(cardTargetPos(i));
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

        BlindState state = mGS->blindState(i);

        // 计算盲注主色(已击败/已跳过会强制变黑)
        QString mainCol = blindMainColour(i, state, mGS->pendingBossEffect());
        QString innerCol = "#374244";   // Small/Big 内陷色一律 BLACK

        bool current  = (state == BlindState::Current);

        // ===== 卡片样式:升起态 vs 未升起态 =====
        QString cardStyle;
        QString objName = b.card->objectName();
        if (current) {
            cardStyle = QString(
                            "QWidget#%1 {"
                            "  background: %2;"
                            "  border: 3px solid %3;"
                            "  border-radius: 14px;"
                            "}"
                            ).arg(objName, innerCol, mainCol);
        } else {
            QColor inner(innerCol); inner.setAlpha(240);
            cardStyle = QString(
                            "QWidget#%1 {"
                            "  background: rgba(%2, %3, %4, %5);"
                            "  border: 3px solid %6;"        // ← 用 mainCol(完全不透明)
                            "  border-radius: 14px;"
                            "}"
                            ).arg(objName)
                            .arg(inner.red()).arg(inner.green()).arg(inner.blue()).arg(inner.alpha())
                            .arg(mainCol);
        }
        b.card->setStyleSheet(cardStyle);
        b.card->style()->unpolish(b.card);
        b.card->style()->polish(b.card);
        b.card->update();
        // ===== Banner =====
        b.banner->setStyleSheet(QString(
                                    "color:white; background:%1;"
                                    "border: 2px solid %2;"
                                    "border-radius: 10px;"
                                    ).arg(darkenHex(mainCol, 0.3), mainCol));

        // ===== 芯片图、目标、奖励 =====
        QPixmap pix = chipPixmap(i);
        if (!pix.isNull())
            b.chipImg->setPixmap(pix.scaled(95, 95, Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation));
        else
            b.chipImg->clear();

        b.targetLbl->setText(QString::number(targetFor(i)));
        QString dollars(rewardFor(i), '$');
        b.rewardSymLbl->setText(dollars + "+");
        b.bossDescLbl->setText(i == 2 ? descs[2] : "");
        b.bossDescLbl->setVisible(i == 2);

        // ===== 状态按钮 =====
        QString text, btnColor, hoverColor;
        bool enabled = false;
        switch (state) {
        case BlindState::Current:
            text = "选择";    btnColor = "#fda200"; hoverColor = "#ffb730"; enabled = true; break;
        case BlindState::Defeated:
            text = "已被击败"; btnColor = "#666666"; hoverColor = btnColor; break;
        case BlindState::Skipped:
            text = "已跳过";   btnColor = "#666666"; hoverColor = btnColor; break;
        case BlindState::Upcoming:
            text = "下一回合"; btnColor = "#666666"; hoverColor = btnColor; break;
        }
        b.actionBtn->setText(text);
        b.actionBtn->setEnabled(enabled);
        b.actionBtn->setStyleSheet(QString(
                                       "QPushButton { background:%1; color:white; border:none;"
                                       "              border-radius: 12px; padding:4px; }"
                                       "QPushButton:hover    { background:%2; }"
                                       "QPushButton:disabled { background:%1; color:#bbb; }"
                                       ).arg(btnColor, hoverColor));

        // 跳过按钮(只跳过 Current 时启用,其他状态禁用)
        if (i < 2) {
            b.skipBtn->setEnabled(state == BlindState::Current);
        }
    }
}

QPoint BlindSelectWidget::cardTargetPos(int idx) const
{
    int x = LEFT_MARGIN + idx * (CARD_W + GAP);
    int y = height() - CARD_H + BOTTOM_OVERFLOW;
    if (mGS->blindState(idx) == BlindState::Current)
        y -= CURRENT_LIFT;
    return QPoint(x, y);
}

void BlindSelectWidget::arrangeCards(bool initialFloat)
{
    for (int i = 0; i < 3; ++i) {
        BlindCard &b = mCards[i];
        if (!b.card) continue;

        QPoint target = cardTargetPos(i);
        QPoint start  = initialFloat
                           ? QPoint(target.x(), height())   // 整体浮入:从屏幕底
                           : b.card->pos();                 // 切换高低:从当前位置
        b.card->move(start);

        auto *anim = new QPropertyAnimation(b.card, "pos", this);
        anim->setDuration(initialFloat ? 450 : 280);
        anim->setStartValue(start);
        anim->setEndValue(target);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }
}
