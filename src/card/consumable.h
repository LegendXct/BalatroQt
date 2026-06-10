#ifndef CONSUMABLE_H
#define CONSUMABLE_H

#include <QString>
#include <QVector>
#include <functional>

class GameState;

enum class ConsumableKind { Tarot, Planet, Spectral };

enum class ConsumableType {
    // ── 塔罗（原版 22 张） ──
    Tarot_Fool,          // 生成上一张使用过的消耗牌
    Tarot_Magician,      // 选≤2张：Lucky 增强
    Tarot_HighPriestess, // 生成最多 2 张星球牌
    Tarot_Empress,       // 选≤2张：Mult 增强
    Tarot_Emperor,       // 生成最多 2 张塔罗牌
    Tarot_Hierophant,    // 选≤2张：Bonus 增强
    Tarot_Lovers,        // 选 1 张：Wild  增强
    Tarot_Chariot,       // 选 1 张：Steel 增强
    Tarot_Justice,       // 选 1 张：Glass 增强
    Tarot_Hermit,        // 不选牌：金币翻倍（最多+20）
    Tarot_Wheel,         // 1/4 概率给随机小丑加版本
    Tarot_Strength,      // 选≤2张：点数 +1
    Tarot_HangedMan,     // 选≤2张：摧毁选中手牌
    Tarot_Death,         // 选 2 张：左牌复制右牌
    Tarot_Temperance,    // 按小丑售价给钱（最多+50）
    Tarot_Devil,         // 选 1 张：Gold 增强
    Tarot_Tower,         // 选 1 张：Stone 增强
    Tarot_Star,          // 选≤3张：变方块
    Tarot_Moon,          // 选≤3张：变梅花
    Tarot_Sun,           // 选≤3张：变红心
    Tarot_Judgement,     // 生成 1 张随机小丑
    Tarot_World,         // 选≤3张：变黑桃

    // ── 行星（12 张） ──
    Planet_Pluto, Planet_Mercury, Planet_Uranus, Planet_Venus,
    Planet_Saturn, Planet_Jupiter, Planet_Earth, Planet_Mars,
    Planet_Neptune, Planet_PlanetX, Planet_Ceres, Planet_Eris,

    // ── 幻灵（原版 18 张） ──
    Spectral_Familiar,   // 摧毁 1 张随机手牌，生成 3 张增强人头牌
    Spectral_Grim,       // 摧毁 1 张随机手牌，生成 2 张增强 A
    Spectral_Incantation,// 摧毁 1 张随机手牌，生成 4 张增强数字牌
    Spectral_Talisman,   // 选 1 张：金色印章
    Spectral_Aura,       // 选 1 张：随机版本 Foil/Holo/Polychrome
    Spectral_Wraith,     // 生成 1 张稀有小丑，金币归零
    Spectral_Sigil,      // 手牌全部变成同一随机花色
    Spectral_Ouija,      // 手牌全部变成同一随机点数，手牌上限 -1
    Spectral_Ectoplasm,  // 随机小丑变负片，手牌上限 -1
    Spectral_Immolate,   // 随机摧毁 5 张手牌，+$20
    Spectral_Ankh,       // 复制 1 张随机小丑，摧毁其他小丑
    Spectral_DejaVu,     // 选 1 张：红色印章
    Spectral_Hex,        // 随机小丑变多彩，摧毁其他小丑
    Spectral_Trance,     // 选 1 张：蓝色印章
    Spectral_Medium,     // 选 1 张：紫色印章
    Spectral_Cryptid,    // 复制 1 张选中手牌 2 次
    Spectral_Soul,       // 生成 1 张传奇小丑
    Spectral_BlackHole,  // 所有牌型等级 +1

    // ── 程设扩展塔罗（追加在枚举末尾，避免挪动 Tarots.png 图集映射；kindOf 显式归类） ──
    Tarot_Iterator,      // 选≤2张：迭代器增强（每次打出后点数+1，K→A→2）
    Tarot_ShallowCopy,   // 选2张：左牌浅拷贝右牌，两牌共享全部状态
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
    bool negative = false;             // Perkeo 复制出的负片消耗牌不占普通槽位
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
