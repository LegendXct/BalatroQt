#include "deck.h"
#include "demoscript.h"
#include <QRandomGenerator>
#include <stdexcept>

Deck::Deck() {
    buildDeck();
}

CardData Deck::clearTransientFlags(CardData card)
{
    // Boss 盲注的 debuff 只是“本盲注临时状态”，不能存回牌组。
    card.isDebuffed = false;
    card.faceUp = true;
    return card;
}

void Deck::buildDeck() {
    mDrawPile.clear();
    mDiscardPile.clear();
    const Suit suits[] = {
        Suit::Spades, Suit::Hearts,
        Suit::Diamonds, Suit::Clubs
    };
    for (Suit s : suits) {
        for (int r = static_cast<int>(Rank::Two); r <= static_cast<int>(Rank::Ace); r++) {
            CardData card;
            card.suit = s;
            card.rank = static_cast<Rank>(r);
            mDrawPile.append(card);
        }
    }
    shuffle();
}

void Deck::reset() {
    // 原版每个盲注开始会重新洗当前完整牌组；这里把弃牌堆合回摸牌堆。
    for (const CardData &c : mDiscardPile)
        mDrawPile.append(clearTransientFlags(c));
    mDiscardPile.clear();
    shuffle();
    // 演示模式：洗完后再把脚本规定的 8 张换到 mDrawPile 前面，保证开局手牌稳定。
    // 必须在 shuffle 之后做，否则会被随机打散；只换位置，不新增/删卡。
    if (DemoScript::active()) DemoScript::reorderDeckForNextBlind(mDrawPile);
}

void Deck::shuffle() {
    // Fisher-Yates 洗牌算法
    for (int i = mDrawPile.size() - 1; i > 0; i--) {
        int j = QRandomGenerator::global()->bounded(i+1);
        mDrawPile.swapItemsAt(i, j);
    }
}

void Deck::discard(const CardData& card) {
    mDiscardPile.append(clearTransientFlags(card));
}

CardData Deck::draw() {
    if (isEmpty()) {
        throw std::out_of_range("Deck is empty");
    }
    return clearTransientFlags(mDrawPile.takeFirst());
}

bool Deck::isEmpty() const {
    return mDrawPile.isEmpty();
}

int Deck::remaining() const {
    return mDrawPile.size();
}

int Deck::totalKnown() const {
    return mDrawPile.size() + mDiscardPile.size();
}

void Deck::addCard(const CardData &card) {
    mDrawPile.append(clearTransientFlags(card));
    shuffle();
}

void Deck::setCards(const QVector<CardData> &cards, bool shuffleCards)
{
    mDrawPile.clear();
    mDiscardPile.clear();
    for (const CardData &card : cards)
        mDrawPile.append(clearTransientFlags(card));
    if (shuffleCards) shuffle();
}

void Deck::returnCards(const QVector<CardData> &cards)
{
    for (const CardData &card : cards)
        mDrawPile.append(clearTransientFlags(card));
}

QVector<CardData> Deck::allKnownCards() const
{
    QVector<CardData> out = mDrawPile;
    for (CardData &c : out) c = clearTransientFlags(c);
    for (const CardData &c : mDiscardPile)
        out.append(clearTransientFlags(c));
    return out;
}

CardData *Deck::findByUid(int uid)
{
    for (CardData &c : mDrawPile)    if (c.uid == uid) return &c;
    for (CardData &c : mDiscardPile) if (c.uid == uid) return &c;
    return nullptr;
}

bool Deck::removeByUid(int uid, CardData *removed)
{
    auto removeFrom = [uid, removed](QVector<CardData> &cards) {
        for (int i = 0; i < cards.size(); ++i) {
            if (cards[i].uid != uid) continue;
            if (removed) *removed = cards[i];
            cards.removeAt(i);
            return true;
        }
        return false;
    };
    return removeFrom(mDrawPile) || removeFrom(mDiscardPile);
}
