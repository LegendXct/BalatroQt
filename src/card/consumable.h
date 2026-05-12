#ifndef CONSUMABLE_H
#define CONSUMABLE_H

#include <QString>
#include <QVector>
#include <functional>

class GameState;

enum class ConsumableKind { Tarot, Planet, Spectral };

enum class ConsumableType {
    // ── 塔罗（先做 6 张） ──
    Tarot_Empress,       // 选≤2张：Mult 增强
    Tarot_Hierophant,    // 选≤2张：Bonus 增强
    Tarot_Chariot,       // 选 1 张：Steel 增强
    Tarot_Lovers,        // 选 1 张：Wild  增强
    Tarot_Hermit,        // 不选牌：金币翻倍（最多+20）
    Tarot_Tower,         // 选 1 张：Stone 增强

    // ── 行星（12 张） ──
    Planet_Pluto, Planet_Mercury, Planet_Uranus, Planet_Venus,
    Planet_Saturn, Planet_Jupiter, Planet_Earth, Planet_Mars,
    Planet_Neptune, Planet_PlanetX, Planet_Ceres, Planet_Eris,

    // ── 幻灵（先接 6 张，足够支撑幻灵包和增强牌测试） ──
    Spectral_Talisman,   // 选 1 张：金色印章
    Spectral_Aura,       // 选 1 张：随机版本 Foil/Holo/Polychrome
    Spectral_Immolate,   // 摧毁最多 5 张选中手牌，+$20
    Spectral_DejaVu,     // 选 1 张：红色印章
    Spectral_Trance,     // 选 1 张：蓝色印章
    Spectral_Medium,     // 选 1 张：紫色印章
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
    int maxSelection = 0;               // 至多生效 N 张；0 表示不限
    ConsumableEffect effect;
};

ConsumableKind kindOf(ConsumableType type);
Consumable createConsumable(ConsumableType type);
ConsumableType randomTarotType();
ConsumableType randomPlanetType();
ConsumableType randomSpectralType();

#endif
