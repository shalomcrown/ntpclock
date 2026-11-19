#include "GuideText.h"

#include <QRegularExpression>
#include <QStringList>

#include <algorithm>

namespace {
//================================================================================
QString inlineFormat(const QString& plain) {
    QString text = plain.toHtmlEscaped();
    text.replace(QRegularExpression(QStringLiteral("!\\[([^\\]]*)\\]\\(([^)]+)\\)")),
                 QStringLiteral("<img src=\"\\2\" alt=\"\\1\" width=\"480\"/>"));
    text.replace(QRegularExpression(QStringLiteral("\\[([^\\]]+)\\]\\(([^)]+)\\)")),
                 QStringLiteral("<a href=\"\\2\">\\1</a>"));
    text.replace(QRegularExpression(QStringLiteral("`([^`]+)`")), QStringLiteral("<code>\\1</code>"));
    text.replace(QRegularExpression(QStringLiteral("\\*\\*([^*]+)\\*\\*")), QStringLiteral("<strong>\\1</strong>"));
    return text;
}

//================================================================================
bool isTableRow(const QString& line) {
    return line.trimmed().startsWith(QLatin1Char('|'));
}

//================================================================================
bool isSeparatorRow(const QString& line) {
    static const QRegularExpression pattern(QStringLiteral("^\\|?\\s*:?-{3,}:?\\s*(\\|\\s*:?-{3,}:?\\s*)+\\|?\\s*$"));
    return pattern.match(line.trimmed()).hasMatch();
}

//================================================================================
QString tableHtml(const QStringList& rows) {
    QString html = QStringLiteral("<table>");
    bool header = true;
    for (const QString& row : rows) {
        if (isSeparatorRow(row)) {
            header = false;
            continue;
        }
        const QString tag = header ? QStringLiteral("th") : QStringLiteral("td");
        html += QStringLiteral("<tr>");
        const QStringList cells = row.split(QLatin1Char('|'));
        for (int index = 0; index < cells.size(); ++index) {
            if ((index == 0 || index + 1 == cells.size()) && cells.at(index).trimmed().isEmpty()) {
                continue;
            }
            html += QStringLiteral("<%1>%2</%1>").arg(tag, inlineFormat(cells.at(index).trimmed()));
        }
        html += QStringLiteral("</tr>");
        header = false;
    }
    html += QStringLiteral("</table>");
    return html;
}
}  // namespace

//================================================================================
QString guideHtml(const QString& markdown) {
    const QStringList lines = markdown.split(QLatin1Char('\n'));
    QString html = QStringLiteral("<html><body style=\"color:#ffffff; background:#000000;\">");
    QString paragraph;
    auto flushParagraph = [&] {
        const QString text = paragraph.trimmed();
        paragraph.clear();
        if (text.isEmpty()) {
            return;
        }
        html += QStringLiteral("<p>") + inlineFormat(text) + QStringLiteral("</p>");
    };
    for (int index = 0; index < lines.size(); ++index) {
        const QString line = lines.at(index).endsWith(QLatin1Char('\r')) ? lines.at(index).chopped(1) : lines.at(index);
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QStringLiteral("```"))) {
            flushParagraph();
            QString code;
            ++index;
            while (index < lines.size() && lines.at(index).trimmed().startsWith(QStringLiteral("```")) == false) {
                if (code.isEmpty() == false) {
                    code += QLatin1Char('\n');
                }
                const QString codeLine = lines.at(index).endsWith(QLatin1Char('\r')) ? lines.at(index).chopped(1) : lines.at(index);
                code += codeLine.toHtmlEscaped();
                ++index;
            }
            html += QStringLiteral("<pre>") + code + QStringLiteral("</pre>");
            continue;
        }
        if (isTableRow(trimmed)) {
            flushParagraph();
            QStringList rows;
            while (index < lines.size() && isTableRow(lines.at(index).trimmed())) {
                rows.append(lines.at(index).trimmed());
                ++index;
            }
            --index;
            html += tableHtml(rows);
            continue;
        }
        if (trimmed.startsWith(QStringLiteral("- "))) {
            flushParagraph();
            html += QStringLiteral("<ul>");
            while (index < lines.size() && lines.at(index).trimmed().startsWith(QStringLiteral("- "))) {
                html += QStringLiteral("<li>") + inlineFormat(lines.at(index).trimmed().mid(2)) + QStringLiteral("</li>");
                ++index;
            }
            --index;
            html += QStringLiteral("</ul>");
            continue;
        }
        if (trimmed.isEmpty()) {
            flushParagraph();
            continue;
        }
        if (trimmed == QStringLiteral("---")) {
            flushParagraph();
            html += QStringLiteral("<hr/>");
            continue;
        }
        if (trimmed.startsWith(QStringLiteral("#"))) {
            flushParagraph();
            int level = 0;
            while (level < trimmed.size() && trimmed.at(level) == QLatin1Char('#')) {
                ++level;
            }
            level = std::clamp(level, 1, 3);
            html += QStringLiteral("<h%1>%2</h%1>").arg(level).arg(inlineFormat(trimmed.mid(level).trimmed()));
            continue;
        }
        if (paragraph.isEmpty() == false) {
            paragraph += QLatin1Char(' ');
        }
        paragraph += trimmed;
    }
    flushParagraph();
    html += QStringLiteral("</body></html>");
    return html;
}
