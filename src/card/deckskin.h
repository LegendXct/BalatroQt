#ifndef DECKSKIN_H
#define DECKSKIN_H

#include <QPixmap>
#include <QString>

// 定制牌组（卡面换肤）。原理：只替换 8BitDeck 图集里 J/Q/K/A 的格子贴图，
// CardData / HandEvaluator / GameState 完全不感知皮肤——数值与游戏逻辑不变。
// 所有需要采样卡面图集的代码统一改从 DeckSkin::deckSheet() 取图，保证全局一致换肤。
class DeckSkin
{
public:
    enum Id { Default = 0, ChengShe = 1 };   // ChengShe = 程设专用牌组

    static int  count() { return 2; }
    static Id   current() { return sCurrent; }
    static void setCurrent(Id id);
    static QString name(Id id);              // 牌组显示名（定制牌组界面用）

    // 当前皮肤生效的整张 8BitDeck 图集：默认皮肤返回原图，
    // 程设皮肤返回 J/Q/K/A 四列 × 四花色行被人像卡面替换过的副本（懒构建并缓存）。
    static const QPixmap &deckSheet();

    // 换肤代数：CardItem 卡面缓存 key 掺入该值，切换皮肤后旧缓存条目自动失效。
    static int generation() { return sGeneration; }

private:
    static QPixmap buildChengSheSheet(const QPixmap &base);
    static Id  sCurrent;
    static int sGeneration;
};

#endif // DECKSKIN_H
