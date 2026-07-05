#ifndef CARDDATA_H
#define CARDDATA_H

#include <QString>
#include <atomic>


inline int nextCardUid()
{
    static std::atomic<int> counter{1};
    return counter.fetch_add(1, std::memory_order_relaxed);
}

// 花色
enum class Suit {
    Spades, // 黑桃
    Hearts, // 红心
    Diamonds, // 方块
    Clubs // 梅花
};

// 点数
enum class Rank {
    Two = 2, Three, Four, Five, Six, Seven,
    Eight, Nine, Ten, Jack, Queen, King, Ace
};

// 迭代器增强（程设扩展）的点数递推：每次打出后 +1，K→A→2 回绕
// （区别于"力量"塔罗的 nextRank：那个 A 封顶）。
// 模型(finalizePlayedHand)与 UI(计分后的翻面演出)共用，规则只此一份。
inline Rank iterNextRank(Rank r)
{
    return (r == Rank::Ace) ? Rank::Two : static_cast<Rank>(static_cast<int>(r) + 1);
}

// 增强类型
enum class Enhancement {
    None,
    Bonus, // +30 筹码
    Mult, // +4 倍率
    Wild, // 可充当任意花色
    Glass, // ×2 倍率，1/4 概率破碎
    Steel, // 持有时 ×1.5 倍率
    Stone, // +50 筹码，无花色，无点数
    Gold, // 回合结束 +3 金币
    Lucky, // 1/5概率 +20 倍率，1/15概率 +20 金币
    Iterator // 程设扩展：每次打出后点数 +1（K→A，A→2），无计分效果
};

// 版本
enum class Edition {
    None,
    Foil, // +50 筹码
    Holographic, // +10 倍率
    Polychrome, // ×1.5 倍率
    Negative // 增加一个小丑槽位
};

enum class Seal {
    None,
    Gold, // 出牌后 +3 金币
    Red, // 重新触发一次
    Blue, // 回合结束后生成星球牌
    Purple // 弃牌时生成塔罗牌
};

class CardData
{
public:
    Suit suit;
    Rank rank;
    bool faceUp = true; // 是否正面朝上
    Enhancement enhancement = Enhancement::None;
    Edition edition = Edition::None;
    Seal seal = Seal::None;
    bool isDebuffed = false;
    int permanentBonusChips = 0; // Hiker/徒步者等给这张牌的永久筹码
    int uid = nextCardUid();

    void assignNewUid() { uid = nextCardUid(); }

    int chipValue() const {
        int r = static_cast<int>(rank);
        if (rank == Rank::Ace) return 11;
        if (r >= static_cast<int>(Rank::Jack)) return 10;
        return r;
    }

    QString toString() const {
        const QString ranks[] = {
            "", "", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K", "A"
        };
        const QString suits[] = {"♠","♥","♦","♣"};
        return ranks[static_cast<int>(rank)]+suits[static_cast<int>(suit)];
    }
};

#endif // CARDDATA_H
