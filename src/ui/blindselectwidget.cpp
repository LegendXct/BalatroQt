#include "blindselectwidget.h"
#include "animatedblindchip.h"
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
#include <QEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QCursor>
#include <cmath>


static double blindUiScale()
{
    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (!screen) return 1.0;

    // 只使用逻辑像素 —— 与 mainwindow.cpp 的 calcUiScale 一致，避免高 DPR 屏把
    // 盲注卡撑大溢出。
    const QSize logical = screen->availableGeometry().size();
    double scale = qMin(logical.width() / 1920.0, logical.height() / 1080.0);

    bool ok = false;
    const double overrideScale = QString::fromLocal8Bit(qgetenv("QT_BALATRO_UI_SCALE")).toDouble(&ok);
    if (ok && overrideScale > 0.1) scale = overrideScale;

    return qBound(0.58, scale, 2.35);
}

static int dp(int px)
{
    return qMax(1, int(std::round(px * blindUiScale())));
}

static int fontPx(int px)
{
    return qMax(1, int(std::round(px * 1.18 * blindUiScale())));
}

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
    case BossEffect::TheHook:    return "#a84024";
    case BossEffect::TheClub:    return "#b9cb92";
    case BossEffect::TheGoad:    return "#b95f92";
    case BossEffect::TheHead:    return "#5f92b9";
    case BossEffect::TheWindow:  return "#d6b04a";
    case BossEffect::TheWall:    return "#8a59a5";
    case BossEffect::ThePlant:   return "#709284";
    case BossEffect::TheNeedle:  return "#5c6e31";
    case BossEffect::TheWater:   return "#347c94";
    case BossEffect::TheManacle: return "#6b5868";
    case BossEffect::ThePsychic: return "#b06aa8";
    case BossEffect::TheFlint:   return "#af5f3f";
    case BossEffect::TheArm:     return "#8b6fb3";
    case BossEffect::TheMouth:   return "#9a3636";
    case BossEffect::TheEye:     return "#2d8a73";
    default:                     return "#a84024";   // 钩子作 fallback
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
    setStyleSheet("background: transparent;");
    buildUi();

    mTagPopup = new QLabel(this);
    mTagPopup->setAttribute(Qt::WA_StyledBackground, true);
    mTagPopup->setWordWrap(true);
    mTagPopup->setAlignment(Qt::AlignCenter);
    QFont tf = mCNFont; tf.setPixelSize(fontPx(15)); tf.setBold(true);
    mTagPopup->setFont(tf);
    mTagPopup->setStyleSheet(
        "color:white; background:rgba(31,37,42,235);"
        "border:2px solid #fda200; border-radius:12px; padding:10px;"
    );
    mTagPopup->hide();
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
        b.card->setFixedSize(dp(CARD_W), dp(CARD_H));
        b.card->setAttribute(Qt::WA_StyledBackground, true);
        b.card->setStyleSheet(QString(
                                  "QWidget#%1 {"
                                  "  background: #374244;"
                                  "  border: 3px solid #4f6367;"
                                  "  border-radius: 14px;"
                                  "}"
                                  ).arg(b.card->objectName()));

        auto *vbl = new QVBoxLayout(b.card);
        vbl->setContentsMargins(dp(14), dp(16), dp(14), 0);   // ← 周围留 14px 给 upperBox 不贴卡片边
        vbl->setSpacing(dp(10));

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
        uvbl->setContentsMargins(dp(12), dp(12), dp(12), dp(12));
        uvbl->setSpacing(dp(8));
        uvbl->setAlignment(Qt::AlignHCenter);

        // actionBtn 现在放在 upperBox 顶部
        b.actionBtn = new QPushButton("", b.upperBox);
        b.actionBtn->setFixedSize(dp(200), dp(46));
        QFont aabf = mCNFont; aabf.setPixelSize(fontPx(20)); aabf.setBold(true);
        b.actionBtn->setFont(aabf);
        b.actionBtn->setCursor(Qt::PointingHandCursor);
        connect(b.actionBtn, &QPushButton::clicked, this, [this, i]() {
            if (mGS->blindState(i) == BlindState::Current)
                emit selectClicked();
        });
        uvbl->addWidget(b.actionBtn, 0, Qt::AlignHCenter);

        // 名字横幅(parent 改成 upperBox)
        b.banner = new QLabel(labels[i], b.upperBox);
        b.banner->setFixedSize(dp(220), dp(42));
        QFont bf = mCNFont; bf.setPixelSize(fontPx(20)); bf.setBold(true);
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

        // 芯片图：稍微缩小给整体卡片留出底部空间，保持上下排版不超出 CARD_H。
        b.chipImg = new AnimatedBlindChip(b.upperBox);
        b.chipImg->setDisplaySize(dp(82));
        uvbl->addWidget(b.chipImg, 0, Qt::AlignHCenter);

        // Boss 描述
        b.bossDescLbl = new QLabel("", b.upperBox);
        QFont df = mCNFont; df.setPixelSize(fontPx(15));
        b.bossDescLbl->setFont(df);
        b.bossDescLbl->setAlignment(Qt::AlignCenter);
        b.bossDescLbl->setStyleSheet("color:#ffffff; background:transparent; border:none;");
        b.bossDescLbl->setWordWrap(true);
        b.bossDescLbl->setFixedHeight(dp(34));
        uvbl->addWidget(b.bossDescLbl);

        // ===== scoreFrame(更暗透明圆角) =====
        QWidget *scoreFrame = new QWidget(b.upperBox);
        scoreFrame->setAttribute(Qt::WA_StyledBackground, true);
        scoreFrame->setStyleSheet(
            "background: #374244;"
            "border-radius: 8px;"
            );
        auto *sfvbl = new QVBoxLayout(scoreFrame);
        sfvbl->setContentsMargins(dp(8), dp(6), dp(8), dp(6));
        sfvbl->setSpacing(dp(2));
        sfvbl->setAlignment(Qt::AlignHCenter);

        QLabel *targetTitle = new QLabel("至少得分", scoreFrame);
        QFont tt2f = mCNFont; tt2f.setPixelSize(fontPx(14));
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
                chipIcon->setPixmap(pix.scaled(dp(36), dp(36), Qt::KeepAspectRatio,
                                               Qt::SmoothTransformation));
            }
        }
        chipIcon->setFixedSize(dp(36), dp(36));
        chipIcon->setStyleSheet("background:transparent; border:none;");
        trbl->addWidget(chipIcon);

        b.targetLbl = new QLabel("300", targetRow);
        QFont t2f = mCNFont; t2f.setPixelSize(fontPx(38)); t2f.setBold(true);
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
        rrbl->setSpacing(dp(4));
        rrbl->setAlignment(Qt::AlignCenter);

        b.rewardTextLbl = new QLabel("奖励:", rewardRow);
        QFont rtf = mCNFont; rtf.setPixelSize(fontPx(16));
        b.rewardTextLbl->setFont(rtf);
        b.rewardTextLbl->setStyleSheet("color:white; background:transparent; border:none;");
        rrbl->addWidget(b.rewardTextLbl);

        b.rewardSymLbl = new QLabel("$$$+", rewardRow);
        QFont rsf = mCNFont; rsf.setPixelSize(fontPx(20));
        b.rewardSymLbl->setFont(rsf);
        b.rewardSymLbl->setStyleSheet("color:#eac058; background:transparent; border:none;");
        rrbl->addWidget(b.rewardSymLbl);

        sfvbl->addWidget(rewardRow);

        uvbl->addWidget(scoreFrame);
        vbl->addWidget(b.upperBox);

        // ===== "或" =====
        b.orLbl = new QLabel("或", b.card);
        QFont of = mCNFont; of.setPixelSize(fontPx(20)); of.setBold(true);
        b.orLbl->setFont(of);
        b.orLbl->setAlignment(Qt::AlignCenter);
        b.orLbl->setStyleSheet("color:white; background:transparent; border:none;");
        vbl->addWidget(b.orLbl, 0, Qt::AlignHCenter);

        // ===== Boss 提示框(i==2 显示,其他卡 hide) =====
        b.bossPromptBox = new QWidget(b.card);
        b.bossPromptBox->setStyleSheet("background: transparent; border: none;");
        b.bossPromptBox->setMinimumWidth(dp(CARD_W - 40));   // ← 撑宽

        auto *bpvbl = new QVBoxLayout(b.bossPromptBox);
        bpvbl->setContentsMargins(dp(20), dp(10), dp(20), dp(10));       // ← 左右更大边距
        // ... 后面不变 ...
        bpvbl->setSpacing(dp(2));
        bpvbl->setAlignment(Qt::AlignCenter);

        QLabel *p1 = new QLabel("提高底注", b.bossPromptBox);
        QFont p1f = mCNFont; p1f.setPixelSize(fontPx(17)); p1f.setBold(true);
        p1->setFont(p1f);
        p1->setAlignment(Qt::AlignCenter);
        p1->setStyleSheet("color:#eab93c; background:transparent; border:none;");
        //                       ↑ 之前 #f0c040
        bpvbl->addWidget(p1);

        QFont p2f = mCNFont; p2f.setPixelSize(fontPx(13));

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

        b.bossRerollBtn = new QPushButton("重掷 Boss $10", b.bossPromptBox);
        QFont rbf = mCNFont; rbf.setPixelSize(fontPx(13)); rbf.setBold(true);
        b.bossRerollBtn->setFont(rbf);
        b.bossRerollBtn->setCursor(Qt::PointingHandCursor);
        b.bossRerollBtn->setFixedHeight(dp(32));
        b.bossRerollBtn->setStyleSheet(
            "QPushButton { background:#8a4fd3; color:white; border:none; border-radius:8px; padding:4px 8px; }"
            "QPushButton:hover { background:#9f63ee; }"
            "QPushButton:disabled { background:#333; color:#777; }"
        );
        connect(b.bossRerollBtn, &QPushButton::clicked, this, [this]() {
            if (mGS && mGS->rerollBoss()) refresh();
        });
        bpvbl->addWidget(b.bossRerollBtn);

        vbl->addWidget(b.bossPromptBox, 0, Qt::AlignHCenter);

        // ===== skipBox(透明灰圆角,横排:tag图标 + 按钮) =====
        b.skipBox = new QWidget(b.card);
        b.skipBox->setStyleSheet("background: transparent; border: none;");
        auto *shbl = new QHBoxLayout(b.skipBox);
        shbl->setContentsMargins(dp(8), dp(8), dp(8), dp(8));
        shbl->setSpacing(dp(8));
        shbl->setAlignment(Qt::AlignCenter);

        // 跳过奖励 tag 图标。原版每个 Small/Big blind 都会预先随机一个 Tag。
        b.tagIcon = new QLabel(b.skipBox);
        b.tagIcon->setFixedSize(dp(48), dp(48));
        b.tagIcon->setStyleSheet("background:transparent; border:none;");
        b.tagIcon->setProperty("blindTagIdx", i);
        b.tagIcon->installEventFilter(this);
        shbl->addWidget(b.tagIcon);

        b.tagName = new QLabel("", b.skipBox);
        b.tagName->hide(); // 名称不占用按钮行空间，改为悬停浮窗显示。

        b.skipBtn = new QPushButton("跳过", b.skipBox);
        b.skipBtn->setFixedSize(dp(108), dp(50));
        b.skipBtn->setProperty("blindTagIdx", i);
        b.skipBtn->installEventFilter(this);
        QFont skf = mCNFont; skf.setPixelSize(fontPx(16)); skf.setBold(true);
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

bool BlindSelectWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::Enter) {
        QWidget *w = qobject_cast<QWidget*>(obj);
        if (w && w->property("blindTagIdx").isValid()) {
            showTagPopup(w->property("blindTagIdx").toInt(), w);
            return false;
        }
    } else if (event->type() == QEvent::Leave) {
        QWidget *w = qobject_cast<QWidget*>(obj);
        if (w && w->property("blindTagIdx").isValid()) {
            hideTagPopup();
            return false;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void BlindSelectWidget::showTagPopup(int idx, QWidget *anchor)
{
    if (!mTagPopup || idx < 0 || idx > 1) return;
    TagData td = tagData(mGS->blindTag(idx));
    mTagPopup->setText(QString("%1\n%2").arg(td.name, td.description));
    mTagPopup->setFixedWidth(dp(260));
    mTagPopup->adjustSize();

    QPoint globalAnchor = anchor->mapTo(this, QPoint(anchor->width() / 2, 0));
    int x = qBound(dp(12), globalAnchor.x() - mTagPopup->width() / 2, width() - mTagPopup->width() - dp(12));
    int y = qMax(dp(12), globalAnchor.y() - mTagPopup->height() - dp(10));
    mTagPopup->move(x, y);
    mTagPopup->raise();
    mTagPopup->show();
}

void BlindSelectWidget::hideTagPopup()
{
    if (mTagPopup) mTagPopup->hide();
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
        row = bossChipRow(mGS->pendingBossEffect());
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
        // BlindChips.png 行号：0=小盲, 1=大盲, 其它=boss 行（bossChipRow）。
        int chipRow = (i == 0) ? 0 : (i == 1 ? 1 : bossChipRow(mGS->pendingBossEffect()));
        if (b.chipImg) {
            b.chipImg->setBlindRow(chipRow);
            b.chipImg->setDisplaySize(dp(82));
        }

        b.targetLbl->setText(QString::number(targetFor(i)));
        QString dollars(rewardFor(i), '$');
        b.rewardSymLbl->setText(dollars + "+");
        b.bossDescLbl->setText(i == 2 ? descs[2] : "");
        b.bossDescLbl->setVisible(i == 2);
        if (b.bossRerollBtn) {
            bool canShow = (i == 2) && (mGS->hasVoucher(VoucherType::DirectorsCut) || mGS->hasVoucher(VoucherType::Retcon));
            b.bossRerollBtn->setVisible(canShow);
            b.bossRerollBtn->setEnabled(canShow && mGS->canRerollBoss());
            b.bossRerollBtn->setText(mGS->hasVoucher(VoucherType::Retcon) ? "重掷 Boss $10" : "重掷 Boss $10");
        }

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

        // 跳过按钮与 Tag 奖励(只跳过 Current 时启用,其他状态禁用)
        if (i < 2) {
            TagData td = tagData(mGS->blindTag(i));
            if (b.tagName) {
                b.tagName->setText(td.name);
                b.tagName->hide();
            }
            if (b.tagIcon) {
                QPixmap tagSheet(":/textures/images/tags.png");
                if (!tagSheet.isNull()) {
                    QPixmap tp = tagSheet.copy(td.spritePos.x() * 68, td.spritePos.y() * 68, 68, 68);
                    b.tagIcon->setPixmap(tp.scaled(dp(48), dp(48), Qt::KeepAspectRatio, Qt::SmoothTransformation));
                }
            }
            b.skipBtn->setEnabled(state == BlindState::Current);
        }
    }
}

QPoint BlindSelectWidget::cardTargetPos(int idx) const
{
    // 三张卡总宽度。当容器够宽时整体居中，确保右侧 Boss 卡不被裁切；
    // 当容器较窄时退化为 LEFT_MARGIN 起步，让最右侧只裁很少。
    const int cardsTotalW = 3 * dp(CARD_W) + 2 * dp(GAP);
    const int sideMargin  = dp(LEFT_MARGIN);
    int startX;
    if (width() >= cardsTotalW + 2 * sideMargin)
        startX = (width() - cardsTotalW) / 2;
    else
        startX = qMax(sideMargin, (width() - cardsTotalW) / 2);
    int x = startX + idx * (dp(CARD_W) + dp(GAP));
    int y = height() - dp(CARD_H) + dp(BOTTOM_OVERFLOW);
    if (mGS->blindState(idx) == BlindState::Current)
        y -= dp(CURRENT_LIFT);
    return QPoint(x, y);
}


void BlindSelectWidget::prepareEntrancePositions()
{
    hideTagPopup();
    for (int i = 0; i < 3; ++i) {
        BlindCard &b = mCards[i];
        if (!b.card) continue;
        const QPoint target = cardTargetPos(i);
        b.card->move(target.x(), height() + 8);
    }
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
