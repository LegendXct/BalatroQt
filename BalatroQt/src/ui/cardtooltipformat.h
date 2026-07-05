#ifndef CARDTOOLTIPFORMAT_H
#define CARDTOOLTIPFORMAT_H

#include "../card/carddata.h"
#include "../card/consumable.h"
#include "../card/joker.h"
#include <QColor>
#include <QString>
#include <QStringList>

// ── 原版 generate_card_ui 配色一致的 playing card / 通用描述 → HTML 帮助函数 ──
//
// 同样的 helpers 之前在 mainwindow.cpp / deckviewwidget.cpp / packopenwidget.cpp 各写了一份，
// 三处分支不一样会导致同一张牌在不同位置（主场景 / 牌组查看 / 塔罗-幻灵包临时手牌）显示不同。
// 这里集中成一份头文件，所有调用者共用。
//
// 配色对齐 globals.lua：
//   G.C.MULT       = #FE5F55  (red, +倍率)
//   G.C.CHIPS      = #009DFF  (blue, +筹码)
//   G.C.MONEY      = #F3B958  ($)
//   G.C.GREEN      = #4BC292  (概率 1/4, 1/5...)
//   G.C.PURPLE     = #8867A5
//   G.C.SUITS      默认偏暗（Spades #374649 等）——本文件用更高对比的 SO_1：
//     Hearts   #F03464
//     Diamonds #F06B3F
//     Spades   #403995  (紫色——和用户反馈 "原版像紫色" 一致)
//     Clubs    #235955  (暗青绿)
//   playing_card 名字模板 "{V:1}#2#{C:light_black}#1#"：
//     花色 = G.C.SUITS[suit] 染色；点数 = light_black 即 G.C.UI.TEXT_DARK = #4F6367
namespace CardTooltipFormat {

inline QString kChipsColor() { return QStringLiteral("#009dff"); }
inline QString kMultColor()  { return QStringLiteral("#fe5f55"); }
inline QString kMoneyColor() { return QStringLiteral("#f3b958"); }
inline QString kProbColor()  { return QStringLiteral("#4bc292"); }
inline QString kRankColor()  { return QStringLiteral("#4f6367"); } // light_black
inline QString kEditionColor() { return QStringLiteral("#4ca893"); }

inline QString suitColor(Suit s)
{
    switch (s) {
    case Suit::Hearts:   return QStringLiteral("#f03464");
    case Suit::Diamonds: return QStringLiteral("#f06b3f");
    case Suit::Spades:   return QStringLiteral("#403995");
    case Suit::Clubs:    return QStringLiteral("#235955");
    }
    return QStringLiteral("#4f6367");
}

inline QString suitText(Suit s)
{
    switch (s) {
    case Suit::Spades:   return QStringLiteral("黑桃");
    case Suit::Hearts:   return QStringLiteral("红桃");
    case Suit::Diamonds: return QStringLiteral("方块");
    case Suit::Clubs:    return QStringLiteral("梅花");
    }
    return QString();
}

inline QString rankText(Rank r)
{
    switch (r) {
    case Rank::Jack:  return QStringLiteral("J");
    case Rank::Queen: return QStringLiteral("Q");
    case Rank::King:  return QStringLiteral("K");
    case Rank::Ace:   return QStringLiteral("A");
    default: return QString::number(static_cast<int>(r));
    }
}

// 小丑稀有度的中文标签（badge 文案）。原版 k_common / k_uncommon / k_rare / k_legendary。
inline QString rarityName(JokerRarity r)
{
    switch (r) {
    case JokerRarity::Common:    return QStringLiteral("普通");
    case JokerRarity::Uncommon:  return QStringLiteral("罕见");
    case JokerRarity::Rare:      return QStringLiteral("稀有");
    case JokerRarity::Legendary: return QStringLiteral("传奇");
    }
    return QString();
}

inline QColor rarityColor(JokerRarity r)
{
    switch (r) {
    case JokerRarity::Common:    return QColor("#009dff");
    case JokerRarity::Uncommon:  return QColor("#4BC292");
    case JokerRarity::Rare:      return QColor("#fe5f55");
    case JokerRarity::Legendary: return QColor("#b26cbb");
    }
    return QColor("#888888");
}

inline QString enhancementName(Enhancement e)
{
    switch (e) {
    case Enhancement::Bonus: return QStringLiteral("奖励牌");
    case Enhancement::Mult:  return QStringLiteral("倍率牌");
    case Enhancement::Wild:  return QStringLiteral("万能牌");
    case Enhancement::Glass: return QStringLiteral("玻璃牌");
    case Enhancement::Steel: return QStringLiteral("钢铁牌");
    case Enhancement::Stone: return QStringLiteral("石头牌");
    case Enhancement::Gold:  return QStringLiteral("黄金牌");
    case Enhancement::Lucky: return QStringLiteral("幸运牌");
    case Enhancement::Iterator: return QStringLiteral("迭代器牌");
    default: return QString();
    }
}

inline QString editionName(Edition e)
{
    switch (e) {
    case Edition::Foil:        return QStringLiteral("闪箔");
    case Edition::Holographic: return QStringLiteral("镭射");
    case Edition::Polychrome:  return QStringLiteral("多彩");
    case Edition::Negative:    return QStringLiteral("负片");
    default: return QString();
    }
}

inline QString editionDesc(Edition e)
{
    switch (e) {
    case Edition::Foil:        return QStringLiteral("+50 筹码");
    case Edition::Holographic: return QStringLiteral("+10 倍率");
    case Edition::Polychrome:  return QStringLiteral("×1.5 倍率");
    case Edition::Negative:    return QStringLiteral("+1 持有槽位");
    default: return QString();
    }
}

inline QString sealName(Seal s)
{
    switch (s) {
    case Seal::Gold:   return QStringLiteral("金色蜡封");
    case Seal::Red:    return QStringLiteral("红色蜡封");
    case Seal::Blue:   return QStringLiteral("蓝色蜡封");
    case Seal::Purple: return QStringLiteral("紫色蜡封");
    default: return QString();
    }
}

inline QString sealDesc(Seal s)
{
    switch (s) {
    case Seal::Gold:   return QStringLiteral("打出并计分后获得 $3");
    case Seal::Red:    return QStringLiteral("重新触发这张牌 1 次");
    case Seal::Blue:   return QStringLiteral("回合结束时生成对应星球牌");
    case Seal::Purple: return QStringLiteral("弃掉时生成一张塔罗牌");
    default: return QString();
    }
}

inline QString span(const QString &text, const QString &color, bool bold = true)
{
    QString style = QStringLiteral("color:%1").arg(color);
    if (bold) style += QLatin1String(";font-weight:bold");
    return QStringLiteral("<span style=\"%1\">%2</span>")
            .arg(style, text.toHtmlEscaped());
}

// 类似原版 {X:mult,C:white} X#1# {} —— 红底 + 白字方块。
inline QString xmultPill(const QString &text)
{
    return QStringLiteral(
        "<span style=\"background:#fe5f55;color:white;font-weight:bold;"
        "padding:0 4px;border-radius:3px;\">%1</span>")
        .arg(text.toHtmlEscaped());
}

// 把 zh_CN.lua 的 "{C:mult}+#1#{}" / "{X:mult,C:white}X#1#{}" / "{C:chips}+30{}"
// markup 转成 QLabel 能渲染的 HTML。
inline QString fromLuaMarkup(const QString &src)
{
    if (src.isEmpty()) return src;
    QString out;
    out.reserve(src.size() + 16);
    QStringList stack;
    int i = 0;
    while (i < src.size()) {
        const QChar ch = src.at(i);
        if (ch == '{') {
            int end = src.indexOf('}', i + 1);
            if (end < 0) { out.append(ch); ++i; continue; }
            QString tag = src.mid(i + 1, end - i - 1);
            if (tag.isEmpty()) {
                if (!stack.isEmpty()) {
                    out.append(QLatin1String("</span>"));
                    stack.removeLast();
                }
            } else {
                QString colorKey, xKey;
                for (const QString &part : tag.split(',', Qt::SkipEmptyParts)) {
                    if (part.startsWith(QLatin1String("C:"))) colorKey = part.mid(2);
                    else if (part.startsWith(QLatin1String("X:"))) xKey = part.mid(2);
                }
                auto colorFor = [](const QString &k) -> QString {
                    if (k == "chips" || k == "blue")  return kChipsColor();
                    if (k == "mult"  || k == "red")   return kMultColor();
                    if (k == "money" || k == "gold")  return kMoneyColor();
                    if (k == "green" || k == "attention") return kProbColor();
                    if (k == "purple") return QStringLiteral("#8867a5");
                    if (k == "white") return QStringLiteral("#ffffff");
                    if (k == "dark_edition") return QStringLiteral("#0a0a0a");
                    if (k == "edition") return kEditionColor();
                    if (k == "spades")   return QStringLiteral("#403995");
                    if (k == "hearts")   return QStringLiteral("#f03464");
                    if (k == "diamonds") return QStringLiteral("#f06b3f");
                    if (k == "clubs")    return QStringLiteral("#235955");
                    return QString();
                };
                if (!xKey.isEmpty()) {
                    QString bg = colorFor(xKey);
                    QString fg = colorFor(colorKey.isEmpty() ? QStringLiteral("white") : colorKey);
                    if (bg.isEmpty()) bg = kMultColor();
                    if (fg.isEmpty()) fg = QStringLiteral("#ffffff");
                    out.append(QStringLiteral(
                        "<span style=\"background:%1;color:%2;font-weight:bold;"
                        "padding:0 4px;border-radius:3px;\">").arg(bg, fg));
                    stack.append(QStringLiteral("xmult"));
                } else {
                    QString c = colorFor(colorKey);
                    if (c.isEmpty()) c = kRankColor();
                    out.append(QStringLiteral(
                        "<span style=\"color:%1;font-weight:bold\">").arg(c));
                    stack.append(QStringLiteral("color"));
                }
            }
            i = end + 1;
        } else if (ch == '\n') {
            out.append(QLatin1String("<br/>"));
            ++i;
        } else if (ch == '<') {
            out.append(QLatin1String("&lt;"));
            ++i;
        } else if (ch == '>') {
            out.append(QLatin1String("&gt;"));
            ++i;
        } else if (ch == '&') {
            out.append(QLatin1String("&amp;"));
            ++i;
        } else {
            out.append(ch);
            ++i;
        }
    }
    while (!stack.isEmpty()) {
        out.append(QLatin1String("</span>"));
        stack.removeLast();
    }
    return out;
}

// playing card 标题：花色染色 + 点数 light_black（白色 name 盒上的暗灰色文字）。
// 标题只放"花色+点数"，增强类型不入标题——对应效果在 body 内联（+8 筹码 / +30 额外筹码 …）。
inline QString cardTitleHtml(const CardData &c)
{
    if (c.enhancement == Enhancement::Stone)
        return span(QStringLiteral("石头牌"), kRankColor());
    return span(suitText(c.suit), suitColor(c.suit))
         + QStringLiteral(" ")
         + span(rankText(c.rank), kRankColor());
}

// ── 单独的副面板正文：每种增强 / edition / seal 各自一个独立面板。
//    cluster 模式下主面板只显示牌面的基础筹码，效果性的描述拆到副面板。

inline QString enhancementBodyHtml(Enhancement e)
{
    switch (e) {
    case Enhancement::Bonus:
        return span(QStringLiteral("+30"), kChipsColor()) + QStringLiteral(" 额外筹码");
    case Enhancement::Mult:
        return span(QStringLiteral("+4"), kMultColor()) + QStringLiteral(" 倍率");
    case Enhancement::Wild:
        return QStringLiteral("可视作任意花色");
    case Enhancement::Glass:
        return xmultPill(QStringLiteral("X2")) + QStringLiteral(" 倍率<br/>")
             + QStringLiteral("有 ") + span(QStringLiteral("1/4"), kProbColor())
             + QStringLiteral(" 概率摧毁此牌");
    case Enhancement::Steel:
        return xmultPill(QStringLiteral("X1.5"))
             + QStringLiteral(" 倍率<br/>当此牌停留在手牌时");
    case Enhancement::Gold:
        return QStringLiteral("回合结束时若仍在手牌：")
             + span(QStringLiteral("$3"), kMoneyColor());
    case Enhancement::Lucky:
        return span(QStringLiteral("1/5"), kProbColor())
             + QStringLiteral(" 概率 ")
             + span(QStringLiteral("+20"), kMultColor())
             + QStringLiteral(" 倍率<br/>")
             + span(QStringLiteral("1/15"), kProbColor())
             + QStringLiteral(" 概率 ")
             + span(QStringLiteral("$20"), kMoneyColor());
    case Enhancement::Stone:
        return span(QStringLiteral("+50"), kChipsColor()) + QStringLiteral(" 筹码<br/>")
             + QStringLiteral("无点数 / 无花色");
    case Enhancement::Iterator:
        return QStringLiteral("每次打出后点数 ")
             + span(QStringLiteral("+1"), kChipsColor())
             + QStringLiteral("<br/>K→A，A→2 循环递增");
    default:
        return QString();
    }
}

inline QString editionBodyHtml(Edition e)
{
    switch (e) {
    case Edition::Foil:        return span(QStringLiteral("+50"), kChipsColor()) + QStringLiteral(" 筹码");
    case Edition::Holographic: return span(QStringLiteral("+10"), kMultColor()) + QStringLiteral(" 倍率");
    case Edition::Polychrome:  return xmultPill(QStringLiteral("X1.5")) + QStringLiteral(" 倍率");
    case Edition::Negative:    return QStringLiteral("+1 持有槽位");
    default: return QString();
    }
}

inline QString sealBodyHtml(Seal s)
{
    switch (s) {
    case Seal::Gold:   return QStringLiteral("打出并计分后获得 ")
                            + span(QStringLiteral("$3"), kMoneyColor());
    case Seal::Red:    return QStringLiteral("重新触发此牌 1 次");
    case Seal::Blue:   return QStringLiteral("回合结束时生成对应等级的<br/>")
                            + span(QStringLiteral("行星牌"), QStringLiteral("#13afce"));
    case Seal::Purple: return QStringLiteral("弃掉时生成一张<br/>")
                            + span(QStringLiteral("塔罗牌"), QStringLiteral("#a782d1"));
    default: return QString();
    }
}

// ── 消耗类副面板映射 ──────────────────────────────────────────
// 塔罗 / 幻灵 等"会授予某种增强 / 蜡封 / edition"的消耗，悬浮时把对应被授予物
// 拆到一只独立副面板。其他不授予效果的（如愚者、女祭司）返回 None。

inline Enhancement consumableGrantsEnhancement(ConsumableType t)
{
    switch (t) {
    case ConsumableType::Tarot_Magician:   return Enhancement::Lucky;
    case ConsumableType::Tarot_Empress:    return Enhancement::Mult;
    case ConsumableType::Tarot_Hierophant: return Enhancement::Bonus;
    case ConsumableType::Tarot_Chariot:    return Enhancement::Steel;
    case ConsumableType::Tarot_Lovers:     return Enhancement::Wild;
    case ConsumableType::Tarot_Justice:    return Enhancement::Glass;
    case ConsumableType::Tarot_Tower:      return Enhancement::Stone;
    case ConsumableType::Tarot_Devil:      return Enhancement::Gold;
    case ConsumableType::Tarot_Iterator:   return Enhancement::Iterator;
    default:                               return Enhancement::None;
    }
}

inline Seal consumableGrantsSeal(ConsumableType t)
{
    switch (t) {
    case ConsumableType::Spectral_Talisman: return Seal::Gold;
    case ConsumableType::Spectral_DejaVu:   return Seal::Red;
    case ConsumableType::Spectral_Trance:   return Seal::Blue;
    case ConsumableType::Spectral_Medium:   return Seal::Purple;
    default:                                return Seal::None;
    }
}

// Spectral_Aura 给随机 Foil/Holo/Polychrome；Tarot_Wheel 也是同样池子。
// 返回 true 表示该消耗会授予一个随机 edition——副面板可以提示三种 edition 池子。
inline bool consumableGrantsRandomEdition(ConsumableType t)
{
    return t == ConsumableType::Spectral_Aura
        || t == ConsumableType::Tarot_Wheel;
}

// 主面板正文（cluster 模式）：只描述牌面本体的基础筹码与点数信息，不写 enhancement/edition/seal。
inline QString cardMainBodyHtml(const CardData &c)
{
    if (c.enhancement == Enhancement::Stone) {
        // 石头牌没有点数与花色——本体没有"基础筹码"以外的信息可显示，
        // 提示文字交给增强副面板，主面板就空着只显示标题。
        return QString();
    }
    int chips = c.chipValue() + c.permanentBonusChips;
    QStringList lines;
    lines << span(QStringLiteral("+%1").arg(chips), kChipsColor())
            + QStringLiteral(" 筹码");
    if (c.isDebuffed)
        lines << span(QStringLiteral("被 Boss 盲注禁用"), kMultColor());
    return lines.join(QLatin1String("<br/>"));
}

// playing card 描述：与原版 m_xxx 模板对齐——chips/+mult/$ 用各自颜色。
//   - 基础筹码（点数 + 累计奖励）单独一行：+8 筹码
//   - 增强加成的筹码 / 倍率 / 概率效果各自换行：+30 额外筹码 / X1.5 倍率 / 1/4 概率…
inline QString cardBodyHtml(const CardData &c)
{
    QStringList lines;
    auto chipsLine = [](int chips, bool extra) {
        return span(QStringLiteral("+%1").arg(chips), kChipsColor())
             + (extra ? QStringLiteral(" 额外筹码") : QStringLiteral(" 筹码"));
    };
    if (c.enhancement == Enhancement::Stone) {
        lines << chipsLine(50 + c.permanentBonusChips, false);
        lines << QStringLiteral("无点数 / 无花色");
    } else {
        int baseChips = c.chipValue() + c.permanentBonusChips;
        lines << chipsLine(baseChips, false);
        switch (c.enhancement) {
        case Enhancement::Bonus:
            lines << chipsLine(30, true);
            break;
        case Enhancement::Mult:
            lines << span(QStringLiteral("+4"), kMultColor())
                   + QStringLiteral(" 倍率");
            break;
        case Enhancement::Wild:
            lines << QStringLiteral("可视作任意花色");
            break;
        case Enhancement::Glass:
            lines << xmultPill(QStringLiteral("X2")) + QStringLiteral(" 倍率");
            lines << QStringLiteral("有 ")
                   + span(QStringLiteral("1/4"), kProbColor())
                   + QStringLiteral(" 概率摧毁此牌");
            break;
        case Enhancement::Steel:
            lines << xmultPill(QStringLiteral("X1.5"))
                   + QStringLiteral(" 倍率（手牌持有时）");
            break;
        case Enhancement::Gold:
            lines << QStringLiteral("回合末若仍在手牌：")
                   + span(QStringLiteral("$3"), kMoneyColor());
            break;
        case Enhancement::Lucky:
            lines << span(QStringLiteral("1/5"), kProbColor())
                   + QStringLiteral(" 概率 ")
                   + span(QStringLiteral("+20"), kMultColor())
                   + QStringLiteral(" 倍率");
            lines << span(QStringLiteral("1/15"), kProbColor())
                   + QStringLiteral(" 概率 ")
                   + span(QStringLiteral("$20"), kMoneyColor());
            break;
        case Enhancement::Iterator:
            lines << QStringLiteral("每次打出后点数 ")
                   + span(QStringLiteral("+1"), kChipsColor())
                   + QStringLiteral("（K→A，A→2）");
            break;
        default: break;
        }
    }
    if (c.edition != Edition::None)
        lines << span(editionName(c.edition), kEditionColor())
               + QStringLiteral("：") + editionDesc(c.edition);
    if (c.seal != Seal::None) {
        QString sc;
        switch (c.seal) {
        case Seal::Gold:   sc = QStringLiteral("#eac058"); break;
        case Seal::Red:    sc = QStringLiteral("#fe5f55"); break;
        case Seal::Blue:   sc = QStringLiteral("#009dff"); break;
        case Seal::Purple: sc = QStringLiteral("#8867a5"); break;
        default: break;
        }
        if (!sc.isEmpty())
            lines << span(sealName(c.seal), sc)
                   + QStringLiteral("：") + sealDesc(c.seal);
    }
    if (c.isDebuffed)
        lines << span(QStringLiteral("被 Boss 盲注禁用"), kMultColor());
    return lines.join(QLatin1String("<br/>"));
}

} // namespace CardTooltipFormat

#endif // CARDTOOLTIPFORMAT_H
