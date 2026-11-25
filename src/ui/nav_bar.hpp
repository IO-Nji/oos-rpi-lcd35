#pragma once
#include "core/framebuffer.hpp"
#include "core/layout.hpp"
#include "ui/screen_manager.hpp"
#include "ui/touch.hpp"

// The row of touch buttons under the dashboard, and what a gesture means.
// The display is 320 x 213 layout units. Everything sits on one rhythm: MARGIN from
// the display edge, and MARGIN between the header, the cards, the footer and the buttons.
class NavBar {
public:
    enum class Action { None, Previous, Next, ToggleHold, Console };
    
    static constexpr int DISPLAY_HEIGHT = 213;            // 320 device pixels at 150 %
    static constexpr int MARGIN = 6;
    static constexpr int BUTTON_COUNT = 4;
    static constexpr int BUTTON_W = 61;                   // 91 x 46 device pixels
    static constexpr int BUTTON_H = 31;
    static constexpr int Y = DISPLAY_HEIGHT - MARGIN - BUTTON_H;
    
    // The dashboard gets everything above the buttons
    static constexpr int DASHBOARD_HEIGHT = Y;
    
    // Buttons are spread evenly, the outer two lining up with the cards above
    static Rect button(int index) {
        const int span = Layout::SCREEN_WIDTH - 2 * MARGIN - BUTTON_W;
        return Rect(MARGIN + span * index / (BUTTON_COUNT - 1), Y, BUTTON_W, BUTTON_H);
    }
    
    void draw(Framebuffer& fb, bool hold) const {
        const char* labels[BUTTON_COUNT] = {"PREV", hold ? "HELD" : "HOLD", "NEXT", "TERM"};
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
