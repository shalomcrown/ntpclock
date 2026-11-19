#include "GuideText.h"
#include "GuideView.h"

#include <QCoreApplication>
#include <QFile>
#include <QLabel>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTextBrowser>
#include <QTextDocument>
#include <QUrl>
#include <QVariant>

#include <gtest/gtest.h>

//================================================================================
TEST(GuideText, TurnsTheReadmeSubsetIntoHtml) {
    const QString html = guideHtml(QStringLiteral(
            "# NTP Clock\n"
            "\n"
            "A **large** clock. See [the code](https://github.com/shalomcrown/ntpclock).\n"
            "\n"
            "![The clock](docs/images/clock.jpg)\n"
            "\n"
            "- Tap Guide\n"
            "- Press Space\n"
            "\n"
            "| Server | What it is |\n"
            "| --- | --- |\n"
            "| pool.ntp.org | NTP pool aggregate |\n"
            "\n"
            "```bash\n"
            "./install-apk.sh\n"
            "```\n"));
    EXPECT_TRUE(html.contains(QStringLiteral("<h1>NTP Clock</h1>")));
    EXPECT_TRUE(html.contains(QStringLiteral("<strong>large</strong>")));
    EXPECT_TRUE(html.contains(QStringLiteral("<a href=\"https://github.com/shalomcrown/ntpclock\">the code</a>")));
    EXPECT_TRUE(html.contains(QStringLiteral("<img src=\"docs/images/clock.jpg\"")));
    EXPECT_TRUE(html.contains(QStringLiteral("<li>Tap Guide</li>")));
    EXPECT_TRUE(html.contains(QStringLiteral("<th>Server</th>")));
    EXPECT_TRUE(html.contains(QStringLiteral("<td>pool.ntp.org</td>")));
    EXPECT_TRUE(html.contains(QStringLiteral("<pre>./install-apk.sh</pre>")));
    EXPECT_FALSE(html.contains(QStringLiteral("<th>---</th>")));
}

//================================================================================
TEST(GuideView, ShowsTheVersionAboveTheReadme) {
    GuideView view;
    view.resize(800, 600);
    view.show();

    auto* version = view.findChild<QLabel*>(QStringLiteral("guideVersion"));
    auto* text = view.findChild<QTextBrowser*>(QStringLiteral("guideText"));
    ASSERT_NE(version, nullptr);
    ASSERT_NE(text, nullptr);
    EXPECT_EQ(version->y(), 12);
    EXPECT_GT(version->height(), 0);
    EXPECT_GT(text->y(), version->y());
    EXPECT_TRUE(text->toPlainText().contains(QStringLiteral("NTP Clock")));
    EXPECT_TRUE(QFile::exists(QStringLiteral(":/guide/docs/images/clock.jpg")));

    const QVariant image = text->document()->resource(
            QTextDocument::ImageResource, QUrl(QStringLiteral("qrc:/guide/docs/images/clock.jpg")));
    EXPECT_TRUE(image.isValid());
}

//================================================================================
TEST(GuideView, DraggingScrollsInsteadOfSelecting) {
    GuideView view;
    view.resize(400, 320);
    view.show();

    auto* text = view.findChild<QTextBrowser*>(QStringLiteral("guideText"));
    ASSERT_NE(text, nullptr);
    EXPECT_FALSE(text->textInteractionFlags().testFlag(Qt::TextSelectableByMouse));
    QWidget* viewport = text->viewport();

    QCoreApplication::processEvents();
    ASSERT_GT(text->verticalScrollBar()->maximum(), 0);
    const int before = text->verticalScrollBar()->value();
    const QPoint start(viewport->width() / 2, viewport->height() * 3 / 4);
    const QPoint end(start.x(), 8);
    QMouseEvent press(QEvent::MouseButtonPress, start, viewport->mapToGlobal(start), Qt::LeftButton, Qt::LeftButton,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(viewport, &press);
    QMouseEvent move(QEvent::MouseMove, end, viewport->mapToGlobal(end), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(viewport, &move);
    QMouseEvent release(QEvent::MouseButtonRelease, end, viewport->mapToGlobal(end), Qt::LeftButton, Qt::NoButton,
                        Qt::NoModifier);
    QCoreApplication::sendEvent(viewport, &release);

    EXPECT_FALSE(text->textCursor().hasSelection());
    EXPECT_GT(text->verticalScrollBar()->value(), before);
}
