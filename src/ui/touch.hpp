#pragma once
#include <algorithm>
#include <cstdlib>

// Touch handling that does not depend on hardware: turning raw panel readings into
// layout coordinates, and a press-move-release sequence into a gesture.

struct RawPoint {
    int x = 0, y = 0;
};

// How the touch panel's raw axes map onto the display. Resistive panels such as the
// XPT2046 report arbitrary ranges, often rotated relative to the picture.
struct TouchCalibration {
    bool swapXY = false;      // the panel's X axis runs along the display's height
    int minX = 0, maxX = 4095;   // raw value at the display's left and right edge
    int minY = 0, maxY = 4095;   // raw value at the display's top and bottom edge
    
    // Raw reading -> position in a width x height area. minX > maxX expresses a
    // mirrored axis.
    void map(RawPoint raw, int width, int height, int& x, int& y) const {
        int rx = swapXY ? raw.y : raw.x;
        int ry = swapXY ? raw.x : raw.y;
        x = scale(rx, minX, maxX, width);
        y = scale(ry, minY, maxY, height);
    }
    
    // Work out the calibration from three taps on targets placed at (inset, inset),
    // (1-inset, inset) and (inset, 1-inset) of the display, as fractions of its size.
    static TouchCalibration solve(RawPoint topLeft, RawPoint topRight, RawPoint bottomLeft,
                                  float inset) {
        TouchCalibration c;
        // Moving right along the top edge changes the raw axis that follows display X
        c.swapXY = std::abs(topRight.y - topLeft.y) > std::abs(topRight.x - topLeft.x);
        int tlX = c.swapXY ? topLeft.y : topLeft.x, tlY = c.swapXY ? topLeft.x : topLeft.y;
        int trX = c.swapXY ? topRight.y : topRight.x;
        int blY = c.swapXY ? bottomLeft.x : bottomLeft.y;
        
        // The targets span (1 - 2*inset) of each axis; extend to the edges
        float span = 1.0f - 2.0f * inset;
        float perUnitX = (trX - tlX) / span, perUnitY = (blY - tlY) / span;
        c.minX = static_cast<int>(tlX - perUnitX * inset);
        c.maxX = static_cast<int>(tlX + perUnitX * (1.0f - inset));
        c.minY = static_cast<int>(tlY - perUnitY * inset);
        c.maxY = static_cast<int>(tlY + perUnitY * (1.0f - inset));
        return c;
    }
    
private:
    static int scale(int raw, int lo, int hi, int size) {
        if (lo == hi) return 0;
        long v = static_cast<long>(raw - lo) * size / (hi - lo);
        return static_cast<int>(std::max(0L, std::min<long>(size - 1, v)));
    }
};

struct Gesture {
    enum class Kind { None, Tap, SwipeLeft, SwipeRight };
    Kind kind = Kind::None;
    int x = 0, y = 0;   // where a tap landed
};

// Classifies one press-to-release sequence. Positions are layout units.
class GestureRecognizer {
public:
    static constexpr int SWIPE_DISTANCE = 40;   // horizontal travel that makes a swipe
    static constexpr int TAP_SLOP = 12;         // travel still counted as a tap
    
    void press(int x, int y) {
        down_ = true;
        startX_ = lastX_ = x;
        startY_ = lastY_ = y;
    }
    
    void move(int x, int y) {
        if (!down_) return;
        lastX_ = x;
        lastY_ = y;
    }
    
    Gesture release() {
        Gesture g;
        if (!down_) return g;
        down_ = false;
        
        int dx = lastX_ - startX_, dy = lastY_ - startY_;
        if (std::abs(dx) >= SWIPE_DISTANCE && std::abs(dx) > 2 * std::abs(dy)) {
            g.kind = dx < 0 ? Gesture::Kind::SwipeLeft : Gesture::Kind::SwipeRight;
        } else if (std::abs(dx) <= TAP_SLOP && std::abs(dy) <= TAP_SLOP) {
            g.kind = Gesture::Kind::Tap;
            g.x = startX_;
            g.y = startY_;
        }
        return g;
    }
    
    bool pressed() const { return down_; }
    
private:
    bool down_ = false;
    int startX_ = 0, startY_ = 0, lastX_ = 0, lastY_ = 0;
};
