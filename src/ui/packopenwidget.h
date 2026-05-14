#ifndef PACKOPENWIDGET_H
#define PACKOPENWIDGET_H

#include <QWidget>
#include <QVector>
#include "../game/boosterpack.h"
#include "../card/consumable.h"

class QLabel;
class QPushButton;
class QTimer;
class PackHandCardWidget;

class PackOpenWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PackOpenWidget(const QFont &cnFont, const QFont &pixelFont,
                            QWidget *parent = nullptr);

    // 二级开包界面：
    // content：包里刷出的牌；packHand：这次开包临时翻出的一手牌；
    // inventoryConsumables：玩家仓库里的塔罗/星球/幻灵，可在开包界面右侧使用。
    void open(const PackContent &content,
              const QVector<CardData> &packHand,
              const QVector<Consumable> &inventoryConsumables,
              int freeJokerSlots);

    void setPackHand(const QVector<CardData> &packHand);
    void setInventoryConsumables(const QVector<Consumable> &inventoryConsumables);
    void setFreeJokerSlots(int freeSlots);
    QVector<int> selectedHandIndices() const { return mSelectedHand; }
    const QVector<CardData> &packHand() const { return mPackHand; }

signals:
    void choiceMade(int chosenIdx, QVector<int> selectedHandIdx);    // 选择/使用包里的第 chosenIdx 张
    void inventoryConsumableRequested(int inventoryIdx, QVector<int> selectedHandIdx);
    void packHandReordered(QVector<CardData> packHand);
    void packFinished();                                             // 达到 choose 数或跳过

protected:
    void resizeEvent(QResizeEvent *e) override;

private slots:
    void onChoose(int idx);
    void onUseInventory(int idx);
    void onHandCardClicked(int idx);
    void onHandCardDragged(int idx, QPoint localPos);
    void onHandCardReleased(int idx, QPoint localPos);
    void onSkip();

private:
    QFont mCNFont, mPixelFont;
    PackContent mContent;
    QVector<CardData> mPackHand;
    QVector<Consumable> mInventoryConsumables;

    int mFreeJokerSlots = 0;
    int mChoicesUsed = 0;
    QVector<int> mChosenOptions;
    QVector<int> mSelectedHand;
    bool mFinishing = false;
    QTimer *mSoulAnimTimer = nullptr;

    QWidget *mPanel = nullptr;
    QWidget *mHandBox = nullptr;
    QWidget *mInventoryBox = nullptr;
    QLabel *mLblTitle = nullptr;
    QLabel *mLblChoose = nullptr;
    QPushButton *mBtnSkip = nullptr;

    struct HandUi {
        PackHandCardWidget *btn = nullptr;
        QLabel *nameLbl = nullptr;
    };
    QVector<HandUi> mHandUi;

    struct OptUi {
        QWidget *card = nullptr;
        QLabel  *imageLbl = nullptr;
        QLabel  *nameLbl = nullptr;
        QLabel  *descLbl = nullptr;
        QPushButton *takeBtn = nullptr;
    };
    QVector<OptUi> mOptUi;

    struct InvUi {
        QWidget *card = nullptr;
        QLabel *imageLbl = nullptr;
        QLabel *nameLbl = nullptr;
        QPushButton *useBtn = nullptr;
    };
    QVector<InvUi> mInvUi;

    void buildUi();
    void layoutPanel();
    void layoutPackHand();
    void refreshAll();
    void refreshHandUi();
    void refreshOptionUi();
    void refreshInventoryUi();
    void finishAndClose();
    void animateCardsIn();
    void applyPackHandOrderMove(int from, int to);

    int optionCount() const;
    QPixmap renderOption(int i) const;
    QPixmap renderPlayingCard(const CardData &c, const QSize &size) const;
    QPixmap renderConsumable(ConsumableType type, const QSize &size) const;
    QString optionName(int i) const;
    QString optionDesc(int i) const;
    bool optionAvailableFor(int i) const;
    bool inventoryAvailableFor(int i) const;
    bool packUsesHandSelection() const;
    bool selectionValidFor(const Consumable &c) const;
    int maxCurrentSelectionLimit() const;
    bool optionAlreadyChosen(int i) const { return mChosenOptions.contains(i); }
};

#endif
