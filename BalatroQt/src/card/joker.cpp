#include "joker.h"
#include "../game/handevaluator.h"
#include "../game/gamestate.h"
#include <QRandomGenerator>
#include <QtGlobal>
#include <climits>
#include <QSet>

// 花色匹配（对齐原版 Card:is_suit）：石头牌无花色、百搭牌算任意花色、被禁用牌不计；
// "模糊小丑"(Smeared)把红/黑同色视为同花色。flushCalc=true 用于同花/黑板类计算
// （被禁用的普通牌仍按花色参与，但被禁用的百搭牌不算），bypassDebuff 绕过单牌禁用。
static bool sameSmearedColor(Suit a, Suit b)
{
    const bool aRed = (a == Suit::Hearts || a == Suit::Diamonds);
    const bool bRed = (b == Suit::Hearts || b == Suit::Diamonds);
    return aRed == bRed;
}

static bool cardIsSuitLikeOriginal(const CardData &card,
                                   Suit suit,
                                   bool bypassDebuff,
                                   bool flushCalc,
                                   bool smeared)
{
    if (!flushCalc && card.isDebuffed && !bypassDebuff) return false;
    if (card.enhancement == Enhancement::Stone) return false;
    if (card.enhancement == Enhancement::Wild) {
        if (flushCalc && card.isDebuffed) return false;
        return true;
    }
    if (smeared && sameSmearedColor(card.suit, suit)) return true;
    return card.suit == suit;
}

static void fillFirstMatchingSuitLikeOriginal(QSet<int> &suits,
                                              const CardData &card,
                                              const QVector<Suit> &order,
                                              bool bypassDebuff,
                                              bool flushCalc,
                                              bool smeared)
{
    for (Suit suit : order) {
        const int key = static_cast<int>(suit);
        if (!suits.contains(key)
            && cardIsSuitLikeOriginal(card, suit, bypassDebuff, flushCalc, smeared)) {
            suits.insert(key);
            return;
        }
    }
}

// 待办清单可指定的牌型（不含五条/同花五条等特殊牌型）。
static QVector<HandType> toDoListVisibleHands()
{
    return {
        HandType::HighCard, HandType::Pair, HandType::TwoPair, HandType::ThreeOfAKind,
        HandType::Straight, HandType::Flush, HandType::FullHouse, HandType::FourOfAKind,
        HandType::StraightFlush
    };
}

static HandType randomToDoListHand(HandType exclude = HandType::FlushFive)
{
    QVector<HandType> pool;
    for (HandType hand : toDoListVisibleHands())
        if (hand != exclude) pool.append(hand);
    return pool.at(QRandomGenerator::global()->bounded(pool.size()));
}

int jokerBaseCost(JokerType t)
{
    switch (t) {
    case JokerType::Joker: return 2;
    case JokerType::JollyJoker:
    case JokerType::SlyJoker: return 3;
    case JokerType::ZanyJoker:
    case JokerType::MadJoker:
    case JokerType::CrazyJoker:
    case JokerType::DrollJoker:
    case JokerType::WilyJoker:
    case JokerType::CleverJoker:
    case JokerType::DeviousJoker:
    case JokerType::CraftyJoker:
    case JokerType::Misprint:
    case JokerType::ToDoList:
    case JokerType::EvenSteven:
    case JokerType::OddTodd:
    case JokerType::Scholar: return 4;
    case JokerType::GreedyJoker:
    case JokerType::LustyJoker:
    case JokerType::WrathfulJoker:
    case JokerType::GluttonousJoker:
    case JokerType::HalfJoker:
    case JokerType::Banner:
    case JokerType::MysticSummit:
    case JokerType::RaisedFist: return 5;
    case JokerType::GoldenJoker:
    case JokerType::Bull: return 6;
    case JokerType::Bootstraps: return 7;
    case JokerType::Fibonacci: return 8;
    case JokerType::AbstractJoker: return 4;
    case JokerType::Supernova: return 5;
    case JokerType::GrosMichel: return 5;
    case JokerType::Cavendish: return 4;
    case JokerType::IceCream: return 5;
    case JokerType::Stuntman: return 7;
    case JokerType::TheDuo: return 8;
    case JokerType::TheTrio: return 8;
    case JokerType::TheFamily: return 8;
    case JokerType::TheOrder: return 8;
    case JokerType::TheTribe: return 8;
    case JokerType::Blackboard: return 6;
    case JokerType::ScaryFace: return 4;
    case JokerType::SmileyFace: return 4;
    case JokerType::WalkieTalkie: return 4;
    case JokerType::Arrowhead: return 7;
    case JokerType::OnyxAgate: return 7;
    case JokerType::RoughGem: return 7;
    case JokerType::Bloodstone: return 7;
    case JokerType::ShootTheMoon: return 5;
    case JokerType::Baron: return 8;
    case JokerType::FlowerPot: return 6;
    case JokerType::Acrobat: return 6;
    case JokerType::Swashbuckler: return 4;
    case JokerType::Ramen: return 6;
    case JokerType::DriversLicense: return 7;
    case JokerType::Hiker: return 5;
    case JokerType::CardSharp: return 6;
    case JokerType::Hologram: return 7;
    case JokerType::MidasMask: return 7;
    case JokerType::Vampire: return 7;
    case JokerType::Constellation: return 6;
    case JokerType::Photograph: return 5;
    case JokerType::HangingChad: return 4;
    case JokerType::SockAndBuskin: return 6;
    case JokerType::Mime: return 5;
    case JokerType::DNA: return 8;
    case JokerType::Blueprint:
    case JokerType::Brainstorm: return 10;
    case JokerType::Caino:
    case JokerType::Triboulet:
    case JokerType::Yorick:
    case JokerType::Chicot:
    case JokerType::Perkeo: return 20;
    case JokerType::JokerStencil: return 8;
    case JokerType::OperatorOverload:
    case JokerType::ClassTemplate: return 8;
    case JokerType::SteelJoker:   return 7;
    case JokerType::StoneJoker:   return 6;
    case JokerType::BlueJoker:    return 5;
    case JokerType::Erosion:      return 6;
    case JokerType::BusinessCard: return 4;
    case JokerType::FacelessJoker:return 4;
    case JokerType::Cloud9:       return 7;
    case JokerType::GoldenTicket: return 5;
    case JokerType::SeeingDouble: return 6;
    case JokerType::SquareJoker:  return 4;
    case JokerType::Runner:       return 5;
    case JokerType::Castle:       return 6;
    case JokerType::GreenJoker:   return 4;
    case JokerType::Obelisk:      return 8;
    case JokerType::RideTheBus:   return 6;
    case JokerType::SpareTrousers:return 6;
    case JokerType::WeeJoker:     return 8;
    case JokerType::HitTheRoad:   return 8;
    case JokerType::GlassJoker:   return 6;
    case JokerType::LuckyCat:     return 6;
    case JokerType::Popcorn:      return 5;
    case JokerType::Juggler:      return 4;
    case JokerType::Drunkard:     return 4;
    case JokerType::MerryAndy:    return 7;
    case JokerType::Troubadour:   return 6;
    case JokerType::DelayedGratification: return 4;
    case JokerType::ToTheMoon:    return 5;
    case JokerType::ReservedParking:      return 6;
    case JokerType::MailInRebate: return 4;
    case JokerType::AncientJoker: return 8;
    case JokerType::TheIdol:      return 6;
    case JokerType::SpaceJoker:   return 5;
    case JokerType::Hack:         return 6;
    case JokerType::RiffRaff:     return 6;
    case JokerType::MarbleJoker:  return 6;
    case JokerType::Burglar:      return 6;
    case JokerType::Cartomancer:  return 6;
    case JokerType::Certificate:  return 6;
    case JokerType::Madness:      return 7;
    case JokerType::EightBall:    return 5;
    case JokerType::Seance:       return 6;
    case JokerType::Vagabond:     return 8;
    case JokerType::Superposition:return 4;
    case JokerType::FlashCard:    return 5;
    case JokerType::Throwback:    return 6;
    case JokerType::Campfire:     return 9;
    case JokerType::FortuneTeller:return 6;
    case JokerType::LoyaltyCard:  return 5;
    case JokerType::Egg:          return 4;
    case JokerType::Rocket:       return 6;
    case JokerType::Satellite:    return 6;
    case JokerType::GiftCard:     return 6;
    case JokerType::Shortcut:     return 7;
    case JokerType::SmearedJoker: return 7;
    case JokerType::Splash:       return 3;
    case JokerType::Showman:      return 5;
    case JokerType::Dusk:            return 5;
    case JokerType::CeremonialDagger:return 6;
    case JokerType::TurtleBean:      return 6;
    case JokerType::Seltzer:         return 6;
    case JokerType::BurntJoker:      return 8;
    case JokerType::ChaosTheClown:   return 4;
    case JokerType::Pareidolia:      return 5;
    case JokerType::Hallucination:   return 4;
    case JokerType::Luchador:        return 5;
    case JokerType::InvisibleJoker:  return 8;
    case JokerType::CreditCard:      return 1;
    case JokerType::MrBones:         return 5;
    case JokerType::DietCola:        return 6;
    case JokerType::FourFingers:     return 7;
    case JokerType::OopsAllSixes:    return 4;
    case JokerType::SixthSense:      return 6;
    case JokerType::RedCard:         return 5;
    case JokerType::BaseballCard:    return 8;
    case JokerType::TradingCard:     return 6;
    case JokerType::Matador:         return 7;
    case JokerType::Astronomer:      return 8;
    }
    return 4;
}

bool jokerEternalCompatible(JokerType t)
{
    switch (t) {
    case JokerType::GrosMichel:
    case JokerType::IceCream:
    case JokerType::Cavendish:
    case JokerType::Luchador:
    case JokerType::TurtleBean:
    case JokerType::DietCola:
    case JokerType::Popcorn:
    case JokerType::Ramen:
    case JokerType::Seltzer:
    case JokerType::MrBones:
    case JokerType::InvisibleJoker:
        return false;
    default:
        return true;
    }
}

bool jokerPerishableCompatible(JokerType t)
{
    switch (t) {
    case JokerType::CeremonialDagger:
    case JokerType::RideTheBus:
    case JokerType::Runner:
    case JokerType::Constellation:
    case JokerType::GreenJoker:
    case JokerType::RedCard:
    case JokerType::Madness:
    case JokerType::SquareJoker:
    case JokerType::Vampire:
    case JokerType::Hologram:
    case JokerType::Rocket:
    case JokerType::Obelisk:
    case JokerType::LuckyCat:
    case JokerType::FlashCard:
    case JokerType::SpareTrousers:
    case JokerType::Castle:
    case JokerType::GlassJoker:
    case JokerType::WeeJoker:
        return false;
    default:
        return true;
    }
}

// 每张小丑的 spawn 池档位（原版 game.lua 里的 rarity 字段，1/2/3/4 = 普通/罕见/稀有/传奇）。
// 影响商店概率、价格基数、info 浮窗底部 pill 配色。未列出的类型默认 Common（rarity=1）。
JokerRarity jokerRarity(JokerType t) {
    switch (t) {
    // Uncommon
    case JokerType::JokerStencil:
    case JokerType::FourFingers:
    case JokerType::Mime:
    case JokerType::CeremonialDagger:
    case JokerType::MarbleJoker:
    case JokerType::LoyaltyCard:
    case JokerType::Dusk:
    case JokerType::Fibonacci:
    case JokerType::SteelJoker:
    case JokerType::Hack:
    case JokerType::Pareidolia:
    case JokerType::SpaceJoker:
    case JokerType::Burglar:
    case JokerType::Blackboard:
    case JokerType::Constellation:
    case JokerType::Hiker:
    case JokerType::CardSharp:
    case JokerType::Madness:
    case JokerType::Seance:
    case JokerType::Vampire:
    case JokerType::Shortcut:
    case JokerType::Hologram:
    case JokerType::Cloud9:
    case JokerType::Rocket:
    case JokerType::MidasMask:
    case JokerType::Luchador:
    case JokerType::GiftCard:
    case JokerType::TurtleBean:
    case JokerType::Erosion:
    case JokerType::ToTheMoon:
    case JokerType::StoneJoker:
    case JokerType::LuckyCat:
    case JokerType::Bull:
    case JokerType::DietCola:
    case JokerType::FlashCard:
    case JokerType::SpareTrousers:
    case JokerType::Ramen:
    case JokerType::Seltzer:
    case JokerType::Castle:
    case JokerType::MrBones:
    case JokerType::Acrobat:
    case JokerType::SockAndBuskin:
    case JokerType::Troubadour:
    case JokerType::Certificate:
    case JokerType::SmearedJoker:
    case JokerType::Throwback:
    case JokerType::RoughGem:
    case JokerType::Bloodstone:
    case JokerType::Arrowhead:
    case JokerType::OnyxAgate:
    case JokerType::GlassJoker:
    case JokerType::Showman:
    case JokerType::FlowerPot:
    case JokerType::MerryAndy:
    case JokerType::OopsAllSixes:
    case JokerType::TheIdol:
    case JokerType::SeeingDouble:
    case JokerType::Satellite:
    case JokerType::Cartomancer:
    case JokerType::Bootstraps:
    case JokerType::SixthSense:
    case JokerType::TradingCard:
    case JokerType::Matador:
    case JokerType::Astronomer:
        return JokerRarity::Uncommon;
    // Rare
    case JokerType::DNA:
    case JokerType::Vagabond:
    case JokerType::Baron:
    case JokerType::Obelisk:
    case JokerType::AncientJoker:
    case JokerType::Campfire:
    case JokerType::Blueprint:
    case JokerType::WeeJoker:
    case JokerType::HitTheRoad:
    case JokerType::TheDuo:
    case JokerType::TheTrio:
    case JokerType::TheFamily:
    case JokerType::TheOrder:
    case JokerType::TheTribe:
    case JokerType::Stuntman:
    case JokerType::InvisibleJoker:
    case JokerType::Brainstorm:
    case JokerType::DriversLicense:
    case JokerType::BurntJoker:
    case JokerType::BaseballCard:
    case JokerType::OperatorOverload:
    case JokerType::ClassTemplate:
        return JokerRarity::Rare;
    // Legendary
    case JokerType::Caino:
    case JokerType::Triboulet:
    case JokerType::Yorick:
    case JokerType::Chicot:
    case JokerType::Perkeo:
        return JokerRarity::Legendary;
    default:
        return JokerRarity::Common;
    }
}

bool jokerBlueprintCompatible(JokerType t)
{
    switch (t) {
    case JokerType::FourFingers:
    case JokerType::CreditCard:
    case JokerType::ChaosTheClown:
    case JokerType::DelayedGratification:
    case JokerType::Pareidolia:
    case JokerType::Egg:
    case JokerType::Splash:
    case JokerType::SixthSense:
    case JokerType::Shortcut:
    case JokerType::Cloud9:
    case JokerType::Rocket:
    case JokerType::MidasMask:
    case JokerType::GiftCard:
    case JokerType::TurtleBean:
    case JokerType::ToTheMoon:
    case JokerType::Juggler:
    case JokerType::Drunkard:
    case JokerType::GoldenJoker:
    case JokerType::TradingCard:
    case JokerType::MrBones:
    case JokerType::Troubadour:
    case JokerType::SmearedJoker:
    case JokerType::Showman:
    case JokerType::MerryAndy:
    case JokerType::OopsAllSixes:
    case JokerType::InvisibleJoker:
    case JokerType::Satellite:
    case JokerType::Astronomer:
    case JokerType::Chicot:
    case JokerType::OperatorOverload:   // 全局事件流变换，复制无意义
    case JokerType::ClassTemplate:      // 自带实例化状态，复制语义不成立
        return false;
    default:
        return true;
    }
}

Joker createJoker(JokerType type) {
    Joker j;
    j.type = type;

    switch (type) {

    case JokerType::Joker:
        j.name = "小丑";
        j.description = "+4 倍率";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &ctx) {
            ctx.result.mult += 4;
        };
        break;

    case JokerType::GreedyJoker:
        j.name = "贪婪小丑";
        j.description = "每张♦计分牌 +3 倍率";
        j.timing = TriggerTiming::OnScoringCard;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.currentCard
                && cardIsSuitLikeOriginal(*ctx.currentCard, Suit::Diamonds, false, false,
                                          ctx.state.hasJokerType(JokerType::SmearedJoker)))
                ctx.result.mult += 3;
        };
        break;

    case JokerType::LustyJoker:
        j.name = "色欲小丑";
        j.description = "每张♥计分牌 +3 倍率";
        j.timing = TriggerTiming::OnScoringCard;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.currentCard
                && cardIsSuitLikeOriginal(*ctx.currentCard, Suit::Hearts, false, false,
                                          ctx.state.hasJokerType(JokerType::SmearedJoker)))
                ctx.result.mult += 3;
        };
        break;

    case JokerType::WrathfulJoker:
        j.name = "愤怒小丑";
        j.description = "每张♠计分牌 +3 倍率";
        j.timing = TriggerTiming::OnScoringCard;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.currentCard
                && cardIsSuitLikeOriginal(*ctx.currentCard, Suit::Spades, false, false,
                                          ctx.state.hasJokerType(JokerType::SmearedJoker)))
                ctx.result.mult += 3;
        };
        break;

    case JokerType::GluttonousJoker:
        j.name = "暴食小丑";
        j.description = "每张♣计分牌 +3 倍率";
        j.timing = TriggerTiming::OnScoringCard;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.currentCard
                && cardIsSuitLikeOriginal(*ctx.currentCard, Suit::Clubs, false, false,
                                          ctx.state.hasJokerType(JokerType::SmearedJoker)))
                ctx.result.mult += 3;
        };
        break;

    case JokerType::HalfJoker:
        j.name = "半张小丑";
        j.description = "出牌≤3张时 +20 倍率";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            // 原版按本次打出的全部牌张数判断，而非计分牌数。
            if (ctx.playedCards && ctx.playedCards->size() <= 3)
                ctx.result.mult += 20;
        };
        break;

    case JokerType::JollyJoker:
        j.name = "开心小丑";
        j.description = "出对子 +8 倍率";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.result.type == HandType::Pair)
                ctx.result.mult += 8;
        };
        break;

    case JokerType::ZanyJoker:
        j.name = "古怪小丑";
        j.description = "出三条 +12 倍率";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.result.type == HandType::ThreeOfAKind)
                ctx.result.mult += 12;
        };
        break;

    case JokerType::MadJoker:
        j.name = "疯狂小丑";
        j.description = "出两对 +10 倍率";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.result.type == HandType::TwoPair)
                ctx.result.mult += 10;
        };
        break;

    case JokerType::CrazyJoker:
        j.name = "狂野小丑";
        j.description = "出顺子 +12 倍率";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.result.type == HandType::Straight)
                ctx.result.mult += 12;
        };
        break;

    case JokerType::DrollJoker:
        j.name = "滑稽小丑";
        j.description = "出同花 +10 倍率";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.result.type == HandType::Flush)
                ctx.result.mult += 10;
        };
        break;

    case JokerType::GoldenJoker:
        j.name = "黄金小丑";
        j.description = "回合结束 +4 金币";
        j.timing = TriggerTiming::OnRoundEnd;
        j.effect = [](TriggerContext &ctx) {
            ctx.state.addGold(4);
        };
        break;

    case JokerType::ToDoList:
        j.name = "待办清单";
        j.description = "打出指定牌型时，获得 $4（牌型每回合变化）";
        j.timing = TriggerTiming::OnPlayedHand;
        j.counter = static_cast<int>(randomToDoListHand());   // 当前目标牌型存于 counter
        j.effect = [](TriggerContext &) {};   // 由 applyResolvedJokerEffect() 比对 counter 处理
        break;
    case JokerType::SlyJoker:
        j.name = "奸诈小丑"; j.description = "出对子 +50 筹码";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.result.type == HandType::Pair) ctx.result.chips += 50;
        };
        break;

    case JokerType::WilyJoker:
        j.name = "狡猾小丑"; j.description = "出三条 +100 筹码";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.result.type == HandType::ThreeOfAKind) ctx.result.chips += 100;
        };
        break;

    case JokerType::CleverJoker:
        j.name = "聪敏小丑"; j.description = "出两对 +80 筹码";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.result.type == HandType::TwoPair) ctx.result.chips += 80;
        };
        break;

    case JokerType::DeviousJoker:
        j.name = "阴险小丑"; j.description = "出顺子 +100 筹码";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.result.type == HandType::Straight) ctx.result.chips += 100;
        };
        break;

    case JokerType::CraftyJoker:
        j.name = "精明小丑"; j.description = "出同花 +80 筹码";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.result.type == HandType::Flush) ctx.result.chips += 80;
        };
        break;

    // ─── 杂项倍率 ───────────────────────────
    case JokerType::Banner:
        j.name = "旗帜"; j.description = "每剩余弃牌 +30 筹码";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &ctx) {
            ctx.result.chips += 30 * ctx.state.discardLeft();
        };
        break;

    case JokerType::MysticSummit:
        j.name = "神秘之峰"; j.description = "0 弃牌时 +15 倍率";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.state.discardLeft() == 0) ctx.result.mult += 15;
        };
        break;

    case JokerType::Misprint:
        j.name = "印错小丑"; j.description = "+0~23 随机倍率";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &ctx) {
            ctx.result.mult += QRandomGenerator::global()->bounded(24);
        };
        break;

    case JokerType::RaisedFist:
        j.name = "致胜之拳"; j.description = "手牌中最低牌点数 ×2 加倍率";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &ctx) {
            int low = INT_MAX;
            for (const CardData &c : ctx.hand) {
                if (c.enhancement == Enhancement::Stone) continue;
                int v = c.chipValue();
                if (v < low) low = v;
            }
            if (low != INT_MAX) ctx.result.mult += low * 2;
        };
        break;

    // ─── 计分牌触发 ─────────────────────────
    case JokerType::Fibonacci:
        j.name = "斐波那契"; j.description = "A/2/3/5/8 计分牌 +8 倍率";
        j.timing = TriggerTiming::OnScoringCard;
        j.effect = [](TriggerContext &ctx) {
            if (!ctx.currentCard || ctx.currentCard->isDebuffed) return;
            Rank r = ctx.currentCard->rank;
            if (r == Rank::Ace || r == Rank::Two || r == Rank::Three
                || r == Rank::Five || r == Rank::Eight)
                ctx.result.mult += 8;
        };
        break;

    case JokerType::EvenSteven:
        j.name = "偶数史蒂文"; j.description = "偶数计分牌 +4 倍率";
        j.timing = TriggerTiming::OnScoringCard;
        j.effect = [](TriggerContext &ctx) {
            if (!ctx.currentCard || ctx.currentCard->isDebuffed) return;
            int rv = static_cast<int>(ctx.currentCard->rank);
            if (rv >= 2 && rv <= 10 && rv % 2 == 0) ctx.result.mult += 4;
        };
        break;

    case JokerType::OddTodd:
        j.name = "奇数托德"; j.description = "奇数计分牌 +31 筹码";
        j.timing = TriggerTiming::OnScoringCard;
        j.effect = [](TriggerContext &ctx) {
            if (!ctx.currentCard || ctx.currentCard->isDebuffed) return;
            int rv = static_cast<int>(ctx.currentCard->rank);
            // 奇数：A/3/5/7/9
            if (ctx.currentCard->rank == Rank::Ace
                || rv == 3 || rv == 5 || rv == 7 || rv == 9)
                ctx.result.chips += 31;
        };
        break;

    case JokerType::Scholar:
        j.name = "学者"; j.description = "A 计分牌 +20 筹码 +4 倍率";
        j.timing = TriggerTiming::OnScoringCard;
        j.effect = [](TriggerContext &ctx) {
            if (!ctx.currentCard || ctx.currentCard->isDebuffed) return;
            if (ctx.currentCard->rank == Rank::Ace) {
                ctx.result.chips += 20;
                ctx.result.mult  += 4;
            }
        };
        break;

    // ─── 经济联动 ───────────────────────────
    case JokerType::Bull:
        j.name = "斗牛"; j.description = "每金币 +2 筹码";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &ctx) {
            ctx.result.chips += 2 * qMax(0, ctx.state.gold());
        };
        break;

    case JokerType::Bootstraps:
        j.name = "提靴带"; j.description = "每 5 金币 +2 倍率";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &ctx) {
            ctx.result.mult += 2 * (qMax(0, ctx.state.gold()) / 5);
        };
        break;


    // ─── Stage 6.2：补充强力/常用小丑 ─────────────────────────
    case JokerType::AbstractJoker:
        j.name = "抽象小丑"; j.description = "每张小丑牌 +3 倍率";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &ctx) {
            ctx.result.mult += 3 * ctx.state.jokers().size();
        };
        break;

    case JokerType::Supernova:
        j.name = "超新星"; j.description = "根据本牌型已出次数增加倍率";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            auto it = ctx.state.handLevels().constFind(ctx.result.type);
            // +1：本牌型的 played 计数要到 finalizePlayedHand 才自增，这里手动把当前这手算进去。
            if (it != ctx.state.handLevels().constEnd()) ctx.result.mult += it->played + 1;
        };
        break;

    case JokerType::GrosMichel:
        j.name = "大麦克香蕉"; j.description = "+15 倍率；回合结束 1/6 概率灭绝，并解锁卡文迪许";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &ctx) { ctx.result.mult += 15; };
        break;

    case JokerType::Cavendish:
        j.name = "卡文迪什"; j.description = "×3 倍率；回合结束 1/1000 概率消失";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &ctx) { ctx.result.xmult *= 3.0; };
        break;

    case JokerType::IceCream:
        j.name = "冰淇淋"; j.description = "+100 筹码，每次出牌后 -5 筹码";
        j.counter = 100;
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &ctx) { ctx.result.chips += 100; }; // 实际数值由 GameState 根据 counter 覆盖
        break;

    case JokerType::Stuntman:
        j.name = "特技演员"; j.description = "+250 筹码，手牌上限 -2";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &ctx) { ctx.result.chips += 250; };
        break;

    case JokerType::TheDuo:
        j.name = "二重奏"; j.description = "若出牌含对子，×2 倍率";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            // 含对子的牌型：三条/葫芦/四条/五条等内部都存在对子。
            if (ctx.result.type == HandType::Pair || ctx.result.type == HandType::TwoPair ||
                ctx.result.type == HandType::ThreeOfAKind || ctx.result.type == HandType::FullHouse ||
                ctx.result.type == HandType::FourOfAKind || ctx.result.type == HandType::FiveOfAKind ||
                ctx.result.type == HandType::FlushHouse || ctx.result.type == HandType::FlushFive)
                ctx.result.xmult *= 2.0;
        };
        break;

    case JokerType::TheTrio:
        j.name = "三重奏"; j.description = "若出牌含三条，×3 倍率";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.result.type == HandType::ThreeOfAKind || ctx.result.type == HandType::FullHouse ||
                ctx.result.type == HandType::FourOfAKind || ctx.result.type == HandType::FiveOfAKind ||
                ctx.result.type == HandType::FlushHouse || ctx.result.type == HandType::FlushFive)
                ctx.result.xmult *= 3.0;
        };
        break;

    case JokerType::TheFamily:
        j.name = "一家人"; j.description = "若出牌含四条，×4 倍率";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.result.type == HandType::FourOfAKind || ctx.result.type == HandType::FiveOfAKind ||
                ctx.result.type == HandType::FlushFive)
                ctx.result.xmult *= 4.0;
        };
        break;

    case JokerType::TheOrder:
        j.name = "秩序"; j.description = "若出顺子，×3 倍率";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.result.type == HandType::Straight || ctx.result.type == HandType::StraightFlush ||
                ctx.result.type == HandType::RoyalFlush)
                ctx.result.xmult *= 3.0;
        };
        break;

    case JokerType::TheTribe:
        j.name = "部落"; j.description = "若出同花，×2 倍率";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.result.type == HandType::Flush || ctx.result.type == HandType::StraightFlush ||
                ctx.result.type == HandType::RoyalFlush || ctx.result.type == HandType::FlushHouse ||
                ctx.result.type == HandType::FlushFive)
                ctx.result.xmult *= 2.0;
        };
        break;

    case JokerType::Blackboard:
        j.name = "黑板"; j.description = "手牌中只含♠/♣时，×3 倍率";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &ctx) {
            // 原版黑板：手牌中所有牌都必须是♠/♣（空手牌也算满足）。
            // flushCalc=true：被禁用的普通牌仍按花色参与，石头牌破坏条件，模糊小丑放宽到同色。
            const bool smeared = ctx.state.hasJokerType(JokerType::SmearedJoker);
            bool ok = true;
            for (const CardData &c : ctx.hand) {
                if (!cardIsSuitLikeOriginal(c, Suit::Clubs, false, true, smeared)
                    && !cardIsSuitLikeOriginal(c, Suit::Spades, false, true, smeared)) {
                    ok = false;
                    break;
                }
            }
            if (ok) ctx.result.xmult *= 3.0;
        };
        break;

    case JokerType::ScaryFace:
        j.name = "恐怖面孔"; j.description = "每张人头计分牌 +30 筹码";
        j.timing = TriggerTiming::OnScoringCard;
        j.effect = [](TriggerContext &ctx) {
            if (!ctx.currentCard || ctx.currentCard->isDebuffed) return;
            if (ctx.state.isFaceCard(*ctx.currentCard)) ctx.result.chips += 30;
        };
        break;

    case JokerType::SmileyFace:
        j.name = "微笑表情"; j.description = "每张人头计分牌 +5 倍率";
        j.timing = TriggerTiming::OnScoringCard;
        j.effect = [](TriggerContext &ctx) {
            if (!ctx.currentCard || ctx.currentCard->isDebuffed) return;
            if (ctx.state.isFaceCard(*ctx.currentCard)) ctx.result.mult += 5;
        };
        break;

    case JokerType::WalkieTalkie:
        j.name = "对讲机"; j.description = "每张10或4计分牌 +10 筹码 +4 倍率";
        j.timing = TriggerTiming::OnScoringCard;
        j.effect = [](TriggerContext &ctx) {
            if (!ctx.currentCard || ctx.currentCard->isDebuffed) return;
            if (ctx.currentCard->rank == Rank::Ten || ctx.currentCard->rank == Rank::Four) {
                ctx.result.chips += 10;
                ctx.result.mult += 4;
            }
        };
        break;

    case JokerType::Arrowhead:
        j.name = "箭头"; j.description = "每张♠计分牌 +50 筹码";
        j.timing = TriggerTiming::OnScoringCard;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.currentCard
                && cardIsSuitLikeOriginal(*ctx.currentCard, Suit::Spades, false, false,
                                          ctx.state.hasJokerType(JokerType::SmearedJoker)))
                ctx.result.chips += 50;
        };
        break;

    case JokerType::OnyxAgate:
        j.name = "缟玛瑙"; j.description = "每张♣计分牌 +7 倍率";
        j.timing = TriggerTiming::OnScoringCard;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.currentCard
                && cardIsSuitLikeOriginal(*ctx.currentCard, Suit::Clubs, false, false,
                                          ctx.state.hasJokerType(JokerType::SmearedJoker)))
                ctx.result.mult += 7;
        };
        break;

    case JokerType::RoughGem:
        j.name = "璞玉"; j.description = "每张♦计分牌 +$1";
        j.timing = TriggerTiming::OnScoringCard;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.currentCard
                && cardIsSuitLikeOriginal(*ctx.currentCard, Suit::Diamonds, false, false,
                                          ctx.state.hasJokerType(JokerType::SmearedJoker)))
                ctx.state.addGold(1);
        };
        break;

    case JokerType::Bloodstone:
        j.name = "血石"; j.description = "每张♥计分牌有 1/2 概率 ×1.5 倍率";
        j.timing = TriggerTiming::OnScoringCard;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.currentCard
                && cardIsSuitLikeOriginal(*ctx.currentCard, Suit::Hearts, false, false,
                                          ctx.state.hasJokerType(JokerType::SmearedJoker))
                && ctx.state.chanceIn(2))
                ctx.result.xmult *= 1.5;
        };
        break;

    case JokerType::ShootTheMoon:
        j.name = "射月"; j.description = "手牌中每张Q +13 倍率";
        j.timing = TriggerTiming::Passive;
        // 由 GameState 的“手牌持有牌触发”阶段处理，事件会落在每张手牌Q上。
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::Baron:
        j.name = "男爵"; j.description = "手牌中每张K ×1.5 倍率";
        j.timing = TriggerTiming::Passive;
        // 由 GameState 的“手牌持有牌触发”阶段处理，事件会落在每张手牌K上。
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::FlowerPot:
        j.name = "花盆"; j.description = "计分牌含四种花色时，×3 倍率";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            // 原版花盆：先让非百搭牌各占一个花色（被禁用牌仍按花色参与，bypassDebuff=true），
            // 再让百搭牌去填补缺的花色。模糊小丑放宽到同色。
            const bool smeared = ctx.state.hasJokerType(JokerType::SmearedJoker);
            const QVector<Suit> order = { Suit::Hearts, Suit::Diamonds, Suit::Spades, Suit::Clubs };
            QSet<int> suits;
            for (const CardData &c : ctx.scoringCards) {
                if (c.enhancement == Enhancement::Wild) continue;
                fillFirstMatchingSuitLikeOriginal(suits, c, order, true, false, smeared);
            }
            for (const CardData &c : ctx.scoringCards) {
                if (c.enhancement != Enhancement::Wild) continue;
                fillFirstMatchingSuitLikeOriginal(suits, c, order, false, false, smeared);
            }
            if (suits.size() >= 4) ctx.result.xmult *= 3.0;
        };
        break;

    case JokerType::Acrobat:
        j.name = "杂技演员"; j.description = "每回合最后一手牌 ×3 倍率";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.state.handsLeft() == 1) ctx.result.xmult *= 3.0;
        };
        break;

    case JokerType::Swashbuckler:
        j.name = "侠盗"; j.description = "其他所有小丑出售价值合计加入倍率";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &ctx) {
            int sum = 0;
            for (const Joker &jj : ctx.state.jokers()) sum += qMax(0, jj.sellValue);
            // 原版只计入「其他」小丑，排除侠盗自身。
            if (ctx.self) sum -= qMax(0, ctx.self->sellValue);
            ctx.result.mult += sum;
        };
        break;

    case JokerType::Ramen:
        j.name = "拉面"; j.description = "×2 倍率，每弃 1 张牌 ×倍率 -0.01";
        j.counter = 200;   // ×倍率 ×100，初始 ×2.00
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::DriversLicense:
        j.name = "驾驶执照"; j.description = "若牌组中至少16张增强牌，×3 倍率";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &ctx) {
            int enhanced = 0;
            for (const CardData &c : ctx.state.fullDeckCards())
                if (c.enhancement != Enhancement::None) ++enhanced;
            if (enhanced >= 16) ctx.result.xmult *= 3.0;
        };
        break;

    case JokerType::Hiker:
        j.name = "徒步者"; j.description = "每张计分牌永久 +5 筹码";
        j.timing = TriggerTiming::OnScoringCard;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.currentCard && !ctx.currentCard->isDebuffed) ctx.result.chips += 5;
        };
        break;

    case JokerType::CardSharp:
        j.name = "老千小丑"; j.description = "若本回合已经出过该牌型，×3 倍率";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.state.handTypePlayedThisRound(ctx.result.type)) ctx.result.xmult *= 3.0;
        };
        break;

    case JokerType::Hologram:
        j.name = "全息影像"; j.description = "每向牌组添加 1 张游戏牌，本小丑 +X0.25 倍率";
        j.counter = 0;
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &ctx) {
            Q_UNUSED(ctx);
            // 真实动态数值由 GameState 按 j.counter 处理，避免复制小丑丢失成长值。
        };
        break;

    case JokerType::MidasMask:
        j.name = "迈达斯面具"; j.description = "计分的人头牌变为黄金牌";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::Vampire:
        j.name = "吸血鬼"; j.description = "移除计分牌增强，每移除 1 张获得 +X0.1 倍率";
        j.counter = 0;
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::Constellation:
        j.name = "星座"; j.description = "每使用 1 张星球牌，本小丑 +X0.1 倍率";
        j.counter = 0;
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::Photograph:
        j.name = "照片"; j.description = "第一张计分人头牌 ×2 倍率";
        j.timing = TriggerTiming::OnScoringCard;
        j.effect = [](TriggerContext &ctx) {
            if (!ctx.currentCard || ctx.currentCard->isDebuffed) return;
            // 仅本手第一张计分人头牌触发（由 GameState 标记 isFirstFaceCard）。
            if (ctx.isFirstFaceCard) ctx.result.xmult *= 2.0;
        };
        break;

    case JokerType::HangingChad:
        j.name = "未断选票"; j.description = "重新触发第一张计分牌 2 次";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::SockAndBuskin:
        j.name = "喜与悲"; j.description = "所有计分人头牌重新触发 1 次";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::Blueprint:
        j.name = "蓝图"; j.description = "复制右侧小丑的计分效果";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::Brainstorm:
        j.name = "头脑风暴"; j.description = "复制最左侧小丑的计分效果";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::DNA:
        j.name = "DNA"; j.description = "若本盲注第一次出牌只有 1 张，永久复制该牌并放入手牌";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.scoringCards.size() == 1)
                ctx.state.createDNACopy(ctx.scoringCards.first());
        };
        break;

    case JokerType::Mime:
        j.name = "哑剧演员"; j.description = "重新触发手牌中保留牌的效果（钢铁牌、男爵K、Q等）";
        j.timing = TriggerTiming::Passive;
        // 重新触发逻辑由 GameState 的留手牌结算阶段统一处理（mimeRetriggers），
        // 这里留空，避免与之重复叠加。
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::Caino:
        j.name = "卡尼奥"; j.description = "传奇小丑：每摧毁 1 张人头牌，本小丑获得 X1 倍率";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &ctx) {
            double x = ctx.state.cainoXMult();
            if (x > 1.0) ctx.result.xmult *= x;
        };
        break;

    case JokerType::Triboulet:
        j.name = "特里布莱"; j.description = "传奇小丑：每张计分的 K 或 Q 各给 X2 倍率";
        j.timing = TriggerTiming::OnScoringCard;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.currentCard && !ctx.currentCard->isDebuffed &&
                (ctx.currentCard->rank == Rank::King || ctx.currentCard->rank == Rank::Queen))
                ctx.result.xmult *= 2.0;
        };
        break;

    case JokerType::Yorick:
        j.name = "约里克"; j.description = "传奇小丑：每弃掉 23 张牌，本小丑获得 X1 倍率";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &ctx) {
            double x = ctx.state.yorickXMult();
            if (x > 1.0) ctx.result.xmult *= x;
        };
        break;

    case JokerType::Chicot:
        j.name = "希科"; j.description = "传奇小丑：禁用 Boss 盲注效果";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::Perkeo:
        j.name = "帕奇欧"; j.description = "传奇小丑：离开商店时复制 1 张随机消耗牌，并使复制品变为负片";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    // ─── Batch 1：纯计分效果小丑 ─────────────────────────
    case JokerType::JokerStencil:
        j.name = "模具小丑"; j.description = "每个空小丑槽位 ×1 倍率（含自身）";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &ctx) {
            int empty = ctx.state.jokerSlots() - ctx.state.jokers().size();
            int stencils = 0;
            for (const Joker &jj : ctx.state.jokers())
                if (!jj.isDebuffed && jj.type == JokerType::JokerStencil) ++stencils;
            int x = empty + stencils;
            if (x > 1) ctx.result.xmult *= x;
        };
        break;

    case JokerType::SteelJoker:
        j.name = "钢铁小丑"; j.description = "牌组每张钢铁牌 +X0.2 倍率";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &ctx) {
            int n = 0;
            for (const CardData &c : ctx.state.fullDeckCards())
                if (c.enhancement == Enhancement::Steel) ++n;
            if (n > 0) ctx.result.xmult *= (1.0 + 0.2 * n);
        };
        break;

    case JokerType::StoneJoker:
        j.name = "石头小丑"; j.description = "牌组每张石头牌 +25 筹码";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &ctx) {
            int n = 0;
            for (const CardData &c : ctx.state.fullDeckCards())
                if (c.enhancement == Enhancement::Stone) ++n;
            ctx.result.chips += 25 * n;
        };
        break;

    case JokerType::BlueJoker:
        j.name = "蓝色小丑"; j.description = "牌堆每剩余 1 张牌 +2 筹码";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &ctx) {
            ctx.result.chips += 2 * ctx.state.deckRemaining();
        };
        break;

    case JokerType::Erosion:
        j.name = "侵蚀"; j.description = "牌组每少于 52 张 1 张 +4 倍率";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &ctx) {
            int below = qMax(0, 52 - ctx.state.fullDeckCards().size());
            ctx.result.mult += 4 * below;
        };
        break;

    case JokerType::BusinessCard:
        j.name = "名片"; j.description = "每张人头计分牌有 1/2 概率 +$2";
        j.timing = TriggerTiming::OnScoringCard;
        j.effect = [](TriggerContext &ctx) {
            if (!ctx.currentCard || ctx.currentCard->isDebuffed) return;
            if (ctx.state.isFaceCard(*ctx.currentCard)
                && ctx.state.chanceIn(2))
                ctx.state.addGold(2);
        };
        break;

    case JokerType::FacelessJoker:
        j.name = "无面小丑"; j.description = "一次弃掉至少 3 张人头牌时 +$5";
        j.timing = TriggerTiming::OnDiscard;
        j.effect = [](TriggerContext &ctx) {
            int faces = 0;
            for (const CardData &c : ctx.scoringCards) {
                if (c.isDebuffed) continue;
                if (ctx.state.isFaceCard(c)) ++faces;
            }
            if (faces >= 3) ctx.state.addGold(5);
        };
        break;

    case JokerType::Cloud9:
        j.name = "9霄云外"; j.description = "回合结束时，牌组每张 9 给 +$1";
        j.timing = TriggerTiming::OnRoundEnd;
        j.effect = [](TriggerContext &ctx) {
            int n = 0;
            for (const CardData &c : ctx.state.fullDeckCards())
                if (c.rank == Rank::Nine) ++n;
            if (n > 0) ctx.state.addGold(n);
        };
        break;

    case JokerType::GoldenTicket:
        j.name = "黄金门票"; j.description = "每张黄金增强计分牌 +$4";
        j.timing = TriggerTiming::OnScoringCard;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.currentCard && !ctx.currentCard->isDebuffed
                && ctx.currentCard->enhancement == Enhancement::Gold)
                ctx.state.addGold(4);
        };
        break;

    case JokerType::SeeingDouble:
        j.name = "重影"; j.description = "计分牌同时含♣牌和其他花色牌时 ×2 倍率";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            // 原版重影：非百搭牌按花色登记（模糊小丑放宽到同色），百搭牌再填补缺色（♣优先）。
            const bool smeared = ctx.state.hasJokerType(JokerType::SmearedJoker);
            QSet<int> suits;
            for (const CardData &c : ctx.scoringCards) {
                if (c.enhancement == Enhancement::Wild) continue;
                const QVector<Suit> order = { Suit::Hearts, Suit::Diamonds, Suit::Spades, Suit::Clubs };
                for (Suit suit : order) {
                    if (cardIsSuitLikeOriginal(c, suit, false, false, smeared))
                        suits.insert(static_cast<int>(suit));
                }
            }
            const QVector<Suit> wildOrder = { Suit::Clubs, Suit::Diamonds, Suit::Spades, Suit::Hearts };
            for (const CardData &c : ctx.scoringCards) {
                if (c.enhancement != Enhancement::Wild) continue;
                fillFirstMatchingSuitLikeOriginal(suits, c, wildOrder, false, false, smeared);
            }
            const bool hasClub = suits.contains(static_cast<int>(Suit::Clubs));
            const bool hasOther = suits.contains(static_cast<int>(Suit::Hearts))
                               || suits.contains(static_cast<int>(Suit::Diamonds))
                               || suits.contains(static_cast<int>(Suit::Spades));
            if (hasClub && hasOther) ctx.result.xmult *= 2.0;
        };
        break;

    // ─── Batch 2：计数器型小丑（动态数值由 GameState 按 counter 处理）───────
    case JokerType::SquareJoker:
        j.name = "方形小丑"; j.description = "每次打出恰好 4 张牌时永久 +4 筹码";
        j.counter = 0; j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::Runner:
        j.name = "跑步选手"; j.description = "每次打出顺子时永久 +15 筹码";
        j.counter = 0; j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::Castle:
        j.name = "城堡"; j.description = "每弃掉 1 张指定花色的牌永久 +3 筹码（花色每回合变化）";
        j.counter = 0; j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::GreenJoker:
        j.name = "绿色小丑"; j.description = "每出 1 手牌 +1 倍率，每弃 1 次牌 -1 倍率";
        j.counter = 0; j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::Obelisk:
        j.name = "方尖石塔"; j.description = "连续不打出最常用牌型时 +X0.2 倍率，打出则重置";
        j.counter = 0; j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::RideTheBus:
        j.name = "搭乘巴士"; j.description = "连续打出不含计分人头牌的手牌时 +1 倍率，含则重置";
        j.counter = 0; j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::SpareTrousers:
        j.name = "备用裤子"; j.description = "每次打出含两对的手牌时永久 +2 倍率";
        j.counter = 0; j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::WeeJoker:
        j.name = "小小丑"; j.description = "每张计分的 2 永久为本牌 +8 筹码";
        j.counter = 0; j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::HitTheRoad:
        j.name = "上路吧杰克"; j.description = "本回合每弃掉 1 张 J +X0.5 倍率，回合结束重置";
        j.counter = 0; j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::GlassJoker:
        j.name = "玻璃小丑"; j.description = "每张玻璃牌破碎 +X0.75 倍率";
        j.counter = 0; j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::LuckyCat:
        j.name = "招财猫"; j.description = "每次幸运牌触发 +X0.25 倍率";
        j.counter = 0; j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::Popcorn:
        j.name = "爆米花"; j.description = "+20 倍率，每回合结束 -4 倍率，归零时消失";
        j.counter = 20; j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    // ─── Batch 3：手牌/弃牌/经济/每回合参数型小丑 ─────────────
    case JokerType::Juggler:
        j.name = "杂耍师"; j.description = "手牌上限 +1";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};   // 由 handSize() 处理
        break;

    case JokerType::Drunkard:
        j.name = "醉汉"; j.description = "每回合 +1 次弃牌";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};   // 由 startBlind() 处理
        break;

    case JokerType::MerryAndy:
        j.name = "快乐安迪"; j.description = "每回合 +3 次弃牌，手牌上限 -1";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::Troubadour:
        j.name = "游吟诗人"; j.description = "手牌上限 +2，每回合 -1 次出牌";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::DelayedGratification:
        j.name = "延迟满足"; j.description = "回合结束时，若未使用弃牌，每个弃牌次数 +$2";
        j.timing = TriggerTiming::OnRoundEnd;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.state.noDiscardsUsedThisRound())
                ctx.state.addGold(2 * ctx.state.discardLeft());
        };
        break;

    case JokerType::ToTheMoon:
        j.name = "冲向月球"; j.description = "每 $5 额外获得 $1 利息";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};   // 由回合结算利息处理
        break;

    case JokerType::ReservedParking:
        j.name = "私人车位"; j.description = "手牌中每张人头牌有 1/2 概率 +$1";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};   // 由留手牌结算阶段处理
        break;

    case JokerType::MailInRebate:
        j.name = "邮件回扣"; j.description = "每弃掉 1 张指定点数的牌 +$5（点数每回合变化）";
        j.timing = TriggerTiming::OnDiscard;
        j.effect = [](TriggerContext &ctx) {
            for (const CardData &c : ctx.scoringCards)
                if (!c.isDebuffed && c.enhancement != Enhancement::Stone
                    && c.rank == ctx.state.mailRank())
                    ctx.state.addGold(5);
        };
        break;

    case JokerType::AncientJoker:
        j.name = "古老小丑"; j.description = "每张指定花色的计分牌 ×1.5 倍率（花色每回合变化）";
        j.timing = TriggerTiming::OnScoringCard;
        j.effect = [](TriggerContext &ctx) {
            if (!ctx.currentCard || ctx.currentCard->isDebuffed) return;
            if (cardIsSuitLikeOriginal(*ctx.currentCard, ctx.state.ancientSuit(), false, false,
                                       ctx.state.hasJokerType(JokerType::SmearedJoker)))
                ctx.result.xmult *= 1.5;
        };
        break;

    case JokerType::TheIdol:
        j.name = "偶像"; j.description = "每张指定点数+花色的计分牌 ×2 倍率（每回合变化）";
        j.timing = TriggerTiming::OnScoringCard;
        j.effect = [](TriggerContext &ctx) {
            if (!ctx.currentCard || ctx.currentCard->isDebuffed) return;
            const bool suitOk = cardIsSuitLikeOriginal(*ctx.currentCard, ctx.state.idolSuit(), false, false,
                                                       ctx.state.hasJokerType(JokerType::SmearedJoker));
            if (ctx.currentCard->rank == ctx.state.idolRank() && suitOk)
                ctx.result.xmult *= 2.0;
        };
        break;

    case JokerType::SpaceJoker:
        j.name = "太空小丑"; j.description = "打出的手牌有 1/4 概率升级该牌型";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.state.chanceIn(4))
                ctx.state.levelUpHand(ctx.result.type);
        };
        break;

    case JokerType::Hack:
        j.name = "烂脱口秀演员"; j.description = "重新触发每张计分的 2/3/4/5";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};   // 由计分重触发阶段处理
        break;

    // ─── Batch 4：造牌型小丑 ─────────────────────────────────
    case JokerType::RiffRaff:
        j.name = "乌合之众"; j.description = "选择盲注时，创建 2 张普通小丑牌";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};   // 由 triggerBlindSelectJokers() 处理
        break;

    case JokerType::MarbleJoker:
        j.name = "大理石小丑"; j.description = "选择盲注时，向牌组添加 1 张石头牌";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::Burglar:
        j.name = "窃贼"; j.description = "选择盲注时，+3 次出牌，但失去所有弃牌";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::Cartomancer:
        j.name = "塔罗术士"; j.description = "选择盲注时，创建 1 张塔罗牌";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::Certificate:
        j.name = "证书"; j.description = "回合开始时，随机添加 1 张带随机蜡封的牌到手牌中";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::Madness:
        j.name = "疯狂"; j.description = "选择小/大盲注时 +X0.5 倍率，并摧毁 1 张随机小丑牌";
        j.counter = 0; j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};   // 由 applyResolvedJokerEffect 读 counter
        break;

    case JokerType::EightBall:
        j.name = "八号球"; j.description = "每张计分的 8 有 1/4 概率创建 1 张塔罗牌";
        j.timing = TriggerTiming::OnScoringCard;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.currentCard && !ctx.currentCard->isDebuffed
                && ctx.currentCard->rank == Rank::Eight
                && ctx.state.chanceIn(4))
                ctx.state.addConsumable(randomTarotType());
        };
        break;

    case JokerType::Seance:
        j.name = "通灵"; j.description = "打出同花顺时，创建 1 张随机幻灵牌";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.result.type == HandType::StraightFlush
                || ctx.result.type == HandType::RoyalFlush)
                ctx.state.addConsumable(randomSpectralType());
        };
        break;

    case JokerType::Vagabond:
        j.name = "流浪者"; j.description = "出牌时若持有 $4 或更少，创建 1 张塔罗牌";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.state.gold() <= 4)
                ctx.state.addConsumable(randomTarotType());
        };
        break;

    case JokerType::Superposition:
        j.name = "叠加态"; j.description = "打出含 A 的顺子时，创建 1 张塔罗牌";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            const HandType t = ctx.result.type;
            const bool straight = (t == HandType::Straight || t == HandType::StraightFlush
                                   || t == HandType::RoyalFlush);
            if (!straight) return;
            for (const CardData &c : ctx.scoringCards)
                if (!c.isDebuffed && c.rank == Rank::Ace) {
                    ctx.state.addConsumable(randomTarotType());
                    break;
                }
        };
        break;

    // ─── Batch 5：计数器/经济型小丑 ─────────────────────────
    case JokerType::FlashCard:
        j.name = "闪示卡"; j.description = "每次商店重摇永久 +2 倍率";
        j.counter = 0; j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::Throwback:
        j.name = "回溯"; j.description = "本局每跳过 1 个盲注 +X0.25 倍率";
        j.counter = 0; j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::Campfire:
        j.name = "篝火"; j.description = "每卖出 1 张牌 +X0.25 倍率，击败 Boss 后重置";
        j.counter = 0; j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::FortuneTeller:
        j.name = "占卜师"; j.description = "本局每使用 1 张塔罗牌 +1 倍率";
        j.counter = 0; j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::LoyaltyCard:
        j.name = "积分卡"; j.description = "每打出 6 手牌，该手 ×4 倍率";
        j.counter = 0; j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::Egg:
        j.name = "鸡蛋"; j.description = "每回合结束，出售价值 +$3";
        j.counter = 0; j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};   // 由回合结算处理
        break;

    case JokerType::Rocket:
        j.name = "火箭"; j.description = "回合结束 +$1，击败 Boss 后每回合收益 +$2";
        j.counter = 1; j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};   // 由回合结算处理
        break;

    case JokerType::Satellite:
        j.name = "卫星"; j.description = "回合结束时，本局每使用过 1 种行星牌 +$1";
        j.counter = 0; j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};   // 由回合结算处理
        break;

    case JokerType::GiftCard:
        j.name = "礼品卡"; j.description = "每回合结束，所有小丑牌与消耗牌出售价值 +$1";
        j.counter = 0; j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};   // 由回合结算处理
        break;

    // ─── Batch 6：牌型判定修正型小丑（逻辑由 HandEvaluator/HandMods 处理）──
    case JokerType::Shortcut:
        j.name = "捷径"; j.description = "顺子允许 1 点间隔（如 3 5 6 7 9）";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::SmearedJoker:
        j.name = "模糊小丑"; j.description = "♥与♦视为同花色，♠与♣视为同花色";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::Splash:
        j.name = "飞溅"; j.description = "每张打出的牌都参与计分";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::Showman:
        j.name = "马戏团长"; j.description = "小丑、塔罗、星球、幻灵牌可在商店重复出现";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    // ─── Batch 7：特殊机制型小丑 ─────────────────────────────
    case JokerType::Dusk:
        j.name = "黄昏"; j.description = "每回合最后一手牌时，重新触发所有计分牌";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};   // 由计分重触发阶段处理
        break;

    case JokerType::CeremonialDagger:
        j.name = "仪式匕首"; j.description = "选择盲注时，摧毁右侧小丑并永久获得其出售价值×2 的倍率";
        j.counter = 0; j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};   // counter 由 applyResolvedJokerEffect 读
        break;

    case JokerType::TurtleBean:
        j.name = "黑龟豆"; j.description = "手牌上限 +5，每回合 -1，归零时消失";
        j.counter = 5; j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};   // 由 handSize() 处理
        break;

    case JokerType::Seltzer:
        j.name = "苏打水"; j.description = "接下来 10 手牌重新触发所有计分牌，之后消失";
        j.counter = 10; j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};   // 由计分重触发阶段处理
        break;

    case JokerType::BurntJoker:
        j.name = "烧焦小丑"; j.description = "每回合第一次弃牌时，升级该牌型等级";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};   // 由 discardCards() 处理
        break;

    case JokerType::ChaosTheClown:
        j.name = "混沌小丑"; j.description = "每次进入商店有 1 次免费重摇";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};   // 由 rerollShop() 处理
        break;

    // ─── Batch 8：杂项机制型小丑 ─────────────────────────────
    case JokerType::Pareidolia:
        j.name = "幻视"; j.description = "所有牌都视为人头牌";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};   // 由 GameState::isFaceCard() 处理
        break;

    case JokerType::Hallucination:
        j.name = "幻觉"; j.description = "打开卡包时有 1/2 概率创建 1 张塔罗牌";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};   // 由 buyPack() 处理
        break;

    case JokerType::Luchador:
        j.name = "摔跤手"; j.description = "出售本牌可禁用当前 Boss 盲注效果";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};   // 由 sellJoker() 处理
        break;

    case JokerType::InvisibleJoker:
        j.name = "隐形小丑"; j.description = "经过 2 个回合后，出售本牌可复制 1 张随机小丑";
        j.counter = 0; j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};   // 由 sellJoker() 处理
        break;

    // ─── Batch 9：收尾小丑 ───────────────────────────────────
    case JokerType::CreditCard:
        j.name = "信用卡"; j.description = "金币可透支至 -$20";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};   // 由购买判定处理
        break;

    case JokerType::MrBones:
        j.name = "骷髅先生"; j.description = "得分达到要求 25% 时免于失败，随后本牌消失";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};   // 由 checkGameOver() 处理
        break;

    case JokerType::DietCola:
        j.name = "零糖可乐"; j.description = "售出这牌就可以创建一个免费的双倍标签";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};   // 由 sellJoker() 处理
        break;

    case JokerType::FourFingers:
        j.name = "四指"; j.description = "同花和顺子只需 4 张牌即可组成";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};   // 由 HandEvaluator/HandMods 处理
        break;

    case JokerType::OopsAllSixes:
        j.name = "六六大顺"; j.description = "所有概率翻倍";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};   // 由 chanceIn() 处理
        break;

    // Batch 10：补充小丑（多数效果由 GameState 接管，见 A3b/A7）
    case JokerType::SixthSense:
        j.name = "第六感";
        j.description = "若本回合第一手只打出一张 6，摧毁它并生成一张幻灵牌";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};   // 由 GameState::playCards() 处理
        break;

    case JokerType::RedCard:
        j.name = "红牌";
        j.description = "每跳过一个补充包，本小丑永久 +3 倍率";
        j.counter = 0;
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};   // 由 applyResolvedJokerEffect() 按 counter 处理
        break;

    case JokerType::BaseballCard:
        j.name = "棒球卡";
        j.description = "每张罕见小丑牌给予 ×1.5 倍率";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};   // 由 applyResolvedJokerEffect() 处理
        break;

    case JokerType::TradingCard:
        j.name = "交易卡";
        j.description = "若本回合第一次弃牌只有 1 张，摧毁它并获得 $3";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};   // 由 GameState::discardCards() 处理
        break;

    case JokerType::Matador:
        j.name = "斗牛士";
        j.description = "若打出的手牌触发 Boss 盲注能力，获得 $8";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};   // 由 GameState::playCards/finalizePlayedHand 处理
        break;

    case JokerType::Astronomer:
        j.name = "天文学家";
        j.description = "商店中的所有星球牌和天体包免费";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};   // 由 Shop 定价处理
        break;

    case JokerType::OperatorOverload:
        j.name = "函数重载";
        j.description = "计分时所有{C:chips}筹码{}与{C:mult}倍率{}的\n"
                        "增减与倍乘{C:attention}互换{}\n"
                        "{C:inactive}swap(mult, chips);";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};   // 交换在 GameState 计分事件流后处理统一执行
        break;

    case JokerType::ClassTemplate:
        j.name = "类模板";
        j.description = "{C:attention}template<?>{} 未实例化\n"
                        "本底注打出的第一种牌型将其实例化\n"
                        "此后该牌型{X:mult,C:white}×构成张数{}\n"
                        "{C:inactive}Boss 击败后重置";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &) {};   // 实例化/倍率在 GameState 出牌管线专门处理（需改 counter）
        break;

    }

    // 原版出售价格约为购买价的一半，至少 $1；版本小丑额外提高出售价值。
    j.sellValue = qMax(1, jokerBaseCost(type) / 2);
    switch (j.edition) {
    case Edition::Foil:
    case Edition::Holographic: j.sellValue += 1; break;
    case Edition::Polychrome:  j.sellValue += 2; break;
    case Edition::Negative:    j.sellValue += 3; break;
    default: break;
    }
    return j;
}
