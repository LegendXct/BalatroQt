#include "deck.h"
#include <QRandomGenerator>
#include <stdexcept>

Deck::Deck() {
    buildDeck();
}

void Deck::buildDeck() {
    mDrawPile.clear();
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
    // 合并弃牌堆到摸牌堆并洗牌
    mDrawPile.append(mDiscardPile);
    mDiscardPile.clear();
    shuffle();
}

void Deck::shuffle() {
    // Fisher-Yates 洗牌算法
    for (int i = mDrawPile.size() - 1; i > 0; i--) {
        int j = QRandomGenerator::global()->bounded(i+1);
        mDrawPile.swapItemsAt(i, j);
    }
}

void Deck::discard(const CardData& card) {
    mDiscardPile.append(card);
}

CardData Deck::draw() {
    if (isEmpty()) {
        throw std::out_of_range("Deck is empty");
    }
    return mDrawPile.takeFirst();
}

bool Deck::isEmpty() const {
    return mDrawPile.isEmpty();
}

int Deck::remaining() const {
    return mDrawPile.size();
}

void Deck::addCard(const CardData &card) {
    mDrawPile.append(card);
    shuffle();
}
