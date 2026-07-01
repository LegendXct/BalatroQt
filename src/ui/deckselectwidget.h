#ifndef DECKSELECTWIDGET_H
#define DECKSELECTWIDGET_H

#include <QPair>
#include <QPixmap>
#include <QVector>
#include <QWidget>
#include "../game/gamedeck.h"

class QLabel;
class QPushButton;

// 开局牌组选择层。主菜单“开始游戏”进入本层，确认后由 MainWindow 注入 GameState。
class DeckSelectWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DeckSelectWidget(const QFont &cnFont, QWidget *parent = nullptr);
    GameDeckId selectedDeck() const { return mSelected; }
    int selectedStake() const { return mSelectedStake; }

signals:
    void startRequested(GameDeckId id, int stake);
    void cancelled();

private:
    void select(int idx);
    void selectStake(int idx);
    void refreshSelection();
    void refreshStakeSelection();
    static QPixmap deckBackPixmapFor(GameDeckId id);
    static QString stakeName(int stake);
    static QString stakeDescription(int stake);
    QPixmap deckStackPixmap(const QPixmap &back) const;

    GameDeckId mSelected = GameDeckId::Red;
    QVector<QPair<GameDeckId, QPixmap>> mDeckOptions;
    QVector<QPixmap> mDeckPreviewPixmaps;
    int mSelectedIndex = 0;
    int mSelectedStake = 1;
    int mUnit = 104;
    QLabel *mPreviewLabel = nullptr;
    QLabel *mNameLabel = nullptr;
    QLabel *mDescLabel = nullptr;
    QLabel *mPageLabel = nullptr;
    QLabel *mStakeChipLabel = nullptr;
    QLabel *mStakeNameLabel = nullptr;
    QLabel *mStakeDescLabel = nullptr;
    QVector<QWidget*> mDeckPips;
    QVector<QWidget*> mStakePips;
    QVector<QWidget*> mDeckStakeRows;
    QPushButton *mPrevButton = nullptr;
    QPushButton *mNextButton = nullptr;
};

#endif // DECKSELECTWIDGET_H
