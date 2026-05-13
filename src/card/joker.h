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
};

// 触发上下文，打包所有触发时可访问的数据
struct TriggerContext {
    HandResult &result;
    GameState &state;
    const QVector<CardData> &hand;
    const QVector<CardData> &scoringCards;
    const CardData *currentCard = nullptr; // 逐张触发时有值
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
