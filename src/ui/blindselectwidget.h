#ifndef BLINDSELECTWIDGET_H
#define BLINDSELECTWIDGET_H

#include <QWidget>
#include "../game/gamestate.h"

class QLabel;
class QPushButton;

class BlindSelectWidget : public QWidget
{
    Q_OBJECT
public:
    explicit BlindSelectWidget(GameState *gs, const QFont &cnFont,
                               const QFont &pixelFont, QWidget *parent = nullptr);
    void refresh();
    void arrangeCards(bool initialFloat);

signals:
    void selectClicked();
    void skipClicked(int idx);

protected:
    void resizeEvent(QResizeEvent *e) override;

private:
    GameState *mGS;
    QFont mCNFont, mPixelFont;

    static constexpr int CARD_W          = 250;
    static constexpr int CARD_H          = 720;
    static constexpr int GAP             = 35;
    static constexpr int LEFT_MARGIN     = 30;    // 距左面板右沿
    static constexpr int CURRENT_LIFT    = 40;    // 当前可选卡上提
    static constexpr int BOTTOM_OVERFLOW = 80;    // 卡片底部超出屏幕

    QPoint cardTargetPos(int idx) const;

    struct BlindCard {
        QWidget     *card;
        QPushButton *actionBtn;     // 顶部状态按钮
        QWidget     *upperBox;
        QLabel      *banner;        // 名字横幅
        QLabel      *chipImg;
        QLabel      *bossDescLbl;
        QLabel      *targetLbl;
        QLabel      *rewardTextLbl = nullptr;    // 中文"奖励"
        QLabel      *rewardSymLbl  = nullptr;    // 像素"$$$"
        QLabel      *orLbl;
        QWidget *bossPromptBox = nullptr;
        QWidget     *skipBox;       // "或" + 跳过按钮容器(Boss 卡隐藏)
        QPushButton *skipBtn;
    };
    BlindCard mCards[3];

    void buildUi();
    QPixmap chipPixmap(int blindIdx) const;
    int     targetFor(int blindIdx) const;
    int     rewardFor(int blindIdx) const;
};

#endif
