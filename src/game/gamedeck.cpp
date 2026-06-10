#include "gamedeck.h"

QString BaseGameDeck::name() const { return QStringLiteral("基础牌组"); }
QString BaseGameDeck::description() const
{
    return QStringLiteral("标准 52 张牌组，无特殊规则。");
}

QString QueueGameDeck::name() const { return QStringLiteral("队列牌组"); }
QString QueueGameDeck::description() const
{
    return QStringLiteral("手牌按抽牌顺序排列（先进先出），禁止整理；"
                          "只能选取最前 %1 张牌出牌或弃牌。\n"
                          "每回合 +%2 出牌次数、+%3 弃牌次数。")
        .arg(Constants::QUEUE_DECK_WINDOW)
        .arg(Constants::QUEUE_DECK_EXTRA_HANDS)
        .arg(Constants::QUEUE_DECK_EXTRA_DISCARDS);
}

std::unique_ptr<GameDeckType> createGameDeck(GameDeckId id)
{
    switch (id) {
    case GameDeckId::Queue: return std::make_unique<QueueGameDeck>();
    case GameDeckId::Base:  break;
    }
    return std::make_unique<BaseGameDeck>();
}
