#ifndef GAMEDECK_H
#define GAMEDECK_H

#include <QString>
#include <memory>
#include "../utils/constants.h"

// 游戏牌组体系：与 DeckSkin（纯卡面换肤）正交，牌组改变游戏核心规则。
// 抽象基类 + 派生类，GameState 经 unique_ptr 持有，开局前由 UI 注入。
class GameDeckType
{
public:
    virtual ~GameDeckType() = default;
    virtual QString name() const = 0;
    virtual QString description() const = 0;         // 选牌组界面的效果文案
    virtual int  extraHands()    const { return 0; }  // 每回合出牌次数修正
    virtual int  extraDiscards() const { return 0; }  // 每回合弃牌次数修正
    virtual int  selectionWindow() const { return 1 << 30; } // 可选牌窗口（默认不限）
    virtual bool allowHandSort() const { return true; }       // 是否允许排序/拖拽重排
    virtual bool mustIncludeNewest() const { return false; }  // 出/弃是否必须含最新到手的牌（栈顶）
    virtual QString handMarkerText() const { return QString(); } // 手牌行标记文案（空 = 不显示）
    virtual bool handMarkerAtTail() const { return false; }   // 标记挂行尾（栈顶）还是行首（队首）
};

// 基础牌组：现状行为，全部默认。
class BaseGameDeck : public GameDeckType
{
public:
    QString name() const override;
    QString description() const override;
};

// 队列牌组：手牌为先进先出队列，仅队首窗口内可选；+1 出牌 +1 弃牌补偿。
class QueueGameDeck : public GameDeckType
{
public:
    QString name() const override;
    QString description() const override;
    int  extraHands()    const override { return Constants::QUEUE_DECK_EXTRA_HANDS; }
    int  extraDiscards() const override { return Constants::QUEUE_DECK_EXTRA_DISCARDS; }
    int  selectionWindow() const override { return Constants::QUEUE_DECK_WINDOW; }
    bool allowHandSort() const override { return false; }
    QString handMarkerText() const override { return QStringLiteral("队首 →"); }
};

// 栈牌组：手牌为后进先出栈，最新到手的"栈顶"牌必须参与每次出牌/弃牌；
// 同样禁止整理手牌（锁定抽牌序），+1 出牌 +1 弃牌补偿。
class StackGameDeck : public GameDeckType
{
public:
    QString name() const override;
    QString description() const override;
    int  extraHands()    const override { return Constants::STACK_DECK_EXTRA_HANDS; }
    int  extraDiscards() const override { return Constants::STACK_DECK_EXTRA_DISCARDS; }
    bool allowHandSort() const override { return false; }
    bool mustIncludeNewest() const override { return true; }
    QString handMarkerText() const override { return QStringLiteral("← 栈顶"); }
    bool handMarkerAtTail() const override { return true; }
};

// 牌组 id（UI 枚举/记忆所选牌组用）与工厂。
enum class GameDeckId { Base = 0, Queue = 1, Stack = 2 };
std::unique_ptr<GameDeckType> createGameDeck(GameDeckId id);

#endif // GAMEDECK_H
