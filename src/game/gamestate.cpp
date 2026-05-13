#include "gamestate.h"
#include <QSet>
#include <QRandomGenerator>
#include <algorithm>
#include <functional>
#include <cmath>

GameState::GameState(QObject *parent)
    : QObject{parent}
{
    startGame();
}

QPair<int, int> GameState::handLevelDelta(HandType t) {
    switch (t) {
    case HandType::HighCard:      return { 10, 1 };
    case HandType::Pair:          return { 15, 1 };
    case HandType::TwoPair:       return { 20, 1 };
    case HandType::ThreeOfAKind:  return { 20, 2 };
    case HandType::Straight:      return { 30, 3 };
    case HandType::Flush:         return { 15, 2 };
    case HandType::FullHouse:     return { 25, 2 };
    case HandType::FourOfAKind:   return { 30, 3 };
    case HandType::StraightFlush: return { 40, 4 };
    case HandType::RoyalFlush:    return { 40, 4 };
    case HandType::FiveOfAKind:   return { 35, 3 };
    case HandType::FlushHouse:    return { 40, 4 };
    case HandType::FlushFive:     return { 50, 3 };
    }
    return { 0, 0 };
}

void GameState::levelUpHand(HandType t, int times) {
    for (Joker &j : mJokers) {
        if (!j.isDebuffed && j.type == JokerType::Constellation) j.counter += times;
    }
    HandLevel &lv = mHandLevels[t];
    auto d = handLevelDelta(t);
    for (int i = 0; i < times; ++i) {
        lv.level++;
        lv.chipsBonus += d.first;
        lv.multBonus  += d.second;
    }
    emit handLevelsChanged();
}

static auto rankComp = [](const CardData &a, const CardData &b) {
    if (a.rank != b.rank) return static_cast<int>(a.rank) > static_cast<int>(b.rank);
    return static_cast<int>(a.suit) < static_cast<int>(b.suit);
};


static bool bossDebuffsCard(BossEffect effect, const CardData &c)
{
    if (effect == BossEffect::TheClub  && c.suit == Suit::Clubs)    return true;
    if (effect == BossEffect::TheGoad  && c.suit == Suit::Spades)   return true;
    if (effect == BossEffect::TheHead  && c.suit == Suit::Hearts)   return true;
    if (effect == BossEffect::TheWindow&& c.suit == Suit::Diamonds) return true;
    if (effect == BossEffect::ThePlant &&
        (c.rank == Rank::Jack || c.rank == Rank::Queen || c.rank == Rank::King)) return true;
    return false;
}

// 原版 Blueprint / Brainstorm 不只是复制计分阶段，也会复制 Perkeo、DNA 等
// calculate_joker 风格效果。这里先把“最终被复制到的真实小丑下标”解析出来，
// 这样计分、离开商店、提示 UI 都能复用同一套规则。
static int resolveCopiedJokerIndex(const QVector<Joker> &jokers, int idx)
{
    QSet<int> seen;
    int cur = idx;
    for (int guard = 0; guard < jokers.size() + 4; ++guard) {
        if (cur < 0 || cur >= jokers.size()) return -1;
        if (seen.contains(cur)) return -1;
        seen.insert(cur);

        const Joker &j = jokers[cur];
        if (j.type == JokerType::Blueprint) {
            cur = cur + 1;
            continue;
        }
        if (j.type == JokerType::Brainstorm) {
            cur = 0;
            continue;
        }
        return cur;
    }
    return -1;
}

static const Joker *resolveCopiedJoker(const QVector<Joker> &jokers, int idx)
{
    int resolved = resolveCopiedJokerIndex(jokers, idx);
    if (resolved < 0 || resolved >= jokers.size()) return nullptr;
    return &jokers[resolved];
}


static int countResolvedJokersOfType(const QVector<Joker> &jokers, JokerType type)
{
    int count = 0;
    for (int i = 0; i < jokers.size(); ++i) {
        if (jokers[i].isDebuffed) continue;
        const Joker *resolved = resolveCopiedJoker(jokers, i);
        if (resolved && !resolved->isDebuffed && resolved->type == type) ++count;
    }
    return count;
}

static void applyResolvedJokerEffect(const Joker &j, TriggerContext &ctx)
{
    if (j.type == JokerType::IceCream) {
        ctx.result.chips += qMax(0, j.counter);
        return;
    }
    if (j.type == JokerType::Hologram) {
        ctx.result.xmult *= (1.0 + 0.25 * qMax(0, j.counter));
        return;
    }
    if (j.type == JokerType::Constellation) {
        ctx.result.xmult *= (1.0 + 0.1 * qMax(0, j.counter));
        return;
    }
    if (j.type == JokerType::Vampire) {
        ctx.result.xmult *= (1.0 + 0.1 * qMax(0, j.counter));
        return;
    }
    j.effect(ctx);
}

static auto suitComp = [](const CardData &a, const CardData &b) {
    if (a.suit != b.suit) return static_cast<int>(a.suit) < static_cast<int>(b.suit);
    return static_cast<int>(a.rank) > static_cast<int>(b.rank);
};

void GameState::sortHandByRank() {
    mSortMode = HandSortMode::ByRank;
    std::sort(mHand.begin(), mHand.end(), rankComp);
    emit handChanged();
}

void GameState::sortHandBySuit() {
    mSortMode = HandSortMode::BySuit;
    std::sort(mHand.begin(), mHand.end(), suitComp);
    emit handChanged();
}


int GameState::deckTotal() const
{
    return mDeck.totalKnown() + mHand.size();
}

QVector<CardData> GameState::remainingDeckCards() const
{
    QVector<CardData> out = mDeck.drawPile();
    bool bossDisabled = hasJokerType(JokerType::Chicot);
    for (CardData &c : out) {
        c.faceUp = true;
        c.isDebuffed = (mPhase == GamePhase::Blind) && !bossDisabled && bossDebuffsCard(mBossEffect, c);
    }
    return out;
}

QVector<CardData> GameState::fullDeckCards() const
{
    QVector<CardData> out = mDeck.allKnownCards();
    bool bossDisabled = hasJokerType(JokerType::Chicot);
    for (CardData &c : out) {
        c.faceUp = true;
        c.isDebuffed = (mPhase == GamePhase::Blind) && !bossDisabled && bossDebuffsCard(mBossEffect, c);
    }
    for (CardData c : mHand) {
        c.faceUp = true;
        c.isDebuffed = (mPhase == GamePhase::Blind) && !bossDisabled && bossDebuffsCard(mBossEffect, c);
        out.append(c);
    }
    return out;
}

QVector<JokerType> GameState::ownedJokerTypes() const
{
    QVector<JokerType> out;
    for (const Joker &j : mJokers) out.append(j.type);
    return out;
}

bool GameState::hasJokerDuplicateBypass() const
{
    // 原版 Showman/马戏团长允许重复小丑。当前项目还没有实现 Showman，
    // 所以这里先恒为 false；以后加 JokerType::Showman 时，在这里返回 true。
    return false;
}

void GameState::syncShopJokerRules()
{
    mShop.setOwnedJokers(ownedJokerTypes(), hasJokerDuplicateBypass());
    mShop.setGrosMichelExtinct(mGrosMichelExtinct);
}

void GameState::dealCards() {
    while (mHand.size() < handSize() && !mDeck.isEmpty())
        mHand.append(mDeck.draw());
    if (mSortMode == HandSortMode::ByRank)
        std::sort(mHand.begin(), mHand.end(), rankComp);
    else if (mSortMode == HandSortMode::BySuit)
        std::sort(mHand.begin(), mHand.end(), suitComp);
    emit handChanged();
}

void GameState::playCards(const QVector<int> &indices) {
    if (indices.isEmpty() || indices.size() > Constants::MAX_PLAY) return;
    if (mHandsLeft <= 0) return;
    if (mAwaitingScoreFinalize) return;

    QVector<int> sorted = indices;
    std::sort(sorted.begin(), sorted.end());
    for (int i : sorted) {
        if (i < 0 || i >= mHand.size()) return;
    }

    // 取出 played，评分动画期间暂时不补牌；补牌要等所有计分动画和收牌动画结束后再做。
    QVector<CardData> played;
    for (int i : sorted) played.append(mHand[i]);

    // DNA：只有本盲注第一次出牌且只打 1 张牌时生效。
    // 多个 DNA / 蓝图 / 头脑风暴复制 DNA 时，本次出牌可以创建多张复制牌。
    const bool firstHandOfBlind = (mHandsLeft == mBlindStartingHands);
    mDNAEligibleThisPlay = (!mDNAUsedThisBlind && firstHandOfBlind && sorted.size() == 1);
    if (firstHandOfBlind) mDNAUsedThisBlind = true;
    mDNACopiesCreatedThisPlay = 0;
    mPendingDNACopies.clear();

    HandResult result = HandEvaluator::evaluate(played);
    if (bossBlocksPlayedHand(result, played.size())) {
        // 原版会阻止非法出牌并给提示；当前 Qt 版先保证不会卡死：
        // 这手按 0 分处理并正常进入收牌/补牌流程。
        result.name = "Boss 限制";
        result.chips = 0;
        result.mult = 0;
        result.xmult = 1.0;
        result.baseChips = 0;
        result.baseMult = 0;
        mPendingHandScore = 0;
        mPendingPlayedIndices = sorted;
        mPendingShattered = QVector<bool>(played.size(), false);
        mPendingDNACopies.clear();
        mAwaitingScoreFinalize = true;
        mLastResult = result;
        emit handPlayed();
        return;
    }

    HandLevel &lv = mHandLevels[result.type];
    if (!hasJokerType(JokerType::Chicot) && mBossEffect == BossEffect::TheArm && lv.level > 1) {
        auto d = handLevelDelta(result.type);
        lv.level = qMax(1, lv.level - 1);
        lv.chipsBonus = qMax(0, lv.chipsBonus - d.first);
        lv.multBonus  = qMax(0, lv.multBonus  - d.second);
        emit handLevelsChanged();
    }
    result.chips += lv.chipsBonus;
    result.mult  += lv.multBonus;
    result.level  = lv.level;
    result.xmult  = 1.0;

    if (!hasJokerType(JokerType::Chicot) && mBossEffect == BossEffect::TheFlint) {
        result.chips = qMax(0, int(std::floor(result.chips * 0.5 + 0.5)));
        result.mult  = qMax(1, int(std::floor(result.mult  * 0.5 + 0.5)));
    }

    result.baseChips = result.chips;
    result.baseMult  = result.mult;

    QVector<int> scoringPlayedIdx;
    QSet<int> used;
    for (const CardData &sc : result.scoringCards) {
        for (int k = 0; k < played.size(); ++k) {
            if (used.contains(k)) continue;
            const CardData &p = played[k];
            if (p.rank == sc.rank && p.suit == sc.suit
                && p.enhancement == sc.enhancement
                && p.edition == sc.edition
                && p.seal == sc.seal) {
                scoringPlayedIdx.append(k);
                used.insert(k);
                break;
            }
        }
    }
    std::sort(scoringPlayedIdx.begin(), scoringPlayedIdx.end());

    QVector<bool> shattered(played.size(), false);

    const int sockRetriggers = countResolvedJokersOfType(mJokers, JokerType::SockAndBuskin);
    const int chadRetriggers = 2 * countResolvedJokersOfType(mJokers, JokerType::HangingChad);
    const int hikerTriggers  = countResolvedJokersOfType(mJokers, JokerType::Hiker);
    const int midasTriggers  = countResolvedJokersOfType(mJokers, JokerType::MidasMask);
    const int vampireTriggers = countResolvedJokersOfType(mJokers, JokerType::Vampire);

    bool firstScoringCard = true;
    for (int playedIdx : scoringPlayedIdx) {
        CardData card = played[playedIdx];
        if (card.isDebuffed) { firstScoringCard = false; continue; }

        const int redSealReps = (card.seal == Seal::Red) ? 1 : 0;
        int triggers = 1 + redSealReps;
        const bool isFace = card.rank == Rank::Jack || card.rank == Rank::Queen || card.rank == Rank::King;
        if (isFace) triggers += sockRetriggers;
        if (firstScoringCard) triggers += chadRetriggers;

        int globalIdx = (playedIdx >= 0 && playedIdx < sorted.size()) ? sorted[playedIdx] : -1;

        if (hikerTriggers > 0 && globalIdx >= 0 && globalIdx < mHand.size()) {
            // 原版 Hiker：每张计分牌本体永久 +5 筹码；蓝图/头脑风暴复制时额外叠加。
            mHand[globalIdx].permanentBonusChips += 5 * hikerTriggers;
            card.permanentBonusChips = mHand[globalIdx].permanentBonusChips;
        }

        for (int t = 0; t < triggers; ++t) {
            if (t > 0 && t <= redSealReps) {
                // 原版红色蜡封在真正重复计算前先显示“再触发”。
                // Sock/Chad 等小丑带来的重复不伪装成红蜡封，只保留各自的计分事件。
                result.events.append({ ScoreEventKind::RedSealRetrigger, playedIdx, -1, -1, 0, 1.0 });
            }
            scoreCard(card, result, playedIdx);
        }

        if (midasTriggers > 0 && isFace && globalIdx >= 0 && globalIdx < mHand.size()) {
            mHand[globalIdx].enhancement = Enhancement::Gold;
        }

        if (vampireTriggers > 0 && globalIdx >= 0 && globalIdx < mHand.size()
            && mHand[globalIdx].enhancement != Enhancement::None) {
            mHand[globalIdx].enhancement = Enhancement::None;
            for (Joker &vj : mJokers) {
                if (!vj.isDebuffed && vj.type == JokerType::Vampire) vj.counter += vampireTriggers;
            }
        }

        if (card.enhancement == Enhancement::Glass
            && QRandomGenerator::global()->bounded(4) == 0) {
            shattered[playedIdx] = true;
            // 破碎发生在这张玻璃牌完成所有计分/重触发之后，和原版 shatter 队列一致。
            result.events.append({ ScoreEventKind::GlassShatter, playedIdx, -1, -1, 0, 1.0 });
        }
        firstScoringCard = false;
    }

    QSet<int> playedHandIdx;
    for (int p : sorted) playedHandIdx.insert(p);
    QVector<CardData> heldHand;
    QVector<int> heldGlobalIdx;
    for (int i = 0; i < mHand.size(); ++i) {
        if (playedHandIdx.contains(i)) continue;
        heldHand.append(mHand[i]);
        heldGlobalIdx.append(i);
    }

    const int mimeRetriggers = countResolvedJokersOfType(mJokers, JokerType::Mime);
    const int baronTriggers = countResolvedJokersOfType(mJokers, JokerType::Baron);
    const int shootMoonTriggers = countResolvedJokersOfType(mJokers, JokerType::ShootTheMoon);
    for (int hi = 0; hi < heldHand.size(); ++hi) {
        const CardData &c = heldHand[hi];
        int heldVisualIdx = hi; // UI 中 mHandCards 已经移除了打出的牌，heldHand 的顺序就是手牌显示顺序
        if (c.isDebuffed) continue;

        // 原版手牌阶段先计算这张手牌是否真的有效果，然后红蜡封和 Mime 才能追加 repetition。
        // 因此红钢 K + 男爵 + 蓝图/头脑风暴/哑剧时，总次数 = 1 + 红蜡封次数 + 解析后的 Mime 次数；
        // 每一次 repetition 都重新执行钢铁、男爵、Shoot the Moon 等全部 hand-card 效果。
        const bool hasHeldEffect = (c.enhancement == Enhancement::Steel)
                                || (c.rank == Rank::King && baronTriggers > 0)
                                || (c.rank == Rank::Queen && shootMoonTriggers > 0);
        const int redSealReps = (hasHeldEffect && c.seal == Seal::Red) ? 1 : 0;
        int retriggers = 1 + redSealReps + (hasHeldEffect ? mimeRetriggers : 0);

        for (int r = 0; r < retriggers; ++r) {
            if (r > 0 && r <= redSealReps)
                result.events.append({ ScoreEventKind::RedSealRetrigger, -1, heldVisualIdx, -1, 0, 1.0 });

            if (c.enhancement == Enhancement::Steel) {
                result.xmult *= 1.5;
                result.events.append({ ScoreEventKind::SteelXMult, -1, heldVisualIdx, -1, 0, 1.5 });
            }
            if (c.rank == Rank::King) {
                for (int b = 0; b < baronTriggers; ++b) {
                    result.xmult *= 1.5;
                    result.events.append({ ScoreEventKind::SteelXMult, -1, heldVisualIdx, -1, 0, 1.5 });
                }
            }
            if (c.rank == Rank::Queen) {
                for (int sm = 0; sm < shootMoonTriggers; ++sm) {
                    result.mult += 13;
                    result.events.append({ ScoreEventKind::EnhancementMult, -1, heldVisualIdx, -1, 13, 1.0 });
                }
            }
        }
    }

    {
        int chipsBefore = result.chips, multBefore = result.mult;
        double xmultBefore = result.xmult;

        TriggerContext ctx{ result, *this, heldHand, result.scoringCards, nullptr };
        for (int ji = 0; ji < mJokers.size(); ++ji) {
            const Joker &j = mJokers[ji];
            if (j.isDebuffed) continue;

            const Joker *effectJoker = resolveCopiedJoker(mJokers, ji);

            if (effectJoker && !effectJoker->isDebuffed &&
                (effectJoker->timing == TriggerTiming::Passive ||
                 effectJoker->timing == TriggerTiming::OnPlayedHand)) {
                applyResolvedJokerEffect(*effectJoker, ctx);
            }

            switch (j.edition) {
            case Edition::Foil:        result.chips += 50; break;
            case Edition::Holographic: result.mult  += 10; break;
            case Edition::Polychrome:  result.xmult *= 1.5; break;
            default: break;
            }

            if (result.chips != chipsBefore) {
                result.events.append({ ScoreEventKind::JokerChip, -1, -1, ji,
                                      result.chips - chipsBefore, 1.0 });
                chipsBefore = result.chips;
            }
            if (result.mult != multBefore) {
                result.events.append({ ScoreEventKind::JokerMult, -1, -1, ji,
                                      result.mult - multBefore, 1.0 });
                multBefore = result.mult;
            }
            if (qAbs(result.xmult - xmultBefore) > 1e-6) {
                result.events.append({ ScoreEventKind::JokerXMult, -1, -1, ji,
                                      0, result.xmult / xmultBefore });
                xmultBefore = result.xmult;
            }
        }
    }

    // 只缓存本手最终分。真正加到回合总分，要等 UI 播完逐张牌/小丑动画之后。
    mPendingHandScore = static_cast<int>(result.chips * result.mult * result.xmult);
    mPendingPlayedIndices = sorted;
    mPendingShattered = shattered;
    mAwaitingScoreFinalize = true;
    mLastResult = result;
    emit handPlayed();
}

void GameState::finalizePlayedHand()
{
    if (!mAwaitingScoreFinalize) return;
    mAwaitingScoreFinalize = false;

    const HandResult result = mLastResult;
    mScore += mPendingHandScore;
    mHandsLeft--;
    mHandLevels[mLastResult.type].played++;

    QVector<QPair<int,int>> idxPairs;
    for (int pidx = 0; pidx < mPendingPlayedIndices.size(); ++pidx)
        idxPairs.append({ mPendingPlayedIndices[pidx], pidx });
    std::sort(idxPairs.begin(), idxPairs.end(),
              [](const auto &a, const auto &b){ return a.first > b.first; });

    for (const auto &pr : idxPairs) {
        if (pr.first < 0 || pr.first >= mHand.size()) continue;
        bool broken = pr.second >= 0 && pr.second < mPendingShattered.size()
                      && mPendingShattered[pr.second];
        if (broken) {
            notifyPlayingCardDestroyed(mHand[pr.first]);
        } else {
            mDeck.discard(mHand[pr.first]);
        }
        mHand.removeAt(pr.first);
    }

    // DNA 复制牌必须等本手计分动画和收牌完成后再放回手牌区。
    // 之前在计分过程中 emit handChanged，会让新牌生成一个脱离正常手牌布局的“幽灵牌”。
    for (const CardData &copy : mPendingDNACopies)
        mHand.append(copy);
    mPendingDNACopies.clear();

    mPendingPlayedIndices.clear();
    mPendingShattered.clear();
    mPendingHandScore = 0;

    if (mDNACopiesCreatedThisPlay > 0) {
        mDNAUsedThisBlind = true;
        mDNAEligibleThisPlay = false;
        mDNACopiesCreatedThisPlay = 0;
    }

    decayEndOfHandJokers();

    applyBossPostPlay();
    applyBossDebuffs();

    emit scoreChanged();
    emit goldChanged();

    if (mScore >= mTargetScore) {
        finishWinningRound();
        return;
    }

    if (mHandsLeft <= 0) {
        checkGameOver();
        return;
    }

    // 只有未过关且还能继续出牌时，才在计分动画和收牌动画结束后补新牌。
    dealCards();
    applyBossDebuffs();
    emit handChanged();
}

void GameState::finishWinningRound()
{
    const HandResult result = mLastResult;

    // 原版 end_of_round 阶段也走“先看这张手牌有没有效果，再由红蜡封/Mime 追加 repetition”。
    // 这里使用解析后的 Mime 数量，Blueprint / Brainstorm 指向 Mime 时也会生效。
    // UI 事件只负责动画提示；实际金币/星球牌仍在这里立刻结算，避免破坏原有流程。
    const int mimeRetriggers = countResolvedJokersOfType(mJokers, JokerType::Mime);
    QVector<ScoreEvent> endRoundEvents;

    for (int i = 0; i < mHand.size(); ++i) {
        const CardData &c = mHand[i];
        if (c.isDebuffed) continue;
        const bool hasEndEffect = (c.enhancement == Enhancement::Gold) || (c.seal == Seal::Blue);
        if (!hasEndEffect) continue;

        const int redSealReps = (c.seal == Seal::Red) ? 1 : 0;
        const int reps = 1 + mimeRetriggers + redSealReps;
        for (int r = 0; r < reps; ++r) {
            if (r > 0 && r <= redSealReps)
                endRoundEvents.append({ ScoreEventKind::RedSealRetrigger, -1, i, -1, 0, 1.0 });

            if (c.enhancement == Enhancement::Gold) {
                addGold(3);
                endRoundEvents.append({ ScoreEventKind::DollarGain, -1, i, -1, 3, 1.0 });
            }
            if (c.seal == Seal::Blue && canAddConsumable()) {
                addConsumable(planetTypeFor(result.type));
                endRoundEvents.append({ ScoreEventKind::BlueSealPlanet, -1, i, -1, 0, 1.0 });
            }
        }
    }

    if (!endRoundEvents.isEmpty())
        emit endRoundCardTriggered(endRoundEvents);

    int blindReward = 0;
    switch (mBlindType) {
    case BlindType::Small: blindReward = 3; break;
    case BlindType::Big:   blindReward = 4; break;
    case BlindType::Boss:  blindReward = 5; break;
    }
    int handBonus = mHandsLeft * Constants::HAND_GOLD;
    mGold += blindReward + handBonus;
    if (mBlindType == BlindType::Boss && mPendingInvestmentBonus > 0) {
        mGold += mPendingInvestmentBonus;
        mPendingInvestmentBonus = 0;
    }
    mOneRoundHandSizeBonus = 0;

    HandResult dummy{};
    TriggerContext rctx{ dummy, *this, mHand, mHand, nullptr };
    for (const Joker &j : mJokers) {
        if (!j.isDebuffed && j.timing == TriggerTiming::OnRoundEnd)
            j.effect(rctx);
    }

    processEndOfRoundJokerExtinctions();

    int interest = qMin(mGold / 5, mInterestCap / 5);
    mGold += interest;

    mPhase = GamePhase::Shop;
    syncShopJokerRules();
    mShop.roll();
    if (mFirstShop) {
        auto &b = mShop.boosterOffersMutable();
        if (b.size() >= 1) {
            ShopOffer &o = b[0];
            o.kind = OfferKind::Pack;
            o.pack = PackKind::Buffoon;
            o.packSize = PackSize::Normal;
            o.cost = 4;
            o.sold = false;
        }
        mFirstShop = false;
    }
    applyTagEffectsToShop();
    emit goldChanged();
    emit roundWon(blindReward, handBonus, interest);
}


void GameState::discardCards(const QVector<int> &indices)
{
    if (indices.isEmpty()) return;
    if (mDiscardLeft <= 0) return;

    QVector<CardData> discarded;
    for (int i : indices) discarded.append(mHand[i]);

    notifyDiscardedCardsForYorick(discarded.size());

    // OnDiscard 小丑触发（当前没有这种类型的小丑,留着挂钩用）
    HandResult dummy{};
    TriggerContext ctx{ dummy, *this, mHand, discarded, nullptr };
    for (const Joker &j : mJokers) {
        if (j.isDebuffed) continue;
        if (j.timing == TriggerTiming::OnDiscard) j.effect(ctx);
    }

    // Purple Seal：被弃的每张 → 生成随机塔罗
    for (const CardData &c : discarded) {
        if (c.seal == Seal::Purple && !c.isDebuffed && canAddConsumable())
            addConsumable(randomTarotType());
    }

    QVector<int> sorted = indices;
    std::sort(sorted.begin(), sorted.end(), std::greater<int>());
    for (int i : sorted) {
        mDeck.discard(mHand[i]);
        mHand.removeAt(i);
    }

    mDiscardLeft--;
    dealCards();
    applyBossDebuffs();

    emit handChanged();
}

int GameState::calcTargetScore() const {
    const int baseScores[] = { 0, 300, 800, 2000, 5000, 11000, 20000, 35000, 50000 };
    double mult = 1.0;
    switch (mBlindType) {
    case BlindType::Small: mult = Constants::SMALL_BLIND_MULT; break;
    case BlindType::Big:   mult = Constants::BIG_BLIND_MULT;   break;
    case BlindType::Boss:  mult = Constants::BOSS_BLIND_MULT;  break;
    }
    int target = static_cast<int>(baseScores[mAnte] * mult);
    if (mBossEffect == BossEffect::TheWall && !hasJokerType(JokerType::Chicot)) target *= 2;
    return target;
}

int GameState::jokerSlots() const {
    int extra = mExtraJokerSlots;
    for (const Joker &j : mJokers)
        if (j.edition == Edition::Negative) extra++;
    return Constants::MAX_JOKER_SLOTS + extra;
}

bool GameState::canAddJokerWithEdition(Edition edition) const {
    // 原版负片小丑自身会给 +1 joker slot，因此即使当前显示为 5/5，
    // 购买负片小丑后也会立即变成 6/6，不应该被满槽判断拦住。
    if (edition == Edition::Negative) return true;
    return canAddJoker();
}

void GameState::applyCardEnhancements(HandResult &result) {
    for (const CardData &c : result.scoringCards) {
        if (c.isDebuffed) continue;

        // 先加筹码
        result.chips += c.chipValue();

        switch (c.enhancement) {
        case Enhancement::Bonus: result.chips += 30; break;
        case Enhancement::Mult: result.mult  += 4; break;
        case Enhancement::Glass: result.mult  *= 2; break;
        case Enhancement::Stone: result.chips += 50; break;
        default: break;
        }

        switch (c.edition) {
        case Edition::Foil: result.chips += 50; break;
        case Edition::Holographic: result.mult += 10; break;
        case Edition::Polychrome: result.mult = static_cast<int>(result.mult * 1.5); break;
        default: break;
        }
    }
}

void GameState::applyJokerEffects(HandResult &result) {
    TriggerContext ctx{
        result, *this, mHand, result.scoringCards, nullptr
    };

    for (const Joker &j : mJokers) {
        if (j.isDebuffed) continue;

        if (j.timing == TriggerTiming::Passive ||
            j.timing == TriggerTiming::OnPlayedHand) {
            j.effect(ctx);
        }

        switch (j.edition) {
        case Edition::Foil:        result.chips += 50; break;
        case Edition::Holographic: result.mult += 10; break;
        case Edition::Polychrome:  result.xmult *= 1.5; break;
        default: break;
        }

        if (j.timing == TriggerTiming::OnScoringCard) {
            for (const CardData &card : result.scoringCards) {
                ctx.currentCard = &card;
                j.effect(ctx);
            }
            ctx.currentCard = nullptr;
        }
    }
}

void GameState::checkGameOver() {
    if (mPhase != GamePhase::Blind) return;       // ← 新增：不在打盲注阶段就不判断
    if (mHandsLeft <= 0 && mScore < mTargetScore) {
        mPhase = GamePhase::GameOver;
        emit gameOver(false);
    }
}

int GameState::roundReward() const {
    int base = 0;
    switch (mBlindType) {
    case BlindType::Small: base = 3; break;
    case BlindType::Big: base = 4; break;
    case BlindType::Boss: base = 5; break;
    }
    int handBonus = mHandsLeft * Constants::HAND_GOLD;
    int interest = qMin(mGold / 5, mInterestCap / 5);
    return base + handBonus + interest;
}

void GameState::rerollShop() {
    if (mPhase != GamePhase::Shop) return;
    int cost = mShop.rerollCost();
    if (mGold < cost) return;

    mGold -= cost;
    mShop.onReroll();
    mShop.rerollShopOnly();   // ← 只 reroll 商品区,不动 booster

    emit goldChanged();
    emit shopChanged();
}

void GameState::applyBossDebuffs() {
    bool bossDisabled = hasJokerType(JokerType::Chicot);
    for (CardData &c : mHand) {
        c.isDebuffed = !bossDisabled && bossDebuffsCard(mBossEffect, c);
    }
}

void GameState::applyBossPostPlay() {
    if (!hasJokerType(JokerType::Chicot) && mBossEffect == BossEffect::TheHook && mHand.size() >= 2) {
        // 随机弃 2 张
        for (int i = 0; i < 2 && !mHand.isEmpty(); ++i) {
            int idx = QRandomGenerator::global()->bounded(mHand.size());
            mDeck.discard(mHand[idx]);
            mHand.removeAt(idx);
        }
    }
}

bool GameState::addConsumable(ConsumableType t) {
    if (!canAddConsumable()) return false;
    mConsumables.append(createConsumable(t));
    emit consumablesChanged();
    return true;
}

bool GameState::addFoolCopyConsumable() {
    // 原版 The Fool：生成上一张使用过的塔罗/星球/幻灵牌，且不复制愚者自己。
    if (!mHasLastUsedConsumable || mLastUsedConsumable == ConsumableType::Tarot_Fool) return false;
    if (!canAddConsumable()) return false;
    mConsumables.append(createConsumable(mLastUsedConsumable));
    emit consumablesChanged();
    return true;
}

bool GameState::useConsumable(int idx, const QVector<int> &selectedHandIdx) {
    if (mPhase == GamePhase::GameOver) return false;
    if (idx < 0 || idx >= mConsumables.size()) return false;

    const Consumable &c = mConsumables[idx];
    if (c.needsSelection > 0 &&
        selectedHandIdx.size() < c.needsSelection) return false;

    QVector<int> sel = selectedHandIdx;
    std::sort(sel.begin(), sel.end());
    sel.erase(std::unique(sel.begin(), sel.end()), sel.end());
    if (c.maxSelection > 0 && sel.size() > c.maxSelection)
        return false;

    UseContext ctx{ *this, sel };
    c.effect(ctx);

    if (c.type != ConsumableType::Tarot_Fool) {
        mLastUsedConsumable = c.type;
        mHasLastUsedConsumable = true;
    }

    mConsumables.removeAt(idx);
    emit consumablesChanged();
    return true;
}

bool GameState::sellConsumable(int idx) {
    if (idx < 0 || idx >= mConsumables.size()) return false;
    int v = mConsumables[idx].sellValue;
    mConsumables.removeAt(idx);
    mGold += v;
    emit consumablesChanged();
    emit goldChanged();
    return true;
}

bool GameState::sellJoker(int idx) {
    if (idx < 0 || idx >= mJokers.size()) return false;
    int v = qMax(1, mJokers[idx].sellValue);
    mJokers.removeAt(idx);
    mGold += v;
    syncShopJokerRules();
    if (mPhase == GamePhase::Shop) {
        mShop.refreshCurrentOfferCosts();
        mShop.ensureShopOfferCount();
    }
    emit jokersChanged();
    emit goldChanged();
    emit shopChanged();
    return true;
}

bool GameState::moveJoker(int from, int to) {
    if (from < 0 || from >= mJokers.size() || to < 0 || to >= mJokers.size()) return false;
    if (from == to) return true;
    mJokers.move(from, to);
    emit jokersChanged();
    return true;
}

bool GameState::moveHandCard(int from, int to) {
    if (from < 0 || from >= mHand.size() || to < 0 || to >= mHand.size()) return false;
    if (from == to) return true;
    mHand.move(from, to);
    // 手动移动只影响当前手牌展示；之后补牌仍按玩家上次选择的“点数/花色”自动理牌。
    emit handChanged();
    return true;
}

void GameState::collectRoundCardsToDeck() {
    // RoundEnd → Shop 时调用：把最后留在手上的牌、以及出牌区已弃进弃牌堆的牌
    // 统一合回摸牌堆并洗牌，保证商店开包/下一盲注看到的是完整当前牌组。
    if (!mHand.isEmpty()) {
        mDeck.returnCards(mHand);
        mHand.clear();
    }
    mDeck.reset();
    emit handChanged();
}

bool GameState::buyShopOffer(int idx) {
    if (mPhase != GamePhase::Shop) return false;
    if (!mShop.canBuyShop(idx, mGold)) return false;
    const ShopOffer &o = mShop.shopOffers()[idx];

    if (o.kind == OfferKind::Joker) {
        // 负片小丑自带 +1 小丑槽，原版允许在槽位满时买入负片。
        // 不要用 canAddJoker() 单独判断，否则 5/5 时会把负片小丑误拦截。
        if (!canAddJokerWithEdition(o.jokerEdition)) return false;
        ShopOffer t = mShop.takeShopOffer(idx);
        mGold -= t.cost;
        Joker j = createJoker(t.joker);
        j.edition = t.jokerEdition;
        switch (j.edition) {
        case Edition::Foil:
        case Edition::Holographic: j.sellValue += 1; break;
        case Edition::Polychrome:  j.sellValue += 2; break;
        case Edition::Negative:    j.sellValue += 3; break;
        default: break;
        }
        mJokers.append(j);
        syncShopJokerRules();
        emit jokersChanged();
    } else if (o.kind == OfferKind::Tarot ||
               o.kind == OfferKind::Planet ||
               o.kind == OfferKind::Spectral) {
        if (!canAddConsumable()) return false;
        ShopOffer t = mShop.takeShopOffer(idx);
        mGold -= t.cost;
        mConsumables.append(createConsumable(t.consumable));
        emit consumablesChanged();
    } else if (o.kind == OfferKind::PlayingCard) {
        ShopOffer t = mShop.takeShopOffer(idx);
        mGold -= t.cost;
        mDeck.addCard(t.playingCard);
        for (Joker &j : mJokers) if (!j.isDebuffed && j.type == JokerType::Hologram) ++j.counter;
    } else {
        return false;
    }

    emit goldChanged();
    emit shopChanged();
    return true;
}

bool GameState::buyVoucherOffer(int idx) {
    if (mPhase != GamePhase::Shop) return false;
    if (!mShop.canBuyVoucher(idx, mGold)) return false;

    ShopOffer t = mShop.takeVoucherOffer(idx);
    mGold -= t.cost;
    mRedeemedVouchers.append(t.voucher);
    mShop.setRedeemedVouchers(mRedeemedVouchers);
    applyVoucher(t.voucher);
    syncShopJokerRules();
    mShop.refreshCurrentOfferCosts();
    mShop.ensureShopOfferCount();

    emit goldChanged();
    emit shopChanged();
    emit consumablesChanged();
    return true;
}

bool GameState::hasVoucher(VoucherType t) const {
    return mRedeemedVouchers.contains(t);
}

void GameState::applyVoucher(VoucherType t) {
    switch (t) {
    case VoucherType::Overstock:
    case VoucherType::OverstockPlus:
        mShop.changeShopSlots(1);
        mShop.rerollShopOnly();
        break;
    case VoucherType::ClearanceSale:
        mShop.setDiscountPercent(25);
        break;
    case VoucherType::Liquidation:
        mShop.setDiscountPercent(50);
        break;
    case VoucherType::RerollSurplus:
        mShop.setRerollDiscount(2);
        break;
    case VoucherType::RerollGlut:
        mShop.setRerollDiscount(4);
        break;
    case VoucherType::TarotMerchant:
        mShop.setTarotRate(9.6);
        break;
    case VoucherType::TarotTycoon:
        mShop.setTarotRate(32.0);
        break;
    case VoucherType::PlanetMerchant:
        mShop.setPlanetRate(9.6);
        break;
    case VoucherType::PlanetTycoon:
        mShop.setPlanetRate(32.0);
        break;
    case VoucherType::MagicTrick:
        // 原版 v_magic_trick: playing_card_rate = 4
        mShop.setPlayingCardRate(4.0);
        break;
    case VoucherType::Illusion:
        mShop.setPlayingCardsEnhanced(true);
        break;
    case VoucherType::CrystalBall:
        mExtraConsumableSlots += 1;
        break;
    case VoucherType::OmenGlobe:
        // generatePackContent 会在奥秘包内按概率把塔罗替换成幻灵。
        break;
    case VoucherType::Grabber:
    case VoucherType::NachoTong:
        mExtraHandsPerRound += 1;
        break;
    case VoucherType::Wasteful:
    case VoucherType::Recyclomancy:
        mExtraDiscardsPerRound += 1;
        break;
    case VoucherType::SeedMoney:
        mInterestCap = 50;
        break;
    case VoucherType::MoneyTree:
        mInterestCap = 100;
        break;
    case VoucherType::PaintBrush:
    case VoucherType::Palette:
        mExtraHandSize += 1;
        break;
    case VoucherType::Antimatter:
        mExtraJokerSlots += 1;
        break;
    case VoucherType::Hieroglyph:
    case VoucherType::Petroglyph:
        mAnte = qMax(1, mAnte - 1);
        mExtraHandsPerRound -= 1;
        break;
    case VoucherType::Hone:
        mShop.setJokerEditionRate(2.0);
        break;
    case VoucherType::GlowUp:
        mShop.setJokerEditionRate(4.0);
        break;
    case VoucherType::Telescope:
    case VoucherType::Observatory:
    case VoucherType::Blank:
    case VoucherType::DirectorsCut:
    case VoucherType::Retcon:
        // 与版本概率 / Boss 重掷 / 天文台持有星球牌联动的细节后续继续细化。
        break;
    }
}

int GameState::consumableSlots() const {
    int negativeSlots = 0;
    for (const Consumable &c : mConsumables) if (c.negative) ++negativeSlots;
    return Constants::MAX_CONSUMABLE_SLOTS + mExtraConsumableSlots + negativeSlots;
}

int GameState::handSize() const {
    int size = Constants::HAND_SIZE + mExtraHandSize + mOneRoundHandSizeBonus;
    for (const Joker &j : mJokers) {
        if (!j.isDebuffed && j.type == JokerType::Stuntman) size -= 2;
    }
    if (mPhase == GamePhase::Blind && mBlindType == BlindType::Boss &&
        mBossEffect == BossEffect::TheManacle && !hasJokerType(JokerType::Chicot)) size -= 1;
    return qMax(1, size);
}


bool GameState::dnaCanTriggerThisPlay() const
{
    return mPhase == GamePhase::Blind && mDNAEligibleThisPlay;
}

void GameState::createDNACopy(const CardData &card)
{
    if (!dnaCanTriggerThisPlay()) return;
    CardData copy = card;
    copy.assignNewUid();              // 这是新复制出的实体牌，不能沿用原牌 uid
    copy.faceUp = true;
    copy.isDebuffed = false;
    // 原版 DNA 的复制牌在计分事件结束后才进入手牌区。这里先暂存，
    // finalizePlayedHand() 收走本手打出的牌后，再把复制牌插到手牌右侧。
    mPendingDNACopies.append(copy);
    ++mDNACopiesCreatedThisPlay;
}

void GameState::decayEndOfHandJokers()
{
    bool changed = false;
    for (Joker &j : mJokers) {
        if (j.type == JokerType::IceCream && j.counter > 0) {
            j.counter = qMax(0, j.counter - 5);
            changed = true;
        }
    }
    if (changed) emit jokersChanged();
}

void GameState::scoreCard(const CardData &card, HandResult &result, int playedIdx)
{
    // 1) chip:Stone 不加点数 chip
    if (card.enhancement != Enhancement::Stone) {
        int v = card.chipValue() + qMax(0, card.permanentBonusChips);
        if (v > 0) {
            result.chips += v;
            result.events.append({ ScoreEventKind::ScoringCardChip, playedIdx, -1, -1, v, 1.0 });
        }
    }

    // 2) enhancement
    switch (card.enhancement) {
    case Enhancement::Bonus:
        result.chips += 30;
        result.events.append({ ScoreEventKind::ScoringCardChip, playedIdx, -1, -1, 30, 1.0 });
        break;
    case Enhancement::Mult:
        result.mult += 4;
        result.events.append({ ScoreEventKind::EnhancementMult, playedIdx, -1, -1, 4, 1.0 });
        break;
    case Enhancement::Glass:
        result.xmult *= 2.0;
        result.events.append({ ScoreEventKind::EnhancementXMult, playedIdx, -1, -1, 0, 2.0 });
        break;
    case Enhancement::Stone:
        result.chips += 50;
        result.events.append({ ScoreEventKind::ScoringCardChip, playedIdx, -1, -1, 50, 1.0 });
        break;
    case Enhancement::Lucky:
        if (QRandomGenerator::global()->bounded(5) == 0) {
            result.mult += 20;
            result.events.append({ ScoreEventKind::EnhancementMult, playedIdx, -1, -1, 20, 1.0 });
        }
        if (QRandomGenerator::global()->bounded(15) == 0) {
            addGold(20);
            result.events.append({ ScoreEventKind::DollarGain, playedIdx, -1, -1, 20, 1.0 });
        }
        break;
    default: break;
    }

    // 3) edition
    switch (card.edition) {
    case Edition::Foil:
        result.chips += 50;
        result.events.append({ ScoreEventKind::EditionChip, playedIdx, -1, -1, 50, 1.0 });
        break;
    case Edition::Holographic:
        result.mult += 10;
        result.events.append({ ScoreEventKind::EditionMult, playedIdx, -1, -1, 10, 1.0 });
        break;
    case Edition::Polychrome:
        result.xmult *= 1.5;
        result.events.append({ ScoreEventKind::EditionXMult, playedIdx, -1, -1, 0, 1.5 });
        break;
    default: break;
    }

    // 4) OnScoringCard 小丑:之前直接调 effect,现在需要在 ctx 里带 idx 让 effect 写事件
    // 但 effect 是 lambda 修改 ctx.result,没法知道是哪个 joker。
    // 简化做法:在 effect 调用前后比较 chips/mult 差值。
    int chipsBefore = result.chips, multBefore = result.mult;
    double xmultBefore = result.xmult;

    for (int ji = 0; ji < mJokers.size(); ++ji) {
        const Joker &j = mJokers[ji];
        if (j.isDebuffed) continue;

        const Joker *effectJoker = resolveCopiedJoker(mJokers, ji);

        if (!effectJoker || effectJoker->isDebuffed || effectJoker->timing != TriggerTiming::OnScoringCard) continue;

        TriggerContext ctx{ result, *this, mHand, result.scoringCards, &card };
        applyResolvedJokerEffect(*effectJoker, ctx);

        if (result.chips != chipsBefore) {
            result.events.append({ ScoreEventKind::JokerChip, playedIdx, -1, ji,
                                  result.chips - chipsBefore, 1.0 });
            chipsBefore = result.chips;
        }
        if (result.mult != multBefore) {
            result.events.append({ ScoreEventKind::JokerMult, playedIdx, -1, ji,
                                  result.mult - multBefore, 1.0 });
            multBefore = result.mult;
        }
        if (qAbs(result.xmult - xmultBefore) > 1e-6) {
            result.events.append({ ScoreEventKind::JokerXMult, playedIdx, -1, ji,
                                  0, result.xmult / xmultBefore });
            xmultBefore = result.xmult;
        }
    }

    // 5) Gold Seal +$3
    if (card.seal == Seal::Gold) {
        addGold(3);
        result.events.append({ ScoreEventKind::DollarGain, playedIdx, -1, -1, 3, 1.0 });
    }
}

ConsumableType GameState::planetTypeFor(HandType t)
{
    switch (t) {
    case HandType::HighCard:      return ConsumableType::Planet_Pluto;
    case HandType::Pair:          return ConsumableType::Planet_Mercury;
    case HandType::TwoPair:       return ConsumableType::Planet_Uranus;
    case HandType::ThreeOfAKind:  return ConsumableType::Planet_Venus;
    case HandType::Straight:      return ConsumableType::Planet_Saturn;
    case HandType::Flush:         return ConsumableType::Planet_Jupiter;
    case HandType::FullHouse:     return ConsumableType::Planet_Earth;
    case HandType::FourOfAKind:   return ConsumableType::Planet_Mars;
    case HandType::StraightFlush:
    case HandType::RoyalFlush:    return ConsumableType::Planet_Neptune;
    case HandType::FiveOfAKind:   return ConsumableType::Planet_PlanetX;
    case HandType::FlushHouse:    return ConsumableType::Planet_Ceres;
    case HandType::FlushFive:     return ConsumableType::Planet_Eris;
    }
    return ConsumableType::Planet_Pluto;
}

bool GameState::buyPack(int idx, PackContent &out)
{
    if (mPhase != GamePhase::Shop) return false;
    if (!mShop.canBuyBooster(idx, mGold)) return false;
    const ShopOffer &o = mShop.boosterOffers()[idx];
    if (o.kind != OfferKind::Pack) return false;

    ShopOffer t = mShop.takeBoosterOffer(idx);
    mGold -= t.cost;
    out = generatePackContent(t.pack, t.packSize, hasVoucher(VoucherType::OmenGlobe), hasVoucher(VoucherType::Telescope),
                              ConsumableType::Planet_Pluto,
                              ownedJokerTypes(), hasJokerDuplicateBypass(), mGrosMichelExtinct);

    emit goldChanged();
    emit shopChanged();
    return true;
}


QVector<CardData> GameState::drawPackHand()
{
    QVector<CardData> out;
    int n = handSize();
    for (int i = 0; i < n; ++i) {
        if (mDeck.isEmpty()) mDeck.reset();
        if (mDeck.isEmpty()) break;
        out.append(mDeck.draw());
    }
    emit scoreChanged();
    return out;
}

void GameState::returnPackHand(const QVector<CardData> &packHand)
{
    // 开包界面的临时手牌来自牌组。没被摧毁的牌要放回牌组；
    // 被 Immolate 等效果删除的牌不会出现在 packHand 里，也就不会放回。
    for (const CardData &c : packHand) {
        mDeck.addCard(c);
    }
    emit scoreChanged();
}

static QVector<int> normalizedSelection(const QVector<int> &selected, int size, int maxSelection)
{
    QVector<int> sel = selected;
    std::sort(sel.begin(), sel.end());
    sel.erase(std::unique(sel.begin(), sel.end()), sel.end());
    sel.erase(std::remove_if(sel.begin(), sel.end(), [size](int idx) {
        return idx < 0 || idx >= size;
    }), sel.end());
    return sel;
}

static bool applyConsumableTypeToPackHand(GameState &state,
                                          ConsumableType type,
                                          const QVector<int> &selectedPackHandIdx,
                                          QVector<CardData> &packHand)
{
    Consumable c = createConsumable(type);
    QVector<int> sel = normalizedSelection(selectedPackHandIdx, packHand.size(), c.maxSelection);
    if (sel.size() < c.needsSelection) return false;
    if (c.maxSelection > 0 && sel.size() > c.maxSelection) return false;

    auto enhance = [&](Enhancement e, int maxN) {
        QVector<int> use = normalizedSelection(sel, packHand.size(), maxN);
        int applied = 0;
        for (int idx : use) { if (applied++ >= maxN) break; packHand[idx].enhancement = e; }
    };
    auto seal = [&](Seal s, int maxN) {
        QVector<int> use = normalizedSelection(sel, packHand.size(), maxN);
        int applied = 0;
        for (int idx : use) { if (applied++ >= maxN) break; packHand[idx].seal = s; }
    };
    auto randomEdition = [&](int maxN) {
        QVector<int> use = normalizedSelection(sel, packHand.size(), maxN);
        int applied = 0;
        for (int idx : use) {
            if (applied++ >= maxN) break;
            int r = QRandomGenerator::global()->bounded(3);
            if      (r == 0) packHand[idx].edition = Edition::Foil;
            else if (r == 1) packHand[idx].edition = Edition::Holographic;
            else             packHand[idx].edition = Edition::Polychrome;
        }
    };
    auto destroyRandomPackCards = [&](int count, int goldGain) {
        QVector<int> idx;
        for (int i = 0; i < packHand.size(); ++i) idx.append(i);
        for (int i = idx.size() - 1; i > 0; --i) {
            int j = QRandomGenerator::global()->bounded(i + 1);
            idx.swapItemsAt(i, j);
        }
        int n = qMin(count, idx.size());
        idx = idx.mid(0, n);
        std::sort(idx.begin(), idx.end(), std::greater<int>());
        for (int i : idx) {
            if (i < 0 || i >= packHand.size()) continue;
            state.notifyPlayingCardDestroyed(packHand[i]);
            packHand.removeAt(i);
        }
        if (n > 0) state.addGold(goldGain);
    };
    auto copySelectedPackCardTwice = [&]() {
        if (sel.size() != 1) return false;
        int idx = sel.first();
        if (idx < 0 || idx >= packHand.size()) return false;
        CardData c = packHand[idx];
        packHand.append(c);
        packHand.append(c);
        return true;
    };
    auto setPackSuit = [&]() {
        Suit ss = static_cast<Suit>(QRandomGenerator::global()->bounded(4));
        for (CardData &c : packHand) if (c.enhancement != Enhancement::Stone) c.suit = ss;
    };
    auto setPackRank = [&]() {
        Rank rr = static_cast<Rank>(QRandomGenerator::global()->bounded(13) + 2);
        for (CardData &c : packHand) if (c.enhancement != Enhancement::Stone) c.rank = rr;
        state.addPermanentHandSizeBonus(-1);
    };

    auto rankUpPack = [&](int maxN) {
        QVector<int> use = normalizedSelection(sel, packHand.size(), maxN);
        for (int idx : use) {
            if (idx < 0 || idx >= packHand.size()) continue;
            if (packHand[idx].enhancement != Enhancement::Stone) {
                if (packHand[idx].rank != Rank::Ace)
                    packHand[idx].rank = static_cast<Rank>(static_cast<int>(packHand[idx].rank) + 1);
            }
        }
    };
    auto suitPack = [&](Suit ss, int maxN) {
        QVector<int> use = normalizedSelection(sel, packHand.size(), maxN);
        for (int idx : use) {
            if (idx < 0 || idx >= packHand.size()) continue;
            if (packHand[idx].enhancement != Enhancement::Stone) packHand[idx].suit = ss;
        }
    };
    auto deathPack = [&]() -> bool {
        if (sel.size() != 2) return false;
        int a = sel[0], b = sel[1];
        if (a < 0 || a >= packHand.size() || b < 0 || b >= packHand.size() || a == b) return false;
        packHand[a] = packHand[b];
        return true;
    };

    switch (type) {
    case ConsumableType::Tarot_Fool:       return state.addFoolCopyConsumable();
    case ConsumableType::Tarot_Magician:   enhance(Enhancement::Lucky, 2); break;
    case ConsumableType::Tarot_HighPriestess:
        for (int i = 0; i < 2; ++i) { if (!state.addConsumable(randomPlanetType())) break; }
        break;
    case ConsumableType::Tarot_Empress:    enhance(Enhancement::Mult, 2); break;
    case ConsumableType::Tarot_Emperor:
        for (int i = 0; i < 2; ++i) { if (!state.addConsumable(randomTarotType())) break; }
        break;
    case ConsumableType::Tarot_Hierophant: enhance(Enhancement::Bonus, 2); break;
    case ConsumableType::Tarot_Chariot:    enhance(Enhancement::Steel, 1); break;
    case ConsumableType::Tarot_Lovers:     enhance(Enhancement::Wild, 1); break;
    case ConsumableType::Tarot_Justice:    enhance(Enhancement::Glass, 1); break;
    case ConsumableType::Tarot_Tower:      enhance(Enhancement::Stone, 1); break;
    case ConsumableType::Tarot_HangedMan: {
        QVector<int> use = normalizedSelection(sel, packHand.size(), 2);
        std::sort(use.begin(), use.end(), std::greater<int>());
        for (int idx : use) {
            if (idx < 0 || idx >= packHand.size()) continue;
            state.notifyPlayingCardDestroyed(packHand[idx]);
            packHand.removeAt(idx);
        }
        break;
    }
    case ConsumableType::Tarot_Hermit: {
        int gain = qMin(state.gold(), 20);
        state.addGold(gain);
        break;
    }
    case ConsumableType::Tarot_Wheel:
        if (QRandomGenerator::global()->bounded(4) == 0) {
            constexpr Edition editions[] = { Edition::Foil, Edition::Holographic, Edition::Polychrome };
            state.setRandomEditionlessJoker(editions[QRandomGenerator::global()->bounded(3)], false, false);
        }
        break;
    case ConsumableType::Tarot_Strength:   rankUpPack(2); break;
    case ConsumableType::Tarot_Death:      return deathPack();
    case ConsumableType::Tarot_Temperance: {
        int total = 0;
        for (const Joker &j : state.jokers()) total += qMax(1, j.sellValue);
        state.addGold(qMin(total, 50));
        break;
    }
    case ConsumableType::Tarot_Devil:      enhance(Enhancement::Gold, 1); break;
    case ConsumableType::Tarot_Star:       suitPack(Suit::Diamonds, 3); break;
    case ConsumableType::Tarot_Moon:       suitPack(Suit::Clubs, 3); break;
    case ConsumableType::Tarot_Sun:        suitPack(Suit::Hearts, 3); break;
    case ConsumableType::Tarot_Judgement:  state.addRandomRareJoker(); break;
    case ConsumableType::Tarot_World:      suitPack(Suit::Spades, 3); break;

    case ConsumableType::Planet_Pluto:     state.levelUpHand(HandType::HighCard); break;
    case ConsumableType::Planet_Mercury:   state.levelUpHand(HandType::Pair); break;
    case ConsumableType::Planet_Uranus:    state.levelUpHand(HandType::TwoPair); break;
    case ConsumableType::Planet_Venus:     state.levelUpHand(HandType::ThreeOfAKind); break;
    case ConsumableType::Planet_Saturn:    state.levelUpHand(HandType::Straight); break;
    case ConsumableType::Planet_Jupiter:   state.levelUpHand(HandType::Flush); break;
    case ConsumableType::Planet_Earth:     state.levelUpHand(HandType::FullHouse); break;
    case ConsumableType::Planet_Mars:      state.levelUpHand(HandType::FourOfAKind); break;
    case ConsumableType::Planet_Neptune:   state.levelUpHand(HandType::StraightFlush); break;
    case ConsumableType::Planet_PlanetX:   state.levelUpHand(HandType::FiveOfAKind); break;
    case ConsumableType::Planet_Ceres:     state.levelUpHand(HandType::FlushHouse); break;
    case ConsumableType::Planet_Eris:      state.levelUpHand(HandType::FlushFive); break;

    case ConsumableType::Spectral_Familiar: {
        if (!packHand.isEmpty()) {
            int victim = QRandomGenerator::global()->bounded(packHand.size());
            state.notifyPlayingCardDestroyed(packHand[victim]);
            packHand.removeAt(victim);
        }
        for (int i = 0; i < 3; ++i) {
            CardData c; c.suit = static_cast<Suit>(QRandomGenerator::global()->bounded(4));
            constexpr Rank faces[] = { Rank::Jack, Rank::Queen, Rank::King };
            c.rank = faces[QRandomGenerator::global()->bounded(3)];
            c.enhancement = Enhancement::Bonus;
            packHand.append(c);
        }
        break;
    }
    case ConsumableType::Spectral_Grim: {
        if (!packHand.isEmpty()) {
            int victim = QRandomGenerator::global()->bounded(packHand.size());
            state.notifyPlayingCardDestroyed(packHand[victim]);
            packHand.removeAt(victim);
        }
        for (int i = 0; i < 2; ++i) {
            CardData c; c.suit = static_cast<Suit>(QRandomGenerator::global()->bounded(4));
            c.rank = Rank::Ace; c.enhancement = Enhancement::Bonus;
            packHand.append(c);
        }
        break;
    }
    case ConsumableType::Spectral_Incantation: {
        if (!packHand.isEmpty()) {
            int victim = QRandomGenerator::global()->bounded(packHand.size());
            state.notifyPlayingCardDestroyed(packHand[victim]);
            packHand.removeAt(victim);
        }
        for (int i = 0; i < 4; ++i) {
            CardData c; c.suit = static_cast<Suit>(QRandomGenerator::global()->bounded(4));
            c.rank = static_cast<Rank>(QRandomGenerator::global()->bounded(9) + 2);
            c.enhancement = Enhancement::Bonus;
            packHand.append(c);
        }
        break;
    }
    case ConsumableType::Spectral_Talisman: seal(Seal::Gold, 1); break;
    case ConsumableType::Spectral_Aura:     randomEdition(1); break;
    case ConsumableType::Spectral_Wraith:
        if (state.addRandomRareJoker()) state.addGold(-state.gold());
        break;
    case ConsumableType::Spectral_Sigil:    setPackSuit(); break;
    case ConsumableType::Spectral_Ouija:    setPackRank(); break;
    case ConsumableType::Spectral_Ectoplasm: state.setRandomEditionlessJoker(Edition::Negative, false, true); break;
    case ConsumableType::Spectral_DejaVu:   seal(Seal::Red, 1); break;
    case ConsumableType::Spectral_Hex:      state.setRandomEditionlessJoker(Edition::Polychrome, true, false); break;
    case ConsumableType::Spectral_Trance:   seal(Seal::Blue, 1); break;
    case ConsumableType::Spectral_Medium:   seal(Seal::Purple, 1); break;
    case ConsumableType::Spectral_Cryptid:
        return copySelectedPackCardTwice();
    case ConsumableType::Spectral_Ankh:
        state.duplicateRandomJokerAndDestroyOthers();
        break;
    case ConsumableType::Spectral_Immolate:
        destroyRandomPackCards(5, 20);
        break;
    case ConsumableType::Spectral_Soul:
        return state.addRandomLegendaryJoker();
    case ConsumableType::Spectral_BlackHole:
        state.levelUpAllHands(1);
        break;
    }
    return true;
}

bool GameState::applyPackChoice(const PackContent &pack, int chosenIdx,
                                const QVector<int> &selectedPackHandIdx,
                                QVector<CardData> &packHand)
{
    if (chosenIdx < 0) return false;

    switch (pack.kind) {
    case PackKind::Standard:
        if (chosenIdx >= pack.standardCards.size()) return false;
        mDeck.addCard(pack.standardCards[chosenIdx]);
        for (Joker &j : mJokers) if (!j.isDebuffed && j.type == JokerType::Hologram) ++j.counter;
        return true;

    case PackKind::Arcana:
    case PackKind::Celestial:
    case PackKind::Spectral:
        if (chosenIdx >= pack.consumables.size()) return false;
        return applyConsumableTypeToPackHand(*this, pack.consumables[chosenIdx],
                                             selectedPackHandIdx, packHand);

    case PackKind::Buffoon:
        if (chosenIdx >= pack.jokers.size()) return false;
        if (!canAddJoker()) return false;
        mJokers.append(createJoker(pack.jokers[chosenIdx]));
        syncShopJokerRules();
        emit jokersChanged();
        emit shopChanged();
        return true;
    }
    return false;
}

bool GameState::useConsumableOnPackHand(int consumableIdx,
                                        const QVector<int> &selectedPackHandIdx,
                                        QVector<CardData> &packHand)
{
    if (consumableIdx < 0 || consumableIdx >= mConsumables.size()) return false;
    Consumable c = mConsumables[consumableIdx];
    QVector<int> sel = normalizedSelection(selectedPackHandIdx, packHand.size(), c.maxSelection);
    if (sel.size() < c.needsSelection) return false;
    if (c.maxSelection > 0 && sel.size() > c.maxSelection) return false;

    bool ok = applyConsumableTypeToPackHand(*this, c.type, sel, packHand);
    if (!ok) return false;

    if (c.type != ConsumableType::Tarot_Fool) {
        mLastUsedConsumable = c.type;
        mHasLastUsedConsumable = true;
    }

    mConsumables.removeAt(consumableIdx);
    emit consumablesChanged();
    return true;
}

void GameState::startGame()
{
    mDeck = Deck();
    mHand.clear();
    mAwaitingScoreFinalize = false;
    mPendingHandScore = 0;
    mPendingPlayedIndices.clear();
    mPendingShattered.clear();
    mGold = Constants::INITIAL_GOLD;
    mAnte = 1;
    mScore = 0;
    mJokers.clear();
    mShop = Shop();
    syncShopJokerRules();
    mHandLevels.clear();
    mConsumables.clear();
    mRedeemedVouchers.clear();
    mExtraConsumableSlots = 0;
    mExtraHandsPerRound = 0;
    mExtraDiscardsPerRound = 0;
    mExtraHandSize = 0;
    mInterestCap = Constants::INTEREST_MAX;
    mExtraJokerSlots = 0;
    mOneRoundHandSizeBonus = 0;
    mPendingInvestmentBonus = 0;
    mTagVoucherNextShop = false;
    mHasTagFreePack = false;
    mActiveTags.clear();
    mLastSkippedTag = TagType::Skip;
    mBlindIdx = 0;
    mPendingBossEffect = BossEffect::None;
    mBossRerollsUsedThisAnte = 0;
    mBossMouthHasHand = false;
    mBossEyePlayedHands.clear();
    mSortMode = HandSortMode::ByRank;
    mCainoXMult = 1.0;
    mYorickXMult = 1.0;
    mYorickDiscardsRemaining = 23;
    mHasLastUsedConsumable = false;
    mLastUsedConsumable = ConsumableType::Tarot_Fool;
    mGrosMichelExtinct = false;

    for (int i = 0; i < 3; ++i)
        mBlindStates[i] = (i == 0) ? BlindState::Current : BlindState::Upcoming;

    mFirstShop = true;
    prepareBlindTags();
    enterBlindSelect();

    emit jokersChanged();
    emit consumablesChanged();
}

void GameState::prepareBlindTags()
{
    mBlindTags[0] = randomTagForAnte(mAnte);
    do {
        mBlindTags[1] = randomTagForAnte(mAnte);
    } while (mBlindTags[1] == mBlindTags[0]);
}

TagType GameState::blindTag(int idx) const
{
    if (idx == 0 || idx == 1) return mBlindTags[idx];
    return TagType::Skip;
}

void GameState::enterBlindSelect() {
    mPhase = GamePhase::BlindSelect;
    mBossEffect = BossEffect::None;  // 已激活 Boss 效果只存在于 Boss 对局内，不能泄漏到下个小盲注。
    mBlindType = static_cast<BlindType>(mBlindIdx);
    if (mPendingBossEffect == BossEffect::None)
        mPendingBossEffect = randomBossEffect(mAnte);
    mTargetScore = calcTargetScore();
    emit blindSelectEntered();
}

void GameState::selectCurrentBlind()
{
    if (mPhase != GamePhase::BlindSelect) return;
    startBlind(mBlindType);
}

void GameState::startBlind(BlindType type)
{
    mBlindType = type;
    mScore = 0;
    mHandsLeft = qMax(1, Constants::INITIAL_HANDS + mExtraHandsPerRound);
    mBlindStartingHands = mHandsLeft;
    mDNAUsedThisBlind = false;
    mDNAEligibleThisPlay = false;
    mDNACopiesCreatedThisPlay = 0;
    mDiscardLeft = qMax(0, Constants::INITIAL_DISCARDS + mExtraDiscardsPerRound);

    if (type == BlindType::Boss) {
        mBossEffect = mPendingBossEffect;     // 用预先确定的
        if (!hasJokerType(JokerType::Chicot)) {
            if (mBossEffect == BossEffect::TheNeedle) { mHandsLeft = 1; mBlindStartingHands = 1; }
            if (mBossEffect == BossEffect::TheWater)  mDiscardLeft = 0;
        }
        mBossMouthHasHand = false;
        mBossEyePlayedHands.clear();
    } else {
        mBossEffect = BossEffect::None;
    }

    mTargetScore = calcTargetScore();
    mPhase = GamePhase::Blind;

    // 原版每个盲注开始都会用当前完整牌组重新洗牌；上一盲注结束时手里没打出的牌也要回到牌组。
    mDeck.returnCards(mHand);
    mHand.clear();
    mDeck.reset();
    dealCards();
    applyBossDebuffs();

    emit handChanged();
    emit blindStarted();
}

void GameState::nextBlind()
{
    mBlindStates[mBlindIdx] = BlindState::Defeated;
    mBlindIdx++;
    if (mBlindIdx > 2) {
        mBlindIdx = 0;
        mAnte++;
        mPendingBossEffect = BossEffect::None;
        mBossRerollsUsedThisAnte = 0;
        prepareBlindTags();
        if (mAnte > 8) {
            mPhase = GamePhase::GameOver;
            emit gameOver(true);
            return;
        }
    }
    // 不论是新 ante 还是同 ante 内推进,都重算所有状态
    for (int i = 0; i < 3; ++i) {
        if (i < mBlindIdx)       mBlindStates[i] = BlindState::Defeated;
        else if (i == mBlindIdx) mBlindStates[i] = BlindState::Current;
        else                     mBlindStates[i] = BlindState::Upcoming;
    }
    enterBlindSelect();
}

void GameState::skipCurrentBlind()
{
    if (mPhase != GamePhase::BlindSelect) return;
    if (mBlindIdx >= 2) return;

    TagType gained = mBlindTags[mBlindIdx];
    mLastSkippedTag = gained;
    mActiveTags.append(gained);
    applySkippedTag(gained);

    mBlindStates[mBlindIdx] = BlindState::Skipped;
    mBlindIdx++;
    mBlindStates[mBlindIdx] = BlindState::Current;

    mJustSkipped = true;        // ← 标记"这次进入是因跳过"
    enterBlindSelect();          // 同步发 blindSelectEntered → onBlindSelectEntered 会读 justSkipped()
    mJustSkipped = false;        // ← 信号处理完后立即复位
}


void GameState::applySkippedTag(TagType t, int recursionDepth)
{
    switch (t) {
    case TagType::Skip:
        addGold(5);
        break;
    case TagType::Economy:
        addGold(qMin(mGold, 40));
        break;
    case TagType::Handy:
        addGold(5);
        break;
    case TagType::Garbage:
        addGold(5);
        break;
    case TagType::Investment:
        mPendingInvestmentBonus += 25;
        break;
    case TagType::Voucher:
        mTagVoucherNextShop = true;
        break;
    case TagType::Coupon:
        mShop.setNextShopFree(true);
        break;
    case TagType::Boss:
        mPendingBossEffect = randomBossEffect(mAnte);
        break;
    case TagType::Standard:
    case TagType::Charm:
    case TagType::Meteor:
    case TagType::Buffoon:
    case TagType::Ethereal:
        // 原版这些标签在 blind choice 立刻打开对应免费包，不等到商店。
        // MainWindow 会根据 lastSkippedTag 直接生成并打开包。
        break;
    case TagType::Juggle:
        mOneRoundHandSizeBonus += 3;
        break;
    case TagType::D6:
        mShop.setRerollDiscount(5);
        break;
    case TagType::Orbital: {
        HandType types[] = { HandType::HighCard, HandType::Pair, HandType::TwoPair,
                             HandType::ThreeOfAKind, HandType::Straight, HandType::Flush,
                             HandType::FullHouse, HandType::FourOfAKind, HandType::StraightFlush };
        levelUpHand(types[QRandomGenerator::global()->bounded(int(sizeof(types)/sizeof(*types)))], 3);
        break;
    }
    case TagType::Double:
        if (recursionDepth < 1) {
            TagType extra = randomTagForAnte(mAnte);
            mActiveTags.append(extra);
            mLastSkippedTag = extra;
            applySkippedTag(extra, recursionDepth + 1);
        }
        break;
    case TagType::Negative:
        mShop.addPendingEditionJoker(Edition::Negative);
        break;
    case TagType::Foil:
        mShop.addPendingEditionJoker(Edition::Foil);
        break;
    case TagType::Holographic:
        mShop.addPendingEditionJoker(Edition::Holographic);
        break;
    case TagType::Polychrome:
        mShop.addPendingEditionJoker(Edition::Polychrome);
        break;
    case TagType::TopUp:
    case TagType::Uncommon:
    case TagType::Rare:
        // 罕见/稀有/充值涉及稀有度池。当前 JokerType 还没有 rarity 字段，暂时只登记图标。
        break;
    }
}

void GameState::applyTagEffectsToShop()
{
    if (mTagVoucherNextShop && !mShop.voucherOffersMutable().isEmpty()) {
        // 原版 Voucher Tag 会在商店阶段追加/刷新优惠券；这里把左侧券槽替换成免费未拥有券。
        mShop.voucherOffersMutable()[0].sold = true;
        mShop.voucherOffersMutable().clear();
        ShopOffer forced;
        forced.kind = OfferKind::Voucher;
        QVector<VoucherType> pool;
        for (VoucherType v : baseVoucherPool()) {
            if (mRedeemedVouchers.contains(v)) continue;
            VoucherType prereq = prerequisiteVoucherFor(v);
            if (prereq != v && !mRedeemedVouchers.contains(prereq)) continue;
            pool.append(v);
        }
        if (pool.isEmpty()) pool.append(VoucherType::Blank);
        forced.voucher = pool[QRandomGenerator::global()->bounded(pool.size())];
        // 原版 Voucher Tag 是“额外增加一张优惠券”，不是免费券。
        forced.cost = voucherData(forced.voucher).cost;
        mShop.voucherOffersMutable().append(forced);
        mTagVoucherNextShop = false;
    }

    if (mHasTagFreePack && !mShop.boosterOffersMutable().isEmpty()) {
        ShopOffer &o = mShop.boosterOffersMutable()[0];
        o.kind = OfferKind::Pack;
        o.pack = mTagFreePackKind;
        o.packSize = PackSize::Normal;
        o.cost = 0;
        o.sold = false;
        mHasTagFreePack = false;
    }
}

void GameState::leaveShop()
{
    if (mPhase != GamePhase::Shop) return;
    triggerPerkeoLeavingShop();
    mShop.resetForNewBlind();
    syncShopJokerRules();
    nextBlind();
}

bool GameState::hasJokerType(JokerType t) const
{
    for (const Joker &j : mJokers) if (j.type == t) return true;
    return false;
}

bool GameState::bossBlocksPlayedHand(const HandResult &result, int playedCount)
{
    if (mBlindType != BlindType::Boss || hasJokerType(JokerType::Chicot)) return false;
    if (mBossEffect == BossEffect::ThePsychic && playedCount < 5) return true;
    if (mBossEffect == BossEffect::TheMouth) {
        if (mBossMouthHasHand && mBossMouthOnlyHand != result.type) return true;
        mBossMouthOnlyHand = result.type;
        mBossMouthHasHand = true;
    }
    if (mBossEffect == BossEffect::TheEye) {
        if (mBossEyePlayedHands.contains(result.type)) return true;
        mBossEyePlayedHands.insert(result.type);
    }
    return false;
}

bool GameState::canRerollBoss() const
{
    if (mPhase != GamePhase::BlindSelect || mBlindIdx > 2) return false;
    if (!hasVoucher(VoucherType::DirectorsCut) && !hasVoucher(VoucherType::Retcon)) return false;
    if (mGold < 10) return false;
    if (hasVoucher(VoucherType::Retcon)) return true;
    return mBossRerollsUsedThisAnte == 0;
}

bool GameState::rerollBoss()
{
    if (!canRerollBoss()) return false;
    mGold -= 10;
    ++mBossRerollsUsedThisAnte;
    BossEffect old = mPendingBossEffect;
    for (int i = 0; i < 12; ++i) {
        mPendingBossEffect = randomBossEffect(mAnte);
        if (mPendingBossEffect != old) break;
    }
    mTargetScore = calcTargetScore();
    emit goldChanged();
    emit blindSelectEntered();
    return true;
}


static bool isFaceRankForLegendary(const CardData &card)
{
    return card.rank == Rank::Jack || card.rank == Rank::Queen || card.rank == Rank::King;
}

void GameState::notifyPlayingCardDestroyed(const CardData &card)
{
    // 原版 Caino：每摧毁 1 张人头牌，获得 X1 倍率；初始为 X1。
    if (hasJokerType(JokerType::Caino) && isFaceRankForLegendary(card)) {
        mCainoXMult += 1.0;
        emit jokersChanged();
    }
}

void GameState::notifyDiscardedCardsForYorick(int count)
{
    // 原版 Yorick：每弃掉 23 张牌，获得 X1 倍率；初始为 X1。
    // UI 需要实时显示“还需要弃 [N/23]”，所以只要计数变化就发 jokersChanged。
    if (!hasJokerType(JokerType::Yorick) || count <= 0) return;
    int remaining = count;
    bool changed = false;
    while (remaining > 0) {
        if (remaining >= mYorickDiscardsRemaining) {
            remaining -= mYorickDiscardsRemaining;
            mYorickXMult += 1.0;
            mYorickDiscardsRemaining = 23;
            changed = true;
        } else {
            mYorickDiscardsRemaining -= remaining;
            remaining = 0;
            changed = true;
        }
    }
    if (changed) emit jokersChanged();
}


int GameState::jokerDynamicCounter(JokerType t) const
{
    for (const Joker &j : mJokers) if (j.type == t) return j.counter;
    return 0;
}

void GameState::processEndOfRoundJokerExtinctions()
{
    bool changed = false;
    for (int i = mJokers.size() - 1; i >= 0; --i) {
        Joker &j = mJokers[i];
        if (j.type == JokerType::GrosMichel) {
            if (QRandomGenerator::global()->bounded(6) == 0) {
                mJokers.removeAt(i);
                mGrosMichelExtinct = true;
                changed = true;
            }
        } else if (j.type == JokerType::Cavendish) {
            if (QRandomGenerator::global()->bounded(1000) == 0) {
                mJokers.removeAt(i);
                changed = true;
            }
        }
    }
    if (changed) {
        syncShopJokerRules();
        emit jokersChanged();
        emit shopChanged();
    }
}

void GameState::triggerPerkeoLeavingShop()
{
    // 原版 Perkeo 在离开商店时触发；Blueprint / Brainstorm 如果最终复制到 Perkeo，
    // 也应当额外触发一次。所以“蓝图 + 佩克欧”会复制 2 张负片消耗牌。
    if (mConsumables.isEmpty()) return;

    int triggerCount = 0;
    for (int ji = 0; ji < mJokers.size(); ++ji) {
        const Joker &j = mJokers[ji];
        if (j.isDebuffed) continue;
        const Joker *resolved = resolveCopiedJoker(mJokers, ji);
        if (resolved && !resolved->isDebuffed && resolved->type == JokerType::Perkeo)
            ++triggerCount;
    }
    if (triggerCount <= 0) return;

    for (int n = 0; n < triggerCount; ++n) {
        if (mConsumables.isEmpty()) break;
        Consumable copy = mConsumables[QRandomGenerator::global()->bounded(mConsumables.size())];
        copy.negative = true;
        copy.sellValue = qMax(copy.sellValue, 1);
        mConsumables.append(copy);       // 负片消耗牌自身提供 +1 消耗牌槽，因此允许满槽追加。
    }
    emit consumablesChanged();
}


bool GameState::addRandomRareJoker()
{
    if (!canAddJoker()) return false;
    QVector<JokerType> rare = {
        JokerType::Blueprint, JokerType::Brainstorm, JokerType::DNA,
        JokerType::Mime, JokerType::Baron, JokerType::Bloodstone,
        JokerType::DriversLicense, JokerType::Hologram, JokerType::Vampire,
        JokerType::Constellation, JokerType::MidasMask, JokerType::CardSharp,
        JokerType::SockAndBuskin, JokerType::Photograph, JokerType::TheDuo, JokerType::TheTrio,
        JokerType::TheFamily, JokerType::TheOrder, JokerType::TheTribe
    };
    if (!hasJokerDuplicateBypass()) {
        QVector<JokerType> owned = ownedJokerTypes();
        rare.erase(std::remove_if(rare.begin(), rare.end(), [&owned](JokerType t){ return owned.contains(t); }), rare.end());
    }
    if (rare.isEmpty()) return false;
    Joker j = createJoker(rare[QRandomGenerator::global()->bounded(rare.size())]);
    mJokers.append(j);
    syncShopJokerRules();
    emit jokersChanged();
    emit shopChanged();
    return true;
}

bool GameState::duplicateRandomJokerAndDestroyOthers()
{
    if (mJokers.isEmpty()) return false;
    int keep = QRandomGenerator::global()->bounded(mJokers.size());
    Joker copy = mJokers[keep];
    mJokers.clear();
    mJokers.append(copy);
    if (canAddJoker()) mJokers.append(copy);
    syncShopJokerRules();
    emit jokersChanged();
    emit shopChanged();
    return true;
}

bool GameState::setRandomEditionlessJoker(Edition e, bool destroyOthers, bool reduceHandSize)
{
    QVector<int> candidates;
    for (int i = 0; i < mJokers.size(); ++i) {
        if (mJokers[i].edition == Edition::None) candidates.append(i);
    }
    if (candidates.isEmpty()) return false;
    int chosen = candidates[QRandomGenerator::global()->bounded(candidates.size())];
    Joker selected = mJokers[chosen];
    selected.edition = e;
    switch (e) {
    case Edition::Foil:
    case Edition::Holographic: selected.sellValue += 1; break;
    case Edition::Polychrome: selected.sellValue += 2; break;
    case Edition::Negative: selected.sellValue += 3; break;
    default: break;
    }
    if (destroyOthers) {
        mJokers.clear();
        mJokers.append(selected);
    } else {
        mJokers[chosen] = selected;
    }
    if (reduceHandSize) addPermanentHandSizeBonus(-1);
    syncShopJokerRules();
    emit jokersChanged();
    emit shopChanged();
    return true;
}

void GameState::addPermanentHandSizeBonus(int delta)
{
    mExtraHandSize += delta;
    if (mExtraHandSize < -6) mExtraHandSize = -6;
    emit handChanged();
}

void GameState::immolateRandomHandCards(int destroyCount, int goldGain)
{
    if (mHand.isEmpty()) return;
    QVector<int> idx;
    for (int i = 0; i < mHand.size(); ++i) idx.append(i);
    for (int i = idx.size() - 1; i > 0; --i) {
        int j = QRandomGenerator::global()->bounded(i + 1);
        idx.swapItemsAt(i, j);
    }
    int n = qMin(destroyCount, idx.size());
    idx = idx.mid(0, n);
    std::sort(idx.begin(), idx.end(), std::greater<int>());
    for (int i : idx) {
        if (i < 0 || i >= mHand.size()) continue;
        notifyPlayingCardDestroyed(mHand[i]);
        mHand.removeAt(i);
    }
    if (n > 0) addGold(goldGain);
    emit handChanged();
}

bool GameState::addRandomLegendaryJoker()
{
    if (!canAddJoker()) return false;
    QVector<JokerType> legends = {
        JokerType::Caino, JokerType::Triboulet, JokerType::Yorick,
        JokerType::Chicot, JokerType::Perkeo
    };
    if (!hasJokerDuplicateBypass()) {
        legends.erase(std::remove_if(legends.begin(), legends.end(), [this](JokerType t){
            return ownedJokerTypes().contains(t);
        }), legends.end());
    }
    if (legends.isEmpty()) legends = { JokerType::Caino, JokerType::Triboulet, JokerType::Yorick, JokerType::Chicot, JokerType::Perkeo };
    Joker j = createJoker(legends[QRandomGenerator::global()->bounded(legends.size())]);
    mJokers.append(j);
    syncShopJokerRules();
    emit jokersChanged();
    return true;
}

void GameState::levelUpAllHands(int times)
{
    const HandType all[] = {
        HandType::HighCard, HandType::Pair, HandType::TwoPair, HandType::ThreeOfAKind,
        HandType::Straight, HandType::Flush, HandType::FullHouse, HandType::FourOfAKind,
        HandType::StraightFlush, HandType::RoyalFlush, HandType::FiveOfAKind,
        HandType::FlushHouse, HandType::FlushFive
    };
    for (HandType t : all) levelUpHand(t, times);
}

HandResult GameState::previewSelection(const QVector<int> &indices) const
{
    QVector<CardData> selected;
    for (int i : indices) {
        if (i >= 0 && i < mHand.size())
            selected.append(mHand[i]);
    }
    HandResult r = HandEvaluator::preview(selected);
    // 加上牌型升级加成(这部分玩家应该看到)
    auto it = mHandLevels.find(r.type);
    if (it != mHandLevels.end()) {
        r.chips += it->chipsBonus;
        r.mult  += it->multBonus;
        r.level  = it->level;
    }
    return r;
}
