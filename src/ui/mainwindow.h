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
#include <QList>
#include <QTimer>
#include <QAbstractAnimation>
#include <QPointer>
#include <QPixmap>
#include <QSizeF>
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
#include "animatedblindchip.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QProgressBar;
class QGraphicsDropShadowEffect;
class QPropertyAnimation;
class QGraphicsObject;

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
    QProgressBar *mScoreProgressBar = nullptr; // 回合分数 / 目标分数进度条
    QGraphicsDropShadowEffect *mScoreProgressGlow = nullptr; // 进度条发光效果
    QPropertyAnimation *mScoreProgressAnim = nullptr; // 进度条平滑过渡
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
    QPushButton *mBtnBestPlay = nullptr; // 最佳出牌提示
    QGraphicsTextItem *mHandCountLabel  = nullptr;  // 8/8
    QPointer<class QVariantAnimation> mHandCountLabelAnim;  // 手牌行下沉时 8/8 标签的动画
    QGraphicsTextItem *mDeckLabel       = nullptr;  // 52/52
    QGraphicsTextItem *mJokerCountLabel = nullptr;  // 0/5

    QVector<CardItem *> mHandCards; // 手牌
    QVector<CardItem *> mPlayedCards; // 出牌区
    QSet<int> mShatteredPlayedIndices;
    QVector <int> mSelected; // 选中的手牌下标
    bool mBestPlayHintActive = false; // 第二次点击最佳出牌时恢复原点数/花色顺序
    // 第一次点最佳出牌前的手牌 uid 顺序快照——第二次点击时直接回到这一顺序，
    // 这样玩家如果在点最佳出牌之前手动拖动过牌，第二次也能精确恢复到那个排列，
    // 而不是被强行 re-sort 到点数/花色默认序。
    QVector<int> mBestPlayHintHandOrder;

    RoundEndOverlay *mRoundEndOverlay = nullptr;

    // 布局常量
    int mLeftW = 380;
    int mWinW = 1920;
    int mWinH = 1080;
    int mSceneW = 1620;
    int mSceneH = 1080;
    static constexpr int CARD_W = CardItem::WIDTH;
    static constexpr int CARD_H = CardItem::HEIGHT;

    // 顶部小丑 / 消耗槽离屏幕上沿留一段呼吸空间——原版那一列贴到顶部以下约 0.2 倍 CARD_H。
    static constexpr int JOKER_Y = 28;
    // 顶部槽位整体高度跟随 JokerItem 显示尺寸（已经从 CARD_H=228 缩到 198）。
    static constexpr int JOKER_H = JokerItem::HEIGHT + 20;
    // 顶部小丑 / 消耗槽使用的卡牌显示宽高——专门给那两行布局用，与 CARD_W/CARD_H 区分。
    static constexpr int TOP_SLOT_W = JokerItem::WIDTH;
    static constexpr int TOP_SLOT_H = JokerItem::HEIGHT;
    static constexpr int PLAY_Y = JOKER_H + 130;   // 卡牌高度增大后稍微靠上一些
    static constexpr int PLAY_H = CARD_H + 50;
    // 右下角牌组宽度 = CARD_W + 边距，需要随卡牌尺寸放大同步增加。
    static constexpr int HAND_RIGHT_RESERVE = CARD_W + 90;
    int mBtnY = 0;
    int mHandY = 0;
    int mHandYNormal  = 0;   // 选牌期间的 hand y
    int mHandYScoring = 0;   // 出牌计分期间的 hand y(下移让出 play 区下方空间)

    bool mGameOverHandled = false;
    bool mScoringInProgress = false;
    bool mSuppressHandReveal = false;   // 消耗牌翻面序列期间，抑制 onHandChanged 里的房屋 Boss 翻正
    int mEndRoundAnimationDelay = 260;

    // ── 局内进程暂停（打开比赛信息/选项/牌组时）──
    bool mGamePaused = false;
    QList<QPointer<QTimer>> mGameTimers;   // scheduleGame 创建的可暂停定时器
    QPointer<QAbstractAnimation> mScoreCountAnim;  // 当前的回合总分计数动画（可暂停）
    void scheduleGame(int delayMs, std::function<void()> fn);  // 替代计分链里的 singleShot
    void pauseGameProcesses();
    void resumeGameProcesses();

    QVector<ConsumableItem*> mConsumableItems;

    PackOpenWidget *mPackOpenWidget = nullptr;
    DeckViewWidget *mDeckViewWidget = nullptr;
    DynamicBackgroundItem *mDynamicBg = nullptr;
    SplashShaderOverlay *mSplashOverlay = nullptr;
    PackContent     mPendingPack;        // 当前正在打开的包
    QVector<CardData> mPendingPackHand;  // 开包界面临时翻出的一手牌
    bool mPackFromTag = false;

    struct PendingSlotFlyIn {
        bool active = false;
        int targetArea = 0;          // 1 = Joker, 2 = 消耗牌
        QPointF sceneStartTopLeft;
        QSizeF sceneSize;
        QPixmap pixmap;             // 顶层飞行动画使用的牌面
        QPoint globalCenter;        // 购买/选择时的屏幕起点，避免被商店/开包 overlay 遮住
    };
    PendingSlotFlyIn mPendingSlotFlyIn;

    QGraphicsTextItem *mConsCountLabel = nullptr;
    QGraphicsRectItem *mPlayBgRect     = nullptr;
    // 升级为 QGraphicsItem* 以容纳圆角矩形（QGraphicsPathItem）。
    QVector<QGraphicsItem*> mJokerSlotRects;
    QVector<QGraphicsItem*> mConsumableSlotRects;
    CardItem          *mDeckBackCard   = nullptr;
    QGraphicsRectItem *mDeckStatsItem  = nullptr;  // 牌堆上的"查看牌组"提示
    QGraphicsRectItem *mDeckPeekPanel  = nullptr;  // 出牌阶段悬停时从屏幕顶部滑入的牌型统计面板
    QPointer<class QVariantAnimation> mDeckPeekAnim;
    bool               mDeckPeekDeployed = false;  // 当前是否处于"hover 牌堆 + 统计面板可见"状态
    bool               mDeckViewOpen     = false;  // "查看牌组"全屏面板是否打开——开启期间屏蔽牌堆 hover
    void showDeckPeekPanel();
    void hideDeckPeekPanel();
    void buildDeckPeekPanel();
    void layoutHandWithDeckPeek(bool peeking);

    QGraphicsProxyWidget *mPlayProxy    = nullptr;
    QGraphicsProxyWidget *mSortProxy    = nullptr;
    QGraphicsProxyWidget *mDiscardProxy = nullptr;
    QGraphicsProxyWidget *mBestPlayProxy = nullptr;

    QLabel *mBlindChipLbl = nullptr;

    // 上下文区(BlindSelect / Blind / Shop 三态)
    QStackedWidget *mContextArea  = nullptr;
    QPointer<class QAbstractAnimation> mContextTransition;  // 当前正在跑的上下文区滑动动画
    QWidget *mCtxBlindSelect      = nullptr;
    QWidget *mCtxBlind            = nullptr;
    QWidget *mCtxShop             = nullptr;
    class AnimatedBlindChip *mCtxBlindChipImg = nullptr;

    QLabel *mLblHandName  = nullptr;   // 牌型名(高牌/对子/...)
    QLabel *mLblHandLevel = nullptr;   // "lv.X"

    // 牌型升级动画（行星牌 / 黑洞 / 同道之星等）：复刻原版 level_up_hand
    // 的侧边栏分步演出（基础倍率→基础筹码→等级颜色，逐步抖动 + 变色）。
    QHash<HandType, HandLevel> mPrevHandLevels;     // 上一次 handLevelsChanged 时的快照
    bool mHandLevelInitialized = false;
    bool mHandLevelAnimating   = false;             // 动画期间冻结 updateHandPreview()
    int  mHandLevelAnimToken   = 0;                 // 防止过期回调踩到新一次动画
    void onHandLevelsChanged();
    void playHandLevelUpAnimation(HandType t, int prevLevel, int newLevel,
                                  int prevChips, int newChips,
                                  int prevMult, int newMult);
    // 黑洞 / 同道之星：一次性提升所有牌型等级时，原版（card.lua:1153）使用
    // "All Hands" + chips/mult 显示为 "+" 而非具体数值的简化动画。
    void playAllHandsLevelUpAnimation();
    // 让一个 QLabel 做一次"juice_up"风格的字号脉冲（先放大再回弹），不依赖 layout 位置改动。
    void juiceLabelPulse(QLabel *lbl, double scaleUp = 1.18, int durationMs = 360);
    // 在某个 QLabel 上方短暂浮出一个"+N"色块，对应原版 attention_text(cover=…)。
    // 用透明子 QLabel 覆盖在 anchor 标签所在矩形之上，淡入 → 悬停 → 淡出后自动销毁。
    void spawnLabelDelta(QLabel *anchor, const QString &text, const QColor &bgColor);

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
    void onDeckHoverChanged(CardItem *card, bool hovered);
    void updateDeckStatsPopup();
    void onJokerPressed(JokerItem *item, Qt::MouseButton btn);
    void onJokerDragMoved(JokerItem *item, QPointF scenePos);
    void onJokerDragReleased(JokerItem *item, QPointF scenePos);
    void showJokerInfo(int idx, bool showSellButton = false);
    void hideJokerInfo();
    // 计算 joker[idx] 的运行时状态后缀（"当前：X1.5 倍率" / "本回合花色：黑桃" 等），
    // 悬浮 hover 和点击 sell 面板共用。
    QString jokerRuntimeStateSuffix(int idx) const;
    // 按 mSelectedJokerIdx 重排所有 joker 的 y——选中那一张抬高 0.2*HEIGHT，其他落回基线。
    void applyJokerSelectionLift();

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
    void prepareSlotFlyInAnimation(const QPixmap &pixmap, const QPoint &globalCenter, int targetArea);
    QRect sceneRectOnPlayPage(const QPointF &sceneTopLeft, const QSizeF &sceneSize) const;
    void animateTopLayerCardToScene(const QPixmap &pixmap, const QPoint &globalCenter,
                                    const QPointF &targetSceneTopLeft, const QSizeF &sceneSize,
                                    bool flipToBack, QGraphicsObject *revealItem = nullptr);
    QPixmap deckBackPixmap() const;

    void refreshHand();
    void refreshScore();
    void updateScoreProgressBar(double displayedScore, bool animate = true);
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
    void onBestPlayHint();
    void setPlayPhaseVisible(bool v);

    void onRoundWon(int blindReward, int handBonus, int interest);
    void onNextBlindClicked();
    void onGameOver(bool won);
    struct ObtainedTagEntry {
        TagType type;
        QGraphicsPixmapItem *item;
    };
    QVector<ObtainedTagEntry> mObtainedTagIcons;
    void addObtainedTag(TagType type, int tagCol, int tagRow); // 在右下角加一个 tag 图标
    void removeObtainedTag(TagType type);                       // 标签被使用后再移除
    void clearObtainedTags();
    void relayoutObtainedTags();

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

    // 统一的"原版样式描述浮窗"：手牌（playing card）、小丑、消耗牌悬浮时都用这个。
    // 关键：必须是 mPlayPage 的子 QWidget，不要塞进 QGraphicsScene——
    // BalatroInfoPanel 自身带 QGraphicsDropShadowEffect，再套一层 QGraphicsProxyWidget
    // 在某些驱动上会渲染成空白/0×0，导致 hover 完全不显示（商店里的浮窗是子 widget 所以正常）。
    class BalatroInfoCluster *mHoverTooltip = nullptr;
    void ensureHoverTooltip();
    void showHoverTooltipNearScene(class QGraphicsObject *anchor, double anchorWidth);
    void showCardHoverTooltip(CardItem *card);
    void showJokerHoverTooltip(int jokerIdx);
    void showConsumableHoverTooltip(int consumableIdx);
    void hideHoverTooltip();

    QWidget           *mPlayPage          = nullptr;     // 对局页（包 mLeftPanel + mView）
    BlindSelectWidget *mBlindSelectWidget = nullptr;

    void onBlindSelectEntered();
    void onBlindStarted();
    QRect lowerOverlayRect() const;
    // 商店覆盖层使用更大的可用区域（不为右侧牌组留宽度），避免商品边缘被裁。
    QRect shopOverlayRect() const;
    void showShopOverlay();
    void animateShopEntrance();
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

    // 拖动小丑/消耗结束 → 通过 moveJoker/moveConsumable 触发 refresh，但希望新生成的
    // item 从"旧 index 的位置"开始 moveTo 到目标，而不是瞬移。
    // refreshXxxSlots 检测到非 {-1,-1} 时按 from/to 推断 new→old 映射做平滑过渡。
    struct PendingReorder { int from = -1; int to = -1; };
    PendingReorder mPendingJokerReorder;
    PendingReorder mPendingConsumableReorder;

    QPointF mPlayBtnHome;
    QPointF mSortBtnHome;
    QPointF mDiscardBtnHome;
    QPointF mBestPlayBtnHome;

    void updateHandPreview();
    void playScoreEvent(const ScoreEvent &ev);
    void animateScoreTotalThenFinalize(double gained, int delayAfterEvents);
    void animatePlayedCardsToDiscardThen(std::function<void()> after);
    void showGameOverOverlay(bool won);
    void hideGameOverOverlay();
    void resetTransientOverlaysForNewRun();

    // 选项菜单使用主窗口内覆盖层，避免全屏 + QOpenGLWidget 场景中弹出原生 QDialog
    // 在部分显卡/驱动上触发黑屏重建。
    QWidget *mOptionsOverlay = nullptr;
    QPointer<QWidget> mRunInfoOverlay;    // 替代旧的 QDialog 形式的"比赛信息"窗口；
                                          // 用 QPointer 防止 deleteLater 后留下悬空指针被二次访问
    void showOptionsOverlay();
    void hideOptionsOverlay();
    void startNewRunFromOptions();

    void hidePlayControlsForScoring();
    void showPlayControlsAfterScoring();
    void fitSceneToView();
    // 当窗口大小或纵横比变化时调整场景宽度，并重排所有依赖 mSceneW/mSceneH 的元素。
    void updateSceneSize();
    void layoutSceneButtons();

protected:
    void resizeEvent(QResizeEvent *event) override;
};
#endif // MAINWINDOW_H
