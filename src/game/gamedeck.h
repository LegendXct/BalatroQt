#ifndef GAMEDECK_H
#define GAMEDECK_H

#include <QPoint>
#include <QString>
#include <QVector>
#include <memory>
#include "../utils/constants.h"

enum class GameDeckId {
    Red = 0,
    Blue,
    Yellow,
    Green,
    Black,
    Magic,
    Nebula,
    Ghost,
    Abandoned,
    Checkered,
    Zodiac,
    Painted,
    Anaglyph,
    Plasma,
    Erratic,

    // 程设扩展牌组：保留旧逻辑，但不进入原版牌组选择列表。
    Queue,
    Stack,
};

// 游戏牌组规则。原版 Back 的数据来自 balatro_original_code/game.lua 的 G.P_CENTERS 配置。
class GameDeckType
{
public:
    virtual ~GameDeckType() = default;
    virtual GameDeckId id() const = 0;
    virtual QString name() const = 0;
    virtual QString description() const = 0;
    virtual QPoint spritePos() const = 0;              // Enhancers.png 中的牌背坐标
    virtual int  extraHands()    const { return 0; }   // 每回合出牌次数修正
    virtual int  extraDiscards() const { return 0; }   // 每回合弃牌次数修正
    virtual int  selectionWindow() const { return 1 << 30; }
    virtual bool allowHandSort() const { return true; }
    virtual bool mustIncludeNewest() const { return false; }
    virtual QString handMarkerText() const { return QString(); }
    virtual bool handMarkerAtTail() const { return false; }
};

class BalatroGameDeck : public GameDeckType
{
public:
    explicit BalatroGameDeck(GameDeckId id) : mId(id) {}
    GameDeckId id() const override { return mId; }
    QString name() const override;
    QString description() const override;
    QPoint spritePos() const override;
    int extraHands() const override;
    int extraDiscards() const override;

private:
    GameDeckId mId;
};

class QueueGameDeck : public GameDeckType
{
public:
    GameDeckId id() const override { return GameDeckId::Queue; }
    QString name() const override;
    QString description() const override;
    QPoint spritePos() const override { return {0, 0}; }
    int  extraHands()    const override { return Constants::QUEUE_DECK_EXTRA_HANDS; }
    int  extraDiscards() const override { return Constants::QUEUE_DECK_EXTRA_DISCARDS; }
    int  selectionWindow() const override { return Constants::QUEUE_DECK_WINDOW; }
    bool allowHandSort() const override { return false; }
    QString handMarkerText() const override { return QStringLiteral("队首 ->"); }
};

class StackGameDeck : public GameDeckType
{
public:
    GameDeckId id() const override { return GameDeckId::Stack; }
    QString name() const override;
    QString description() const override;
    QPoint spritePos() const override { return {0, 0}; }
    int  extraHands()    const override { return Constants::STACK_DECK_EXTRA_HANDS; }
    int  extraDiscards() const override { return Constants::STACK_DECK_EXTRA_DISCARDS; }
    bool allowHandSort() const override { return false; }
    bool mustIncludeNewest() const override { return true; }
    QString handMarkerText() const override { return QStringLiteral("-> 栈顶"); }
    bool handMarkerAtTail() const override { return true; }
};

QVector<GameDeckId> originalGameDeckOrder();
std::unique_ptr<GameDeckType> createGameDeck(GameDeckId id);

#endif // GAMEDECK_H
