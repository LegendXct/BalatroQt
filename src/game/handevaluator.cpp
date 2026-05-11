#include "handevaluator.h"
#include <algorithm>

HandResult HandEvaluator::evaluate(const QVector<CardData> &cards) {
    QVector <CardData> effective = cards;
    std::sort(effective.begin(), effective.end(), [](const CardData &a, const CardData &b) {
        return static_cast<int>(a.rank) > static_cast<int>(b.rank);
    });
    bool flush = isFlush(effective);
    bool straight = isStraight(effective);
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

bool HandEvaluator::isFlush(const QVector<CardData> &cards) {
    if (cards.size() < 5) return false;

    Suit refSuit; // 第一张非万能花色
    bool foundRef = false;
    for (const CardData &c : cards) {
        if (c.enhancement == Enhancement::Wild) continue;
        if (c.enhancement == Enhancement::Stone) return false;
        if (!foundRef) {
            refSuit = c.suit;
            foundRef = true;
        }
        else if (c.suit != refSuit) return false;
    }
    return true;
}

bool HandEvaluator::isStraight(const QVector<CardData> &sorted) {
    if (sorted.size() < 5) return false;

    QVector<int> ranks;
    for (const CardData &c : sorted) {
        // 跳过石头牌
        if (c.enhancement == Enhancement::Stone) continue;
        ranks.append(static_cast<int>(c.rank));
    }
    ranks.erase(std::unique(ranks.begin(), ranks.end()), ranks.end());
    if (ranks.size() < 5) return false;

    bool normal = true;
    for (int i = 0; i < 4; i++)
        if (ranks[i] - ranks[i+1] != 1) {
            normal = false; break;
        }
    if (normal) return true;

    if (ranks[0] == static_cast<int>(Rank::Ace)) {
        QVector<int> low = ranks.mid(1);
        low.append(1);
        std::sort(low.begin(), low.end(), std::greater<int>());
        bool lowStraight = true;
        for (int i = 0; i < 4; i++)
            if (low[i] - low[i+1] != 1) {
                lowStraight = false;
                break;
            }
        return lowStraight;
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

HandResult HandEvaluator::preview(const QVector<CardData> &cards)
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
    return evaluate(cards);
}
