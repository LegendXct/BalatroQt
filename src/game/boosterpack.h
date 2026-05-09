#ifndef BOOSTERPACK_H
#define BOOSTERPACK_H

#include <QString>
#include <QVector>
#include "../card/carddata.h"
#include "../card/joker.h"
#include "../card/consumable.h"

enum class PackKind {
    Standard,    // 扑克牌包
    Arcana,      // 塔罗包
    Celestial,   // 行星包
    Buffoon,     // 小丑包
};

struct PackContent {
    PackKind kind;
    QVector<CardData>       standardCards;   // Standard 用
    QVector<JokerType>      jokers;          // Buffoon 用
    QVector<ConsumableType> consumables;     // Arcana / Celestial 用
};

QString packDisplayName(PackKind k);
PackContent generatePackContent(PackKind k);

#endif
