#ifndef PACKOPENWIDGET_H
#define PACKOPENWIDGET_H

#include <QWidget>
#include "../game/boosterpack.h"

class QLabel;
class QPushButton;

class PackOpenWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PackOpenWidget(const QFont &cnFont, const QFont &pixelFont,
                            QWidget *parent = nullptr);

    // freeJokerSlots / freeConsumableSlots：决定哪些选项的"选择"按钮可点
    void open(const PackContent &content,
              int freeConsumableSlots, int freeJokerSlots);

signals:
    void choiceMade(int chosenIdx);    // -1 = 跳过

protected:
    void resizeEvent(QResizeEvent *e) override;

private slots:
    void onChoose(int idx);
    void onSkip();

private:
    QFont mCNFont, mPixelFont;
    PackContent mContent;
    int mFreeJokerSlots = 0;
    int mFreeConsumableSlots = 0;

    QWidget *mPanel = nullptr;
    QLabel *mLblTitle = nullptr;
    QPushButton *mBtnSkip = nullptr;

    struct OptUi {
        QWidget *card;
        QLabel  *imageLbl, *nameLbl, *descLbl;
        QPushButton *takeBtn;
    };
    QVector<OptUi> mOptUi;

    void buildUi();
    void layoutPanel();

    int optionCount() const;
    QPixmap renderOption(int i) const;
    QString optionName(int i) const;
    QString optionDesc(int i) const;
    bool slotAvailableFor(int i) const;
};

#endif
