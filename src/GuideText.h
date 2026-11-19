#pragma once

#include <QString>

//================================================================================
// The guide page shows the README. This turns the markdown subset that file
// uses into HTML a QTextBrowser can draw.
QString guideHtml(const QString& markdown);
