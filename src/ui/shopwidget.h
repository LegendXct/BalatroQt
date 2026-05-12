#ifndef SHOPWIDGET_H
#define SHOPWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include "../game/gamestate.h"
#include "../card/consumable.h"

class ShopWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ShopWidget(GameState *gs,
                        const QFont &cnFont, const QFont &pixelFont,
                        QWidget *parent = nullptr);
    void refresh();

signals:
    void leaveClicked();
    void packBuyRequested(int slot);

protected:
    void resizeEvent(QResizeEvent *e) override;

private slots:
    void onReroll();

private:
    struct OfferUi {
        QWidget     *card     = nullptr;
        QLabel      *imageLbl = nullptr;   // 当前 cpp 没用,保留以备拓展
        QLabel      *nameLbl  = nullptr;
        QLabel      *priceLbl = nullptr;
        QPushButton *cardBtn  = nullptr;
    };

    void buildUi();
    void layoutPanel();
    OfferUi createOfferSlot(QWidget *parent, bool isBooster);
    QPixmap offerPixmap(const ShopOffer &o) const;

    void onBuyShop(int slot);
    void onBuyBooster(int slot);
    void onBuyVoucher(int slot);
    QPixmap playingCardPixmap(const CardData &c) const;

    GameState *mGS;
    QFont mCNFont;
    QFont mPixelFont;

    QWidget     *mPanel        = nullptr;
    QPushButton *mBtnNextRound = nullptr;
    QPushButton *mBtnReroll    = nullptr;
    QLabel      *mLblGold      = nullptr;

    QVector<OfferUi> mShopUi;
    QVector<OfferUi> mVoucherUi;
    QVector<OfferUi> mBoosterUi;
};

#endif
