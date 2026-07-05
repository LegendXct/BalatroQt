#include "deckselectwidget.h"
#include "../card/carditem.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>
#include <cmath>

namespace {
static int cardW(int u) { return int(std::round((2.4 * 35.0 / 41.0) * u)); }
static int cardH(int u) { return int(std::round((2.4 * 47.0 / 41.0) * u)); }

QVector<GameDeckId> selectableDeckOrder()
{
    QVector<GameDeckId> ids = originalGameDeckOrder();
    ids.append(GameDeckId::Queue);
    ids.append(GameDeckId::Stack);
    return ids;
}

QPixmap hueShifted(const QPixmap &src, int dh)
{
    QImage img = src.toImage().convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < img.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            const QColor c = QColor::fromRgba(line[x]);
            int h, s, v;
            c.getHsv(&h, &s, &v);
            if (h < 0) continue;
            QColor shifted;
            shifted.setHsv((h + dh) % 360, s, v, c.alpha());
            line[x] = shifted.rgba();
        }
    }
    return QPixmap::fromImage(img);
}

QColor stakeColour(int stake)
{
    switch (stake) {
    case 1: return QColor("#f0f3f2");
    case 2: return QColor("#ff4a4a");
    case 3: return QColor("#35c66d");
    case 4: return QColor("#191919");
    case 5: return QColor("#3f84f7");
    case 6: return QColor("#8847f4");
    case 7: return QColor("#ff9b2a");
    case 8: return QColor("#ffd34d");
    default: return QColor("#f0f3f2");
    }
}

QPoint stakeSpritePos(int stake)
{
    switch (stake) {
    case 1: return {0, 0};
    case 2: return {1, 0};
    case 3: return {2, 0};
    case 4: return {4, 0};
    case 5: return {3, 0};
    case 6: return {0, 1};
    case 7: return {1, 1};
    case 8: return {2, 1};
    default: return {0, 0};
    }
}

QPixmap stakeChipPixmap(int stake, int size)
{
    static QPixmap sheet(QStringLiteral(":/textures/images/chips.png"));
    if (sheet.isNull()) return QPixmap();
    constexpr int frame = 58;
    const QPoint pos = stakeSpritePos(stake);
    return sheet.copy(pos.x() * frame, pos.y() * frame, frame, frame)
        .scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QString panelStyle(const QString &bg, int radius, const QString &border = QStringLiteral("#172225"))
{
    return QString(
        "background:%1;"
        "border:1px solid %2;"
        "border-radius:%3px;")
        .arg(bg, border)
        .arg(radius);
}

QString buttonStyle(const QString &bg, int radius)
{
    const QColor c(bg);
    return QString(
        "QPushButton {"
        " background:%1;"
        " color:#fffaf0; border:0; border-radius:%2px;"
        " border-bottom:4px solid %3; font-weight:bold; padding:0 10px;"
        "}"
        "QPushButton:hover { background:%4; }"
        "QPushButton:pressed { padding-top:2px; border-bottom:2px solid %3; }"
        "QPushButton:disabled { background:#111719; color:#53676b; }")
        .arg(c.name())
        .arg(radius)
        .arg(c.darker(170).name())
        .arg(c.lighter(115).name());
}

QWidget *makePanel(QWidget *parent, const QString &bg, int radius, const QString &border = QStringLiteral("#dbe7e7"))
{
    auto *panel = new QWidget(parent);
    panel->setAttribute(Qt::WA_StyledBackground, true);
    panel->setStyleSheet(panelStyle(bg, radius, border));
    return panel;
}

QPushButton *makeButton(const QString &text, const QFont &font, QWidget *parent, const QString &bg, int radius)
{
    auto *button = new QPushButton(text, parent);
    button->setFont(font);
    button->setFocusPolicy(Qt::NoFocus);
    button->setCursor(Qt::PointingHandCursor);
    button->setStyleSheet(buttonStyle(bg, radius));
    return button;
}

QWidget *makePip(QWidget *parent, int u)
{
    auto *pip = new QWidget(parent);
    const int s = qMax(5, int(0.080 * u));
    pip->setFixedSize(s, s);
    return pip;
}

QWidget *makeStakeMark(QWidget *parent, int u)
{
    auto *mark = new QWidget(parent);
    mark->setFixedSize(int(0.42 * u), int(0.22 * u));
    return mark;
}
} // namespace

DeckSelectWidget::DeckSelectWidget(const QFont &cnFont, QWidget *parent)
    : QWidget(parent)
{
    const int base = parent ? int(std::round(std::min(parent->width() / 18.5, parent->height() / 10.85))) : 96;
    mUnit = qBound(76, base, 118);
    const int U = mUnit;

    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet("background:rgba(0,0,0,64);");

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setAlignment(Qt::AlignCenter);

    auto *panel = makePanel(this, QStringLiteral("#263436"), int(0.10 * U));
    panel->setFixedSize(int(9.95 * U), int(8.65 * U));
    auto *pv = new QVBoxLayout(panel);
    pv->setContentsMargins(int(0.24 * U), int(0.28 * U), int(0.24 * U), int(0.24 * U));
    pv->setSpacing(int(0.07 * U));

    for (GameDeckId id : selectableDeckOrder()) {
        const QPixmap back = deckBackPixmapFor(id);
        mDeckOptions.append({ id, back });
        // 预览牌堆图较贵（叠 10 张 + 逐张剪影）。以前在这里一次性把所有牌组都建好，
        // 从主菜单进入时会明显卡一下。改为按需构建（deckPreviewFor），并跨次打开静态缓存。
        mDeckPreviewPixmaps.append(QPixmap());
    }

    auto makeArrow = [&](const QString &text, QWidget *parentWidget, int hMul) {
        QFont f = cnFont; f.setPixelSize(int(0.31 * U)); f.setBold(true);
        auto *b = makeButton(text, f, parentWidget, QStringLiteral("#fe5f55"), int(0.10 * U));
        b->setFixedSize(int(0.62 * U), hMul);
        return b;
    };

    auto *mid = makePanel(panel, QStringLiteral("#050607"), int(0.10 * U), QStringLiteral("#11191b"));
    mid->setFixedSize(int(7.28 * U), int(3.30 * U));
    auto *midH = new QHBoxLayout(mid);
    midH->setContentsMargins(int(0.18 * U), int(0.16 * U), int(0.14 * U), int(0.22 * U));
    midH->setSpacing(int(0.09 * U));

    auto *deckArea = new QWidget(mid);
    deckArea->setFixedSize(int(2.18 * U), int(2.90 * U));
    deckArea->setStyleSheet("background:transparent; border:none;");
    auto *deckAreaV = new QVBoxLayout(deckArea);
    deckAreaV->setContentsMargins(0, 0, 0, 0);
    deckAreaV->setAlignment(Qt::AlignCenter);
    mPreviewLabel = new QLabel(deckArea);
    mPreviewLabel->setFixedSize(int(2.10 * U), int(2.86 * U));
    mPreviewLabel->setAlignment(Qt::AlignCenter);
    mPreviewLabel->setStyleSheet("background:transparent; border:none;");
    deckAreaV->addWidget(mPreviewLabel);
    midH->addWidget(deckArea, 0, Qt::AlignCenter);

    auto *info = makePanel(mid, QStringLiteral("#111a1c"), int(0.10 * U), QStringLiteral("#202c2f"));
    info->setFixedSize(int(4.06 * U), int(2.52 * U));
    auto *infoV = new QVBoxLayout(info);
    infoV->setContentsMargins(int(0.11 * U), int(0.10 * U), int(0.11 * U), int(0.16 * U));
    infoV->setSpacing(int(0.05 * U));

    mNameLabel = new QLabel(info);
    QFont nf = cnFont; nf.setPixelSize(int(0.31 * U)); nf.setBold(true);
    mNameLabel->setFont(nf);
    mNameLabel->setAlignment(Qt::AlignCenter);
    mNameLabel->setFixedHeight(int(0.60 * U));
    mNameLabel->setStyleSheet(QStringLiteral("color:white; background:#263436; border:0; border-radius:%1px;")
                                  .arg(int(0.08 * U)));
    infoV->addWidget(mNameLabel);

    mDescLabel = new QLabel(info);
    QFont df = cnFont; df.setPixelSize(int(0.20 * U)); df.setBold(true);
    mDescLabel->setFont(df);
    mDescLabel->setAlignment(Qt::AlignCenter);
    mDescLabel->setWordWrap(true);
    mDescLabel->setStyleSheet(QStringLiteral("color:#142022; background:#e9f1f1; border:0; border-radius:%1px; padding:%2px;")
                                  .arg(int(0.08 * U)).arg(int(0.05 * U)));
    infoV->addWidget(mDescLabel, 1);
    midH->addWidget(info, 0, Qt::AlignCenter);

    auto *stakeColumn = new QWidget(mid);
    stakeColumn->setFixedSize(int(0.50 * U), int(2.52 * U));
    stakeColumn->setStyleSheet("background:transparent; border:none;");
    auto *stakeColumnV = new QVBoxLayout(stakeColumn);
    stakeColumnV->setContentsMargins(0, int(0.04 * U), 0, int(0.02 * U));
    stakeColumnV->setSpacing(int(0.03 * U));
    auto *stakeText = new QLabel(QStringLiteral("赌注"), stakeColumn);
    QFont sf = cnFont; sf.setPixelSize(int(0.16 * U)); sf.setBold(true);
    stakeText->setFont(sf);
    stakeText->setAlignment(Qt::AlignCenter);
    stakeText->setStyleSheet("color:#53676b; background:transparent; border:none;");
    stakeColumnV->addWidget(stakeText, 0, Qt::AlignCenter);
    for (int i = 8; i >= 1; --i) {
        auto *mark = makeStakeMark(stakeColumn, U);
        mDeckStakeRows.append(mark);
        stakeColumnV->addWidget(mark, 0, Qt::AlignCenter);
    }
    midH->addWidget(stakeColumn, 0, Qt::AlignCenter);

    auto *deckCycle = new QWidget(panel);
    deckCycle->setFixedSize(int(8.72 * U), int(3.80 * U));
    deckCycle->setStyleSheet("background:transparent; border:none;");
    auto *deckCycleH = new QHBoxLayout(deckCycle);
    deckCycleH->setContentsMargins(0, 0, 0, 0);
    deckCycleH->setSpacing(int(0.10 * U));
    mPrevButton = makeArrow(QStringLiteral("<"), deckCycle, int(3.05 * U));
    mNextButton = makeArrow(QStringLiteral(">"), deckCycle, int(3.05 * U));

    auto *deckCenter = new QWidget(deckCycle);
    deckCenter->setStyleSheet("background:transparent; border:none;");
    auto *deckCenterV = new QVBoxLayout(deckCenter);
    deckCenterV->setContentsMargins(0, 0, 0, 0);
    deckCenterV->setSpacing(int(0.04 * U));
    deckCenterV->addWidget(mid, 0, Qt::AlignCenter);

    auto *deckPips = new QWidget(deckCenter);
    deckPips->setStyleSheet("background:transparent; border:none;");
    auto *deckPipsH = new QHBoxLayout(deckPips);
    deckPipsH->setContentsMargins(0, 0, 0, 0);
    deckPipsH->setSpacing(int(0.035 * U));
    for (int i = 0; i < mDeckOptions.size(); ++i) {
        auto *pip = makePip(deckPips, U);
        mDeckPips.append(pip);
        deckPipsH->addWidget(pip);
    }
    deckCenterV->addWidget(deckPips, 0, Qt::AlignCenter);
    deckCycleH->addWidget(mPrevButton, 0, Qt::AlignCenter);
    deckCycleH->addWidget(deckCenter, 0, Qt::AlignCenter);
    deckCycleH->addWidget(mNextButton, 0, Qt::AlignCenter);
    pv->addWidget(deckCycle, 0, Qt::AlignCenter);

    auto *stakeMid = makePanel(panel, QStringLiteral("#050607"), int(0.10 * U), QStringLiteral("#11191b"));
    stakeMid->setFixedSize(int(7.30 * U), int(1.70 * U));
    auto *stakeH = new QHBoxLayout(stakeMid);
    stakeH->setContentsMargins(int(0.12 * U), int(0.12 * U), int(0.14 * U), int(0.18 * U));
    stakeH->setSpacing(int(0.10 * U));

    auto *verticalStake = new QLabel(QStringLiteral("赌注"), stakeMid);
    QFont vertFont = cnFont; vertFont.setPixelSize(int(0.18 * U)); vertFont.setBold(true);
    verticalStake->setFont(vertFont);
    verticalStake->setFixedSize(int(0.50 * U), int(1.25 * U));
    verticalStake->setAlignment(Qt::AlignCenter);
    verticalStake->setStyleSheet(QStringLiteral("color:#53676b; background:#111a1c; border:1px solid #202c2f; border-radius:%1px;")
                                     .arg(int(0.05 * U)));
    stakeH->addWidget(verticalStake, 0, Qt::AlignCenter);

    mStakeChipLabel = new QLabel(stakeMid);
    mStakeChipLabel->setFixedSize(int(0.82 * U), int(0.82 * U));
    mStakeChipLabel->setAlignment(Qt::AlignCenter);
    mStakeChipLabel->setStyleSheet("background:transparent; border:none;");
    stakeH->addWidget(mStakeChipLabel, 0, Qt::AlignCenter);

    auto *stakeInfo = new QWidget(stakeMid);
    stakeInfo->setStyleSheet("background:transparent; border:none;");
    auto *stakeInfoV = new QVBoxLayout(stakeInfo);
    stakeInfoV->setContentsMargins(0, 0, 0, 0);
    stakeInfoV->setSpacing(int(0.04 * U));
    mStakeNameLabel = new QLabel(stakeInfo);
    QFont snf = cnFont; snf.setPixelSize(int(0.22 * U)); snf.setBold(true);
    mStakeNameLabel->setFont(snf);
    mStakeNameLabel->setAlignment(Qt::AlignCenter);
    mStakeNameLabel->setStyleSheet("color:white; background:transparent; border:none;");
    stakeInfoV->addWidget(mStakeNameLabel);
    mStakeDescLabel = new QLabel(stakeInfo);
    QFont sdf = cnFont; sdf.setPixelSize(int(0.21 * U)); sdf.setBold(true);
    mStakeDescLabel->setFont(sdf);
    mStakeDescLabel->setAlignment(Qt::AlignCenter);
    mStakeDescLabel->setWordWrap(true);
    mStakeDescLabel->setStyleSheet(QStringLiteral("color:#142022; background:#e9f1f1; border:0; border-radius:%1px; padding:%2px;")
                                      .arg(int(0.08 * U)).arg(int(0.04 * U)));
    stakeInfoV->addWidget(mStakeDescLabel, 1);
    stakeH->addWidget(stakeInfo, 1);

    auto *stakeCycle = new QWidget(panel);
    stakeCycle->setFixedSize(int(8.30 * U), int(2.03 * U));
    stakeCycle->setStyleSheet("background:transparent; border:none;");
    auto *stakeCycleH = new QHBoxLayout(stakeCycle);
    stakeCycleH->setContentsMargins(0, 0, 0, 0);
    stakeCycleH->setSpacing(int(0.10 * U));
    auto *stakePrev = makeArrow(QStringLiteral("<"), stakeCycle, int(1.62 * U));
    auto *stakeNext = makeArrow(QStringLiteral(">"), stakeCycle, int(1.62 * U));

    auto *stakeCenter = new QWidget(stakeCycle);
    stakeCenter->setStyleSheet("background:transparent; border:none;");
    auto *stakeCenterV = new QVBoxLayout(stakeCenter);
    stakeCenterV->setContentsMargins(0, 0, 0, 0);
    stakeCenterV->setSpacing(int(0.04 * U));
    stakeCenterV->addWidget(stakeMid, 0, Qt::AlignCenter);
    auto *stakePips = new QWidget(stakeCenter);
    stakePips->setStyleSheet("background:transparent; border:none;");
    auto *stakePipsH = new QHBoxLayout(stakePips);
    stakePipsH->setContentsMargins(0, 0, 0, 0);
    stakePipsH->setSpacing(int(0.05 * U));
    for (int i = 0; i < 8; ++i) {
        auto *pip = makePip(stakePips, U);
        mStakePips.append(pip);
        stakePipsH->addWidget(pip);
    }
    stakeCenterV->addWidget(stakePips, 0, Qt::AlignCenter);
    stakeCycleH->addWidget(stakePrev, 0, Qt::AlignCenter);
    stakeCycleH->addWidget(stakeCenter, 0, Qt::AlignCenter);
    stakeCycleH->addWidget(stakeNext, 0, Qt::AlignCenter);
    pv->addWidget(stakeCycle, 0, Qt::AlignCenter);

    QFont startFont = cnFont; startFont.setPixelSize(int(0.43 * U)); startFont.setBold(true);
    auto *btnStart = makeButton(QStringLiteral("开始游戏"), startFont, panel, QStringLiteral("#009dff"), int(0.10 * U));
    btnStart->setFixedSize(int(4.85 * U), int(0.88 * U));
    pv->addWidget(btnStart, 0, Qt::AlignCenter);
    pv->addStretch(1);

    QFont bf = cnFont; bf.setPixelSize(int(0.29 * U)); bf.setBold(true);
    auto *btnBack = makeButton(QStringLiteral("返回"), bf, panel, QStringLiteral("#fda200"), int(0.08 * U));
    btnBack->setFixedSize(int(8.70 * U), int(0.62 * U));
    pv->addWidget(btnBack, 0, Qt::AlignCenter);

    connect(mPrevButton, &QPushButton::clicked, this, [this]() { select(mSelectedIndex - 1); });
    connect(mNextButton, &QPushButton::clicked, this, [this]() { select(mSelectedIndex + 1); });
    connect(stakePrev, &QPushButton::clicked, this, [this]() { selectStake(mSelectedStake - 1); });
    connect(stakeNext, &QPushButton::clicked, this, [this]() { selectStake(mSelectedStake + 1); });
    connect(btnBack, &QPushButton::clicked, this, [this]() { emit cancelled(); });
    connect(btnStart, &QPushButton::clicked, this, [this]() { emit startRequested(mSelected, mSelectedStake); });

    outer->addWidget(panel);
    select(0);
    selectStake(1);
}

void DeckSelectWidget::select(int idx)
{
    if (mDeckOptions.isEmpty()) return;
    mSelectedIndex = (idx % mDeckOptions.size() + mDeckOptions.size()) % mDeckOptions.size();
    mSelected = mDeckOptions[mSelectedIndex].first;
    refreshSelection();
}

void DeckSelectWidget::selectStake(int idx)
{
    mSelectedStake = (idx - 1 + 8) % 8 + 1;
    refreshStakeSelection();
}

void DeckSelectWidget::refreshSelection()
{
    const auto deck = createGameDeck(mSelected);
    setUpdatesEnabled(false);
    if (mPreviewLabel)
        mPreviewLabel->setPixmap(deckPreviewFor(mSelectedIndex));
    if (mNameLabel)
        mNameLabel->setText(deck->name());
    if (mDescLabel)
        mDescLabel->setText(deck->description());
    if (mPageLabel)
        mPageLabel->setText(deck->name());
    for (int i = 0; i < mDeckPips.size(); ++i) {
        const bool on = i == mSelectedIndex;
        mDeckPips[i]->setStyleSheet(QStringLiteral("background:%1; border-radius:%2px; border:none;")
                                        .arg(on ? QStringLiteral("#ffffff") : QStringLiteral("#050607"))
                                        .arg(qMax(2, mDeckPips[i]->width() / 2)));
    }
    setUpdatesEnabled(true);
}

void DeckSelectWidget::refreshStakeSelection()
{
    setUpdatesEnabled(false);
    if (mStakeChipLabel)
        mStakeChipLabel->setPixmap(stakeChipPixmap(mSelectedStake, int(0.72 * mUnit)));
    if (mStakeNameLabel)
        mStakeNameLabel->setText(stakeName(mSelectedStake));
    if (mStakeDescLabel)
        mStakeDescLabel->setText(stakeDescription(mSelectedStake));
    for (int i = 0; i < mStakePips.size(); ++i) {
        const bool on = i == mSelectedStake - 1;
        mStakePips[i]->setStyleSheet(QStringLiteral("background:%1; border-radius:%2px; border:none;")
                                         .arg(on ? QStringLiteral("#ffffff") : QStringLiteral("#050607"))
                                         .arg(qMax(2, mStakePips[i]->width() / 2)));
    }
    for (int i = 0; i < mDeckStakeRows.size(); ++i) {
        const int stake = 8 - i;
        const bool selected = stake == mSelectedStake;
        const QColor fill = stake <= mSelectedStake ? stakeColour(stake) : QColor(255, 255, 255, 45);
        mDeckStakeRows[i]->setStyleSheet(QString(
            "background:%1; border:%2px solid %3; border-radius:%4px;")
            .arg(fill.name(QColor::HexArgb))
            .arg(selected ? 1 : 0)
            .arg(selected ? QStringLiteral("#202c2f") : QStringLiteral("transparent"))
            .arg(qMax(3, mDeckStakeRows[i]->height() / 2)));
    }
    setUpdatesEnabled(true);
}

QString DeckSelectWidget::stakeName(int stake)
{
    switch (stake) {
    case 1: return QStringLiteral("白注");
    case 2: return QStringLiteral("红注");
    case 3: return QStringLiteral("绿注");
    case 4: return QStringLiteral("黑注");
    case 5: return QStringLiteral("蓝注");
    case 6: return QStringLiteral("紫注");
    case 7: return QStringLiteral("橙注");
    case 8: return QStringLiteral("金注");
    default: return QStringLiteral("白注");
    }
}

QString DeckSelectWidget::stakeDescription(int stake)
{
    switch (stake) {
    case 1: return QStringLiteral("基础难度");
    case 2: return QStringLiteral("小盲注没有奖励金\n之前所有赌注也都起效");
    case 3: return QStringLiteral("底注提升时过关需求分数的增速更快\n之前所有赌注也都起效");
    case 4: return QStringLiteral("商店可能会出现永恒小丑牌\n（无法卖出或摧毁）\n之前所有赌注也都起效");
    case 5: return QStringLiteral("弃牌次数 -1\n之前所有赌注也都起效");
    case 6: return QStringLiteral("底注提升时过关需求分数的增速更快\n之前所有赌注也都起效");
    case 7: return QStringLiteral("商店可能会出现易腐小丑牌\n（经过 5 回合后被削弱）\n之前所有赌注也都起效");
    case 8: return QStringLiteral("商店可能会出现租用小丑牌\n（售价为 $1，每回合花费 $3）\n之前所有赌注也都起效");
    default: return QStringLiteral("基础难度");
    }
}

QPixmap DeckSelectWidget::deckBackPixmapFor(GameDeckId id)
{
    QPixmap sheet(QStringLiteral(":/textures/images/Enhancers.png"));
    if (sheet.isNull()) return CardItem::cardBackPixmap();
    if (id == GameDeckId::Queue)
        return QPixmap(QStringLiteral(":/textures/images/deck_queue.png"));
    if (id == GameDeckId::Stack)
        return QPixmap(QStringLiteral(":/textures/images/deck_stack.png"));
    const auto deck = createGameDeck(id);
    const QPoint pos = deck->spritePos();
    return sheet.copy(pos.x() * CardItem::SRC_W,
                      pos.y() * CardItem::SRC_H,
                      CardItem::SRC_W,
                      CardItem::SRC_H);
}

QPixmap DeckSelectWidget::deckPreviewFor(int index)
{
    if (index < 0 || index >= mDeckOptions.size()) return QPixmap();
    // 本次会话已建好就直接用。
    if (index < mDeckPreviewPixmaps.size() && !mDeckPreviewPixmaps[index].isNull())
        return mDeckPreviewPixmaps[index];
    // 跨次打开的静态缓存：牌堆预览只与牌组 id 和当前尺寸 mUnit 有关，构建一次后
    // 再次进入牌组选择界面即可秒开。
    static QHash<QString, QPixmap> sStackCache;
    const QString key = QString::number(int(mDeckOptions[index].first))
                      + QLatin1Char('|') + QString::number(mUnit);
    QPixmap pm = sStackCache.value(key);
    if (pm.isNull()) {
        pm = deckStackPixmap(mDeckOptions[index].second);
        sStackCache.insert(key, pm);
    }
    if (index < mDeckPreviewPixmaps.size())
        mDeckPreviewPixmaps[index] = pm;
    return pm;
}

QPixmap DeckSelectWidget::deckStackPixmap(const QPixmap &back) const
{
    const int U = mUnit;
    const QSize target(int(2.10 * U), int(2.86 * U));
    QPixmap out(target);
    out.fill(Qt::transparent);
    if (back.isNull()) return out;

    QPainter p(&out);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.setRenderHint(QPainter::Antialiasing, false);

    const int stepX = qMax(1, int(0.018 * U));
    const int stepY = qMax(1, int(0.010 * U));
    const int maxLeftShift = 9 * stepX;
    const int maxUpShift = 9 * stepY;
    const QPoint shadowOffset(int(0.035 * U), int(0.045 * U));
    const QSize cardBounds(qMax(1, qMin(cardW(U), target.width() - maxLeftShift - shadowOffset.x())),
                           qMax(1, qMin(cardH(U), target.height() - maxUpShift - shadowOffset.y())));
    const QPixmap card = back.scaled(cardBounds, Qt::KeepAspectRatio, Qt::FastTransformation);
    const QPoint base(maxLeftShift + qMax(0, (target.width() - maxLeftShift - shadowOffset.x() - card.width()) / 2),
                      maxUpShift + qMax(0, (target.height() - maxUpShift - shadowOffset.y() - card.height()) / 2));

    for (int i = 9; i >= 0; --i) {
        const QPoint offset(-i * stepX, -i * stepY);
        QPixmap shadow(card.size());
        shadow.fill(Qt::transparent);
        {
            QPainter sp(&shadow);
            sp.setCompositionMode(QPainter::CompositionMode_Source);
            sp.drawPixmap(0, 0, card);
            sp.setCompositionMode(QPainter::CompositionMode_SourceIn);
            sp.fillRect(shadow.rect(), QColor(0, 0, 0, 105));
        }
        p.drawPixmap(base + offset + shadowOffset, shadow);
        p.drawPixmap(base + offset, card);
    }

    QFont stickerFont;
    stickerFont.setBold(true);
    stickerFont.setPixelSize(int(0.18 * U));
    p.setFont(stickerFont);
    const QRect sticker(base.x() + card.width() - int(0.31 * U),
                        base.y() + int(0.08 * U),
                        int(0.25 * U), int(0.25 * U));
    p.setBrush(QColor("#ffd447"));
    p.setPen(QPen(QColor("#6b4a00"), qMax(1, int(0.018 * U))));
    p.drawEllipse(sticker);
    p.setPen(QColor("#6b4a00"));
    p.drawText(sticker, Qt::AlignCenter, QStringLiteral("✓"));
    return out;
}
