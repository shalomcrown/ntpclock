#include "VisibleArea.h"

#include <QtGlobal>

#include <algorithm>
#include <cmath>

#if defined(Q_OS_ANDROID)
#include <QJniEnvironment>
#include <QJniObject>
#endif

//================================================================================
QSize VisibleArea::devicePixels() {
#if defined(Q_OS_ANDROID)
    QJniObject activity = QJniObject::callStaticObjectMethod(
            "org/qtproject/qt/android/QtNative", "activity", "()Landroid/app/Activity;");
    if (activity.isValid() == false) {
        return {};
    }
    const jint contentId = 0x01020002;  // android.R.id.content
    QJniObject content = activity.callObjectMethod("findViewById", "(I)Landroid/view/View;", contentId);
    QJniEnvironment env;
    if (env.checkAndClearExceptions() || content.isValid() == false) {
        return {};
    }
    const int width = content.callMethod<jint>("getWidth");
    const int height = content.callMethod<jint>("getHeight");
    if (env.checkAndClearExceptions() || width <= 0 || height <= 0) {
        return {};
    }
    return {width, height};
#else
    return {};
#endif
}

//================================================================================
QMargins VisibleArea::hiddenMargins(const QSize& widgetSize, const QSize& visiblePixels, qreal devicePixelRatio) {
    if (visiblePixels.isValid() == false || devicePixelRatio <= 0) {
        return {};
    }
    const int visibleWidth = static_cast<int>(std::floor(visiblePixels.width() / devicePixelRatio));
    const int visibleHeight = static_cast<int>(std::floor(visiblePixels.height() / devicePixelRatio));
    const int right = std::max(0, widgetSize.width() - visibleWidth);
    const int bottom = std::max(0, widgetSize.height() - visibleHeight);
    return {0, 0, right, bottom};
}
