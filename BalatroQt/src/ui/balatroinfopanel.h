#ifndef BALATROINFOPANEL_H
#define BALATROINFOPANEL_H

#include <QWidget>
#include <QFont>
#include <QString>
#include <QColor>
#include <QVector>
#include <QPair>

class QLabel;
class QVBoxLayout;
class QHBoxLayout;

// 复刻原版 Balatro 的卡牌信息浮窗（generate_card_ui / create_UIBox_detailed_tooltip）样式：
//   - 外层：lighten(JOKER_GREY, 0.5) ≈ #dfe3ea 浅灰底，圆角 + 浮雕。
//   - 内层：adjust_alpha(darken(BLACK,0.1),0.8) ≈ rgba(50,59,61,204) 暗青底。
//   - 标题（白色加粗） + 描述正文（白色）。
//   - 底部一行彩色 pill 标签：每个 pill 用对应集合色（如塔罗紫 #a782d1 / 行星青 #13afce），
//     原版 create_badge 的等价物。
//
// 多个调用点（mCardInfoPanel / mJokerInfoPanel / mConsumableInfoPanel / 开包悬浮窗）共用同一外观。
class BalatroInfoPanel : public QWidget {
    Q_OBJECT
public:
    struct Badge {
        QString text;
        QColor bg;                  // pill 背景色（如 SECONDARY_SET 的紫/青/蓝）
        QColor fg = QColor("#ffffff");
    };

    // 一个并排的副信息条目：增强 / 闪箔多彩等 edition / 蜡封——
    // 原版 generate_card_ui 在描述下方会为每一种额外效果各自生成一个独立的小框。
    struct SideEntry {
        QString name;               // 副面板标题（暗底白字）
        QString body;               // 副面板正文 HTML
        QVector<Badge> badges;      // 副面板底部小标签（通常 0-1 个）
        int preferredWidth = 150;
    };

    explicit BalatroInfoPanel(const QFont &cnFont, QWidget *parent = nullptr);

    // 设置内容。preferredWidth 为整体面板期望宽度（含外层 padding），实际会按文字自适应高度。
    //
    // nameHasWhiteBox: 原版 generate_card_ui 里 name_from_rows() 的第二参数——
    //   playing card (扑克手牌) = true → 名字也包在白色圆角盒里；
    //   joker / tarot / planet / spectral / voucher = false → 名字直接是白字落在暗底上，
    //   只有下方描述（desc_from_rows）有白色圆角盒。
    void setContent(const QString &name, const QString &body,
                    const QVector<Badge> &badges, int preferredWidth,
                    bool nameHasWhiteBox = false);

    // 仅设置 body 高度（描述很短时也保证一致的最小高度），可选。
    void setBodyMinHeight(int h);

    // 便利函数：根据 ConsumableType / Joker rarity / Edition 等返回该 set 对应的 pill 颜色。
    static QColor tarotPillColor();        // #a782d1
    static QColor planetPillColor();       // #13afce
    static QColor spectralPillColor();     // #4584fa
    static QColor jokerCommonColor();      // #009dff
    static QColor jokerUncommonColor();    // #4BC292
    static QColor jokerRareColor();        // #fe5f55
    static QColor jokerLegendaryColor();   // #b26cbb
    static QColor voucherPillColor();      // #fd682b
    static QColor editionPillColor();      // 黑底
    static QColor sealPillColor(int kind); // 0=gold #eac058, 1=red #fe5f55, 2=blue #009dff, 3=purple #8867a5

private:
    QWidget *mInner = nullptr;
    QWidget *mNameBox = nullptr;       // 可见时白色圆角盒（仅 playing card 用），否则透明壳
    QLabel  *mNameLbl = nullptr;       // 名字文本（在 mNameBox 内）
    QWidget *mBodyBox = nullptr;       // 描述用白色圆角盒——所有类型都用
    QLabel  *mBodyLbl = nullptr;       // 描述文本（在 mBodyBox 内）
    QWidget *mBadgesRow = nullptr;
    QHBoxLayout *mBadgesLayout = nullptr;
    QFont   mCNFont;
};

// 并排展示一组 BalatroInfoPanel——对齐原版 generate_card_ui：playing card 主面板右侧依次
// 排开"增强 / edition / 蜡封"等独立子面板，让镭射/全息/蜡封各自占一个独立的暗底圆角矩形。
class BalatroInfoCluster : public QWidget {
    Q_OBJECT
public:
    explicit BalatroInfoCluster(const QFont &cnFont, QWidget *parent = nullptr);

    // 清空全部子面板。
    void clear();

    // 主面板内容：与 BalatroInfoPanel::setContent 同义。
    void setMainContent(const QString &name, const QString &body,
                        const QVector<BalatroInfoPanel::Badge> &badges,
                        int preferredWidth, bool nameHasWhiteBox);

    // 追加一个副面板（出现在主面板右侧）；按顺序排列。
    void addSidePanel(const BalatroInfoPanel::SideEntry &entry);

    // 整体重排——会把所有子面板移到水平排列里，并自动 adjustSize。
    void relayout();

private:
    QFont mCNFont;
    QHBoxLayout *mRowLayout = nullptr;
    BalatroInfoPanel *mMain = nullptr;
    QVector<BalatroInfoPanel*> mSidePanels;
};

#endif // BALATROINFOPANEL_H
