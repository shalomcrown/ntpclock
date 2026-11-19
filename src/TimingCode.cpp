#include "TimingCode.h"

//================================================================================
int timingValueFromUs(qint64 displayUs) {
    qint64 wrapped = (displayUs / 1000) % kTimingModulus;
    if (wrapped < 0) {
        wrapped += kTimingModulus;
    }
    return static_cast<int>(wrapped);
}

//================================================================================
int latencyMs(int clockValue, int otherValue) {
    int delta = clockValue - otherValue;
    delta %= kTimingModulus;
    if (delta < 0) {
        delta += kTimingModulus;
    }
    if (delta > kTimingModulus / 2) {
        delta -= kTimingModulus;
    }
    return delta;
}
