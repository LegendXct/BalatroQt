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
    void playEnterAnimation();

signals:
    void selectClicked();
    void skipClicked(int idx);

protected:
    void resizeEvent(QResizeEvent *e) override;

private:
    GameState *mGS;
    QFont mCNFont, mPixelFont;

    QWidget *mContainer = nullptr;

    struct BlindCard {
        QWidget     *card;
        QPushButton *actionBtn;     // 顶部状态按钮
        QLabel      *banner;        // 名字横幅
        QLabel      *chipImg;
        QLabel      *bossDescLbl;
        QLabel      *targetLbl;
        QLabel      *rewardLbl;
        QWidget     *skipBox;       // "或" + 跳过按钮容器(Boss 卡隐藏)
        QPushButton *skipBtn;
    };
    BlindCard mCards[3];

    static constexpr int CARD_W = 280;
    static constexpr int CARD_H = 540;
    static constexpr int GAP    = 8;

    void buildUi();
    QPixmap chipPixmap(int blindIdx) const;
    int     targetFor(int blindIdx) const;
    int     rewardFor(int blindIdx) const;
};

#endif
