#ifndef DECKSELECTWIDGET_H
#define DECKSELECTWIDGET_H

#include <QWidget>
#include <QVector>
#include "../game/gamedeck.h"

class QLabel;
class QPushButton;

// 开局牌组选择层（overlay，遵循项目"不用原生 QDialog"的规矩）。
// 主菜单"开始游戏"→ 本层 → startRequested(deckId) → MainWindow 注入 GameState 后开局。
class DeckSelectWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DeckSelectWidget(const QFont &cnFont, QWidget *parent = nullptr);
    GameDeckId selectedDeck() const { return mSelected; }

signals:
    void startRequested(GameDeckId id);
    void cancelled();

private:
    void select(int idx);
    static QPixmap hueShifted(const QPixmap &src, int dh);

    GameDeckId mSelected = GameDeckId::Base;
    QVector<QWidget*> mOptionFrames;
    QLabel *mDescLabel = nullptr;
};

#endif // DECKSELECTWIDGET_H
