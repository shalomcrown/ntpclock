#include "DigitRead.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

constexpr int kGridW = 24;
constexpr int kGridH = 40;
constexpr int kInk = 150;

//================================================================================
struct Ink {
    std::vector<unsigned char> cells = std::vector<unsigned char>(kGridW * kGridH, 0);

//================================================================================
    bool on(int x, int y) const {
        return cells[static_cast<size_t>(y * kGridW + x)] != 0;
    }
};

//================================================================================
int pickThreshold(const QImage& glyph) {
    int hist[256] = {};
    const int area = std::max(1, glyph.width() * glyph.height());
    for (int y = 0; y < glyph.height(); ++y) {
        const uchar* row = glyph.constScanLine(y);
        for (int x = 0; x < glyph.width(); ++x) {
            ++hist[row[x]];
        }
    }
    int seen = 0;
    for (int level = 255; level >= 0; --level) {
        seen += hist[level];
        if (seen >= area / 5 || level <= 90) {
            return std::max(90, level);
        }
    }
    return 90;
}

//================================================================================
Ink pool(const QImage& glyph, int threshold) {
    Ink ink;
    if (glyph.width() < 1 || glyph.height() < 1) {
        return ink;
    }
    for (int y = 0; y < kGridH; ++y) {
        const int y0 = y * glyph.height() / kGridH;
        const int y1 = std::max(y0 + 1, (y + 1) * glyph.height() / kGridH);
        for (int x = 0; x < kGridW; ++x) {
            const int x0 = x * glyph.width() / kGridW;
            const int x1 = std::max(x0 + 1, (x + 1) * glyph.width() / kGridW);
            bool hit = false;
            for (int sy = y0; sy < y1 && sy < glyph.height() && hit == false; ++sy) {
                const uchar* row = glyph.constScanLine(sy);
                for (int sx = x0; sx < x1 && sx < glyph.width(); ++sx) {
                    if (row[sx] >= threshold) {
                        hit = true;
                        break;
                    }
                }
            }
            ink.cells[static_cast<size_t>(y * kGridW + x)] = hit ? 1 : 0;
        }
    }
    return ink;
}

//================================================================================
double spanAt(const Ink& ink, double frac) {
    const int y = std::clamp(static_cast<int>(std::lround((kGridH - 1) * frac)), 0, kGridH - 1);
    double samples[3];
    for (int offset = -1; offset <= 1; ++offset) {
        const int row = std::clamp(y + offset, 0, kGridH - 1);
        int first = -1;
        int last = -1;
        for (int x = 0; x < kGridW; ++x) {
            if (ink.on(x, row)) {
                if (first < 0) {
                    first = x;
                }
                last = x;
            }
        }
        samples[offset + 1] = first < 0 ? 0.0 : static_cast<double>(last - first + 1) / kGridW;
    }
    std::sort(samples, samples + 3);
    return samples[1];
}

//================================================================================
void sideInk(const Ink& ink, double y0f, double y1f, double* left, double* right) {
    const int y0 = static_cast<int>(kGridH * y0f);
    const int y1 = std::max(y0 + 1, static_cast<int>(kGridH * y1f));
    int leftCount = 0;
    int rightCount = 0;
    int area = 0;
    for (int y = y0; y < y1 && y < kGridH; ++y) {
        for (int x = 0; x < kGridW; ++x) {
            ++area;
            if (ink.on(x, y) == false) {
                continue;
            }
            if (x < kGridW * 0.40) {
                ++leftCount;
            } else if (x > kGridW * 0.60) {
                ++rightCount;
            }
        }
    }
    const double denom = std::max(1, area);
    *left = leftCount / denom;
    *right = rightCount / denom;
}

//================================================================================
std::vector<double> holeCenters(const Ink& ink) {
    std::vector<unsigned char> seen(static_cast<size_t>(kGridW * kGridH), 0);
    auto mark = [&](int x, int y) { seen[static_cast<size_t>(y * kGridW + x)] = 1; };
    auto seenAt = [&](int x, int y) { return seen[static_cast<size_t>(y * kGridW + x)] != 0; };
    std::vector<std::pair<int, int>> queue;
    for (int x = 0; x < kGridW; ++x) {
        for (int y : {0, kGridH - 1}) {
            if (ink.on(x, y) == false && seenAt(x, y) == false) {
                mark(x, y);
                queue.emplace_back(x, y);
            }
        }
    }
    for (int y = 0; y < kGridH; ++y) {
        for (int x : {0, kGridW - 1}) {
            if (ink.on(x, y) == false && seenAt(x, y) == false) {
                mark(x, y);
                queue.emplace_back(x, y);
            }
        }
    }
    for (size_t index = 0; index < queue.size(); ++index) {
        const int x = queue[index].first;
        const int y = queue[index].second;
        const int next[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (const auto& step : next) {
            const int nx = x + step[0];
            const int ny = y + step[1];
            if (nx < 0 || ny < 0 || nx >= kGridW || ny >= kGridH) {
                continue;
            }
            if (ink.on(nx, ny) || seenAt(nx, ny)) {
                continue;
            }
            mark(nx, ny);
            queue.emplace_back(nx, ny);
        }
    }
    std::vector<double> centers;
    for (int y = 0; y < kGridH; ++y) {
        for (int x = 0; x < kGridW; ++x) {
            if (ink.on(x, y) || seenAt(x, y)) {
                continue;
            }
            std::vector<std::pair<int, int>> hole;
            mark(x, y);
            hole.emplace_back(x, y);
            for (size_t index = 0; index < hole.size(); ++index) {
                const int hx = hole[index].first;
                const int hy = hole[index].second;
                const int next[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                for (const auto& step : next) {
                    const int nx = hx + step[0];
                    const int ny = hy + step[1];
                    if (nx < 0 || ny < 0 || nx >= kGridW || ny >= kGridH) {
                        continue;
                    }
                    if (ink.on(nx, ny) || seenAt(nx, ny)) {
                        continue;
                    }
                    mark(nx, ny);
                    hole.emplace_back(nx, ny);
                }
            }
            if (static_cast<int>(hole.size()) < 28) {
                continue;
            }
            double sum = 0;
            for (const auto& cell : hole) {
                sum += cell.second;
            }
            centers.push_back(sum / static_cast<double>(hole.size()) / kGridH);
        }
    }
    return centers;
}

//================================================================================
int classify(const Ink& ink, double aspect) {
    const double top = spanAt(ink, 0.08);
    const double up = spanAt(ink, 0.30);
    const double mid = spanAt(ink, 0.50);
    const double low = spanAt(ink, 0.70);
    const double bot = spanAt(ink, 0.92);
    const std::vector<double> holes = holeCenters(ink);
    double upLeft = 0;
    double upRight = 0;
    double midLeft = 0;
    double midRight = 0;
    double botLeft = 0;
    double botRight = 0;
    sideInk(ink, 0.10, 0.42, &upLeft, &upRight);
    sideInk(ink, 0.38, 0.68, &midLeft, &midRight);
    sideInk(ink, 0.72, 0.96, &botLeft, &botRight);

    if (holes.size() >= 2) {
        int upper = 0;
        int lower = 0;
        int middle = 0;
        for (double center : holes) {
            if (center < 0.45) {
                ++upper;
            } else if (center > 0.55) {
                ++lower;
            } else {
                ++middle;
            }
        }
        if (upper > 0 && lower > 0 && middle == 0) {
            return 8;
        }
        if (upper > 0 && lower == 0 && middle == 0) {
            return 9;
        }
        if (lower > 0 && upper == 0 && middle == 0) {
            return 6;
        }
        return -1;
    }
    if (holes.size() == 1) {
        const double center = holes[0];
        if (center >= 0.58) {
            return 6;
        }
        if (center <= 0.40) {
            return 9;
        }
        if (bot <= 0.45 && (low >= 0.70 || mid >= 0.65)) {
            return 4;
        }
        if (bot >= 0.55 && mid >= 0.75) {
            return 0;
        }
        return -1;
    }

    const bool stem = mid <= 0.42 && low <= 0.42;
    const bool rightUpper = upRight >= upLeft + 0.05;
    const bool leftUpper = upLeft >= upRight + 0.05;
    const bool rightMid = midRight >= midLeft + 0.06;
    const double widest = std::max({top, up, mid, low, bot});
    if (top >= 0.72 && up <= 0.50 && mid <= 0.50 && low <= 0.50 && bot <= 0.48) {
        return 7;
    }
    if (aspect < 0.48 && widest <= 0.65 && top < 0.80) {
        return 1;
    }
    if (leftUpper && top >= 0.70 && bot >= 0.65 && (rightMid || mid >= 0.70)) {
        return 5;
    }
    if (leftUpper && rightMid && bot >= 0.65 && top >= 0.55 &&
        (mid <= 0.35 && low <= 0.35 && up <= 0.40 && top < 0.70) == false) {
        return 5;
    }
    if (stem && rightUpper == false && (rightMid && leftUpper) == false &&
        ((top <= 0.70 && bot >= 0.68 && up <= 0.70) || widest <= 0.55)) {
        return 1;
    }
    if (bot >= 0.70 && mid <= 0.36 && low <= 0.55 && top >= 0.58 && (rightUpper || rightMid) &&
        botLeft + 0.02 >= botRight) {
        return 2;
    }
    if (top >= 0.62 && bot >= 0.62 && (rightUpper || rightMid) && botRight + 0.04 >= botLeft &&
        (mid >= 0.34 || low >= 0.55)) {
        return 3;
    }
    return -1;
}

//================================================================================
QRect clip(const QImage& image, QRect area) {
    return area.intersected(QRect(0, 0, image.width(), image.height()));
}

//================================================================================
// One connected bright mark. Its size is whatever the picture gave it.
struct Blob {
    QRect rect;
    int area = 0;
};

//================================================================================
int centerX(const QRect& rect) {
    return rect.left() + rect.width() / 2;
}

//================================================================================
// Ink that touches is one mark. A gap of any width splits marks, so a digit
// that is 20 pixels tall and one that is 200 are found the same way.
std::vector<Blob> blobsIn(const QImage& gray, const QRect& area) {
    std::vector<Blob> blobs;
    const QRect bounds = clip(gray, area);
    if (bounds.width() < 2 || bounds.height() < 2) {
        return blobs;
    }
    const int width = bounds.width();
    const int height = bounds.height();
    std::vector<char> seen(static_cast<size_t>(width * height), 0);
    std::vector<std::pair<int, int>> queue;
    for (int y = 0; y < height; ++y) {
        const uchar* row = gray.constScanLine(bounds.top() + y);
        for (int x = 0; x < width; ++x) {
            const size_t start = static_cast<size_t>(y * width + x);
            if (seen[start] != 0 || row[bounds.left() + x] < kInk) {
                continue;
            }
            seen[start] = 1;
            queue.clear();
            queue.emplace_back(x, y);
            int minX = x;
            int maxX = x;
            int minY = y;
            int maxY = y;
            int areaCount = 0;
            for (size_t index = 0; index < queue.size(); ++index) {
                const int cx = queue[index].first;
                const int cy = queue[index].second;
                ++areaCount;
                minX = std::min(minX, cx);
                maxX = std::max(maxX, cx);
                minY = std::min(minY, cy);
                maxY = std::max(maxY, cy);
                const int next[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                for (const auto& step : next) {
                    const int nx = cx + step[0];
                    const int ny = cy + step[1];
                    if (nx < 0 || ny < 0 || nx >= width || ny >= height) {
                        continue;
                    }
                    const size_t at = static_cast<size_t>(ny * width + nx);
                    if (seen[at] != 0) {
                        continue;
                    }
                    if (gray.constScanLine(bounds.top() + ny)[bounds.left() + nx] < kInk) {
                        continue;
                    }
                    seen[at] = 1;
                    queue.emplace_back(nx, ny);
                }
            }
            Blob blob;
            blob.rect = QRect(bounds.left() + minX, bounds.top() + minY, maxX - minX + 1, maxY - minY + 1);
            blob.area = areaCount;
            blobs.push_back(blob);
        }
    }
    std::sort(blobs.begin(), blobs.end(), [](const Blob& left, const Blob& right) {
        return left.rect.left() < right.rect.left();
    });
    return blobs;
}

//================================================================================
// Marks belong to one line when their vertical spans overlap. The overlap is
// a fraction of the shorter mark, not a fixed number of pixels.
bool sameLine(const QRect& line, const QRect& mark) {
    const int top = std::max(line.top(), mark.top());
    const int bottom = std::min(line.bottom(), mark.bottom());
    if (bottom < top) {
        return false;
    }
    const int overlap = bottom - top + 1;
    const int shorter = std::max(1, std::min(line.height(), mark.height()));
    return overlap * 2 >= shorter;
}

//================================================================================
// A haze that touches across the picture has a huge box and little ink. A
// stroke fills a real fraction of its own box. The cut is that fraction, not a
// pixel size.
void dropSparse(std::vector<Blob>* blobs) {
    blobs->erase(std::remove_if(blobs->begin(), blobs->end(), [](const Blob& blob) {
        return blob.area * 5 < blob.rect.width() * std::max(1, blob.rect.height());
    }), blobs->end());
}

//================================================================================
bool shortInGap(const std::vector<Blob>& shorts, const QRect& left, const QRect& right) {
    for (const Blob& mark : shorts) {
        const int x = centerX(mark.rect);
        if (x > left.right() && x < right.left()) {
            return true;
        }
    }
    return false;
}

//================================================================================
// Six tall marks and a short mark in each colon gap. Heights are compared with
// the tallest mark on the line, so the line can be any size.
bool parseTimeLine(const QImage& gray, const std::vector<Blob>& line, PaintedClock* clock) {
    int maxHeight = 0;
    for (const Blob& mark : line) {
        maxHeight = std::max(maxHeight, mark.rect.height());
    }
    if (maxHeight < 2) {
        return false;
    }
    std::vector<Blob> digits;
    std::vector<Blob> shorts;
    for (const Blob& mark : line) {
        const double aspect = static_cast<double>(mark.rect.width()) / std::max(1, mark.rect.height());
        if (mark.rect.height() * 10 >= maxHeight * 7 && aspect >= 0.18 && aspect <= 1.05) {
            digits.push_back(mark);
        } else if (mark.rect.height() * 2 <= maxHeight && aspect <= 1.4) {
            shorts.push_back(mark);
        }
    }
    if (digits.size() != 6) {
        return false;
    }
    std::sort(digits.begin(), digits.end(), [](const Blob& left, const Blob& right) {
        return left.rect.left() < right.rect.left();
    });
    std::vector<int> areas;
    for (const Blob& digit : digits) {
        areas.push_back(digit.area);
    }
    std::sort(areas.begin(), areas.end());
    const int speck = std::max(1, areas[areas.size() / 2] / 50);
    std::vector<Blob> kept;
    for (const Blob& mark : shorts) {
        if (mark.area >= speck) {
            kept.push_back(mark);
        }
    }
    // Colons sit between the pairs. A short mark inside a pair is not a time.
    if (shortInGap(kept, digits[1].rect, digits[2].rect) == false ||
        shortInGap(kept, digits[3].rect, digits[4].rect) == false) {
        return false;
    }
    if (shortInGap(kept, digits[0].rect, digits[1].rect) || shortInGap(kept, digits[2].rect, digits[3].rect) ||
        shortInGap(kept, digits[4].rect, digits[5].rect)) {
        return false;
    }
    int values[6];
    for (int index = 0; index < 6; ++index) {
        values[index] = readDigit(gray.copy(digits[index].rect));
        if (values[index] < 0) {
            return false;
        }
    }
    const int hour = values[0] * 10 + values[1];
    const int minute = values[2] * 10 + values[3];
    const int second = values[4] * 10 + values[5];
    if (hour > 23 || minute > 59 || second > 59) {
        return false;
    }
    clock->timeOk = true;
    clock->hour = hour;
    clock->minute = minute;
    clock->second = second;
    return true;
}

//================================================================================
bool readTime(const QImage& gray, const QRect& block, PaintedClock* clock) {
    // The hours line can sit anywhere above the block. A band two blocks tall
    // misses it on a tall screen, and then a second rollover cannot be checked.
    const QRect above(0, 0, gray.width(), block.top());
    const QRect search = clip(gray, above);
    if (search.height() < 2) {
        return false;
    }
    QRect limited = search;
    if (limited.bottom() >= block.top()) {
        limited.setBottom(block.top() - 1);
    }
    std::vector<Blob> blobs = blobsIn(gray, limited);
    dropSparse(&blobs);
    std::sort(blobs.begin(), blobs.end(), [](const Blob& left, const Blob& right) {
        return left.rect.top() < right.rect.top();
    });
    std::vector<std::vector<Blob>> lines;
    for (const Blob& mark : blobs) {
        bool placed = false;
        for (std::vector<Blob>& line : lines) {
            QRect span = line[0].rect;
            for (const Blob& other : line) {
                span = span.united(other.rect);
            }
            if (sameLine(span, mark.rect)) {
                line.push_back(mark);
                placed = true;
                break;
            }
        }
        if (placed == false) {
            lines.push_back({mark});
        }
    }
    int bestTop = -1;
    PaintedClock best;
    for (const std::vector<Blob>& line : lines) {
        PaintedClock parsed;
        if (parseTimeLine(gray, line, &parsed) == false) {
            continue;
        }
        const int top = line.front().rect.top();
        if (top > bestTop) {
            bestTop = top;
            best = parsed;
        }
    }
    if (best.timeOk == false) {
        return false;
    }
    clock->timeOk = true;
    clock->hour = best.hour;
    clock->minute = best.minute;
    clock->second = best.second;
    return true;
}

//================================================================================
bool readMillis(const QImage& gray, const QRect& block, PaintedClock* clock) {
    const int blockRight = block.left() + block.width();
    const int inset = std::max(1, std::min(block.width(), block.height()) / 40);
    const QRect sides[2] = {
            clip(gray, QRect(blockRight + inset, block.top(), block.width() * 3, block.height())),
            clip(gray, QRect(block.left() - block.width() * 3, block.top(), block.width() * 3 - inset, block.height())),
    };
    for (const QRect& side : sides) {
        std::vector<Blob> blobs = blobsIn(gray, side);
        dropSparse(&blobs);
        int maxHeight = 0;
        for (const Blob& mark : blobs) {
            maxHeight = std::max(maxHeight, mark.rect.height());
        }
        if (maxHeight < 2) {
            continue;
        }
        std::vector<Blob> digits;
        for (const Blob& mark : blobs) {
            const double aspect = static_cast<double>(mark.rect.width()) / std::max(1, mark.rect.height());
            if (mark.rect.height() * 10 < maxHeight * 7 || aspect < 0.18 || aspect > 1.05) {
                continue;
            }
            digits.push_back(mark);
        }
        if (digits.size() < 3) {
            continue;
        }
        std::sort(digits.begin(), digits.end(), [&](const Blob& left, const Blob& right) {
            const int leftDistance = std::min(std::abs(left.rect.left() - blockRight), std::abs(left.rect.right() - block.left()));
            const int rightDistance = std::min(std::abs(right.rect.left() - blockRight), std::abs(right.rect.right() - block.left()));
            return leftDistance < rightDistance;
        });
        std::vector<Blob> chosen;
        for (const Blob& mark : digits) {
            if (chosen.size() == 3) {
                break;
            }
            if (chosen.empty() == false) {
                const int low = std::min(chosen[0].rect.height(), mark.rect.height());
                const int high = std::max(chosen[0].rect.height(), mark.rect.height());
                if (low * 5 < high * 4) {
                    continue;
                }
            }
            chosen.push_back(mark);
        }
        if (chosen.size() != 3) {
            continue;
        }
        std::sort(chosen.begin(), chosen.end(), [](const Blob& left, const Blob& right) {
            return left.rect.left() < right.rect.left();
        });
        int values[3];
        bool ok = true;
        for (int index = 0; index < 3; ++index) {
            values[index] = readDigit(gray.copy(chosen[index].rect));
            if (values[index] < 0) {
                ok = false;
            }
        }
        if (ok == false) {
            continue;
        }
        clock->millisOk = true;
        clock->millis = values[0] * 100 + values[1] * 10 + values[2];
        return true;
    }
    return false;
}

}  // namespace

//================================================================================
int readDigit(const QImage& glyph) {
    if (glyph.isNull() || glyph.width() < 2 || glyph.height() < 2) {
        return -1;
    }
    const QImage gray = glyph.format() == QImage::Format_Grayscale8 ? glyph : glyph.convertToFormat(QImage::Format_Grayscale8);
    const double aspect = static_cast<double>(gray.width()) / gray.height();
    return classify(pool(gray, pickThreshold(gray)), aspect);
}

//================================================================================
PaintedClock readPaintedClock(const QImage& image, const QRect& block) {
    PaintedClock clock;
    if (image.isNull() || block.width() < 5 || block.height() < 5) {
        return clock;
    }
    const QImage gray = image.format() == QImage::Format_Grayscale8 ? image : image.convertToFormat(QImage::Format_Grayscale8);
    readTime(gray, block, &clock);
    readMillis(gray, block, &clock);
    return clock;
}
