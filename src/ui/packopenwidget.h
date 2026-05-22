#ifndef PACKOPENWIDGET_H
#define PACKOPENWIDGET_H

#include <QWidget>
#include <QVector>
#include <QPixmap>
#include <QPoint>
#include "../game/boosterpack.h"
#include "../card/consumable.h"

class QLabel;
class QPushButton;
class QTimer;
class QGraphicsScene;
class QGraphicsView;
class QGraphicsPixmapItem;
class CardItem;
class BalatroInfoPanel;

class PackOpenWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PackOpenWidget(const QFont &cnFont, const QFont &pixelFont,
                            QWidget *parent = nullptr);

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
    void choiceAnimationRequested(const QPixmap &pixmap, const QPoint &globalCenter, int targetArea);
    void choiceMade(int chosenIdx, QVector<int> selectedHandIdx);
    void inventoryConsumableRequested(int inventoryIdx, QVector<int> selectedHandIdx);
    void packHandReordered(QVector<CardData> packHand);
    void packFinished();

protected:
    void resizeEvent(QResizeEvent *e) override;
    bool eventFilter(QObject *obj, QEvent *e) override;

private slots:
    void onChoose(int idx);
    void onUseInventory(int idx);
    void onSkip();
    void onPackCardClicked(CardItem *card);
    void onPackCardDragMoved(CardItem *card, QPointF scenePos);
    void onPackCardDragReleased(CardItem *card, QPointF scenePos);
    void onPackCardHoverChanged(CardItem *card, bool hovered);

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
    int mLastDragTo = -1;

    QWidget *mPanel = nullptr;
    QGraphicsScene *mHandScene = nullptr;
    QGraphicsView  *mHandView  = nullptr;
    QVector<CardItem*> mPackHandItems;

    // 开包动画覆盖层：盖在 mPanel 上方 raise()，播完后 hide()，让原本的选项和手牌"从包里炸出来"。
    QGraphicsScene *mRevealScene = nullptr;
    QGraphicsView  *mRevealView  = nullptr;
    QGraphicsPixmapItem *mRevealPackItem = nullptr;
    QTimer *mRevealDissolveTimer = nullptr;
    double mRevealDissolveT = 0.0;
    QPixmap mRevealPackBase;
    bool mRevealActive = false;
    QWidget *mInventoryBox = nullptr;
    QLabel *mLblTitle = nullptr;
    QLabel *mLblChoose = nullptr;
    QPushButton *mBtnSkip = nullptr;

    struct OptUi {
        QWidget *card = nullptr;
        QLabel  *imageLbl = nullptr;
        QLabel  *nameLbl = nullptr;
        QPushButton *takeBtn = nullptr;
        QRect imageRestRect;     // 未聚焦时 imageLbl 的几何
        QRect imageLiftRect;     // 聚焦时 imageLbl 上移后的几何
        QRect nameRestRect;
        QRect nameLiftRect;
    };
    QVector<OptUi> mOptUi;
    // 当前被点击聚焦的选项（对齐原版：点击牌 -> 牌上移 -> 弹出"选择/使用"按钮）。
    int mFocusedOptIdx = -1;
    void setOptionFocused(int idx, bool focused, bool animate);

    // 描述浮窗：手牌 / 选项卡 hover 时出现，遵循原版 generate_card_ui 样式。
    BalatroInfoPanel *mInfoTooltip = nullptr;
    void showOptionTooltip(int idx);
    void showHandCardTooltip(CardItem *card);
    void hideTooltip();
    int  mHoveredOptIdx = -1;

    struct InvUi {
        QWidget *card = nullptr;
        QLabel *imageLbl = nullptr;
        QLabel *nameLbl = nullptr;
        QPushButton *useBtn = nullptr;
    };
    QVector<InvUi> mInvUi;

    void buildUi();
    void layoutPanel();
    void layoutPackHand(int skipIdx = -1, bool instant = false);
    void refreshAll();
    void refreshHandUi();
    void refreshOptionUi();
    void refreshInventoryUi();
    void finishAndClose();
    void animateCardsIn();
    void applyPackHandOrderMove(int from, int to);

    // 整套开包序列：浮现 → 晃动 → 溶解 → 喷射选项卡。
    void buildRevealOverlay();
    QPixmap renderPackBigPixmap() const;
    void startPackReveal();
    void endPackReveal();

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
