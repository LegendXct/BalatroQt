#ifndef HANDEVALUATOR_H
#define HANDEVALUATOR_H

#include <QVector>
#include <QMap>
#include "../card/carddata.h"
#include "../utils/constants.h"

// 牌型枚举，值越大牌型越强
enum class HandType {
    HighCard, // 高牌
    Pair, // 对子
    TwoPair, // 两对
    ThreeOfAKind, // 三条
    Straight, // 顺子
    Flush, // 同花
    FullHouse, // 葫芦
    FourOfAKind, // 四条
    StraightFlush, // 同花顺
    RoyalFlush, // 皇家同花顺
    FiveOfAKind, // 五条
    FlushHouse, // 同花葫芦
    FlushFive // 同花五条
};

enum class ScoreEventKind {
    ScoringCardChip,    // 计分牌的 chipValue + Bonus/Stone 增强(蓝)
    EnhancementMult,    // 计分牌 Mult 增强 +4(红)
    EnhancementXMult,   // 计分牌 Glass ×2(红 xmult)
    EditionChip,        // Foil +50 chip(蓝)
    EditionMult,        // Holographic +10 mult(红)
    EditionXMult,       // Polychrome ×1.5(红 xmult)
    SteelXMult,         // 手牌 Steel ×1.5(红 xmult)
    JokerChip,          // 小丑加 chip(蓝)
    JokerMult,          // 小丑加 mult(红)
    JokerXMult,         // 小丑加 xmult(红)
    DollarGain,         // 幸运牌/金色蜡封/黄金牌等获得金钱
    RedSealRetrigger,   // 红色蜡封重新触发
    GlassShatter,       // 玻璃牌破碎动画
    BlueSealPlanet,     // 蓝色蜡封生成星球牌提示
};

struct ScoreEvent {
    ScoreEventKind kind;
    int    sourceCardIdx = -1;     // -1 表示不是 played 卡(比如 Steel 在手牌、joker 来源)
    int    sourceHandIdx = -1;     // 手牌位置(Steel 用)
    int    sourceJokerIdx = -1;    // 小丑位置(JokerXxx 用)
    int    intValue   = 0;          // chip/mult 整数加值
    double xmultValue = 1.0;        // xmult 倍率
};

struct HandResult {
    HandType type = HandType::HighCard;
    QVector<CardData> scoringCards; // 参与计分的牌
    int chips = 0; // 基础筹码
    int mult = 0; // 基础倍率
    double xmult = 1.0; // ×倍率，Glass/Polychrome/Steel用
    int level = 1; // 牌型等级
    QString name; // 牌型名称，用于UI显示
    int baseChips = 0;
    int baseMult  = 0;
    QVector<ScoreEvent> events;
};

class HandEvaluator
{
    static bool isStraight(const QVector<CardData> &sorted); // 判断顺子
    static bool isFlush(const QVector<CardData> &cards); // 判断同花
    static QMap<Rank, QVector<CardData>> groupByRank(const QVector<CardData> &cards); // 计算各个点数的数量
public:
    static HandResult evaluate(const QVector<CardData> &cards);
    static QString handTypeName(HandType type);
    static HandResult preview(const QVector<CardData> &cards);
};

#endif // HANDEVALUATOR_H
