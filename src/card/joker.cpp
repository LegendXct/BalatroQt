#include "joker.h"
#include "../game/handevaluator.h"
#include "../game/gamestate.h"
#include <QRandomGenerator>
#include <climits>

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
        j.name = "半张小丑";
        j.description = "出牌≤3张时 +20 倍率";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.scoringCards.size() <= 3)
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
        j.description = "出高牌 +4 金币";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.result.type == HandType::HighCard)
                ctx.state.addGold(4);
        };
        break;
    case JokerType::SlyJoker:
        j.name = "狡猾小丑"; j.description = "出对子 +50 筹码";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.result.type == HandType::Pair) ctx.result.chips += 50;
        };
        break;

    case JokerType::WilyJoker:
        j.name = "狡黠小丑"; j.description = "出三条 +100 筹码";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.result.type == HandType::ThreeOfAKind) ctx.result.chips += 100;
        };
        break;

    case JokerType::CleverJoker:
        j.name = "聪明小丑"; j.description = "出两对 +80 筹码";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.result.type == HandType::TwoPair) ctx.result.chips += 80;
        };
        break;

    case JokerType::DeviousJoker:
        j.name = "奸诈小丑"; j.description = "出顺子 +100 筹码";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.result.type == HandType::Straight) ctx.result.chips += 100;
        };
        break;

    case JokerType::CraftyJoker:
        j.name = "灵巧小丑"; j.description = "出同花 +80 筹码";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.result.type == HandType::Flush) ctx.result.chips += 80;
        };
        break;

    // ─── 杂项倍率 ───────────────────────────
    case JokerType::Banner:
        j.name = "横幅"; j.description = "每剩余弃牌 +30 筹码";
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
        j.name = "举拳"; j.description = "手牌中最低牌点数 ×2 加倍率";
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
        j.name = "公牛"; j.description = "每金币 +2 筹码";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &ctx) {
            ctx.result.chips += 2 * qMax(0, ctx.state.gold());
        };
        break;

    case JokerType::Bootstraps:
        j.name = "靴子"; j.description = "每 5 金币 +2 倍率";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &ctx) {
            ctx.result.mult += 2 * (qMax(0, ctx.state.gold()) / 5);
        };
        break;
    }

    return j;
}
