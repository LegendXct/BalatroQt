#ifndef DECKSKIN_H
#define DECKSKIN_H

#include <QPixmap>
#include <QString>
#include <QRect>
#include "carddata.h"

class QPainter;

// 定制牌组（卡面换肤）。原理：只替换 8BitDeck 图集里 J/Q/K/A 的格子贴图，
// CardData / HandEvaluator / GameState 完全不感知皮肤——数值与游戏逻辑不变。
// 所有需要采样卡面图集的代码统一改从 DeckSkin::deckSheet() 取图，保证全局一致换肤。
class DeckSkin
{
public:
    enum Id { Default = 0, ChengShe = 1 };   // ChengShe = 程设牌组

    static int  count() { return 2; }
    static Id   current() { return sCurrent; }
    static void setCurrent(Id id);
    static QString name(Id id);              // 牌组显示名（定制牌组界面用）

    // 当前皮肤生效的整张 8BitDeck 图集：默认皮肤返回原图，
    // 程设皮肤返回 J/Q/K/A 四列 × 四花色行被人像卡面替换过的副本（懒构建并缓存）。
    static const QPixmap &deckSheet();

    // 换肤代数：CardItem 卡面缓存 key 掺入该值，切换皮肤后旧缓存条目自动失效。
    static int generation() { return sGeneration; }

    // 程设整卡人像 × 背景式增强（奖励/倍率/万能/幸运/玻璃/钢铁/黄金）的层序协定：
    //   默认皮肤：增强底图在下、卡面在上（卡面四周透明，底色透出）——调用方原有画法；
    //   程设整卡人像不透明会盖死底色，改为人像在下、增强以 100% 不透明的"边框"叠在
    //   人像之上（中心开圆角窗；玻璃贴图自带半透明则整张直接叠加），角标原位回贴。
    // 调用方先按原层序画完增强+卡面，再在 enhancementOverArt() 为真时补画顶层。
    static bool enhancementOverArt(Rank rank, Enhancement enh);
    static void drawEnhancementOverArt(QPainter *p,
                                       const QPixmap &enhSheet, const QRect &enhSrc,
                                       Rank rank, Suit suit, Enhancement enh);

private:
    static QPixmap buildChengSheSheet(const QPixmap &base);
    static const QPixmap &baseSheet();   // 原版 8BitDeck 图集（角标回贴取材用）
    static Id  sCurrent;
    static int sGeneration;
};

#endif // DECKSKIN_H
