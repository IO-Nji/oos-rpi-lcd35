// Unit tests for the device-specific logic that needs no hardware.
#include "device.hpp"
#include <iostream>

static int failures = 0;
#define CHECK(cond) \
    do { if (!(cond)) { std::cerr << __FILE__ << ":" << __LINE__ << ": " #cond "\n"; failures++; } } while (0)

static void testCalibration() {
    // Panel mounted the same way round as the display
    TouchCalibration c;
    c.minX = 200; c.maxX = 3900; c.minY = 300; c.maxY = 3800;
    int x, y;
    c.map({200, 300}, 320, 213, x, y);
    CHECK(x == 0 && y == 0);
    c.map({3900, 3800}, 320, 213, x, y);
    CHECK(x == 319 && y == 212);
    c.map({2050, 2050}, 320, 213, x, y);
    CHECK(x == 160 && y == 106);
    c.map({0, 5000}, 320, 213, x, y);       // outside the calibrated range: clamped
    CHECK(x == 0 && y == 212);
    
    // Rotated a quarter turn with one axis mirrored, as these HATs usually are:
    // display X follows raw Y, display Y follows raw X backwards
    auto raw = [](float fx, float fy) {
        return RawPoint{static_cast<int>(3800 - fy * 3500), static_cast<int>(250 + fx * 3600)};
    };
    TouchCalibration s = TouchCalibration::solve(raw(0.1f, 0.1f), raw(0.9f, 0.1f), raw(0.1f, 0.9f), 0.1f);
    CHECK(s.swapXY);
    CHECK(std::abs(s.minX - 250) <= 2 && std::abs(s.maxX - 3850) <= 2);
    CHECK(std::abs(s.minY - 3800) <= 2 && std::abs(s.maxY - 300) <= 2);
    s.map(raw(0.5f, 0.5f), 320, 213, x, y);
    CHECK(std::abs(x - 160) <= 1 && std::abs(y - 106) <= 1);
    s.map(raw(0.95f, 0.05f), 320, 213, x, y);
    CHECK(x > 300 && y < 12);
    
    TouchCalibration plain = TouchCalibration::solve({500, 500}, {3500, 520}, {510, 3500}, 0.1f);
    CHECK(!plain.swapXY && plain.minX < plain.maxX && plain.minY < plain.maxY);
}

static void testGestures() {
    GestureRecognizer g;
    CHECK(g.release().kind == Gesture::Kind::None);
    
    g.press(100, 50);
    g.move(104, 52);
    Gesture tap = g.release();
    CHECK(tap.kind == Gesture::Kind::Tap && tap.x == 100 && tap.y == 50);
    
    g.press(250, 80);
    g.move(150, 90);
    CHECK(g.release().kind == Gesture::Kind::SwipeLeft);
    
    g.press(50, 80);
    g.move(120, 85);
    CHECK(g.release().kind == Gesture::Kind::SwipeRight);
    
    g.press(100, 20);       // a vertical drag is neither
    g.move(110, 150);
    CHECK(g.release().kind == Gesture::Kind::None);
}

static void testNavBar() {
    // Buttons tile the bar inside the display and are big enough to hit with a finger
    for (int i = 0; i < NavBar::BUTTON_COUNT; i++) {
        Rect r = NavBar::button(i);
        CHECK(r.x >= 0 && r.right() <= Layout::SCREEN_WIDTH);
        CHECK(r.y >= Layout::SCREEN_HEIGHT && r.bottom() <= NavBar::DISPLAY_HEIGHT);
        CHECK(r.w * 3 / 2 >= 100 && r.h * 3 / 2 >= 55);
        if (i > 0) CHECK(r.x >= NavBar::button(i - 1).right());
    }
    
    auto tapOn = [](int i) {
        Rect r = NavBar::button(i);
        Gesture g;
        g.kind = Gesture::Kind::Tap;
        g.x = r.x + r.w / 2;
        g.y = r.y + r.h / 2;
        return NavBar::actionFor(g);
    };
    CHECK(tapOn(0) == NavBar::Action::Previous);
    CHECK(tapOn(1) == NavBar::Action::ToggleHold);
    CHECK(tapOn(2) == NavBar::Action::Next);
    CHECK(tapOn(3) == NavBar::Action::Console);
    
    Gesture page;
    page.kind = Gesture::Kind::Tap;
    page.x = 160; page.y = 80;
    CHECK(NavBar::actionFor(page) == NavBar::Action::None);
    page.kind = Gesture::Kind::SwipeLeft;
    CHECK(NavBar::actionFor(page) == NavBar::Action::Next);
    
    // Actions drive the shared page manager
    Config config = Config::defaultsFor(device::SERVICE_NAME);
    Dashboard dashboard(config, device::display());
    ScreenManager& screens = dashboard.screens();
    CHECK(screens.pageCount() == 6 && screens.currentPage() == 0);
    NavBar::apply(NavBar::Action::Previous, screens);
    CHECK(screens.currentPage() == 5);
    NavBar::apply(NavBar::Action::Next, screens);
    NavBar::apply(NavBar::Action::Next, screens);
    CHECK(screens.currentPage() == 1);
    NavBar::apply(NavBar::Action::ToggleHold, screens);
    CHECK(screens.hold());
    
    CHECK(dashboard.framebuffer().deviceWidth() == device::PIXEL_WIDTH);
    CHECK(dashboard.framebuffer().deviceHeight() == device::PIXEL_HEIGHT);
}

int main() {
    testCalibration();
    testGestures();
    testNavBar();
    if (failures) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "all tests passed\n";
    return 0;
}
