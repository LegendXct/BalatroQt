#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsTextItem>
#include <QGraphicsRectItem>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVector>
#include "../game/gamestate.h"
#include "../card/carditem.h"
#include "roundendoverlay.h"
#include <QMessageBox>
#include "shopwidget.h"
#include "../card/jokeritem.h"
#include "../card/consumableitem.h"
#include "packopenwidget.h"
#include "blindselectwidget.h"
#include <QStackedWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    QGraphicsScene *mScene = new QGraphicsScene;
    QGraphicsView *mView = nullptr;
    GameState *mGameState = nullptr;

    QFont mPixelFont; // m6x11plus
    QFont mCNFont; // NotoSansSC

    QWidget *mLeftPanel = nullptr;

    QLabel *mLblBlind = nullptr; // 大/小/Boss 盲注名称
    QLabel *mLblTarget = nullptr; // 目标分数
    QLabel *mLblReward = nullptr;
    QLabel *mLblScore = nullptr; // 回合分数
    QLabel *mLblChips = nullptr; // 筹码值
    QLabel *mLblMult = nullptr; // 倍率值
    QLabel *mLblHands = nullptr; // 剩余出牌次数
    QLabel *mLblDiscards = nullptr; // 剩余弃牌次数
    QLabel *mLblGold = nullptr; // 金币
    QLabel *mLblAnte = nullptr; // 底注
    QLabel *mLblRound = nullptr; // 回合

    QPushButton *mBtnPlay = nullptr; // 出牌
    QPushButton *mBtnSort = nullptr;
    QPushButton *mBtnSortNum = nullptr; // 点数理牌
    QPushButton *mBtnSortSuit = nullptr; // 花色理牌
    QPushButton *mBtnDiscard = nullptr; // 弃牌
    QGraphicsTextItem *mHandCountLabel  = nullptr;  // 8/8
    QGraphicsTextItem* mHandTypeLabel = nullptr; // 牌型名称
    QGraphicsTextItem *mHandScoreLabel = nullptr; // 本次得分
    QGraphicsTextItem *mDeckLabel       = nullptr;  // 52/52

    QVector<CardItem *> mHandCards; // 手牌
    QVector<CardItem *> mPlayedCards; // 出牌区
    QVector <int> mSelected; // 选中的手牌下标

    RoundEndOverlay *mRoundEndOverlay = nullptr;

    // 布局常量
    int mLeftW = 340;
    int mWinW = 1920;
    int mWinH = 1080;
    int mSceneW = 1620;
    int mSceneH = 1080;
    static constexpr int CARD_W = CardItem::WIDTH;
    static constexpr int CARD_H = CardItem::HEIGHT;

    static constexpr int JOKER_Y = 8;
    static constexpr int JOKER_H = CARD_H + 20;
    static constexpr int PLAY_Y = JOKER_H + 16;
    static constexpr int PLAY_H = 240;
    int mBtnY = 0;
    int mHandY = 0;

    bool mGameOverHandled = false;

    QVector<ConsumableItem*> mConsumableItems;

    PackOpenWidget *mPackOpenWidget = nullptr;
    PackContent     mPendingPack;        // 当前正在打开的包

    QGraphicsTextItem *mConsCountLabel = nullptr;
    QGraphicsRectItem *mPlayBgRect     = nullptr;
    QVector<QGraphicsRectItem*> mJokerSlotRects;
    QVector<QGraphicsRectItem*> mConsumableSlotRects;
    CardItem          *mDeckBackCard   = nullptr;

    QGraphicsProxyWidget *mPlayProxy    = nullptr;
    QGraphicsProxyWidget *mSortProxy    = nullptr;
    QGraphicsProxyWidget *mDiscardProxy = nullptr;

    QLabel *mBlindChipLbl = nullptr;

    // 上下文区(BlindSelect / Blind / Shop 三态)
    QStackedWidget *mContextArea  = nullptr;
    QWidget *mCtxBlindSelect      = nullptr;
    QWidget *mCtxBlind            = nullptr;
    QWidget *mCtxShop             = nullptr;
    QLabel  *mCtxBlindChipImg     = nullptr;

    void setContextPage(int page);
    void onSkipBlind(int idx);

    void onPackBuyRequested(int slot);
    void onPackChoiceMade(int chosenIdx);

    void refreshConsumableSlots();
    void onConsumableClicked(ConsumableItem *item, Qt::MouseButton btn);

    void loadFonts();
    void setupLeftPanel();
    void setupScene();
    void setupSceneButtons();
    void setupConnections();

    void refreshHand();
    void refreshScore();
    void refreshGold();
    void refreshCounters();
    void clearPlayedCards();
    void layoutHandCards();
    void layoutPlayedCards();

    void onCardClicked(CardItem *card);
    void onPlayClicked();
    void onDiscardClicked();
    void onHandPlayed();
    void onSortByNum();
    void onSortBySuit();

    void onRoundWon(int blindReward, int handBonus, int interest);
    void onNextBlindClicked();
    void onGameOver(bool won);

    ShopWidget *mShopWidget = nullptr;
    QVector<JokerItem*> mJokerItems;     // 主场景上已持有的小丑视图

    QWidget           *mPlayPage          = nullptr;     // 对局页（包 mLeftPanel + mView）
    BlindSelectWidget *mBlindSelectWidget = nullptr;

    void onBlindSelectEntered();
    void onBlindStarted();
    void onSelectBlindClicked();

    void onLeaveShopClicked();
    void refreshJokerSlots();

    bool eventFilter(QObject *obj, QEvent *ev) override;

protected:
    void resizeEvent(QResizeEvent *event) override;
};
#endif // MAINWINDOW_H
