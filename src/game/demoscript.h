#ifndef DEMOSCRIPT_H
#define DEMOSCRIPT_H

#include <QVector>
#include "../card/carddata.h"
#include "../card/joker.h"
#include "../card/consumable.h"
#include "bossblind.h"
#include "boosterpack.h"

struct ShopOffer;

// 路演演示模式：游戏设置里勾选后开关启用。开启时每次"开始新的一局"按下面这个固定剧本
// 演出：小盲注 → 第一商店（蓝图/头脑风暴/DNA/特里布莱+ Grabber + 天体包木星 + 奥秘包正义）
//   → 大盲注（人头同花 + 对子 + 玻璃 ♠7）→ 第二商店（不买东西）→ Boss 支柱（同花顺含玻璃 ♠7）。
//
// 实现哲学：用静态状态机替代 RNG，hook 点尽量少——
//   Deck::reset()        → reorderDeckForNextBlind 把脚本规定的 8 张卡换到顶
//   Shop::rerollShopOnly → scriptedShopOffers 替换主货架
//   Shop::roll()         → scriptedVoucherOffers / scriptedBoosterOffers 替换 voucher/包
//   generatePackContent  → scriptedPackContent 替换奥秘/天体包内容
//   randomBossEffect     → 第一 Ante 直接返回 ThePillar
class DemoScript {
public:
    static bool active() { return sActive; }
    static void setActive(bool on);

    // 生命周期 hook（由 GameState 在对应时点调用）：
    static void onStartGame();   // 重置全部计数
    static void onEnterShop();   // 进商店 → shopVisit++
    static void onShopReroll();  // 商店重摇 → rerollCount++
    static void onEnterBlind();  // 进入新盲注（包括小盲）→ blindEntered++

    static int shopVisit()   { return sShopVisit; }
    static int rerollCount() { return sShopRerolls; }
    static int blindEntered(){ return sBlindEntered; }

    // 把 pile 重排：脚本指定的 8 张卡放到最前，其它顺序不动。Deck::reset() 里调用。
    static void reorderDeckForNextBlind(QVector<CardData> &pile);

    // 主货架/voucher/包：调用方先 clear 再调用。out 容量由 caller 控制。
    // 注意参数名不能用 slots——那是 Qt MOC 的关键字宏（会被 #define 成空），编译会炸。
    static void scriptedShopOffers(QVector<ShopOffer> &out, int slotCount);
    static void scriptedVoucherOffers(QVector<ShopOffer> &out);
    static void scriptedBoosterOffers(QVector<ShopOffer> &out);

    // 包内容覆盖：返回 true 表示已写入 out，调用方跳过默认随机生成。
    static bool scriptedPackContent(PackKind kind, PackSize size, PackContent &out);

    // 第一 Ante Boss 固定为支柱；其它 Ante 不干预（路演 4 分钟内打不到）。
    // 返回 BossEffect::None 表示不干预，由原 RNG 决定。
    static BossEffect scriptedBoss(int ante);

private:
    static bool sActive;
    static int  sShopVisit;
    static int  sShopRerolls;
    static int  sBlindEntered;
};

#endif // DEMOSCRIPT_H
