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
    void prepareEntrancePositions();
    void arrangeCards(bool initialFloat);

signals:
    void selectClicked();
    void skipClicked(int idx);

protected:
    void resizeEvent(QResizeEvent *e) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    GameState *mGS;
    QFont mCNFont, mPixelFont;

    // 对齐原版 blind_select 比例：原版盲注卡约 2.9 tile × 7 tile ≈ 1:2.4。
    // 上一版 300×620 (1:2.07) 把高度压得太短；改回更接近原版的 280×680 (1:2.43)，
    // 保持横向能塞下三张 + 间距，又让卡看上去依旧"高瘦"。
    static constexpr int CARD_W          = 280;
    static constexpr int CARD_H          = 680;
    static constexpr int GAP             = 28;
    static constexpr int LEFT_MARGIN     = 24;    // 距左面板右沿
    static constexpr int CURRENT_LIFT    = 38;    // 当前可选卡上提
    static constexpr int BOTTOM_OVERFLOW = 28;    // 卡片底部超出屏幕

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
