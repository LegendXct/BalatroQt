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

struct HandResult {
    HandType type;
    QVector<CardData> scoringCards; // 参与计分的牌
    int chips; // 基础筹码
    int mult; // 基础倍率
    QString name; // 牌型名称，用于UI显示
};

class HandEvaluator
{
    static bool isStraight(const QVector<CardData> &sorted); // 判断顺子
    static bool isFlush(const QVector<CardData> &cards); // 判断同花
    static QMap<Rank, QVector<CardData>> groupByRank(const QVector<CardData> &cards); // 计算各个点数的数量
public:
    static HandResult evaluate(const QVector<CardData> &cards);
    static QString handTypeName(HandType type);
};

#endif // HANDEVALUATOR_H
