#include "bossblind.h"
#include "demoscript.h"
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
    case BossEffect::TheOx:      return {"公牛",   "打出最常用牌型时金币归零"};
    case BossEffect::TheHouse:   return {"房屋",   "第一手牌背面朝下发出"};
    case BossEffect::TheWheel:   return {"轮子",   "1/7 的牌背面朝下发出"};
    case BossEffect::TheFish:    return {"鱼",     "每次出牌后补的牌背面朝下"};
    case BossEffect::TheMark:    return {"标记",   "所有人头牌背面朝下发出"};
    case BossEffect::ThePillar:  return {"支柱",   "本 Ante 之前打出过的牌被禁用"};
    case BossEffect::TheTooth:   return {"牙齿",   "每打出 1 张牌失去 $1"};
    case BossEffect::TheSerpent: return {"巨蛇",   "出牌或弃牌后固定只补 3 张"};
    case BossEffect::AmberAcorn: return {"琥珀橡果", "所有小丑翻面并打乱顺序"};
    case BossEffect::CeruleanBell:return {"蔚蓝铃铛","强制 1 张手牌始终被选中"};
    case BossEffect::CrimsonHeart:return {"绯红之心","每出一手随机禁用 1 张小丑"};
    case BossEffect::VerdantLeaf: return {"翠绿之叶","所有牌被禁用，直到卖出 1 张小丑"};
    case BossEffect::VioletVessel:return {"紫罗兰之器","超大盲注"};
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
    case BossEffect::TheOx:      return 2;
    case BossEffect::TheHouse:   return 3;
    case BossEffect::TheFish:    return 5;
    case BossEffect::TheWheel:   return 10;
    case BossEffect::TheSerpent: return 15;
    case BossEffect::ThePillar:  return 16;
    case BossEffect::TheTooth:   return 22;
    case BossEffect::TheMark:    return 23;
    case BossEffect::CrimsonHeart:return 25;
    case BossEffect::CeruleanBell:return 26;
    case BossEffect::AmberAcorn: return 27;
    case BossEffect::VerdantLeaf:return 28;
    case BossEffect::VioletVessel:return 29;
    default: return 7;
    }
}

struct BossCandidate { BossEffect effect; int minAnte; int maxAnte; };

BossEffect randomBossEffect(int ante) {
    // 演示模式：脚本指定的 ante（默认仅第一 Ante）固定为支柱；脚本说"不干预"再走原 RNG。
    if (DemoScript::active()) {
        BossEffect scripted = DemoScript::scriptedBoss(ante);
        if (scripted != BossEffect::None) return scripted;
    }
    // 终盘 Boss：ante 为 8 的倍数时（含无尽模式的 16/24…）固定出现。
    if (ante > 0 && ante % 8 == 0) {
        static const BossEffect finishers[] = {
            BossEffect::AmberAcorn, BossEffect::CeruleanBell,
            BossEffect::CrimsonHeart, BossEffect::VerdantLeaf,
            BossEffect::VioletVessel,
        };
        return finishers[QRandomGenerator::global()->bounded(int(sizeof(finishers)/sizeof(*finishers)))];
    }

    static const BossCandidate pool[] = {
        {BossEffect::TheHook, 1, 10}, {BossEffect::TheClub, 1, 10},
        {BossEffect::TheGoad, 1, 10}, {BossEffect::TheHead, 1, 10},
        {BossEffect::TheWindow, 1, 10}, {BossEffect::ThePsychic, 1, 10},
        {BossEffect::TheManacle, 1, 10}, {BossEffect::TheWall, 2, 10},
        {BossEffect::TheWater, 2, 10}, {BossEffect::TheNeedle, 2, 10},
        {BossEffect::TheFlint, 2, 10}, {BossEffect::TheMouth, 2, 10},
        {BossEffect::TheEye, 3, 10}, {BossEffect::TheArm, 2, 10},
        {BossEffect::ThePlant, 4, 10},
        {BossEffect::TheOx, 6, 10}, {BossEffect::TheHouse, 2, 10},
        {BossEffect::TheWheel, 2, 10}, {BossEffect::TheFish, 2, 10},
        {BossEffect::TheMark, 2, 10}, {BossEffect::ThePillar, 1, 10},
        {BossEffect::TheTooth, 3, 10}, {BossEffect::TheSerpent, 5, 10},
    };
    QVector<BossEffect> legal;
    for (const auto &c : pool) {
        if (ante >= c.minAnte && ante <= c.maxAnte) legal.append(c.effect);
    }
    if (legal.isEmpty()) legal.append(BossEffect::TheHook);
    return legal[QRandomGenerator::global()->bounded(legal.size())];
}
