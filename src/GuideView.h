#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QTextBrowser;

//================================================================================
// The README, with the running version above it.
class GuideView : public QWidget {
    Q_OBJECT

public:
//================================================================================
    explicit GuideView(QWidget* parent = nullptr);

signals:
//================================================================================
    void backRequested();

protected:
//================================================================================
    void resizeEvent(QResizeEvent* event) override;
//================================================================================
    void showEvent(QShowEvent* event) override;

private:
//================================================================================
    void placeChildren();

    QLabel* m_version = nullptr;
    QTextBrowser* m_text = nullptr;
    QPushButton* m_back = nullptr;
};
