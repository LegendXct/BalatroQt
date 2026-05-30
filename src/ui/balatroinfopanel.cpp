#include "balatroinfopanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextDocument>

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
    // 之前在构造时强制 setMinimumHeight(40) 作为"单行描述兜底"，会让 A→B 切换时
    // 高度无法收缩到当前内容（用户反馈：B 长描述被裁切）。改为按实测高度走，最小不设。
    inner->addWidget(mBodyBox);

    mBadgesRow = new QWidget(mInner);
    mBadgesRow->setAttribute(Qt::WA_StyledBackground, true);
    mBadgesRow->setStyleSheet("background: transparent;");
    mBadgesLayout = new QHBoxLayout(mBadgesRow);
    mBadgesLayout->setContentsMargins(0, 2, 0, 0);
    mBadgesLayout->setSpacing(5);
    // 不再使用 AlignCenter——badges 在 setContent 时会设为 Expanding，平均瓜分整条宽度
    // （原版底部那条"塔罗牌 / 普通"等胶囊就是横向撑满）。
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
        QFont pf = mCNFont; pf.setPixelSize(13); pf.setBold(true);
        pill->setFont(pf);
        // 横向 Expanding：多个 pill 在 BadgesRow 里平均瓜分宽度——对齐原版底部 "塔罗牌"
        // 这种胶囊撑满整条 info 框的视觉。
        pill->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        pill->setMinimumHeight(22);
        // 原版 create_badge：彩色圆角块 + 白字，emboss 加一道深一档的下沿色——这里用描边模拟。
        const QString bgHex = b.bg.name();
        const QColor darker = b.bg.darker(135);
        pill->setStyleSheet(QString(
            "QLabel { background: %1; color: %2;"
            "  border: 1px solid %3;"
            "  border-bottom: 2px solid %3;"
            "  border-radius: 7px; padding: 2px 8px; }"
        ).arg(bgHex, b.fg.name(), darker.name()));
        // stretch=1 平分；如果将来想做"主 badge 占主体 + 小 badge 收尾"可改成不同 stretch。
        mBadgesLayout->addWidget(pill, 1);
    }
    mBadgesRow->setVisible(!badges.isEmpty());

    // 宽度严格固定，让高度跟随实际内容自适应。
    // Qt 的 QLabel + wordWrap 需要显式给出宽度，heightForWidth 才能算出正确的高度；
    // 不然 adjustSize() 会按"单行不换行"算高度，长描述被截断（用户反馈6）。
    const int w = qMax(110, preferredWidth);
    setFixedWidth(w);
    setMaximumHeight(QWIDGETSIZE_MAX);
    // 关键：第一次显示时高度不对的根因——QLabel 的 wordWrap 必须把 heightForWidth 打开，
    // 父 layout 才会按"按当前宽度算环绕高度"算。光设 minimumWidth + adjustSize 不够。
    auto enableHFW = [](QLabel *lbl) {
        if (!lbl) return;
        QSizePolicy sp = lbl->sizePolicy();
        sp.setHeightForWidth(true);
        lbl->setSizePolicy(sp);
    };
    enableHFW(mBodyLbl);
    enableHFW(mNameLbl);
    const int bodyWidth = qMax(40, w - 38);
    // 用 QTextDocument 真实排版一次拿换行后的高度——QLabel 的 heightForWidth 在
    // 首次 show 前 font metrics 还没就绪，返回值会偏小，导致第一次 hover info 被截。
    auto measureHtmlHeight = [](const QFont &f, const QString &text, int width) -> int {
        if (text.isEmpty()) return 0;
        QTextDocument doc;
        doc.setDefaultFont(f);
        doc.setTextWidth(width);
        if (text.contains(QLatin1Char('<'))) doc.setHtml(text);
        else doc.setPlainText(text);
        return int(doc.size().height() + 0.999);
    };
    // 上次的修复用 setMinimumHeight(0)+heightForWidth 还是会被 QLabel 缓存
    //（wordWrap + Preferred 政策在切到更短描述时不收高）。改成直接 setFixedHeight
    // 用 QTextDocument 实测的值——彻底跳过 Qt 的 heightForWidth 计算。
    if (mBodyLbl) {
        mBodyLbl->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        mBodyLbl->setFixedWidth(bodyWidth);
        int measured = measureHtmlHeight(mBodyLbl->font(), body, bodyWidth);
        if (body.isEmpty()) measured = 0;
        mBodyLbl->setFixedHeight(measured > 0 ? measured + 4 : 0);
    }
    if (mNameLbl) {
        mNameLbl->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        mNameLbl->setFixedWidth(bodyWidth);
        int measured = measureHtmlHeight(mNameLbl->font(), name, bodyWidth);
        if (name.isEmpty()) measured = 0;
        mNameLbl->setFixedHeight(measured > 0 ? measured + 2 : 0);
    }
    // 容器层全部解锁——让 adjustSize 真正自由收缩到当前内容大小。
    setMinimumSize(0, 0);
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    if (mInner)   { mInner->setMinimumSize(0, 0);   mInner->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX); }
    if (mBodyBox) { mBodyBox->setMinimumSize(0, 0); mBodyBox->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX); }
    if (mNameBox) { mNameBox->setMinimumSize(0, 0); mNameBox->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX); }

    ensurePolished();
    if (mInner) mInner->ensurePolished();
    if (mBodyBox) mBodyBox->ensurePolished();
    if (mBodyLbl) mBodyLbl->ensurePolished();
    if (mNameLbl) mNameLbl->ensurePolished();

    // 沿着层次链强制 updateGeometry → 各级 layout 重新算 sizeHint。
    if (mBodyLbl) mBodyLbl->updateGeometry();
    if (mNameLbl) mNameLbl->updateGeometry();
    if (mBodyBox) mBodyBox->updateGeometry();
    if (mNameBox) mNameBox->updateGeometry();
    if (mInner)   mInner->updateGeometry();
    updateGeometry();
    if (mInner && mInner->layout())   mInner->layout()->invalidate();
    if (layout()) layout()->invalidate();

    for (int i = 0; i < 3; ++i) {
        if (mInner && mInner->layout()) mInner->layout()->activate();
        if (layout()) layout()->activate();
        if (mInner) mInner->adjustSize();
        adjustSize();
    }
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

// ── BalatroInfoCluster ────────────────────────────────────────────────────
BalatroInfoCluster::BalatroInfoCluster(const QFont &cnFont, QWidget *parent)
    : QWidget(parent), mCNFont(cnFont)
{
    setAttribute(Qt::WA_StyledBackground, false);
    mRowLayout = new QHBoxLayout(this);
    mRowLayout->setContentsMargins(0, 0, 0, 0);
    mRowLayout->setSpacing(8);
    mRowLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    mMain = new BalatroInfoPanel(mCNFont, this);
    mRowLayout->addWidget(mMain, 0, Qt::AlignTop);
}

void BalatroInfoCluster::clear()
{
    for (auto *p : mSidePanels) {
        mRowLayout->removeWidget(p);
        p->deleteLater();
    }
    mSidePanels.clear();
}

void BalatroInfoCluster::setMainContent(const QString &name, const QString &body,
                                        const QVector<BalatroInfoPanel::Badge> &badges,
                                        int preferredWidth, bool nameHasWhiteBox)
{
    mMain->setContent(name, body, badges, preferredWidth, nameHasWhiteBox);
}

void BalatroInfoCluster::addSidePanel(const BalatroInfoPanel::SideEntry &entry)
{
    auto *p = new BalatroInfoPanel(mCNFont, this);
    // 副面板：name 走暗底白字（不包白盒），body 仍是白底圆角文字区。
    p->setContent(entry.name, entry.body, entry.badges,
                  qMax(110, entry.preferredWidth), /*nameHasWhiteBox=*/false);
    mRowLayout->addWidget(p, 0, Qt::AlignTop);
    mSidePanels.append(p);
}

void BalatroInfoCluster::relayout()
{
    ensurePolished();
    if (mMain) mMain->ensurePolished();
    for (auto *p : mSidePanels) if (p) p->ensurePolished();
    // 重置 cluster 自己的尺寸地板/天花板——上一张牌的高度可能比当前牌大，
    // 不显式清零 adjustSize 不会收回去。
    setMinimumSize(0, 0);
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    if (mMain) mMain->updateGeometry();
    for (auto *p : mSidePanels) if (p) p->updateGeometry();
    if (layout()) layout()->invalidate();
    updateGeometry();
    for (int i = 0; i < 3; ++i) {
        if (auto *lay = layout()) lay->activate();
        adjustSize();
    }
}
