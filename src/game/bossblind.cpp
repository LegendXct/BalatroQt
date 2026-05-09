#include "bossblind.h"
#include <QRandomGenerator>

BossInfo bossInfo(BossEffect e) {
    switch (e) {
    case BossEffect::TheHook:   return {"钩子",   "出牌后随机弃 2 张手牌"};
    case BossEffect::TheWall:   return {"围墙",   "目标分数 ×2"};
    case BossEffect::TheNeedle: return {"针",     "每回合只能出 1 次"};
    case BossEffect::TheClub:   return {"梅花",   "所有 ♣ 牌被禁用"};
    case BossEffect::ThePlant:  return {"植株",   "所有 J/Q/K 被禁用"};
    case BossEffect::None:      return {"",       ""};
    }
    return {"", ""};
}

BossEffect randomBossEffect() {
    constexpr BossEffect pool[] = {
        BossEffect::TheHook, BossEffect::TheWall,
        BossEffect::TheNeedle, BossEffect::TheClub,
        BossEffect::ThePlant,
    };
    int n = int(sizeof(pool) / sizeof(pool[0]));
    return pool[QRandomGenerator::global()->bounded(n)];
}
