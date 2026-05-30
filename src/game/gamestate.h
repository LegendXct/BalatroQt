#ifndef GAMESTATE_H
#define GAMESTATE_H

#include <QObject>
#include <QVector>
#include <QHash>
#include <QSet>
#include "deck.h"
#include "handevaluator.h"
#include "../card/carddata.h"
#include "../card/joker.h"
#include "../utils/constants.h"
#include "shop.h"
#include "bossblind.h"
#include "tag.h"
#include "../card/consumable.h"

struct HandLevel {
    int level = 1;
    int played = 0;
    int chipsBonus = 0;
    int multBonus = 0;
};

enum class BlindType {
    Small,
    Big,
    Boss
};

enum class BlindState {
    Upcoming,
    Current,
    Defeated,
    Skipped,
};

enum class GamePhase {
    BlindSelect,
    Blind,
    Shop,
    GameOver
};

enum class HandSortMode {
    ByRank,
    BySuit,
    Manual
};

// 补牌场景：用于区分开局发牌与出牌/弃牌后的补牌（巨蛇、鱼等 Boss 需要）。
enum class DrawContext {
    BlindStart,
    AfterPlay,
    AfterDiscard
};

class GameState : public QObject
{
    Q_OBJECT
public:
    explicit GameState(QObject *parent = nullptr);
    void startGame();
    void startBlind(BlindType type);
    void playCards(const QVector<int> &indices);
    void finalizePlayedHand();
    void discardCards(const QVector<int> &indices);
    void nextBlind();
    const QVector<CardData> &hand() const {return mHand;}
    const QVector<Joker> &jokers() const {return mJokers;}
    bool hasJokerType(JokerType t) const;
    int gold() const {return mGold;}
    // 信用卡：可透支 $20；全是6：所有概率分母减半
    int spendableGold() const { return mGold + (hasJokerType(JokerType::CreditCard) ? 20 : 0); }
    // 商店侧栏"预览下一回合"用：暴露当前的修正值（不含 Boss 影响——Boss 还没选）。
    int extraHandsPerRoundPreview() const {
        int delta = mExtraHandsPerRound;
        for (const Joker &j : mJokers) {
            if (j.isDebuffed) continue;
            if (j.type == JokerType::Troubadour) delta -= 1;
        }
        return delta;
    }
    int extraDiscardsPerRoundPreview() const {
        int delta = mExtraDiscardsPerRound;
        for (const Joker &j : mJokers) {
            if (j.isDebuffed) continue;
            if (j.type == JokerType::Drunkard)  delta += 1;
            if (j.type == JokerType::MerryAndy) delta += 3;
        }
        return delta;
    }
    // 混沌小丑：本次进商店是否还有免费重摇可用。
    bool hasFreeShopReroll() const {
        return !mChaosFreeRerollUsed && hasJokerType(JokerType::ChaosTheClown);
    }
    int probDenom(int n) const { return hasJokerType(JokerType::OopsAllSixes) ? qMax(1, n / 2) : n; }
    int pendingRoundPayout() const { return mPendingRoundPayout; }
    bool claimRoundPayout();
    int handsLeft() const {return mHandsLeft;}
    int discardLeft() const {return mDiscardLeft;}
    double score() const {return mScore;}
    double targetScore() const {return mTargetScore;}
    int ante() const {return mAnte;}
    int jokerSlots() const;
    int currentBlindStartingHands() const { return mBlindStartingHands; }
    BlindType blindType() const {return mBlindType;}
    GamePhase phase() const {return mPhase;}
    void addGold(int amount) { if (mDryRun) return; mGold += amount; if (!mSuppressGoldSignal) emit goldChanged(); }
    const HandResult &lastResult() const{return mLastResult;}
    void sortHandByRank();
    void sortHandBySuit();
    HandSortMode sortMode() const { return mSortMode; }
    int deckRemaining() const {return mDeck.remaining();}
    int deckTotal() const;
    QVector<CardData> remainingDeckCards() const;
    QVector<CardData> fullDeckCards() const;
    BlindState blindState(int idx) const { return mBlindStates[idx]; }
    void skipCurrentBlind();
    TagType blindTag(int idx) const;
    TagType lastSkippedTag() const { return mLastSkippedTag; }
    const QVector<TagType> &activeTags() const { return mActiveTags; }

    // 商店相关
    Shop &shop() { return mShop; }
    void rerollShop();
    void leaveShop();                // 离开商店 → 进入下一盲注
    bool canAddJoker() const { return mJokers.size() < jokerSlots(); }
    // 负片小丑自己提供 +1 小丑槽。满槽时，普通小丑不能买，但负片小丑必须允许买入。
    bool canAddJokerWithEdition(Edition edition) const;
    bool buyVoucherOffer(int offerIdx);
    bool hasVoucher(VoucherType t) const;
    const QVector<VoucherType> &redeemedVouchers() const { return mRedeemedVouchers; }

    const QHash<HandType, HandLevel> &handLevels() const { return mHandLevels; }
    void levelUpHand(HandType t, int times = 1);   // 行星牌用
    bool handTypePlayedThisRound(HandType t) const { return mHandTypesPlayedThisRound.contains(static_cast<int>(t)); }
    // 幻想性错觉：持有时所有牌（石头牌除外）都算人头牌
    bool isFaceCard(const CardData &c) const {
        if (c.enhancement == Enhancement::Stone) return false;
        return hasJokerType(JokerType::Pareidolia)
            || c.rank == Rank::Jack || c.rank == Rank::Queen || c.rank == Rank::King;
    }
    // Batch 3：每回合随机参数 / 状态查询，供小丑效果读取
    bool noDiscardsUsedThisRound() const { return mDiscardLeft == mBlindStartingDiscards; }
    Rank mailRank() const { return mMailRank; }
    Suit ancientSuit() const { return mAncientSuit; }
    Suit castleSuit() const { return mCastleSuit; }
    Rank idolRank() const { return mIdolRank; }
    Suit idolSuit() const { return mIdolSuit; }

    BossEffect bossEffect() const { return mBossEffect; }
    BossInfo currentBossInfo() const { return bossInfo(mBossEffect); }

    QVector<CardData> &handMutable() { return mHand; }
    void notifyHandChanged() { emit handChanged(); }

    // 消耗品接口
    const QVector<Consumable> &consumables() const { return mConsumables; }
    int consumableSlots() const;
    int handSize() const;
    bool canAddConsumable() const { return mConsumables.size() < consumableSlots(); }
    bool addConsumable(ConsumableType t);
    bool addFoolCopyConsumable();
    bool canUseFool() const;
    bool useConsumable(int idx, const QVector<int> &selectedHandIdx);
    bool sellConsumable(int idx);
    bool sellJoker(int idx);
    bool moveJoker(int from, int to);
    bool moveConsumable(int from, int to);
    bool moveHandCard(int from, int to);
    bool moveShopOffer(int from, int to);
    bool moveBoosterOffer(int from, int to);
    void collectRoundCardsToDeck();

    // 商店买入：扩展为支持 3 种 offer
    bool buyShopOffer(int offerIdx);     // ← 替代旧的 buyJoker
    // 商店内消耗牌的"购买并使用"：买下后立即调用 useConsumable()。
    // 仅当槽位够（或可放进临时槽）且选中牌满足消耗品需求时返回 true。
    bool buyAndUseShopConsumable(int offerIdx, const QVector<int> &selectedHandIdx);
    // 判断商店中的某个消耗牌是否可"购买并使用"（即不需要选牌，或可空选）。
    bool canBuyAndUseShopConsumable(int offerIdx) const;
    bool buyPack(int offerIdx, PackContent &outContent);
    QVector<CardData> drawPackHand();
    void returnPackHand(const QVector<CardData> &packHand);
    bool applyPackChoice(const PackContent &pack, int chosenIdx,
                         const QVector<int> &selectedPackHandIdx,
                         QVector<CardData> &packHand);
    bool useConsumableOnPackHand(int consumableIdx,
                                 const QVector<int> &selectedPackHandIdx,
                                 QVector<CardData> &packHand);

    int blindIdx() const { return mBlindIdx; }       // 当前 ante 内打到第几个 (0/1/2)
    BossEffect pendingBossEffect() const { return mPendingBossEffect; }
    bool canRerollBoss() const;
    bool rerollBoss();
    bool addRandomLegendaryJoker();
    bool addRandomRareJoker();
    bool duplicateRandomJokerAndDestroyOthers();
    bool setRandomEditionlessJoker(Edition e, bool destroyOthers, bool reduceHandSize);
    void addPermanentHandSizeBonus(int delta);
    void immolateRandomHandCards(int destroyCount, int goldGain);
    void notifyPlayingCardDestroyed(const CardData &card);
    bool dnaCanTriggerThisPlay() const;
    void createDNACopy(const CardData &card);
    double cainoXMult() const { return mCainoXMult; }
    double yorickXMult() const { return mYorickXMult; }
    int yorickDiscardsRemaining() const { return mYorickDiscardsRemaining; }
    int jokerDynamicCounter(JokerType t) const;
    void levelUpAllHands(int times = 1);
    void selectCurrentBlind();                       // 玩家从 BlindSelect 点"选择"
    bool justSkipped() const { return mJustSkipped; }
    bool grosMichelExtinct() const { return mGrosMichelExtinct; }

    // 无尽模式：通关 ante 8 后继续游玩。
    bool isEndlessMode() const { return mEndlessMode; }
    void continueEndless();
    // 蔚蓝铃铛 Boss：被强制选中的手牌 uid（-1 表示无）。
    int ceruleanForcedUid() const { return mCeruleanForcedUid; }

    HandResult previewSelection(const QVector<int> &indices) const;

    // 最佳出牌提示：遍历所有出牌组合/排列，返回得分最高的一组手牌下标（按最优顺序）。
    QVector<int> findBestPlay();
    // 把指定手牌按给定顺序移到手牌最前，排序模式切到 Manual。
    void bringHandCardsToFront(const QVector<int> &indices);
    // 按指定 uid 序列重新排列手牌。最佳出牌"取消"时需要把手牌恢复到玩家最近
    // 一次手动整理后的顺序，而不是默认点数/花色 sort。
    void reorderHandByUids(const QVector<int> &uidOrder);
signals:
    void handChanged();
    void scoreChanged();
    void goldChanged();
    void roundWon(int blindReward, int handBonus, int interest);
    void gameOver(bool won);
    void handPlayed();
    void endRoundCardTriggered(const QVector<ScoreEvent> &events);
    void jokersChanged();
    void shopChanged();
    void handLevelsChanged();
    void consumablesChanged();
    void blindSelectEntered();                       // 切到选择页
    void blindStarted();                             // 切到对局页

private:
    Deck mDeck;
    QVector<CardData> mHand;
    QVector<Joker> mJokers;
    // Default initializers prevent uninitialized reads before startBlind().
    // 没有它们时，"开始新的一局" 后 refreshCounters() 会读到栈/堆垃圾值（曾出现巨大负数）。
    int mGold = Constants::INITIAL_GOLD;
    double mScore = 0.0;
    double mTargetScore = 0.0;
    int mHandsLeft = Constants::INITIAL_HANDS;
    int mDiscardLeft = Constants::INITIAL_DISCARDS;
    int mAnte = 1;
    BlindType mBlindType;
    GamePhase mPhase;
    HandResult mLastResult;
    Shop mShop;
    QHash<HandType, HandLevel> mHandLevels;
    QSet<int> mHandTypesPlayedThisRound;   // 锋利卡牌：本回合已打出过的牌型
    Suit mCastleSuit = Suit::Spades;       // 城堡：本回合计数的花色
    int  mBlindStartingDiscards = Constants::INITIAL_DISCARDS;  // 延迟满足
    Rank mMailRank = Rank::Two;            // 邮件回扣：本回合返钱的点数
    Suit mAncientSuit = Suit::Spades;      // 远古小丑：本回合 ×1.5 的花色
    Rank mIdolRank = Rank::Ace;            // 偶像：本回合 ×2 的点数
    Suit mIdolSuit = Suit::Spades;         // 偶像：本回合 ×2 的花色
    QSet<int> mPlanetsUsedThisRun;         // 卫星：本局用过的行星牌种类
    bool mFirstDiscardThisRound = true;    // 焦痕小丑：本回合是否还没弃过牌
    bool mChaosFreeRerollUsed = false;     // 混沌小丑：本次商店免费重摇是否已用
    int  mPendingDoubleTags = 0;           // 双倍标签：未消耗的复制次数（每次新非 Double 标签会被复制一次并 -1）
    int  mTotalSkipsThisRun = 0;           // Skip Tag 用：本局已跳过的盲注次数
    int  mTotalHandsPlayedThisRun = 0;     // Handy Tag 用：本局已打出的总手数（累加，不重置）
    int  mUnusedDiscardsThisRun = 0;       // Garbage Tag 用：上一回合结束时累加的"剩余弃牌数"
    void cleanupDepletedJokers();          // 移除计数耗尽的小丑（爆米花/拉面/苏打水/海龟豆）
    BlindState mBlindStates[3] = {
        BlindState::Current, BlindState::Upcoming, BlindState::Upcoming
    };

    bool mJustSkipped = false;

    bool mAwaitingScoreFinalize = false;
    double mPendingHandScore = 0.0;
    QVector<int> mPendingPlayedIndices;
    QVector<bool> mPendingShattered;
    int mPendingRoundPayout = 0; // 胜利结算先暂存，点击“提现”后再真正加到金币
    bool mSuppressGoldSignal = false; // 回合末临时计算提现金额时，避免提前刷新左侧金币
    void finishWinningRound();

    int mBlindIdx = 0;
    BossEffect mPendingBossEffect = BossEffect::None;

    HandSortMode mSortMode = HandSortMode::ByRank;   // 默认按点数
    void enterBlindSelect();

    void dealCards(DrawContext ctx = DrawContext::BlindStart); // 补牌
    double calcTargetScore() const;
    void checkGameOver();
    int roundReward() const;
    static QPair<int, int> handLevelDelta(HandType t);   // {chipsΔ, multΔ}

    BossEffect mBossEffect = BossEffect::None;
    void applyBossDebuffs();   // 给手牌打 debuff 标记
    void applyBossPostPlay();  // 出牌后 The Hook 等
    bool bossBlocksPlayedHand(const HandResult &result, int playedCount);

    bool mFirstShop = true;

    QVector<Consumable> mConsumables;
    QVector<VoucherType> mRedeemedVouchers;

    int mExtraConsumableSlots = 0;
    int mExtraHandsPerRound = 0;
    int mExtraDiscardsPerRound = 0;
    int mExtraHandSize = 0;
    int mInterestCap = Constants::INTEREST_MAX;
    int mExtraJokerSlots = 0;
    int mOneRoundHandSizeBonus = 0;
    int mPendingInvestmentBonus = 0;
    int mBlindStartingHands = Constants::INITIAL_HANDS;
    bool mDNAUsedThisBlind = false;
    bool mDNAEligibleThisPlay = false;
    int mDNACopiesCreatedThisPlay = 0;
    QVector<CardData> mPendingDNACopies;
    bool mTagVoucherNextShop = false;
    PackKind mTagFreePackKind = PackKind::Standard;
    bool mHasTagFreePack = false;
    int mBossRerollsUsedThisAnte = 0;
    HandType mBossMouthOnlyHand = HandType::HighCard;
    bool mBossMouthHasHand = false;
    QSet<HandType> mBossEyePlayedHands;
    TagType mBlindTags[2] = { TagType::Skip, TagType::Skip };
    TagType mLastSkippedTag = TagType::Skip;
    QVector<TagType> mActiveTags;

    // 无尽模式 / 新增 Boss 状态
    bool mEndlessMode = false;
    QSet<int> mCardsPlayedThisAnte;   // 支柱(The Pillar)：本 Ante 已打出过的牌 uid
    int mCrimsonHeartDisabled = -1;   // 绯红之心：本手被禁用的小丑下标
    bool mVerdantLeafActive = false;  // 翠绿之叶：尚未卖出小丑时为 true
    int mCeruleanForcedUid = -1;      // 蔚蓝铃铛：强制选中的手牌 uid
    void refreshCeruleanForced();     // 强制牌离开手牌后重新挑一张

    double mCainoXMult = 1.0;
    double mYorickXMult = 1.0;
    int mYorickDiscardsRemaining = 23;
    bool mHasLastUsedConsumable = false;
    bool mGrosMichelExtinct = false;
    ConsumableType mLastUsedConsumable = ConsumableType::Tarot_Fool;
    void notifyDiscardedCardsForYorick(int count);
    void triggerBlindSelectJokers(BlindType type);   // Batch 4：选盲注时造牌型小丑
    void triggerPerkeoLeavingShop();
    void processEndOfRoundJokerExtinctions();

    void applyVoucher(VoucherType t);
    void prepareBlindTags();
    void applySkippedTag(TagType t, int recursionDepth = 0);
    void applyTagEffectsToShop();
    QVector<JokerType> ownedJokerTypes() const;
    QVector<ConsumableType> ownedConsumableTypes() const;
    bool hasJokerDuplicateBypass() const;
    HandMods currentHandMods() const;   // Batch 6：由持有小丑生成牌型判定修正
    void syncShopJokerRules();
    void updateOwnedSellValues();
    void scoreCard(const CardData &card, HandResult &result, int playedIdx, bool firstFaceCard = false);
    void decayEndOfHandJokers();

    // 最佳出牌提示用的无副作用计分模拟器：返回该出牌（按 orderedIndices 顺序）的得分。
    // 随机效果按期望值估算；mDryRun 期间所有状态改写被抑制。
    bool mDryRun = false;
    double simulatePlayScore(const QVector<int> &orderedIndices);
    static ConsumableType planetTypeFor(HandType t);
};

#endif // GAMESTATE_H
