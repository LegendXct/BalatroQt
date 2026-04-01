#ifndef JOKER_H
#define JOKER_H

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
    // 常驻倍率
    Joker, // +4 倍率
    GreedyJoker, // 每张♦计分牌 +3 倍率
    LustyJoker, // 每张♥计分牌 +3 倍率
    WrathfulJoker, // 每张♠计分牌 +3 倍率
    GluttonousJoker, // 每张♣计分牌 +3 倍率
    HalfJoker, // 出牌≤3张 +20 倍率
    // 筹码类
    JollyJoker, // 出对子 +8 倍率
    ZanyJoker, // 出三条 +12 倍率
    MadJoker, // 出两对 +10 倍率
    CrazyJoker, // 出顺子 +12 倍率
    DrollJoker, // 出同花 +10 倍率
    // 金币类
    GoldTicket, // 回合结束 +4 金币
    ToDoList, // 出指定牌型 +4 金币
    // 可扩展...
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

    TriggerTiming timing;
    JokerEffect effect;
};

Joker createJoker(JokerType type);

#endif // JOKER_H
