#ifndef BOSSBLIND_H
#define BOSSBLIND_H

#include <QString>

enum class BossEffect {
    None,
    TheHook,      // 出牌后随机弃 2 张手牌
    TheWall,      // 目标分数 ×2
    TheNeedle,    // 每回合只能出 1 次
    TheClub,      // 所有 ♣ 牌被禁用
    ThePlant,     // 所有 J/Q/K 被禁用
    TheGoad,      // 所有 ♠ 牌被禁用
    TheWindow,    // 所有 ♦ 牌被禁用
    TheHead,      // 所有 ♥ 牌被禁用
    TheWater,     // 没有弃牌次数
    TheManacle,   // 手牌上限 -1
    ThePsychic,   // 必须出 5 张牌
    TheFlint,     // 基础筹码与倍率减半
    TheArm,       // 出牌牌型等级下降
    TheMouth,     // 本回合只能出一种牌型
    TheEye        // 本回合不能重复牌型
};

struct BossInfo {
    QString name;
    QString description;
};

BossInfo bossInfo(BossEffect e);
BossEffect randomBossEffect(int ante = 1);   // 按原版 min/max ante 从 Boss 池里随机
int bossChipRow(BossEffect e);                // BlindChips.png 行号

#endif
