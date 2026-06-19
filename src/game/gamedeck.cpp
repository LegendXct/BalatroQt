#include "gamedeck.h"

namespace {
struct DeckMeta {
    GameDeckId id;
    QString name;
    QString description;
    QPoint spritePos;
    int extraHands = 0;
    int extraDiscards = 0;
};

const QVector<DeckMeta> &deckMetas()
{
    static const QVector<DeckMeta> metas = {
        { GameDeckId::Red,       QStringLiteral("红色牌组"),
          QStringLiteral("每回合 +1 次弃牌。"), {0, 0}, 0, 1 },
        { GameDeckId::Blue,      QStringLiteral("蓝色牌组"),
          QStringLiteral("每回合 +1 次出牌。"), {0, 2}, 1, 0 },
        { GameDeckId::Yellow,    QStringLiteral("黄色牌组"),
          QStringLiteral("开局时额外获得 $10。"), {1, 2} },
        { GameDeckId::Green,     QStringLiteral("绿色牌组"),
          QStringLiteral("每回合结束时：每剩 1 次出牌获得 $2，每剩 1 次弃牌获得 $1。不赚取利息。"), {2, 2} },
        { GameDeckId::Black,     QStringLiteral("黑色牌组"),
          QStringLiteral("小丑牌槽位 +1。每回合 -1 次出牌。"), {3, 2}, -1, 0 },
        { GameDeckId::Magic,     QStringLiteral("魔法牌组"),
          QStringLiteral("开局时拥有水晶球优惠券和 2 张愚者。"), {0, 3} },
        { GameDeckId::Nebula,    QStringLiteral("星云牌组"),
          QStringLiteral("开局时拥有望远镜优惠券。消耗牌槽位 -1。"), {3, 0} },
        { GameDeckId::Ghost,     QStringLiteral("幽灵牌组"),
          QStringLiteral("商店中可能出现幻灵牌。初始带有妖法牌。"), {6, 2} },
        { GameDeckId::Abandoned, QStringLiteral("废弃牌组"),
          QStringLiteral("开局时玩家牌组中没有人头牌。"), {3, 3} },
        { GameDeckId::Checkered, QStringLiteral("方格牌组"),
          QStringLiteral("开局时牌组中有 26 张黑桃和 26 张红桃。"), {1, 3} },
        { GameDeckId::Zodiac,    QStringLiteral("黄道牌组"),
          QStringLiteral("开局时拥有塔罗牌商人、星球牌商人和库存过剩。"), {3, 4} },
        { GameDeckId::Painted,   QStringLiteral("彩绘牌组"),
          QStringLiteral("手牌上限 +2。小丑牌槽位 -1。"), {4, 3} },
        { GameDeckId::Anaglyph,  QStringLiteral("浮雕牌组"),
          QStringLiteral("每次击败 Boss 盲注后获得一个双倍标签。"), {2, 4} },
        { GameDeckId::Plasma,    QStringLiteral("等离子牌组"),
          QStringLiteral("计算出牌分数时平衡筹码和倍率。盲注要求分数 X2。"), {4, 2} },
        { GameDeckId::Erratic,   QStringLiteral("古怪牌组"),
          QStringLiteral("牌组中所有牌的点数和花色都是随机的。"), {2, 3} },
    };
    return metas;
}

const DeckMeta *findMeta(GameDeckId id)
{
    for (const DeckMeta &meta : deckMetas())
        if (meta.id == id) return &meta;
    return nullptr;
}
} // namespace

QString BalatroGameDeck::name() const
{
    if (const DeckMeta *meta = findMeta(mId)) return meta->name;
    return QStringLiteral("红色牌组");
}

QString BalatroGameDeck::description() const
{
    if (const DeckMeta *meta = findMeta(mId)) return meta->description;
    return QStringLiteral("每回合 +1 次弃牌。");
}

QPoint BalatroGameDeck::spritePos() const
{
    if (const DeckMeta *meta = findMeta(mId)) return meta->spritePos;
    return {0, 0};
}

int BalatroGameDeck::extraHands() const
{
    if (const DeckMeta *meta = findMeta(mId)) return meta->extraHands;
    return 0;
}

int BalatroGameDeck::extraDiscards() const
{
    if (const DeckMeta *meta = findMeta(mId)) return meta->extraDiscards;
    return 0;
}

QString QueueGameDeck::name() const { return QStringLiteral("队列牌组"); }
QString QueueGameDeck::description() const
{
    return QStringLiteral("手牌按抽牌顺序排列（先进先出），禁止整理；\n"
                          "只能选取最前 %1 张牌出牌或弃牌。\n"
                          "每回合 +%2 出牌次数、+%3 弃牌次数。")
        .arg(Constants::QUEUE_DECK_WINDOW)
        .arg(Constants::QUEUE_DECK_EXTRA_HANDS)
        .arg(Constants::QUEUE_DECK_EXTRA_DISCARDS);
}

QString StackGameDeck::name() const { return QStringLiteral("栈牌组"); }
QString StackGameDeck::description() const
{
    return QStringLiteral("手牌按抽牌顺序排列（后进先出），禁止整理；\n"
                          "出牌/弃牌必须包含最新到手的“栈顶”牌。\n"
                          "每回合 +%1 出牌次数、+%2 弃牌次数。")
        .arg(Constants::STACK_DECK_EXTRA_HANDS)
        .arg(Constants::STACK_DECK_EXTRA_DISCARDS);
}

QVector<GameDeckId> originalGameDeckOrder()
{
    QVector<GameDeckId> ids;
    ids.reserve(deckMetas().size());
    for (const DeckMeta &meta : deckMetas()) ids.append(meta.id);
    return ids;
}

std::unique_ptr<GameDeckType> createGameDeck(GameDeckId id)
{
    switch (id) {
    case GameDeckId::Queue: return std::make_unique<QueueGameDeck>();
    case GameDeckId::Stack: return std::make_unique<StackGameDeck>();
    default:                return std::make_unique<BalatroGameDeck>(findMeta(id) ? id : GameDeckId::Red);
    }
}
