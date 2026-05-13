#include "joker.h"
#include "../game/handevaluator.h"
#include "../game/gamestate.h"
#include <QRandomGenerator>
#include <QtGlobal>
#include <climits>
#include <QSet>


static int jokerBaseCost(JokerType t)
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
    case JokerType::Mime: return 5;
    case JokerType::DNA: return 8;
    case JokerType::Blueprint:
    case JokerType::Brainstorm: return 10;
    case JokerType::Caino:
    case JokerType::Triboulet:
    case JokerType::Yorick:
    case JokerType::Chicot:
    case JokerType::Perkeo: return 20;
    }
    return 4;
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
            if (it != ctx.state.handLevels().constEnd()) ctx.result.mult += it->played;
        };
        break;

    case JokerType::GrosMichel:
        j.name = "大麦克香蕉"; j.description = "+15 倍率";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &ctx) { ctx.result.mult += 15; };
        break;

    case JokerType::Cavendish:
        j.name = "卡文迪许香蕉"; j.description = "×3 倍率";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &ctx) { ctx.result.xmult *= 3.0; };
        break;

    case JokerType::IceCream:
        j.name = "冰淇淋"; j.description = "+100 筹码";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &ctx) { ctx.result.chips += 100; };
        break;

    case JokerType::Stuntman:
        j.name = "特技演员"; j.description = "+250 筹码";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &ctx) { ctx.result.chips += 250; };
        break;

    case JokerType::TheDuo:
        j.name = "二人组"; j.description = "若出牌含对子，×2 倍率";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.result.type == HandType::Pair || ctx.result.type == HandType::TwoPair ||
                ctx.result.type == HandType::FullHouse || ctx.result.type == HandType::FourOfAKind ||
                ctx.result.type == HandType::FiveOfAKind)
                ctx.result.xmult *= 2.0;
        };
        break;

    case JokerType::TheTrio:
        j.name = "三人组"; j.description = "若出牌含三条，×3 倍率";
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
            bool ok = !ctx.hand.isEmpty();
            for (const CardData &c : ctx.hand) {
                if (c.enhancement == Enhancement::Stone) continue;
                if (!(c.suit == Suit::Spades || c.suit == Suit::Clubs)) { ok = false; break; }
            }
            if (ok) ctx.result.xmult *= 3.0;
        };
        break;

    case JokerType::ScaryFace:
        j.name = "恐怖面孔"; j.description = "每张人头计分牌 +30 筹码";
        j.timing = TriggerTiming::OnScoringCard;
        j.effect = [](TriggerContext &ctx) {
            if (!ctx.currentCard || ctx.currentCard->isDebuffed) return;
            Rank r = ctx.currentCard->rank;
            if (r == Rank::Jack || r == Rank::Queen || r == Rank::King) ctx.result.chips += 30;
        };
        break;

    case JokerType::SmileyFace:
        j.name = "笑脸"; j.description = "每张人头计分牌 +5 倍率";
        j.timing = TriggerTiming::OnScoringCard;
        j.effect = [](TriggerContext &ctx) {
            if (!ctx.currentCard || ctx.currentCard->isDebuffed) return;
            Rank r = ctx.currentCard->rank;
            if (r == Rank::Jack || r == Rank::Queen || r == Rank::King) ctx.result.mult += 5;
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
            if (ctx.currentCard && !ctx.currentCard->isDebuffed && ctx.currentCard->suit == Suit::Spades)
                ctx.result.chips += 50;
        };
        break;

    case JokerType::OnyxAgate:
        j.name = "黑玛瑙"; j.description = "每张♣计分牌 +7 倍率";
        j.timing = TriggerTiming::OnScoringCard;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.currentCard && !ctx.currentCard->isDebuffed && ctx.currentCard->suit == Suit::Clubs)
                ctx.result.mult += 7;
        };
        break;

    case JokerType::RoughGem:
        j.name = "原石"; j.description = "每张♦计分牌 +$1";
        j.timing = TriggerTiming::OnScoringCard;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.currentCard && !ctx.currentCard->isDebuffed && ctx.currentCard->suit == Suit::Diamonds)
                ctx.state.addGold(1);
        };
        break;

    case JokerType::Bloodstone:
        j.name = "血石"; j.description = "每张♥计分牌有 1/2 概率 ×1.5 倍率";
        j.timing = TriggerTiming::OnScoringCard;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.currentCard && !ctx.currentCard->isDebuffed && ctx.currentCard->suit == Suit::Hearts &&
                QRandomGenerator::global()->bounded(2) == 0)
                ctx.result.xmult *= 1.5;
        };
        break;

    case JokerType::ShootTheMoon:
        j.name = "射月"; j.description = "手牌中每张Q +13 倍率";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &ctx) {
            for (const CardData &c : ctx.hand)
                if (!c.isDebuffed && c.rank == Rank::Queen) ctx.result.mult += 13;
        };
        break;

    case JokerType::Baron:
        j.name = "男爵"; j.description = "手牌中每张K ×1.5 倍率";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &ctx) {
            for (const CardData &c : ctx.hand)
                if (!c.isDebuffed && c.rank == Rank::King) ctx.result.xmult *= 1.5;
        };
        break;

    case JokerType::FlowerPot:
        j.name = "花盆"; j.description = "计分牌含四种花色时，×3 倍率";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            QSet<int> suits;
            for (const CardData &c : ctx.scoringCards) {
                if (c.isDebuffed || c.enhancement == Enhancement::Stone) continue;
                suits.insert(static_cast<int>(c.suit));
                if (c.enhancement == Enhancement::Wild) {
                    suits.insert(0); suits.insert(1); suits.insert(2); suits.insert(3);
                }
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
        j.name = "侠盗"; j.description = "所有小丑出售价值合计加入倍率";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &ctx) {
            int sum = 0;
            for (const Joker &jj : ctx.state.jokers()) sum += qMax(0, jj.sellValue);
            ctx.result.mult += sum;
        };
        break;

    case JokerType::Ramen:
        j.name = "拉面"; j.description = "×2 倍率";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &ctx) { ctx.result.xmult *= 2.0; };
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
        j.name = "DNA"; j.description = "若本回合出牌只有 1 张，复制该牌到手牌";
        j.timing = TriggerTiming::OnPlayedHand;
        j.effect = [](TriggerContext &ctx) {
            if (ctx.scoringCards.size() == 1) {
                ctx.state.handMutable().append(ctx.scoringCards.first());
                ctx.state.notifyHandChanged();
            }
        };
        break;

    case JokerType::Mime:
        j.name = "哑剧演员"; j.description = "重新触发手牌中保留牌的效果：钢铁牌、黄金牌、蓝色蜡封，以及男爵的K效果";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &ctx) {
            for (const CardData &c : ctx.hand) {
                bool isScoring = false;
                for (const CardData &sc : ctx.scoringCards) {
                    if (sc.rank == c.rank && sc.suit == c.suit && sc.enhancement == c.enhancement && sc.seal == c.seal && sc.edition == c.edition) {
                        isScoring = true; break;
                    }
                }
                if (isScoring || c.isDebuffed) continue;
                // Held Steel card retrigger
                if (c.enhancement == Enhancement::Steel) ctx.result.xmult *= 1.5;
                // Baron gives each held King X1.5; Mime retriggers that held-card effect.
                if (ctx.state.hasJokerType(JokerType::Baron) && c.rank == Rank::King) ctx.result.xmult *= 1.5;
            }
        };
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
        j.name = "奇科特"; j.description = "传奇小丑：禁用 Boss 盲注效果";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
        break;

    case JokerType::Perkeo:
        j.name = "佩克欧"; j.description = "传奇小丑：离开商店时复制 1 张随机消耗牌，并使复制品变为负片";
        j.timing = TriggerTiming::Passive;
        j.effect = [](TriggerContext &) {};
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
