#ifndef CONSTANTS_H
#define CONSTANTS_H

namespace Constants {
    // 手牌与出牌
    constexpr int HAND_SIZE = 8; // 每回合手牌数
    constexpr int MAX_PLAY = 5; // 每次最多出牌数
    constexpr int INITIAL_HANDS = 4; // 初始出牌次数
    constexpr int INITIAL_DISCARDS = 3; // 初始弃牌次数

    // 经济
    constexpr int INITIAL_GOLD = 104; // 初始资金（调试）
    constexpr int INTEREST_MAX = 5; // 利息上限（每5金币+1）
    constexpr int HAND_GOLD = 1; // 每次出牌获得金币
    constexpr int WIN_GOLD = 3; // 通过盲注奖励

    // 槽位
    constexpr int MAX_JOKER_SLOTS = 5; // 小丑牌槽位数
    constexpr int MAX_CONSUMABLE_SLOTS = 2; // 消耗牌槽位数

    // 盲注倍率（小盲/大盲/Boss）
    constexpr double SMALL_BLIND_MULT = 1.0;
    constexpr double BIG_BLIND_MULT = 1.5;
    constexpr double BOSS_BLIND_MULT = 2.0;

    // 基础牌型分数 {筹码，倍率}：{chips, mult}
    // 高牌
    constexpr int BASE_HIGH_CARD_CHIPS   = 5;   constexpr int BASE_HIGH_CARD_MULT   = 1;
    // 对子
    constexpr int BASE_PAIR_CHIPS        = 10;  constexpr int BASE_PAIR_MULT        = 2;
    // 两对
    constexpr int BASE_TWO_PAIR_CHIPS    = 20;  constexpr int BASE_TWO_PAIR_MULT    = 2;
    // 三条
    constexpr int BASE_THREE_CHIPS       = 30;  constexpr int BASE_THREE_MULT       = 3;
    // 顺子
    constexpr int BASE_STRAIGHT_CHIPS    = 30;  constexpr int BASE_STRAIGHT_MULT    = 4;
    // 同花
    constexpr int BASE_FLUSH_CHIPS       = 35;  constexpr int BASE_FLUSH_MULT       = 4;
    // 葫芦
    constexpr int BASE_FULL_HOUSE_CHIPS  = 40;  constexpr int BASE_FULL_HOUSE_MULT  = 4;
    // 四条
    constexpr int BASE_FOUR_CHIPS        = 60;  constexpr int BASE_FOUR_MULT        = 7;
    // 同花顺
    constexpr int BASE_STRAIGHT_FLUSH_CHIPS = 100; constexpr int BASE_STRAIGHT_FLUSH_MULT = 8;
    // 皇家同花顺
    constexpr int BASE_ROYAL_FLUSH_CHIPS = 100; constexpr int BASE_ROYAL_FLUSH_MULT = 8;
    // 五条
    constexpr int BASE_FIVE_CHIPS        = 120; constexpr int BASE_FIVE_MULT        = 12;
    // 同花葫芦
    constexpr int BASE_FLUSH_HOUSE_CHIPS = 140; constexpr int BASE_FLUSH_HOUSE_MULT = 14;
    // 同花五条
    constexpr int BASE_FLUSH_FIVE_CHIPS  = 160; constexpr int BASE_FLUSH_FIVE_MULT  = 16;
}

#endif // CONSTANTS_H
