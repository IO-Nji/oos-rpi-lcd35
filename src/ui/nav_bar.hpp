#pragma once
#include "core/framebuffer.hpp"
#include "core/layout.hpp"
#include "ui/screen_manager.hpp"
#include "ui/touch.hpp"

// The strip of touch buttons under the dashboard frame, and what a gesture means.
// The display is 320 x 213 layout units: the frame takes the top 170, this bar the rest.
class NavBar {
public:
    enum class Action { None, Previous, Next, ToggleHold, Console };
    
    static constexpr int DISPLAY_HEIGHT = 213;            // 320 device pixels at 150 %
    static constexpr int BUTTON_COUNT = 4;
    static constexpr int GAP = 3;
    static constexpr int X = Layout::BORDER_X;
    static constexpr int Y = Layout::SCREEN_HEIGHT;       // directly under the frame
    static constexpr int W = Layout::BORDER_W;
    static constexpr int H = DISPLAY_HEIGHT - Y - Layout::OUTER_PADDING;
    
    static Rect button(int index) {
        return Rect(X, Y, W, H).column(index, BUTTON_COUNT, GAP);
    }
    
    void draw(Framebuffer& fb, bool hold) const {
        const char* labels[BUTTON_COUNT] = {"PREV", hold ? "HELD" : "HOLD", "NEXT", "CONSOLE"};
        for (int i = 0; i < BUTTON_COUNT; i++) {
            Rect r = button(i);
            bool active = i == 1 && hold;
            fb.fillRoundedRect(r.x, r.y, r.w, r.h, 3, Theme::SURFACE);
            fb.drawRoundedRect(r.x, r.y, r.w, r.h, 3, active ? Theme::WARN : Theme::BORDER);
            fb.drawTextCentered(r.x + r.w / 2, r.y + (r.h - fb.textHeight(Font::Title)) / 2,
                                labels[i], active ? Theme::WARN : Theme::TITLE, Font::Title);
        }
    }
    
    // A tap on a button presses it; swiping across the page turns it
    static Action actionFor(const Gesture& g) {
        switch (g.kind) {
            case Gesture::Kind::SwipeLeft:  return Action::Next;
            case Gesture::Kind::SwipeRight: return Action::Previous;
            case Gesture::Kind::Tap:        break;
            default:                        return Action::None;
        }
        const Action actions[BUTTON_COUNT] = {Action::Previous, Action::ToggleHold,
                                              Action::Next, Action::Console};
        for (int i = 0; i < BUTTON_COUNT; i++) {
            Rect r = button(i);
            if (g.x >= r.x && g.x < r.right() && g.y >= r.y && g.y < r.bottom()) return actions[i];
        }
        return Action::None;
    }
    
    // Carry out a navigation action; Console is left to the caller
    static void apply(Action action, ScreenManager& screens) {
        switch (action) {
            case Action::Previous:   screens.prevPage(); break;
            case Action::Next:       screens.nextPage(); break;
            case Action::ToggleHold: screens.setHold(!screens.hold()); break;
            default: break;
        }
    }
};
