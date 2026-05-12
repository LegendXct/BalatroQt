#ifndef BOOSTERPACK_H
#define BOOSTERPACK_H

#include <QString>
#include <QPoint>
#include <QVector>
#include "../card/carddata.h"
#include "../card/joker.h"
#include "../card/consumable.h"

enum class PackKind {
    Standard,    // 扑克牌包
    Arcana,      // 塔罗包
    Celestial,   // 行星包
    Buffoon,     // 小丑包
    Spectral,    // 幻灵包
};

enum class PackSize {
    Normal,
    Jumbo,
    Mega,
};

struct PackContent {
    PackKind kind = PackKind::Standard;
    PackSize size = PackSize::Normal;
    int optionsToShow = 3;      // 原版 config.extra：包里翻出几张
    int choicesAllowed = 1;     // 原版 config.choose：可拿/可用几张

    QVector<CardData>       standardCards;   // Standard 用
    QVector<JokerType>      jokers;          // Buffoon 用
    QVector<ConsumableType> consumables;     // Arcana / Celestial / Spectral 用
};

QString packDisplayName(PackKind k);
QString packDisplayName(PackKind k, PackSize s);
QPoint  packSpritePos(PackKind k, PackSize s);

PackContent generatePackContent(PackKind k,
                                PackSize s = PackSize::Normal,
                                bool omenGlobe = false,
                                bool telescope = false,
                                ConsumableType telescopePlanet = ConsumableType::Planet_Pluto,
                                const QVector<JokerType> &ownedJokers = QVector<JokerType>(),
                                bool allowDuplicateJokers = false);

#endif
