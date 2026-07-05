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
    TheEye,       // 本回合不能重复牌型
    TheOx,        // 打出最常用牌型时金币归零
    TheHouse,     // 第一手牌背面朝下发出
    TheWheel,     // 1/7 的牌背面朝下发出
    TheFish,      // 每次出牌后补的牌背面朝下
    TheMark,      // 所有人头牌背面朝下发出
    ThePillar,    // 本 Ante 之前打出过的牌被禁用
    TheTooth,     // 每打出 1 张牌失去 $1
    TheSerpent,   // 出牌/弃牌后固定只补 3 张
    AmberAcorn,   // 终盘：所有小丑翻面并打乱顺序
    CeruleanBell, // 终盘：强制 1 张手牌始终被选中
    CrimsonHeart, // 终盘：每手随机禁用 1 张小丑
    VerdantLeaf,  // 终盘：所有牌被禁用，直到卖出 1 张小丑
    VioletVessel  // 终盘：超大盲注
};

// 终盘 Boss（仅在 ante 为 8 的倍数时出现）
inline bool isFinisherBoss(BossEffect e) {
    return e == BossEffect::AmberAcorn || e == BossEffect::CeruleanBell
        || e == BossEffect::CrimsonHeart || e == BossEffect::VerdantLeaf
        || e == BossEffect::VioletVessel;
}

struct BossInfo {
    QString name;
    QString description;
};

BossInfo bossInfo(BossEffect e);
BossEffect randomBossEffect(int ante = 1);   // 按原版 min/max ante 从 Boss 池里随机
int bossChipRow(BossEffect e);                // BlindChips.png 行号

#endif
