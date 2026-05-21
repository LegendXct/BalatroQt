#ifndef JOKER_HH
#define JOKER_HH

#include <QString>
#include <functional>
#include <QVector>
#include "carddata.h"

class GameState;
struct HandResult;

enum class TriggerTiming {
    OnScoringCard, // 每张记分牌触发一次
    OnPlayedHand, // 出牌后触发
    OnDiscard, // 弃牌时触发
    OnRoundEnd, // 回合结束（进商店）时触发
    Passive // 持有时常驻，直接加入计算
};

enum class JokerType {
    Joker, GreedyJoker, LustyJoker, WrathfulJoker, GluttonousJoker,
    HalfJoker, JollyJoker, ZanyJoker, MadJoker, CrazyJoker, DrollJoker,
    GoldenJoker, ToDoList,
    SlyJoker, WilyJoker, CleverJoker, DeviousJoker, CraftyJoker,  // 牌型筹码
    Banner, MysticSummit, Misprint, RaisedFist,                    // 杂项倍率
    Fibonacci, EvenSteven, OddTodd, Scholar,                       // 计分牌触发
    Bull, Bootstraps,                                              // 经济联动
    AbstractJoker, Supernova, GrosMichel, Cavendish, IceCream, Stuntman,
    TheDuo, TheTrio, TheFamily, TheOrder, TheTribe, Blackboard,
    ScaryFace, SmileyFace, WalkieTalkie, Arrowhead, OnyxAgate,
    RoughGem, Bloodstone, ShootTheMoon, Baron, FlowerPot, Acrobat,
    Swashbuckler, Ramen, DriversLicense,
    Hiker, CardSharp, Hologram, MidasMask, Vampire, Constellation, Photograph, HangingChad, SockAndBuskin,
    Blueprint, Brainstorm, DNA, Mime,
    Caino, Triboulet, Yorick, Chicot, Perkeo,
    // Batch 1：纯计分效果小丑
    JokerStencil, SteelJoker, StoneJoker, BlueJoker, Erosion,
    BusinessCard, FacelessJoker, Cloud9, GoldenTicket, SeeingDouble,
    // Batch 2：计数器型小丑
    SquareJoker, Runner, Castle, GreenJoker, Obelisk, RideTheBus,
    SpareTrousers, WeeJoker, HitTheRoad, GlassJoker, LuckyCat, Popcorn,
    // Batch 3：手牌/弃牌/经济/每回合参数型小丑
    Juggler, Drunkard, MerryAndy, Troubadour, DelayedGratification,
    ToTheMoon, ReservedParking, MailInRebate, AncientJoker, TheIdol,
    SpaceJoker, Hack,
    // Batch 4：造牌型小丑
    RiffRaff, MarbleJoker, Burglar, Cartomancer, Certificate, Madness,
    EightBall, Seance, Vagabond, Superposition,
    // Batch 5：计数器/经济型小丑
    FlashCard, Throwback, Campfire, FortuneTeller, LoyaltyCard,
    Egg, Rocket, Satellite, GiftCard,
    // Batch 6：牌型判定修正型小丑
    Shortcut, SmearedJoker, Splash, Showman,
    // Batch 7：特殊机制型小丑
    Dusk, CeremonialDagger, TurtleBean, Seltzer, BurntJoker, ChaosTheClown,
    // Batch 8：杂项机制型小丑
    Pareidolia, Hallucination, Luchador, InvisibleJoker,
    // Batch 9：收尾小丑
    CreditCard, MrBones, DietCola, FourFingers, OopsAllSixes,
};

class Joker; // 前置声明，供 TriggerContext::self 使用

// 触发上下文，打包所有触发时可访问的数据
struct TriggerContext {
    HandResult &result;
    GameState &state;
    const QVector<CardData> &hand;
    const QVector<CardData> &scoringCards;
    const CardData *currentCard = nullptr; // 逐张触发时有值
    const QVector<CardData> *playedCards = nullptr; // OnPlayedHand：本次打出的全部牌
    const Joker *self = nullptr;           // 当前正在结算的小丑自身
    bool isFirstFaceCard = false;          // OnScoringCard：currentCard 是本手第一张计分人头牌
};

using JokerEffect = std::function<void(TriggerContext &)>;

class Joker
{
public:
    JokerType type;
    QString name;
    QString description;
    Edition edition = Edition::None;
    int sellValue = 2;
    bool isDebuffed = false;
    int counter = 0; // 动态数值：冰淇淋当前筹码等

    TriggerTiming timing;
    JokerEffect effect;
};

Joker createJoker(JokerType type);

#endif // JOKER_HH
