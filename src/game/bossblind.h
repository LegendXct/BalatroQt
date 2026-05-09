#ifndef BOSSBLIND_H
#define BOSSBLIND_H

#include <QString>

enum class BossEffect {
    None,
    TheHook,    // 出牌后随机弃 2 张手牌
    TheWall,    // 目标分数 ×2
    TheNeedle,  // 每回合只能出 1 次
    TheClub,    // 所有 ♣ 牌被禁用
    ThePlant,   // 所有 J/Q/K 被禁用
};

struct BossInfo {
    QString name;
    QString description;
};

BossInfo bossInfo(BossEffect e);
BossEffect randomBossEffect();   // 从非 None 里随机

#endif
