#include "consumable.h"
#include "../game/gamestate.h"
#include "../game/handevaluator.h"
#include <QRandomGenerator>

ConsumableKind kindOf(ConsumableType t) {
    return (static_cast<int>(t) < static_cast<int>(ConsumableType::Planet_Pluto))
    ? ConsumableKind::Tarot : ConsumableKind::Planet;
}

// 把 ctx.selectedHandIdx 指到的手牌应用 enhancement，最多 maxN 张
static void enhanceSelected(UseContext &ctx, Enhancement e, int maxN) {
    auto &hand = ctx.state.handMutable();
    int applied = 0;
    for (int idx : ctx.selectedHandIdx) {
        if (applied >= maxN) break;
        if (idx < 0 || idx >= hand.size()) continue;
        hand[idx].enhancement = e;
        applied++;
    }
    ctx.state.notifyHandChanged();
}

static void planetLevelUp(UseContext &ctx, HandType t) {
    ctx.state.levelUpHand(t, 1);
}

Consumable createConsumable(ConsumableType type) {
    Consumable c;
    c.type = type;
    c.kind = kindOf(type);
    c.sellValue = 1;

    switch (type) {
    case ConsumableType::Tarot_Empress:
        c.name = "皇后";   c.description = "把最多 2 张选中手牌变为 Mult 增强";
        c.needsSelection = 1; c.maxSelection = 2;
        c.effect = [](UseContext &ctx) { enhanceSelected(ctx, Enhancement::Mult, 2); };
        break;
    case ConsumableType::Tarot_Hierophant:
        c.name = "教皇";   c.description = "把最多 2 张选中手牌变为 Bonus 增强";
        c.needsSelection = 1; c.maxSelection = 2;
        c.effect = [](UseContext &ctx) { enhanceSelected(ctx, Enhancement::Bonus, 2); };
        break;
    case ConsumableType::Tarot_Chariot:
        c.name = "战车";   c.description = "把 1 张选中手牌变为 Steel 增强";
        c.needsSelection = 1; c.maxSelection = 1;
        c.effect = [](UseContext &ctx) { enhanceSelected(ctx, Enhancement::Steel, 1); };
        break;
    case ConsumableType::Tarot_Lovers:
        c.name = "恋人";   c.description = "把 1 张选中手牌变为 Wild 增强";
        c.needsSelection = 1; c.maxSelection = 1;
        c.effect = [](UseContext &ctx) { enhanceSelected(ctx, Enhancement::Wild, 1); };
        break;
    case ConsumableType::Tarot_Tower:
        c.name = "塔";     c.description = "把 1 张选中手牌变为 Stone 增强";
        c.needsSelection = 1; c.maxSelection = 1;
        c.effect = [](UseContext &ctx) { enhanceSelected(ctx, Enhancement::Stone, 1); };
        break;
    case ConsumableType::Tarot_Hermit:
        c.name = "隐者";   c.description = "金币翻倍（最多 +20）";
        c.effect = [](UseContext &ctx) {
            int gain = qMin(ctx.state.gold(), 20);
            ctx.state.addGold(gain);
        };
        break;

    // 行星——除了名字和升级目标都一样
    case ConsumableType::Planet_Pluto:
        c.name = "冥王星";   c.description = "升级【高牌】";
        c.effect = [](UseContext &ctx) { planetLevelUp(ctx, HandType::HighCard); }; break;
    case ConsumableType::Planet_Mercury:
        c.name = "水星";     c.description = "升级【对子】";
        c.effect = [](UseContext &ctx) { planetLevelUp(ctx, HandType::Pair); }; break;
    case ConsumableType::Planet_Uranus:
        c.name = "天王星";   c.description = "升级【两对】";
        c.effect = [](UseContext &ctx) { planetLevelUp(ctx, HandType::TwoPair); }; break;
    case ConsumableType::Planet_Venus:
        c.name = "金星";     c.description = "升级【三条】";
        c.effect = [](UseContext &ctx) { planetLevelUp(ctx, HandType::ThreeOfAKind); }; break;
    case ConsumableType::Planet_Saturn:
        c.name = "土星";     c.description = "升级【顺子】";
        c.effect = [](UseContext &ctx) { planetLevelUp(ctx, HandType::Straight); }; break;
    case ConsumableType::Planet_Jupiter:
        c.name = "木星";     c.description = "升级【同花】";
        c.effect = [](UseContext &ctx) { planetLevelUp(ctx, HandType::Flush); }; break;
    case ConsumableType::Planet_Earth:
        c.name = "地球";     c.description = "升级【葫芦】";
        c.effect = [](UseContext &ctx) { planetLevelUp(ctx, HandType::FullHouse); }; break;
    case ConsumableType::Planet_Mars:
        c.name = "火星";     c.description = "升级【四条】";
        c.effect = [](UseContext &ctx) { planetLevelUp(ctx, HandType::FourOfAKind); }; break;
    case ConsumableType::Planet_Neptune:
        c.name = "海王星";   c.description = "升级【同花顺】";
        c.effect = [](UseContext &ctx) { planetLevelUp(ctx, HandType::StraightFlush); }; break;
    case ConsumableType::Planet_PlanetX:
        c.name = "X 行星";   c.description = "升级【五条】";
        c.effect = [](UseContext &ctx) { planetLevelUp(ctx, HandType::FiveOfAKind); }; break;
    case ConsumableType::Planet_Ceres:
        c.name = "谷神星";   c.description = "升级【同花葫芦】";
        c.effect = [](UseContext &ctx) { planetLevelUp(ctx, HandType::FlushHouse); }; break;
    case ConsumableType::Planet_Eris:
        c.name = "厄里斯";   c.description = "升级【同花五条】";
        c.effect = [](UseContext &ctx) { planetLevelUp(ctx, HandType::FlushFive); }; break;
    }
    return c;
}

ConsumableType randomTarotType() {
    constexpr ConsumableType pool[] = {
        ConsumableType::Tarot_Empress,    ConsumableType::Tarot_Hierophant,
        ConsumableType::Tarot_Chariot,    ConsumableType::Tarot_Lovers,
        ConsumableType::Tarot_Hermit,     ConsumableType::Tarot_Tower,
    };
    int n = int(sizeof(pool)/sizeof(pool[0]));
    return pool[QRandomGenerator::global()->bounded(n)];
}

ConsumableType randomPlanetType() {
    constexpr ConsumableType pool[] = {
        ConsumableType::Planet_Pluto,   ConsumableType::Planet_Mercury,
        ConsumableType::Planet_Uranus,  ConsumableType::Planet_Venus,
        ConsumableType::Planet_Saturn,  ConsumableType::Planet_Jupiter,
        ConsumableType::Planet_Earth,   ConsumableType::Planet_Mars,
        ConsumableType::Planet_Neptune, ConsumableType::Planet_PlanetX,
        ConsumableType::Planet_Ceres,   ConsumableType::Planet_Eris,
    };
    int n = int(sizeof(pool)/sizeof(pool[0]));
    return pool[QRandomGenerator::global()->bounded(n)];
}
