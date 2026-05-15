#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsTextItem>
#include <QGraphicsRectItem>
#include <QGraphicsProxyWidget>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVector>
#include <QSet>
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
#include <functional>
#include "floatingscore.h"
#include "deckviewwidget.h"
#include "dynamicbackgrounditem.h"
#include "splashshaderoverlay.h"

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
    QPushButton *mBtnSortNum = nullptr; // 点数理牌
    QPushButton *mBtnSortSuit = nullptr; // 花色理牌
    QPushButton *mBtnDiscard = nullptr; // 弃牌
    QGraphicsTextItem *mHandCountLabel  = nullptr;  // 8/8
    QGraphicsTextItem *mDeckLabel       = nullptr;  // 52/52
    QGraphicsTextItem *mJokerCountLabel = nullptr;  // 0/5

    QVector<CardItem *> mHandCards; // 手牌
    QVector<CardItem *> mPlayedCards; // 出牌区
    QSet<int> mShatteredPlayedIndices;
    QVector <int> mSelected; // 选中的手牌下标

    RoundEndOverlay *mRoundEndOverlay = nullptr;

    // 布局常量
    int mLeftW = 390;
    int mWinW = 1920;
    int mWinH = 1080;
    int mSceneW = 1620;
    int mSceneH = 1080;
    static constexpr int CARD_W = CardItem::WIDTH;
    static constexpr int CARD_H = CardItem::HEIGHT;

    static constexpr int JOKER_Y = 8;
    static constexpr int JOKER_H = CARD_H + 20;
    static constexpr int PLAY_Y = JOKER_H + 150;   // ← 原 +16, 改成 +150
    static constexpr int PLAY_H = 240;
    int mBtnY = 0;
    int mHandY = 0;

    bool mGameOverHandled = false;
    bool mScoringInProgress = false;
    int mEndRoundAnimationDelay = 260;

    QVector<ConsumableItem*> mConsumableItems;

    PackOpenWidget *mPackOpenWidget = nullptr;
    DeckViewWidget *mDeckViewWidget = nullptr;
    DynamicBackgroundItem *mDynamicBg = nullptr;
    SplashShaderOverlay *mSplashOverlay = nullptr;
    PackContent     mPendingPack;        // 当前正在打开的包
    QVector<CardData> mPendingPackHand;  // 开包界面临时翻出的一手牌
    bool mPackFromTag = false;

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

    QLabel *mLblHandName  = nullptr;   // 牌型名(高牌/对子/...)
    QLabel *mLblHandLevel = nullptr;   // "lv.X"

    QVector<FloatingScore*> mFloatingScores;

    void setContextPage(int page);
    void onSkipBlind(int idx);

    void onPackBuyRequested(int slot);
    void onPackChoiceMade(int chosenIdx, QVector<int> selectedPackHandIdx);
    void onInventoryConsumableUseRequested(int inventoryIdx, QVector<int> selectedPackHandIdx);
    void onPackFinished();
    void openImmediateTagPack(PackKind kind);
    void setBackgroundMoodForPhase();
    void setBackgroundMoodForPack(PackKind kind);
    void onDeckClicked(CardItem *card);
    void onJokerPressed(JokerItem *item, Qt::MouseButton btn);
    void onJokerDragMoved(JokerItem *item, QPointF scenePos);
    void onJokerDragReleased(JokerItem *item, QPointF scenePos);
    void showJokerInfo(int idx, bool showSellButton = false);
    void hideJokerInfo();

    void refreshConsumableSlots();
    void layoutConsumableItems(bool animate = true);
    void onConsumableClicked(ConsumableItem *item, Qt::MouseButton btn);
    void onConsumablePressed(ConsumableItem *item, Qt::MouseButton btn);
    void onConsumableDragMoved(ConsumableItem *item, QPointF scenePos);
    void onConsumableDragReleased(ConsumableItem *item, QPointF scenePos);
    void animateConsumableUseThen(int idx, std::function<void()> after);
    void flashConsumableActionError();

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
    void onHandCardDragMoved(CardItem *card, QPointF scenePos);
    void onHandCardDragReleased(CardItem *card, QPointF scenePos);
    void showCardInfo(CardItem *card);
    void hideCardInfo();
    void onPlayClicked();
    void onDiscardClicked();
    void onHandPlayed();
    void onSortByNum();
    void onSortBySuit();
    void setPlayPhaseVisible(bool v);

    void onRoundWon(int blindReward, int handBonus, int interest);
    void onNextBlindClicked();
    void onGameOver(bool won);
    QVector<QGraphicsPixmapItem*> mObtainedTagIcons;
    void addObtainedTag(int tagCol, int tagRow);   // 在右下角加一个 tag 图
    void clearObtainedTags();

    ShopWidget *mShopWidget = nullptr;
    QVector<JokerItem*> mJokerItems;     // 主场景上已持有的小丑视图
    int mSelectedJokerIdx = -1;           // 当前打开操作面板的小丑
    QGraphicsProxyWidget *mJokerInfoProxy = nullptr;
    QWidget *mJokerInfoPanel = nullptr;
    QLabel *mJokerInfoName = nullptr;
    QLabel *mJokerInfoDesc = nullptr;
    QLabel *mJokerInfoMeta = nullptr;
    QPushButton *mJokerSellButton = nullptr;

    // 消耗牌点击后的原版式小操作牌片（使用 / 售出）
    int mSelectedConsumableIdx = -1;
    QGraphicsProxyWidget *mConsumableActionProxy = nullptr;
    QWidget *mConsumableActionPanel = nullptr;
    QLabel *mConsumableActionPrice = nullptr;
    QPushButton *mConsumableUseButton = nullptr;
    QPushButton *mConsumableSellButton = nullptr;
    void showConsumableAction(int idx);
    void hideConsumableAction();

    QGraphicsProxyWidget *mGameOverProxy = nullptr;
    QWidget *mGameOverPanel = nullptr;

    QGraphicsProxyWidget *mCardInfoProxy = nullptr;
    QWidget *mCardInfoPanel = nullptr;
    QLabel *mCardInfoName = nullptr;
    QLabel *mCardInfoDesc = nullptr;

    QWidget           *mPlayPage          = nullptr;     // 对局页（包 mLeftPanel + mView）
    BlindSelectWidget *mBlindSelectWidget = nullptr;

    void onBlindSelectEntered();
    void onBlindStarted();
    QRect lowerOverlayRect() const;
    void showShopOverlay();
    void animateCollectRoundCardsThen(std::function<void()> after);
    void onSelectBlindClicked();

    void onLeaveShopClicked();
    void refreshJokerSlots();
    void refreshJokerSlotFrames();
    void refreshConsumableSlotFrames();

    bool eventFilter(QObject *obj, QEvent *ev) override;
    void spawnFloatingText(const QPointF &nearPos, const QString &text, const QColor &color);
    void clearFloatingScores();

    double mDisplayedChips = 0.0;   // 当前显示中的 chips(动画过程中)，用 double 避免高倍率溢出
    double mDisplayedMult  = 0.0;

    // 原版每次累加分数都重算火焰强度:earned >= required 才点燃,log5 公式控制大小。
    // 不再用单次触发布尔,真正按"当前 displayed chips × mult"实时驱动。
    QWidget *mChipFlame = nullptr;       // 蓝色筹码方块上的火焰
    QWidget *mMultFlame = nullptr;       // 红色倍率方块上的火焰
    double   mChipFlameTarget = 0.0;
    double   mMultFlameTarget = 0.0;
    double   mChipFlameReal   = 0.0;
    double   mMultFlameReal   = 0.0;
    QTimer  *mFlameTick = nullptr;
    QWidget *mChipsRowWidget = nullptr;      // 引用 chipsRow 用于定位火焰
    void triggerSplashShader();              // 原版 splash.fs 全屏 GPU 溅射(占位)
    void updateFlameIntensity();
    void resetScoreFlame();

    // 拖拽时记录上一次目标位置，避免每次 dragMoved 都触发 moveTo()
    int mLastJokerDragTo = -1;
    int mLastHandCardDragTo = -1;
    int mLastConsumableDragTo = -1;

    QPointF mPlayBtnHome;
    QPointF mSortBtnHome;
    QPointF mDiscardBtnHome;

    void updateHandPreview();
    void playScoreEvent(const ScoreEvent &ev);
    void animateScoreTotalThenFinalize(double gained, int delayAfterEvents);
    void animatePlayedCardsToDiscardThen(std::function<void()> after);
    void showGameOverOverlay(bool won);
    void hideGameOverOverlay();
    void resetTransientOverlaysForNewRun();

    void hidePlayControlsForScoring();
    void showPlayControlsAfterScoring();

protected:
    void resizeEvent(QResizeEvent *event) override;
};
#endif // MAINWINDOW_H
