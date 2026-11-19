#pragma once

#include <QtGlobal>

//================================================================================
// The block encodes this many distinct instants. That is four seconds of
// milliseconds, long enough for a video-latency reading and short enough for
// a 3 by 4 Gray code.
inline constexpr int kTimingModulus = 4000;

//================================================================================
// Milliseconds within the current 4-second window of an absolute timestamp.
int timingValueFromUs(qint64 displayUs);

//================================================================================
// How far ahead clockValue is of otherValue, in milliseconds, wrapped into
// -2000..2000. Positive means the other screen is behind this clock.
int latencyMs(int clockValue, int otherValue);
