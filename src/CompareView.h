#pragma once

#include <QImage>
#include <QString>
#include <QWidget>

class QLabel;
class QPushButton;

//================================================================================
// The other screen's picture beside a screenshot of this clock, taken together.
class CompareView : public QWidget {
    Q_OBJECT

public:
//================================================================================
    explicit CompareView(QWidget* parent = nullptr);

//================================================================================
    void setPair(const QImage& otherScreen, const QImage& clock, const QString& latency = QString());

signals:
//================================================================================
    void backRequested();

protected:
//================================================================================
    void paintEvent(QPaintEvent* event) override;
//================================================================================
    void resizeEvent(QResizeEvent* event) override;
//================================================================================
    void showEvent(QShowEvent* event) override;

private:
//================================================================================
    void drawPane(QPainter& painter, const QRect& pane, const QImage& image, const QString& caption) const;
//================================================================================
    void placeChildren();

    QImage m_other;
    QImage m_clock;
    QPushButton* m_back = nullptr;
    QLabel* m_result = nullptr;
};
