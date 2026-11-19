#include "GuideView.h"

#include "GuideText.h"

#include <QCoreApplication>
#include <QEvent>
#include <QFile>
#include <QFrame>
#include <QLabel>
#include <QMouseEvent>
#include <QPalette>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QScroller>
#include <QScrollerProperties>
#include <QShowEvent>
#include <QTextBrowser>
#include <QUrl>

#include <algorithm>

namespace {
//================================================================================
// QTextBrowser treats a finger drag as a selection. Once the finger has moved,
// this filter scrolls the page and keeps the selection from starting.
class GuideDragScroll : public QObject {
public:
//================================================================================
    explicit GuideDragScroll(QTextBrowser* browser) : QObject(browser), m_browser(browser) {
        // A touch drag is a gesture. Accepting it keeps Qt from also starting a selection.
        QScroller::grabGesture(m_browser->viewport(), QScroller::TouchGesture);
        QScroller* scroller = QScroller::scroller(m_browser->viewport());
        QScrollerProperties properties = scroller->scrollerProperties();
        properties.setScrollMetric(QScrollerProperties::DragStartDistance, 0.001);
        scroller->setScrollerProperties(properties);
        m_browser->viewport()->installEventFilter(this);
    }

protected:
//================================================================================
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (m_forwarding || watched != m_browser->viewport()) {
            return false;
        }
        if (event->type() == QEvent::MouseButtonPress) {
            const auto* mouse = static_cast<QMouseEvent*>(event);
            if (mouse->button() != Qt::LeftButton) {
                return false;
            }
            // Keep the press from placing a text cursor. Otherwise the browser
            // scrolls back to that cursor after the finger moves.
            m_press = mouse->position().toPoint();
            m_last = m_press;
            m_dragged = false;
            return true;
        }
        if (event->type() == QEvent::MouseMove) {
            const auto* mouse = static_cast<QMouseEvent*>(event);
            if ((mouse->buttons() & Qt::LeftButton) == 0) {
                return false;
            }
            scrollFrom(mouse->position().toPoint());
            return true;
        }
        if (event->type() == QEvent::MouseButtonRelease) {
            const auto* mouse = static_cast<QMouseEvent*>(event);
            const bool dragged = m_dragged;
            m_dragged = false;
            if (dragged == false && mouse->button() == Qt::LeftButton) {
                forwardTap(mouse->position().toPoint());
            }
            return true;
        }
        return false;
    }

//================================================================================
    void forwardTap(const QPoint& pos) {
        auto* viewport = m_browser->viewport();
        const QPoint global = viewport->mapToGlobal(pos);
        QMouseEvent press(QEvent::MouseButtonPress, pos, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease, pos, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        m_forwarding = true;
        QCoreApplication::sendEvent(viewport, &press);
        QCoreApplication::sendEvent(viewport, &release);
        m_forwarding = false;
    }

private:
//================================================================================
    void scrollFrom(const QPoint& pos) {
        if (m_dragged == false && (pos - m_press).manhattanLength() < 8) {
            return;
        }
        m_dragged = true;
        m_browser->verticalScrollBar()->setValue(m_browser->verticalScrollBar()->value() + (m_last.y() - pos.y()));
        m_browser->horizontalScrollBar()->setValue(m_browser->horizontalScrollBar()->value() + (m_last.x() - pos.x()));
        m_last = pos;
    }

    QTextBrowser* m_browser;
    QPoint m_press;
    QPoint m_last;
    bool m_dragged = false;
    bool m_forwarding = false;
};
}  // namespace

//================================================================================
GuideView::GuideView(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("guideView"));
    setAutoFillBackground(true);
    QPalette palette = this->palette();
    palette.setColor(QPalette::Window, Qt::black);
    palette.setColor(QPalette::WindowText, Qt::white);
    setPalette(palette);

    m_version = new QLabel(QCoreApplication::applicationVersion(), this);
    m_version->setObjectName(QStringLiteral("guideVersion"));
    m_version->setAlignment(Qt::AlignCenter);
    m_version->setTextInteractionFlags(Qt::NoTextInteraction);
    QFont versionFont = m_version->font();
    versionFont.setPixelSize(18);
    m_version->setFont(versionFont);
    m_version->setStyleSheet(QStringLiteral("QLabel { color: white; background: black; }"));

    m_text = new QTextBrowser(this);
    m_text->setObjectName(QStringLiteral("guideText"));
    m_text->setOpenExternalLinks(true);
    QPalette browserPalette = m_text->palette();
    browserPalette.setColor(QPalette::Base, Qt::black);
    browserPalette.setColor(QPalette::Text, Qt::white);
    m_text->setPalette(browserPalette);
    m_text->setFrameStyle(QFrame::NoFrame);
    m_text->setStyleSheet(QStringLiteral(
            "QTextBrowser { background: black; color: white; font-size: 18px; }"));
    m_text->document()->setDefaultStyleSheet(QStringLiteral(
            "body { color: white; background: black; }"
            "h1, h2, h3 { color: white; }"
            "a { color: #9ecbff; }"
            "code, pre { color: #f2f2f2; }"
            "th, td { color: white; padding: 4px 8px; }"));
    m_text->document()->setBaseUrl(QUrl(QStringLiteral("qrc:/guide/")));

    QFile readme(QStringLiteral(":/guide/README.md"));
    QString markdown;
    if (readme.open(QIODevice::ReadOnly)) {
        markdown = QString::fromUtf8(readme.readAll());
    } else {
        markdown = QStringLiteral("The guide could not be loaded.");
    }
    m_text->setHtml(guideHtml(markdown));
    // A finger drag was starting a text selection. Links stay tappable, and the
    // drag scrolls the page instead.
    m_text->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
    m_text->setCursor(Qt::ArrowCursor);
    new GuideDragScroll(m_text);

    m_back = new QPushButton(QStringLiteral("Back"), this);
    m_back->setObjectName(QStringLiteral("guideBack"));
    m_back->setMinimumHeight(56);
    m_back->setStyleSheet(QStringLiteral("QPushButton { background: white; color: black; font-size: 20px; }"));
    connect(m_back, &QPushButton::clicked, this, &GuideView::backRequested);
}

//================================================================================
void GuideView::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    placeChildren();
}

//================================================================================
void GuideView::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    placeChildren();
    m_back->setFocus();
}

//================================================================================
void GuideView::placeChildren() {
    m_version->setGeometry(16, 12, std::max(0, width() - 32), 28);
    const int buttonWidth = std::min(220, std::max(0, width() - 32));
    m_back->setGeometry((width() - buttonWidth) / 2, std::max(0, height() - 72), buttonWidth, 56);
    const int top = m_version->geometry().bottom() + 8;
    const int bottom = m_back->geometry().top() - 12;
    m_text->setGeometry(16, top, std::max(0, width() - 32), std::max(0, bottom - top));
}
