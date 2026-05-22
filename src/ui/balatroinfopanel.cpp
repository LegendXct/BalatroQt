#include "balatroinfopanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

BalatroInfoPanel::BalatroInfoPanel(const QFont &cnFont, QWidget *parent)
    : QWidget(parent), mCNFont(cnFont)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName("BalatroInfoPanel");
    // 外层：lighten(G.C.JOKER_GREY=#bfc7d5, 0.5) = #dfe3ea。圆角对齐原版 r=0.12。
    // 描边一点深色（原版 emboss 偏暗的轮廓感）。
    setStyleSheet(
        "QWidget#BalatroInfoPanel {"
        "  background: #dfe3ea;"
        "  border: 2px solid #aab2bb;"
        "  border-radius: 14px;"
        "}"
    );
    // 之前在面板上挂 QGraphicsDropShadowEffect（blur=18），Qt 在每次 paint 都要重渲染整张
    // 阴影位图，hover 跨多张牌时（小丑/手牌）会明显卡顿。外层 #aab2bb 描边 + 内层暗底
    // 已经提供了足够的层次感，去掉 effect 改善流畅度。

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(7, 7, 7, 7);
    outer->setSpacing(0);

    mInner = new QWidget(this);
    mInner->setObjectName("BalatroInfoPanelInner");
    mInner->setAttribute(Qt::WA_StyledBackground, true);
    // 内层：原版 adjust_alpha(darken(BLACK=#374244, 0.1), 0.8) ≈ rgba(50, 59, 61, 204)。
    // 这里不能用透明色（QSS 渲染到不透明的浅外层上不好看），改成不带透明的同色实底 #323b3d。
    mInner->setStyleSheet(
        "QWidget#BalatroInfoPanelInner {"
        "  background: #323b3d;"
        "  border-radius: 10px;"
        "}"
    );
    auto *inner = new QVBoxLayout(mInner);
    // 上下边距收紧一些——原版整个 inner padding ≈ 0.07 lua 单位 ≈ 5px；
    // spacing 控制 name/desc 之间的间隙。
    inner->setContentsMargins(6, 5, 6, 5);
    inner->setSpacing(4);

    // ── 名字盒：默认透明，setContent(playing_card=true) 时换成白色圆角盒
    //     对齐原版 name_from_rows(name_nodes, G.C.WHITE/nil)。
    mNameBox = new QWidget(mInner);
    mNameBox->setObjectName("BalatroInfoPanelNameBox");
    mNameBox->setAttribute(Qt::WA_StyledBackground, true);
    auto *nameLay = new QVBoxLayout(mNameBox);
    nameLay->setContentsMargins(4, 2, 4, 2);
    nameLay->setSpacing(0);
    mNameLbl = new QLabel(mNameBox);
    // 名字字号往上调一档（对齐原版 generate_card_ui 里 0.36 scale 的视觉，约 22-23 px）。
    QFont nf = mCNFont; nf.setPixelSize(22); nf.setBold(true);
    mNameLbl->setFont(nf);
    mNameLbl->setAlignment(Qt::AlignCenter);
    mNameLbl->setWordWrap(true);
    mNameLbl->setStyleSheet("background:transparent; border:none;");
    nameLay->addWidget(mNameLbl);
    inner->addWidget(mNameBox);

    // ── 描述盒：白色圆角，emboss 0.05 ≈ 微浮雕——所有类型都有。
    //     原版 desc_from_rows()：colour = G.C.UI.BACKGROUND_WHITE, r=0.1, padding=0.04, minw=2, minh=0.8。
    mBodyBox = new QWidget(mInner);
    mBodyBox->setObjectName("BalatroInfoPanelBodyBox");
    mBodyBox->setAttribute(Qt::WA_StyledBackground, true);
    mBodyBox->setStyleSheet(
        "QWidget#BalatroInfoPanelBodyBox {"
        "  background: #ffffff;"
        "  border-radius: 6px;"
        "  border: 1px solid #b9c2c8;"
        "  border-bottom: 2px solid #b9c2c8;"
        "}"
    );
    auto *bodyLay = new QVBoxLayout(mBodyBox);
    // 原版 padding 0.04 lua 单位 ≈ 2-3 px；这里上下 padding 给到 6 让"+10 筹码"竖向不挤
    // ——同时也匹配用户反馈"下面圆角矩形可以稍微高一点"。
    bodyLay->setContentsMargins(6, 6, 6, 6);
    bodyLay->setSpacing(2);
    mBodyLbl = new QLabel(mBodyBox);
    // 之前 14 太小，对齐原版描述文字大致是 0.32 lua 单位 ≈ 18-19 px；这里取 17 兼顾中文清晰度。
    QFont bf = mCNFont; bf.setPixelSize(17); bf.setBold(true);
    mBodyLbl->setFont(bf);
    mBodyLbl->setAlignment(Qt::AlignCenter);
    mBodyLbl->setWordWrap(true);
    // 白底上默认暗色文字，HTML 内联色（chips=蓝/mult=红/...）覆盖。
    mBodyLbl->setStyleSheet("background:transparent; border:none; color:#4f6367;");
    bodyLay->addWidget(mBodyLbl);
    // 原版 minh = 0.8 lua 单位 ≈ 48 px——给描述盒一个最小高度，让单行描述也不会太扁。
    mBodyBox->setMinimumHeight(40);
    inner->addWidget(mBodyBox);

    mBadgesRow = new QWidget(mInner);
    mBadgesRow->setAttribute(Qt::WA_StyledBackground, true);
    mBadgesRow->setStyleSheet("background: transparent;");
    mBadgesLayout = new QHBoxLayout(mBadgesRow);
    mBadgesLayout->setContentsMargins(0, 2, 0, 0);
    mBadgesLayout->setSpacing(5);
    mBadgesLayout->setAlignment(Qt::AlignCenter);
    inner->addWidget(mBadgesRow);

    outer->addWidget(mInner);
}

void BalatroInfoPanel::setContent(const QString &name, const QString &body,
                                  const QVector<Badge> &badges, int preferredWidth,
                                  bool nameHasWhiteBox)
{
    mNameLbl->setText(name);
    mNameBox->setVisible(!name.isEmpty());
    if (nameHasWhiteBox) {
        // 仅 playing card 走这条：白色圆角盒 + 暗色文字，与下方描述盒同色。
        mNameBox->setStyleSheet(
            "QWidget#BalatroInfoPanelNameBox {"
            "  background: #ffffff;"
            "  border-radius: 6px;"
            "  border: 1px solid #b9c2c8;"
            "  border-bottom: 2px solid #b9c2c8;"
            "}"
        );
        // 名字默认色：playing card 用白底所以默认是暗色；内嵌的 <span> 颜色会覆盖。
        mNameLbl->setStyleSheet("background:transparent; border:none; color:#4f6367;");
    } else {
        mNameBox->setStyleSheet("QWidget#BalatroInfoPanelNameBox { background: transparent; border: none; }");
        // 暗底上的名字直接是白字；HTML 内联色（花色色）会覆盖。
        mNameLbl->setStyleSheet("background:transparent; border:none; color:#ffffff;");
    }
    mBodyLbl->setText(body);
    mBodyBox->setVisible(!body.isEmpty());

    // 清理旧 pill。
    while (auto *item = mBadgesLayout->takeAt(0)) {
        if (auto *w = item->widget()) {
            w->setParent(nullptr);
            w->deleteLater();
        }
        delete item;
    }
    for (const Badge &b : badges) {
        auto *pill = new QLabel(b.text, mBadgesRow);
        pill->setAttribute(Qt::WA_StyledBackground, true);
        pill->setAlignment(Qt::AlignCenter);
        QFont pf = mCNFont; pf.setPixelSize(12); pf.setBold(true);
        pill->setFont(pf);
        // 原版 create_badge：彩色圆角块 + 白字，emboss 加一道深一档的下沿色——这里用上下渐变 + 描边模拟。
        const QString bgHex = b.bg.name();
        const QColor darker = b.bg.darker(135);
        pill->setStyleSheet(QString(
            "QLabel { background: %1; color: %2;"
            "  border: 1px solid %3;"
            "  border-bottom: 2px solid %3;"
            "  border-radius: 7px; padding: 2px 10px; }"
        ).arg(bgHex, b.fg.name(), darker.name()));
        mBadgesLayout->addWidget(pill);
    }
    mBadgesRow->setVisible(!badges.isEmpty());

    setFixedWidth(qMax(160, preferredWidth));
    adjustSize();
}

void BalatroInfoPanel::setBodyMinHeight(int h) {
    if (mBodyLbl) mBodyLbl->setMinimumHeight(h);
}

QColor BalatroInfoPanel::tarotPillColor()       { return QColor("#a782d1"); }
QColor BalatroInfoPanel::planetPillColor()      { return QColor("#13afce"); }
QColor BalatroInfoPanel::spectralPillColor()    { return QColor("#4584fa"); }
QColor BalatroInfoPanel::jokerCommonColor()     { return QColor("#009dff"); }
QColor BalatroInfoPanel::jokerUncommonColor()   { return QColor("#4BC292"); }
QColor BalatroInfoPanel::jokerRareColor()       { return QColor("#fe5f55"); }
QColor BalatroInfoPanel::jokerLegendaryColor()  { return QColor("#b26cbb"); }
QColor BalatroInfoPanel::voucherPillColor()     { return QColor("#fd682b"); }
QColor BalatroInfoPanel::editionPillColor()     { return QColor("#000000"); }
QColor BalatroInfoPanel::sealPillColor(int kind) {
    switch (kind) {
    case 0: return QColor("#eac058");  // gold
    case 1: return QColor("#fe5f55");  // red
    case 2: return QColor("#009dff");  // blue
    case 3: return QColor("#8867a5");  // purple
    }
    return QColor("#888888");
}
