#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsRectItem>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVector>
#include "../game/gamestate.h"
#include "../card/carditem.h"

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
    QGraphicsScene *mScene = nullptr;
    QGraphicsView *mView = nullptr;

    GameState *mGameState = nullptr;
    QWidget *mLeftPanel = nullptr;
    QLabel *mLblScore = nullptr; // 回合分数
    QLabel *mLblChips = nullptr; // 筹码值
    QLabel *mLblMult = nullptr; // 倍率值
    QLabel *mLblHands = nullptr; // 剩余出牌次数
    QLabel *mLblDiscards = nullptr; // 剩余弃牌次数
    QLabel *mLblGold = nullptr; // 金币
    QLabel *mLblAnte = nullptr; // 底注/回合
    QLabel *mLblTarget = nullptr; // 目标分数
    QPushButton *mBtnPlay = nullptr; // 出牌
    QPushButton *mBtnDiscard = nullptr; // 弃牌

    QVector<CardItem *> mHandCards; // 手牌
    QVector<CardItem *> mPlayedCards; // 出牌区
    QVector <int> mSelected; // 选中的手牌下标

    static constexpr int LEFT_W = 300;
    static constexpr int WIN_W = 1280;
    static constexpr int WIN_H = 720;
    static constexpr int SCENE_W = WIN_W - LEFT_W;
    static constexpr int SCENE_H = WIN_H;
    static constexpr int JOKER_Y = 10;
    static constexpr int JOKER_H = 110;
    static constexpr int PLAY_Y = 130;
    static constexpr int PLAY_H = 230;
    static constexpr int HAND_Y = 480;
    static constexpr int HAND_H = 220;
    static constexpr int CARD_W = CardItem::WIDTH;
    static constexpr int CARD_H = CardItem::HEIGHT;

    void setupLeftPanel();
    void setupScene();
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
};
#endif // MAINWINDOW_H
