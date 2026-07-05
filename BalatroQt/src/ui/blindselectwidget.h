#ifndef BLINDSELECTWIDGET_H
#define BLINDSELECTWIDGET_H

#include <QWidget>
#include <functional>
#include "../game/gamestate.h"

class QLabel;
class QPushButton;
class AnimatedBlindChip;

class BlindSelectWidget : public QWidget
{
    Q_OBJECT
public:
    explicit BlindSelectWidget(GameState *gs, const QFont &cnFont,
                               const QFont &pixelFont, QWidget *parent = nullptr);
    void refresh();
    void prepareEntrancePositions();
    void arrangeCards(bool initialFloat);
    void animateBossReroll(std::function<void()> applyChange);

signals:
    void selectClicked();
    void skipClicked(int idx);

protected:
    void resizeEvent(QResizeEvent *e) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    GameState *mGS;
    QFont mCNFont, mPixelFont;

    // 对齐原版 blind_select：原版三张盲注卡始终插在屏幕下沿，下半截看不到，整体高大。
    // 这里把 CARD_H 推到 820 让卡看上去"很壮"，BOTTOM_OVERFLOW > CURRENT_LIFT 让当前
    // 卡升起时仍然看不到自己的下边缘（也看不到旁边卡的下边缘）。
    static constexpr int CARD_W          = 320;   // 介于早期 290 与一度尝试的 360 之间，避免视觉过粗
    static constexpr int CARD_H          = 820;
    static constexpr int GAP             = 56;   // 28→56 让三张盲注卡更舒展，不再挤成一堆
    static constexpr int LEFT_MARGIN     = 24;    // 距左面板右沿
    static constexpr int CURRENT_LIFT    = 56;    // 当前可选卡上提
    // 下方至少超出 CURRENT_LIFT + 16 dp，使升起后底边仍在屏幕下方。
    static constexpr int BOTTOM_OVERFLOW = 84;

    QPoint cardTargetPos(int idx) const;

    struct BlindCard {
        QWidget     *card;
        QPushButton *actionBtn;     // 顶部状态按钮
        QWidget     *upperBox;
        QLabel      *banner;        // 名字横幅
        AnimatedBlindChip *chipImg = nullptr;
        QLabel      *bossDescLbl;
        QLabel      *targetLbl;
        QLabel      *rewardTextLbl = nullptr;    // 中文"奖励"
        QLabel      *rewardSymLbl  = nullptr;    // 像素"$$$"
        QLabel      *orLbl;
        QWidget *bossPromptBox = nullptr;
        QPushButton *bossRerollBtn = nullptr;
        QWidget     *skipBox;       // "或" + 跳过按钮容器(Boss 卡隐藏)
        QLabel      *tagIcon = nullptr;
        QLabel      *tagName = nullptr;
        QPushButton *skipBtn;
    };
    BlindCard mCards[3];

    void buildUi();
    void showTagPopup(int idx, QWidget *anchor);
    void hideTagPopup();
    QLabel *mTagPopup = nullptr;
    QPixmap chipPixmap(int blindIdx) const;
    int     targetFor(int blindIdx) const;
    int     rewardFor(int blindIdx) const;
};

#endif
