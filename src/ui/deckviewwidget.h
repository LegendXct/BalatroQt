#ifndef DECKVIEWWIDGET_H
#define DECKVIEWWIDGET_H

#include <QWidget>
#include <QVector>
#include "../card/carddata.h"

class QLabel;
class QPushButton;
class QScrollArea;
class QGridLayout;
class QResizeEvent;

class DeckViewWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DeckViewWidget(const QFont &cnFont, const QFont &pixelFont,
                            QWidget *parent = nullptr);

    void open(const QVector<CardData> &remainingCards,
              const QVector<CardData> &fullDeckCards);

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void showRemaining();
    void showFull();
    void closeView();

private:
    QFont mCNFont;
    QFont mPixelFont;
    QVector<CardData> mRemainingCards;
    QVector<CardData> mFullDeckCards;
    bool mShowingFull = false;

    QWidget *mPanel = nullptr;
    QLabel *mTitle = nullptr;
    QLabel *mSubtitle = nullptr;
    QPushButton *mBtnRemaining = nullptr;
    QPushButton *mBtnFull = nullptr;
    QPushButton *mBtnClose = nullptr;
    QScrollArea *mScroll = nullptr;
    QWidget *mGridHost = nullptr;
    QGridLayout *mGrid = nullptr;

    void buildUi();
    void layoutPanel();
    void refreshTabs();
    void refreshGrid();
    QPixmap renderCard(const CardData &card, const QSize &size) const;
    QString cardExtraText(const CardData &card) const;
};

#endif // DECKVIEWWIDGET_H
