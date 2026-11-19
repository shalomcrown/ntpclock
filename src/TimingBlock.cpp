#include "TimingBlock.h"

#include <algorithm>

#include <QPainter>

namespace {
//================================================================================
int grayEncode(int value) {
    return value ^ (value >> 1);
}

//================================================================================
int grayDecode(int bits) {
    int value = bits;
    value ^= value >> 1;
    value ^= value >> 2;
    value ^= value >> 4;
    value ^= value >> 8;
    return value;
}

//================================================================================
void mapCell(int col, int row, int rotation, bool mirror, int* imageCol, int* imageRow) {
    if (mirror) {
        col = kTimingGridCols - 1 - col;
    }
    switch (rotation & 3) {
    case 0:
        *imageCol = col;
        *imageRow = row;
        break;
    case 1:
        *imageCol = kTimingGridRows - 1 - row;
        *imageRow = col;
        break;
    case 2:
        *imageCol = kTimingGridCols - 1 - col;
        *imageRow = kTimingGridRows - 1 - row;
        break;
    default:
        *imageCol = row;
        *imageRow = kTimingGridCols - 1 - col;
        break;
    }
}

//================================================================================
struct View {
    const uchar* bits = nullptr;
    int width = 0;
    int height = 0;
    int stride = 0;
    int threshold = 128;

//================================================================================
    int at(int x, int y) const {
        if (x < 0 || y < 0 || x >= width || y >= height) {
            return 0;
        }
        return bits[y * stride + x];
    }
};

//================================================================================
struct GridMap {
    int boxCols = kTimingGridCols;
    int boxRows = kTimingGridRows;
    int frameCol[18] = {};
    int frameRow[18] = {};
    bool frameWhite[18] = {};
    int frameCount = 0;
    int quick[4] = {};
    int dataCol[12] = {};
    int dataRow[12] = {};
};

//================================================================================
GridMap makeMap(int rotation, bool mirror) {
    GridMap map;
    map.boxCols = (rotation & 1) != 0 ? kTimingGridRows : kTimingGridCols;
    map.boxRows = (rotation & 1) != 0 ? kTimingGridCols : kTimingGridRows;
    for (int row = 0; row < kTimingGridRows; ++row) {
        for (int col = 0; col < kTimingGridCols; ++col) {
            const bool frame = col == 0 || row == 0 || col == kTimingGridCols - 1 || row == kTimingGridRows - 1;
            if (frame == false) {
                continue;
            }
            int imageCol = 0;
            int imageRow = 0;
            mapCell(col, row, rotation, mirror, &imageCol, &imageRow);
            map.frameCol[map.frameCount] = imageCol;
            map.frameRow[map.frameCount] = imageRow;
            map.frameWhite[map.frameCount] = (col == 0 && row == 0) == false;
            ++map.frameCount;
        }
    }
    int data = 0;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 3; ++col) {
            mapCell(col + 1, row + 1, rotation, mirror, &map.dataCol[data], &map.dataRow[data]);
            ++data;
        }
    }
    // The four frame cells nearest the top-left of this orientation. They stay
    // inside the right square when the cell size is only close.
    int order[18];
    for (int index = 0; index < map.frameCount; ++index) {
        order[index] = index;
    }
    std::sort(order, order + map.frameCount, [&](int left, int right) {
        const int leftKey = map.frameCol[left] + map.frameRow[left];
        const int rightKey = map.frameCol[right] + map.frameRow[right];
        return leftKey < rightKey;
    });
    for (int probe = 0; probe < 4; ++probe) {
        map.quick[probe] = order[probe];
    }
    return map;
}

//================================================================================
int cellCenter(int origin, int index, int cell) {
    return origin + index * cell + cell / 2;
}

//================================================================================
bool quickProbe(const View& view, const GridMap& map, int originX, int originY, int cell) {
    for (int probe = 0; probe < 4; ++probe) {
        const int index = map.quick[probe];
        const int lum = view.at(cellCenter(originX, map.frameCol[index], cell),
                                cellCenter(originY, map.frameRow[index], cell));
        const bool white = lum > view.threshold;
        if (white != map.frameWhite[index]) {
            return false;
        }
    }
    return true;
}

//================================================================================
struct Match {
    int value = -1;
    int contrast = 0;
    int edges = 0;
    QRect bounds;
};

//================================================================================
std::optional<Match> tryMatch(const View& view, const GridMap& map, int originX, int originY, int cell) {
    if (cell < 3) {
        return std::nullopt;
    }
    if (originX < 0 || originY < 0) {
        return std::nullopt;
    }
    if (originX + map.boxCols * cell > view.width || originY + map.boxRows * cell > view.height) {
        return std::nullopt;
    }
    int whiteSum = 0;
    int whiteCount = 0;
    int blackSum = 0;
    for (int index = 0; index < map.frameCount; ++index) {
        const int lum = view.at(cellCenter(originX, map.frameCol[index], cell),
                                cellCenter(originY, map.frameRow[index], cell));
        const bool white = lum > view.threshold;
        if (white != map.frameWhite[index]) {
            return std::nullopt;
        }
        if (map.frameWhite[index]) {
            whiteSum += lum;
            ++whiteCount;
        } else {
            blackSum += lum;
        }
    }
    const int contrast = whiteCount == 0 ? 0 : whiteSum / whiteCount - blackSum;
    if (contrast < 40) {
        return std::nullopt;
    }
    // Sample solid frame, not the outer pixel a photo blurs. The pixel just
    // outside the box has to be much darker. A box shifted inward compares two
    // white pixels here and is rejected.
    const int into = std::max(1, std::min(3, cell / 4));
    const int midX = originX + map.boxCols * cell / 2;
    const int midY = originY + map.boxRows * cell / 2;
    const int lastX = originX + map.boxCols * cell - 1;
    const int lastY = originY + map.boxRows * cell - 1;
    const int edge[4][4] = {
            {midX, originY + into, midX, originY - 1},
            {midX, lastY - into, midX, lastY + 1},
            {originX + into, midY, originX - 1, midY},
            {lastX - into, midY, lastX + 1, midY},
    };
    int edgeScore = 0;
    for (const auto& side : edge) {
        if (view.at(side[0], side[1]) - view.at(side[2], side[3]) >= 80) {
            ++edgeScore;
        }
    }

    int bits = 0;
    int ambiguous = 0;
    const int band = std::max(8, contrast / 6);
    for (int index = 0; index < 12; ++index) {
        const int lum = view.at(cellCenter(originX, map.dataCol[index], cell),
                                cellCenter(originY, map.dataRow[index], cell));
        const int bit = 11 - index;
        if (lum <= view.threshold) {
            continue;
        }
        if (lum < view.threshold + band) {
            ambiguous |= 1 << bit;
            continue;
        }
        bits |= 1 << bit;
    }
    int value = -1;
    if (ambiguous == 0) {
        value = grayDecode(bits);
    } else if ((ambiguous & (ambiguous - 1)) == 0) {
        const int low = grayDecode(bits);
        const int high = grayDecode(bits ^ ambiguous);
        if (std::abs(low - high) == 1) {
            value = std::min(low, high);
        }
    }
    if (value < 0 || value >= kTimingModulus) {
        return std::nullopt;
    }
    Match match;
    match.value = value;
    match.contrast = contrast;
    match.edges = edgeScore;
    match.bounds = QRect(originX, originY, map.boxCols * cell, map.boxRows * cell);
    return match;
}

//================================================================================
int otsu(const View& view) {
    int hist[256] = {};
    for (int y = 0; y < view.height; ++y) {
        const uchar* row = view.bits + y * view.stride;
        for (int x = 0; x < view.width; ++x) {
            ++hist[row[x]];
        }
    }
    const int total = view.width * view.height;
    double sum = 0;
    for (int level = 0; level < 256; ++level) {
        sum += static_cast<double>(level) * hist[level];
    }
    double backgroundSum = 0;
    int background = 0;
    double best = -1;
    int threshold = 128;
    for (int level = 0; level < 256; ++level) {
        background += hist[level];
        if (background == 0) {
            continue;
        }
        const int foreground = total - background;
        if (foreground == 0) {
            break;
        }
        backgroundSum += static_cast<double>(level) * hist[level];
        const double backgroundMean = backgroundSum / background;
        const double foregroundMean = (sum - backgroundSum) / foreground;
        const double variance = background * static_cast<double>(foreground) * (backgroundMean - foregroundMean) *
                                (backgroundMean - foregroundMean);
        if (variance > best) {
            best = variance;
            threshold = level;
        }
    }
    return best <= 0 ? -1 : threshold;
}

//================================================================================
std::optional<Match> searchView(const View& view) {
    if (view.threshold < 0 || view.width < kTimingGridCols * 3 || view.height < kTimingGridRows * 3) {
        return std::nullopt;
    }
    GridMap maps[8];
    for (int rotation = 0; rotation < 4; ++rotation) {
        maps[rotation] = makeMap(rotation, false);
        maps[4 + rotation] = makeMap(rotation, true);
    }
    std::optional<Match> best;
    bool ambiguous = false;
    const auto strong = [](const Match& match) { return match.edges == 4 && match.contrast >= 200; };
    // Four sharp sides identify a clean rendering and outrank anything found in
    // the digits. A photograph has a softer edge, so the largest frame wins.
    auto consider = [&](const std::optional<Match>& match) {
        if (match.has_value() == false) {
            return;
        }
        const bool matchStrong = strong(*match);
        const bool bestStrong = best.has_value() && strong(*best);
        if (matchStrong == false && bestStrong) {
            return;
        }
        const int area = match->bounds.width() * match->bounds.height();
        const int bestArea = best.has_value() ? best->bounds.width() * best->bounds.height() : 0;
        if (best.has_value() == false || (matchStrong && bestStrong == false) ||
            (matchStrong == bestStrong && area > bestArea)) {
            best = match;
            ambiguous = false;
            return;
        }
        if (matchStrong != bestStrong || area * 6 / 5 < bestArea) {
            return;
        }
        if (match->value != best->value && match->contrast + 15 >= best->contrast) {
            ambiguous = true;
        }
    };
    const auto foundEnough = [&]() {
        return best.has_value() && ambiguous == false && strong(*best);
    };
    for (const GridMap& map : maps) {
        if (view.width % map.boxCols == 0 && view.height == map.boxRows * (view.width / map.boxCols)) {
            consider(tryMatch(view, map, 0, 0, view.width / map.boxCols));
        }
    }
    if (foundEnough()) {
        return best;
    }
    // The frame's outer edge is a sharp step. Follow those edges instead of
    // trying every pixel, then test the few boxes that fit each edge.
    const auto propose = [&](int originX, int originY, int cell) {
        if (cell < 3 || originX < 0 || originY < 0 || foundEnough()) {
            return;
        }
        for (const GridMap& map : maps) {
            if (originX + map.boxCols * cell > view.width || originY + map.boxRows * cell > view.height) {
                continue;
            }
            if (quickProbe(view, map, originX, originY, cell) == false) {
                continue;
            }
            consider(tryMatch(view, map, originX, originY, cell));
            if (foundEnough()) {
                return;
            }
        }
    };
    const auto proposeRun = [&](int start, int across, int run, bool horizontal, bool boxStartsHere) {
        if (run < 16) {
            return;
        }
        for (const int parts : {4, 5}) {
            const int cell = run / parts;
            for (const int near : {cell - 1, cell, cell + 1}) {
                if (near < 3) {
                    continue;
                }
                for (const int notch : {0, near}) {
                    const int along = start - notch;
                    for (const int slide : {0, near / 2}) {
                        if (horizontal) {
                            const int originY = boxStartsHere ? across : across - kTimingGridRows * near;
                            propose(along - slide, originY, near);
                            propose(along - slide, originY - slide, near);
                            propose(along, boxStartsHere ? across : across - kTimingGridCols * near, near);
                        } else {
                            const int originX = boxStartsHere ? across : across - kTimingGridCols * near;
                            propose(originX, along - slide, near);
                            propose(originX - slide, along - slide, near);
                            propose(boxStartsHere ? across : across - kTimingGridRows * near, along, near);
                        }
                        if (foundEnough()) {
                            return;
                        }
                    }
                }
            }
        }
    };
    // A photographed edge is sharp for only part of its length. The white frame
    // itself is a long run, and that length gives the cell size.
    for (int y = 1; y < view.height && foundEnough() == false; ++y) {
        int x = 0;
        while (x < view.width && foundEnough() == false) {
            if (view.at(x, y) < 140 || view.at(x, y - 1) >= 140) {
                ++x;
                continue;
            }
            const int x0 = x;
            while (x < view.width && view.at(x, y) >= 140 && view.at(x, y - 1) < 140) {
                ++x;
            }
            proposeRun(x0, y, x - x0, true, true);
        }
    }
    for (int x = 1; x < view.width && foundEnough() == false; ++x) {
        int y = 0;
        while (y < view.height && foundEnough() == false) {
            if (view.at(x, y) < 140 || view.at(x - 1, y) >= 140) {
                ++y;
                continue;
            }
            const int y0 = y;
            while (y < view.height && view.at(x, y) >= 140 && view.at(x - 1, y) < 140) {
                ++y;
            }
            proposeRun(y0, x, y - y0, false, true);
        }
    }
    for (int y = 1; y < view.height && foundEnough() == false; ++y) {
        int x = 0;
        while (x < view.width) {
            const int rise = view.at(x, y) - view.at(x, y - 1);
            const int fall = -rise;
            if (rise < 80 && fall < 80) {
                ++x;
                continue;
            }
            const bool rising = rise >= 80;
            const int x0 = x;
            while (x < view.width) {
                const int step = view.at(x, y) - view.at(x, y - 1);
                if ((rising && step < 80) || (rising == false && step > -80)) {
                    break;
                }
                ++x;
            }
            proposeRun(x0, y, x - x0, true, rising);
        }
    }
    for (int x = 1; x < view.width && foundEnough() == false; ++x) {
        int y = 0;
        while (y < view.height) {
            const int rise = view.at(x, y) - view.at(x - 1, y);
            if (rise < 80 && rise > -80) {
                ++y;
                continue;
            }
            const bool rising = rise >= 80;
            const int y0 = y;
            while (y < view.height) {
                const int step = view.at(x, y) - view.at(x - 1, y);
                if ((rising && step < 80) || (rising == false && step > -80)) {
                    break;
                }
                ++y;
            }
            proposeRun(y0, x, y - y0, false, rising);
        }
    }
    if (ambiguous) {
        return std::nullopt;
    }
    return best;
}

}  // namespace

//================================================================================
TimingPlacement placeTimingBlock(const QRect& band, int digitWidth, int digitHeight, const QRect& avoid) {
    TimingPlacement placed;
    placed.digits = band;
    if (band.width() < kTimingGridCols * 4 || band.height() < kTimingGridRows * 4) {
        return placed;
    }
    const int gap = std::max(4, std::min(band.width(), band.height()) / 40);
    digitWidth = std::clamp(digitWidth, 1, band.width());
    digitHeight = std::clamp(digitHeight, 1, band.height());
    const int sideSlack = band.width() - digitWidth;
    const int verticalSlack = band.height() - digitHeight;
    const bool avoidOnRight = avoid.isEmpty() == false && avoid.center().x() >= band.center().x();

    auto placeSide = [&](int cell, bool onRight, TimingPlacement* out) {
        if (cell < 4) {
            return false;
        }
        const int blockWidth = kTimingGridCols * cell;
        const int blockHeight = kTimingGridRows * cell;
        if (blockWidth + gap + digitWidth > band.width() || blockHeight > band.height()) {
            return false;
        }
        const int x = onRight ? band.right() - blockWidth + 1 : band.left();
        const int y = band.top() + (band.height() - blockHeight) / 2;
        const QRect block(x, y, blockWidth, blockHeight);
        if (avoid.isEmpty() == false && block.intersects(avoid)) {
            return false;
        }
        QRect digits;
        if (onRight) {
            digits = QRect(band.left(), band.top(), block.left() - gap - band.left(), band.height());
        } else {
            const int left = block.right() + 1 + gap;
            digits = QRect(left, band.top(), band.right() - left + 1, band.height());
        }
        if (digits.width() < digitWidth || digits.height() < digitHeight) {
            return false;
        }
        out->block = block;
        out->digits = digits;
        return true;
    };
    auto placeBelow = [&](int cell, bool onRight, TimingPlacement* out) {
        if (cell < 4) {
            return false;
        }
        const int blockWidth = kTimingGridCols * cell;
        const int blockHeight = kTimingGridRows * cell;
        if (blockHeight + gap + digitHeight > band.height() || blockWidth > band.width()) {
            return false;
        }
        const int y = band.top() + digitHeight + gap;
        if (y + blockHeight - 1 > band.bottom()) {
            return false;
        }
        const int x = onRight ? band.right() - blockWidth + 1 : band.left();
        const QRect block(x, y, blockWidth, blockHeight);
        if (avoid.isEmpty() == false && block.intersects(avoid)) {
            return false;
        }
        const QRect digits(band.left(), band.top(), band.width(), digitHeight);
        if (digits.intersects(block)) {
            return false;
        }
        out->block = block;
        out->digits = digits;
        return true;
    };

    const int sideCell = std::min(band.height() / kTimingGridRows, (sideSlack - gap) / kTimingGridCols);
    const int belowCell = std::min(band.width() / kTimingGridCols, (verticalSlack - gap) / kTimingGridRows);
    const bool preferSide = sideSlack >= verticalSlack;
    const bool firstRight = avoidOnRight == false;
    if (preferSide && (placeSide(sideCell, firstRight, &placed) || placeSide(sideCell, !firstRight, &placed))) {
        return placed;
    }
    if (placeBelow(belowCell, firstRight, &placed) || placeBelow(belowCell, !firstRight, &placed)) {
        return placed;
    }
    if (preferSide == false && (placeSide(sideCell, firstRight, &placed) || placeSide(sideCell, !firstRight, &placed))) {
        return placed;
    }
    return placed;
}

//================================================================================
void paintTimingBlock(QPainter& painter, const QRect& bounds, int value) {
    if (value < 0 || value >= kTimingModulus || bounds.width() < kTimingGridCols || bounds.height() < kTimingGridRows) {
        return;
    }
    const int cell = std::min(bounds.width() / kTimingGridCols, bounds.height() / kTimingGridRows);
    if (cell < 1) {
        return;
    }
    painter.fillRect(bounds, Qt::black);
    // The border is the same thickness as a time square. The gap between squares
    // stays wide enough that a slightly soft photo does not merge them.
    const int gap = cell >= 8 ? std::max(1, cell / 5) : 0;
    const int bar = cell - 2 * gap;
    const int gray = grayEncode(value);
    const int lastCol = kTimingGridCols - 1;
    const int lastRow = kTimingGridRows - 1;
    for (int row = 0; row < kTimingGridRows; ++row) {
        for (int col = 0; col < kTimingGridCols; ++col) {
            const bool frame = col == 0 || row == 0 || col == lastCol || row == lastRow;
            if (frame == false || (col == 0 && row == 0)) {
                continue;
            }
            const int left = bounds.left() + col * cell;
            const int top = bounds.top() + row * cell;
            if (row == 0) {
                painter.fillRect(QRect(left, top, cell, bar), Qt::white);
            } else if (row == lastRow) {
                painter.fillRect(QRect(left, top + cell - bar, cell, bar), Qt::white);
            }
            if (col == 0) {
                painter.fillRect(QRect(left, top, bar, cell), Qt::white);
            } else if (col == lastCol) {
                painter.fillRect(QRect(left + cell - bar, top, bar, cell), Qt::white);
            }
        }
    }
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 3; ++col) {
            const int bit = 11 - (row * 3 + col);
            if ((gray & (1 << bit)) == 0) {
                continue;
            }
            painter.fillRect(QRect(bounds.left() + (col + 1) * cell + gap, bounds.top() + (row + 1) * cell + gap, bar, bar),
                             Qt::white);
        }
    }
}

//================================================================================
QImage renderTimingBlock(int value, int cellPx) {
    QImage image(kTimingGridCols * std::max(1, cellPx), kTimingGridRows * std::max(1, cellPx), QImage::Format_RGB32);
    image.fill(Qt::black);
    QPainter painter(&image);
    paintTimingBlock(painter, image.rect(), value);
    return image;
}

//================================================================================
std::optional<TimingHit> findTimingBlock(const QImage& image) {
    if (image.isNull() || image.width() < kTimingGridCols * 3 || image.height() < kTimingGridRows * 3) {
        return std::nullopt;
    }
    const QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
    View view;
    view.bits = gray.constBits();
    view.width = gray.width();
    view.height = gray.height();
    view.stride = gray.bytesPerLine();
    view.threshold = otsu(view);
    const std::optional<Match> found = searchView(view);
    if (found.has_value() == false) {
        return std::nullopt;
    }
    TimingHit hit;
    hit.value = found->value;
    hit.bounds = found->bounds;
    return hit;
}

//================================================================================
LatencyReading measureLatency(const QImage& otherScreen, int clockValue) {
    LatencyReading reading;
    reading.clockValue = clockValue;
    if (clockValue < 0 || clockValue >= kTimingModulus) {
        return reading;
    }
    const std::optional<TimingHit> hit = findTimingBlock(otherScreen);
    if (hit.has_value() == false) {
        return reading;
    }
    reading.ok = true;
    reading.otherValue = hit->value;
    reading.milliseconds = latencyMs(clockValue, hit->value);
    return reading;
}

//================================================================================
QString latencyLabel(const LatencyReading& reading) {
    if (reading.ok == false) {
        return QStringLiteral("Could not read the timing block in the other picture.");
    }
    const int magnitude = std::abs(reading.milliseconds);
    const auto direction = reading.milliseconds >= 0 ? QStringLiteral("behind") : QStringLiteral("ahead");
    const int otherMs = reading.otherValue % 1000;
    const int clockMs = reading.clockValue % 1000;
    return QStringLiteral("%1 ms %2\n.%3 other · .%4 this")
            .arg(magnitude)
            .arg(direction)
            .arg(otherMs, 3, 10, QLatin1Char('0'))
            .arg(clockMs, 3, 10, QLatin1Char('0'));
}
