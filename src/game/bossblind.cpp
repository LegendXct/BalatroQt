#include "bossblind.h"
#include "demoscript.h"
#include <QRandomGenerator>
#include <QVector>

BossInfo bossInfo(BossEffect e) {
    switch (e) {
    case BossEffect::TheHook:    return {"钩子",   "每次出牌随机弃掉2张手牌"};
    case BossEffect::TheWall:    return {"围墙",   "特大盲注"};
    case BossEffect::TheNeedle:  return {"针",     "本回合只能出一次牌"};
    case BossEffect::TheClub:    return {"梅花",   "所有梅花牌都被削弱"};
    case BossEffect::ThePlant:   return {"植物",   "所有人头牌都被削弱"};
    case BossEffect::TheGoad:    return {"挑衅",   "所有黑桃牌都被削弱"};
    case BossEffect::TheWindow:  return {"窗口",   "所有方片牌都被削弱"};
    case BossEffect::TheHead:    return {"头部",   "所有红桃牌都被削弱"};
    case BossEffect::TheWater:   return {"水",     "初始弃牌次数为0"};
    case BossEffect::TheManacle: return {"镣铐",   "手牌上限-1"};
    case BossEffect::ThePsychic: return {"灵媒",   "必须出 5 张牌"};
    case BossEffect::TheFlint:   return {"燧石",   "基础筹码和倍率减半"};
    case BossEffect::TheArm:     return {"手臂",   "降低打出的牌型等级"};
    case BossEffect::TheMouth:   return {"嘴巴",   "本回合只能打出1种牌型"};
    case BossEffect::TheEye:     return {"眼睛",   "本回合中不可打出重复牌型"};
    case BossEffect::TheOx:      return {"公牛",   "打出指定牌型时资金归$0"};
    case BossEffect::TheHouse:   return {"房屋",   "第一次的手牌以背面朝上方式抽取"};
    case BossEffect::TheWheel:   return {"车轮",   "1/7几率，抽到的牌会是背面朝上"};
    case BossEffect::TheFish:    return {"鱼",     "出牌后自动抽取的牌都是背面朝上"};
    case BossEffect::TheMark:    return {"标记",   "所有人头牌都是以背面朝上的方式抽取"};
    case BossEffect::ThePillar:  return {"支柱",   "在这一底注中打出过的牌都被削弱"};
    case BossEffect::TheTooth:   return {"牙齿",   "每出一张牌损失$1"};
    case BossEffect::TheSerpent: return {"巨蟒",   "出牌或弃牌后总是抽 3 张牌"};
    case BossEffect::AmberAcorn: return {"琥珀之实", "翻转并洗乱所有小丑牌"};
    case BossEffect::CeruleanBell:return {"蔚蓝之铃","迫使 1 张牌总是被选中"};
    case BossEffect::CrimsonHeart:return {"绯红之心","每次出牌使随机一张小丑牌失效"};
    case BossEffect::VerdantLeaf: return {"翠绿之叶","所有卡牌都被削弱，直到售出1张小丑牌"};
    case BossEffect::VioletVessel:return {"靛紫之杯","超大盲注"};
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
    // 演示模式：脚本指定的 ante（默认仅第一 Ante）固定 Boss；脚本说"不干预"再走原 RNG。
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
