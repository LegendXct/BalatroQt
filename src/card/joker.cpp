#include "joker.h"
#include "../game/handevaluator.h"
#include "../game/gamestate.h"

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
        j.name = "贪心小丑";
        j.description = "每张♦计分牌 +3 倍率";
        j.timing = TriggerTiming::OnScoringCard;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.currentCard &&
                ctx.currentCard->suit == Suit::Diamonds &&
                !ctx.currentCard->isDebuffed)
                ctx.result.mult += 3;
        };
        break;

    case JokerType::LustyJoker:
        j.name = "色欲小丑";
        j.description = "每张♥计分牌 +3 倍率";
        j.timing = TriggerTiming::OnScoringCard;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.currentCard &&
                ctx.currentCard->suit == Suit::Hearts &&
                !ctx.currentCard->isDebuffed)
                ctx.result.mult += 3;
        };
        break;

    case JokerType::WrathfulJoker:
        j.name = "愤怒小丑";
        j.description = "每张♠计分牌 +3 倍率";
        j.timing = TriggerTiming::OnScoringCard;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.currentCard &&
                ctx.currentCard->suit == Suit::Spades &&
                !ctx.currentCard->isDebuffed)
                ctx.result.mult += 3;
        };
        break;

    case JokerType::GluttonousJoker:
        j.name = "暴食小丑";
        j.description = "每张♣计分牌 +3 倍率";
        j.timing = TriggerTiming::OnScoringCard;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.currentCard &&
                ctx.currentCard->suit == Suit::Clubs &&
                !ctx.currentCard->isDebuffed)
                ctx.result.mult += 3;
        };
        break;

    case JokerType::HalfJoker:
        j.name = "半个小丑";
        j.description = "出牌≤3张时 +20 倍率";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.scoringCards.size() <= 3)
                ctx.result.mult += 20;
        };
        break;

    case JokerType::JollyJoker:
        j.name = "欢乐小丑";
        j.description = "出对子 +8 倍率";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.result.type == HandType::Pair)
                ctx.result.mult += 8;
        };
        break;

    case JokerType::ZanyJoker:
        j.name = "疯狂小丑";
        j.description = "出三条 +12 倍率";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.result.type == HandType::ThreeOfAKind)
                ctx.result.mult += 12;
        };
        break;

    case JokerType::MadJoker:
        j.name = "愤怒小丑";
        j.description = "出两对 +10 倍率";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.result.type == HandType::TwoPair)
                ctx.result.mult += 10;
        };
        break;

    case JokerType::CrazyJoker:
        j.name = "疯癫小丑";
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

    case JokerType::GoldTicket:
        j.name = "金票";
        j.description = "回合结束 +4 金币";
        j.timing = TriggerTiming::OnRoundEnd;
        j.effect = [](TriggerContext &ctx) {
            // 金币修改通过 GameState 接口处理
            // 在 GameState::enterShop 里遍历调用
            Q_UNUSED(ctx);
        };
        break;

    case JokerType::ToDoList:
        j.name = "待办清单";
        j.description = "出高牌 +4 金币";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.result.type == HandType::HighCard)
                ctx.state.addGold(4);
        };
        break;
    }

    return j;
}
