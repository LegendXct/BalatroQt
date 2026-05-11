#ifndef CONSUMABLE_H
#define CONSUMABLE_H

#include <QString>
#include <QVector>
#include <functional>

class GameState;

enum class ConsumableKind { Tarot, Planet };

enum class ConsumableType {
    // ── 塔罗（先做 6 张） ──
    Tarot_Empress,       // 选≤2张：Mult 增强
    Tarot_Hierophant,    // 选≤2张：Bonus 增强
    Tarot_Chariot,       // 选 1 张：Steel 增强
    Tarot_Lovers,        // 选 1 张：Wild  增强
    Tarot_Hermit,        // 不选牌：金币翻倍（最多+20）
    Tarot_Tower,         // 选 1 张：Stone 增强

    // ── 行星（12 张全做） ──
    Planet_Pluto, Planet_Mercury, Planet_Uranus, Planet_Venus,
    Planet_Saturn, Planet_Jupiter, Planet_Earth, Planet_Mars,
    Planet_Neptune, Planet_PlanetX, Planet_Ceres, Planet_Eris,
};

struct UseContext {
    GameState &state;
    QVector<int> selectedHandIdx;       // 已升序
};

using ConsumableEffect = std::function<void(UseContext &)>;

class Consumable {
public:
    ConsumableType type;
    ConsumableKind kind;
    QString name;
    QString description;
    int sellValue = 1;
    int needsSelection = 0;             // 至少选 N 张
    int maxSelection = 0;               // 至多生效 N 张
    ConsumableEffect effect;
};

ConsumableKind kindOf(ConsumableType type);
Consumable createConsumable(ConsumableType type);
ConsumableType randomTarotType();
ConsumableType randomPlanetType();

#endif
