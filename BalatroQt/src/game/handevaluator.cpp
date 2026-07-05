#include "handevaluator.h"
#include <algorithm>

HandResult HandEvaluator::evaluate(const QVector<CardData> &cards, const HandMods &mods) {
    // 石头牌没有点数和花色：不参与牌型判定，只在 GameState 计分阶段作为额外计分牌 +50。
    QVector<CardData> effective;
    effective.reserve(cards.size());
    for (const CardData &c : cards) {
        if (c.enhancement != Enhancement::Stone) effective.append(c);
    }

    std::sort(effective.begin(), effective.end(), [](const CardData &a, const CardData &b) {
        return static_cast<int>(a.rank) > static_cast<int>(b.rank);
    });

    if (effective.isEmpty()) {
        HandResult result;
        result.type = HandType::HighCard;
        result.scoringCards.clear();
        result.name = handTypeName(HandType::HighCard);
        result.chips = Constants::BASE_HIGH_CARD_CHIPS;
        result.mult = Constants::BASE_HIGH_CARD_MULT;
        result.xmult = 1.0;
        return result;
    }

    bool flush = isFlush(effective, mods);
    bool straight = isStraight(effective, mods);
    auto groups = groupByRank(effective);

    QVector<int> counts;
    for (auto &g : groups) counts.append(g.size());
    std::sort(counts.begin(), counts.end(), std::greater<int>());

    while (counts.size() < 5) counts.append(0);

    HandType type;
    QVector<CardData> scoringCards;

    if (flush && counts[0] == 5) {
        type = HandType::FlushFive;
        scoringCards = effective;
    } else if (flush && counts[0] == 3 && counts[1] == 2) {
        type = HandType::FlushHouse;
        scoringCards = effective;
    } else if (counts[0] == 5) {
        type = HandType::FiveOfAKind;
        scoringCards = effective;
    } else if (flush && straight &&
               static_cast<int>(effective[0].rank)
                   == static_cast<int>(Rank::Ace)) {
        type = HandType::RoyalFlush;
        scoringCards = effective;
    } else if (flush && straight) {
        type = HandType::StraightFlush;
        scoringCards = effective;
    } else if (counts[0] == 4) {
        type = HandType::FourOfAKind;
        for (auto &g : groups)
            if (g.size() == 4) scoringCards = g;
    } else if (counts[0] == 3 && counts[1] == 2) {
        type = HandType::FullHouse;
        scoringCards = effective;
    } else if (flush) {
        type = HandType::Flush;
        scoringCards = effective;
    } else if (straight) {
        type = HandType::Straight;
        scoringCards = effective;
    } else if (counts[0] == 3) {
        type = HandType::ThreeOfAKind;
        for (auto &g : groups)
            if (g.size() == 3) scoringCards = g;
    } else if (counts[0] == 2 && counts[1] == 2) {
        type = HandType::TwoPair;
        for (auto &g : groups)
            if (g.size() == 2)
                scoringCards.append(g);
    } else if (counts[0] == 2) {
        type = HandType::Pair;
        for (auto &g : groups)
            if (g.size() == 2) scoringCards = g;
    } else {
        type = HandType::HighCard;
        scoringCards.append(effective[0]);
    }

    // 水花：所有打出的牌（石头牌之外）都参与计分
    if (mods.splash) scoringCards = effective;

    HandResult result;
    result.type = type;
    result.scoringCards = scoringCards;
    result.name = handTypeName(type);

    switch (type) {
    case HandType::HighCard:
        result.chips = Constants::BASE_HIGH_CARD_CHIPS;
        result.mult  = Constants::BASE_HIGH_CARD_MULT; break;
    case HandType::Pair:
        result.chips = Constants::BASE_PAIR_CHIPS;
        result.mult  = Constants::BASE_PAIR_MULT; break;
    case HandType::TwoPair:
        result.chips = Constants::BASE_TWO_PAIR_CHIPS;
        result.mult  = Constants::BASE_TWO_PAIR_MULT; break;
    case HandType::ThreeOfAKind:
        result.chips = Constants::BASE_THREE_CHIPS;
        result.mult  = Constants::BASE_THREE_MULT; break;
    case HandType::Straight:
        result.chips = Constants::BASE_STRAIGHT_CHIPS;
        result.mult  = Constants::BASE_STRAIGHT_MULT; break;
    case HandType::Flush:
        result.chips = Constants::BASE_FLUSH_CHIPS;
        result.mult  = Constants::BASE_FLUSH_MULT; break;
    case HandType::FullHouse:
        result.chips = Constants::BASE_FULL_HOUSE_CHIPS;
        result.mult  = Constants::BASE_FULL_HOUSE_MULT; break;
    case HandType::FourOfAKind:
        result.chips = Constants::BASE_FOUR_CHIPS;
        result.mult  = Constants::BASE_FOUR_MULT; break;
    case HandType::StraightFlush:
        result.chips = Constants::BASE_STRAIGHT_FLUSH_CHIPS;
        result.mult  = Constants::BASE_STRAIGHT_FLUSH_MULT; break;
    case HandType::RoyalFlush:
        result.chips = Constants::BASE_ROYAL_FLUSH_CHIPS;
        result.mult  = Constants::BASE_ROYAL_FLUSH_MULT; break;
    case HandType::FiveOfAKind:
        result.chips = Constants::BASE_FIVE_CHIPS;
        result.mult  = Constants::BASE_FIVE_MULT; break;
    case HandType::FlushHouse:
        result.chips = Constants::BASE_FLUSH_HOUSE_CHIPS;
        result.mult  = Constants::BASE_FLUSH_HOUSE_MULT; break;
    case HandType::FlushFive:
        result.chips = Constants::BASE_FLUSH_FIVE_CHIPS;
        result.mult  = Constants::BASE_FLUSH_FIVE_MULT; break;
    }

    return result;
}

bool HandEvaluator::isFlush(const QVector<CardData> &cards, const HandMods &mods) {
    const int need = mods.fourFingers ? 4 : 5;
    if (cards.size() < need) return false;

    // 涂抹小丑：♥♦归为一色、♠♣归为一色
    auto suitKey = [&](Suit s) -> int {
        if (!mods.smeared) return static_cast<int>(s);
        return (s == Suit::Hearts || s == Suit::Diamonds) ? 0 : 1;
    };

    int cnt[4] = {0, 0, 0, 0};
    int wild = 0;
    for (const CardData &c : cards) {
        if (c.enhancement == Enhancement::Stone) return false;
        if (c.enhancement == Enhancement::Wild) { ++wild; continue; }
        ++cnt[suitKey(c.suit)];
    }
    for (int k = 0; k < 4; ++k)
        if (cnt[k] + wild >= need) return true;
    return false;
}

bool HandEvaluator::isStraight(const QVector<CardData> &sorted, const HandMods &mods) {
    const int need = mods.fourFingers ? 4 : 5;
    if (sorted.size() < need) return false;

    QVector<int> ranks;
    for (const CardData &c : sorted) {
        if (c.enhancement == Enhancement::Stone) continue;   // 跳过石头牌
        ranks.append(static_cast<int>(c.rank));
    }
    std::sort(ranks.begin(), ranks.end(), std::greater<int>());
    ranks.erase(std::unique(ranks.begin(), ranks.end()), ranks.end());
    if (ranks.size() < need) return false;

    // A 可作高(14)亦可作低(1)：有 A 时额外加入 1
    QVector<int> rs = ranks;
    if (ranks.contains(static_cast<int>(Rank::Ace))) {
        rs.append(1);
        std::sort(rs.begin(), rs.end(), std::greater<int>());
        rs.erase(std::unique(rs.begin(), rs.end()), rs.end());
    }

    // 捷径：相邻点数差为 1 或 2 都算连续
    auto okGap = [&](int d) { return d == 1 || (mods.shortcut && d == 2); };

    // 在降序点数表中寻找长度为 need 的连续窗口
    for (int start = 0; start + need <= rs.size(); ++start) {
        bool ok = true;
        for (int i = start; i < start + need - 1; ++i)
            if (!okGap(rs[i] - rs[i+1])) { ok = false; break; }
        if (ok) return true;
    }
    return false;
}

QMap<Rank, QVector<CardData>> HandEvaluator::groupByRank(const QVector<CardData> &cards) {
    QMap<Rank, QVector<CardData>> groups;
    for (const CardData &c : cards) {
        if (c.enhancement == Enhancement::Stone) continue;
        groups[c.rank].append(c);
    }
    return groups;
}

QString HandEvaluator::handTypeName(HandType type) {
    switch (type) {
    case HandType::HighCard:      return "高牌";
    case HandType::Pair:          return "对子";
    case HandType::TwoPair:       return "两对";
    case HandType::ThreeOfAKind:  return "三条";
    case HandType::Straight:      return "顺子";
    case HandType::Flush:         return "同花";
    case HandType::FullHouse:     return "葫芦";
    case HandType::FourOfAKind:   return "四条";
    case HandType::StraightFlush: return "同花顺";
    case HandType::RoyalFlush:    return "皇家同花顺";
    case HandType::FiveOfAKind:   return "五条";
    case HandType::FlushHouse:    return "同花葫芦";
    case HandType::FlushFive:     return "同花五条";
    default:                      return "";
    }
}

HandResult HandEvaluator::preview(const QVector<CardData> &cards, const HandMods &mods)
{
    if (cards.isEmpty()) {
        HandResult r;
        r.type = HandType::HighCard;
        r.chips = 0;
        r.mult  = 0;
        r.name = "";
        r.level = 1;
        r.xmult = 1.0;
        return r;
    }
    // 复用 evaluate 的牌型判定逻辑,只取基础部分
    return evaluate(cards, mods);
}
