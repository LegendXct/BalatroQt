#include "bossblind.h"
#include <QRandomGenerator>
#include <QVector>

BossInfo bossInfo(BossEffect e) {
    switch (e) {
    case BossEffect::TheHook:    return {"钩子",   "出牌后随机弃 2 张手牌"};
    case BossEffect::TheWall:    return {"围墙",   "目标分数 ×2"};
    case BossEffect::TheNeedle:  return {"针",     "每回合只能出 1 次"};
    case BossEffect::TheClub:    return {"梅花",   "所有 ♣ 牌被禁用"};
    case BossEffect::ThePlant:   return {"植物",   "所有 J/Q/K 被禁用"};
    case BossEffect::TheGoad:    return {"尖刺",   "所有 ♠ 牌被禁用"};
    case BossEffect::TheWindow:  return {"窗口",   "所有 ♦ 牌被禁用"};
    case BossEffect::TheHead:    return {"头颅",   "所有 ♥ 牌被禁用"};
    case BossEffect::TheWater:   return {"水",     "本盲注没有弃牌次数"};
    case BossEffect::TheManacle: return {"镣铐",   "本盲注手牌上限 -1"};
    case BossEffect::ThePsychic: return {"灵媒",   "必须出 5 张牌"};
    case BossEffect::TheFlint:   return {"燧石",   "基础筹码和倍率减半"};
    case BossEffect::TheArm:     return {"手臂",   "出牌后该牌型等级下降"};
    case BossEffect::TheMouth:   return {"嘴",     "本盲注只能出第一种牌型"};
    case BossEffect::TheEye:     return {"眼",     "本盲注不能重复出同一种牌型"};
    case BossEffect::None:       return {"",       ""};
    }
    return {"", ""};
}

int bossChipRow(BossEffect e)
{
    switch (e) {
    case BossEffect::TheHook:    return 7;
    case BossEffect::TheClub:    return 4;
    case BossEffect::TheWall:    return 9;
    case BossEffect::ThePlant:   return 19;
    case BossEffect::TheNeedle:  return 20;
    case BossEffect::TheGoad:    return 13;
    case BossEffect::TheWindow:  return 6;
    case BossEffect::TheHead:    return 21;
    case BossEffect::TheWater:   return 14;
    case BossEffect::TheManacle: return 8;
    case BossEffect::ThePsychic: return 12;
    case BossEffect::TheFlint:   return 24;
    case BossEffect::TheArm:     return 11;
    case BossEffect::TheMouth:   return 18;
    case BossEffect::TheEye:     return 17;
    default: return 7;
    }
}

struct BossCandidate { BossEffect effect; int minAnte; int maxAnte; };

BossEffect randomBossEffect(int ante) {
    static const BossCandidate pool[] = {
        {BossEffect::TheHook, 1, 10}, {BossEffect::TheClub, 1, 10},
        {BossEffect::TheGoad, 1, 10}, {BossEffect::TheHead, 1, 10},
        {BossEffect::TheWindow, 1, 10}, {BossEffect::ThePsychic, 1, 10},
        {BossEffect::TheManacle, 1, 10}, {BossEffect::TheWall, 2, 10},
        {BossEffect::TheWater, 2, 10}, {BossEffect::TheNeedle, 2, 10},
        {BossEffect::TheFlint, 2, 10}, {BossEffect::TheMouth, 2, 10},
        {BossEffect::TheEye, 3, 10}, {BossEffect::TheArm, 2, 10},
        {BossEffect::ThePlant, 4, 10},
    };
    QVector<BossEffect> legal;
    for (const auto &c : pool) {
        if (ante >= c.minAnte && ante <= c.maxAnte) legal.append(c.effect);
    }
    if (legal.isEmpty()) legal.append(BossEffect::TheHook);
    return legal[QRandomGenerator::global()->bounded(legal.size())];
}
