#pragma once
#include "app/dashboard.hpp"
#include "ui/nav_bar.hpp"

// What makes this device different from the S1: a 480x320 display showing the same
// pages at 150 %, spaced out for a low-contrast panel, with touch buttons underneath.
namespace device {
    constexpr const char* SERVICE_NAME = "oos-rpi-lcd35";
    constexpr const char* DEFAULT_CONFIG = "/etc/oos-rpi-lcd35/config.conf";
    constexpr int PIXEL_WIDTH = 480;
    constexpr int PIXEL_HEIGHT = 320;
    
    inline DisplayProfile display() {
        DisplayProfile p;
        p.width = Layout::SCREEN_WIDTH;
        p.height = NavBar::DISPLAY_HEIGHT;
        p.scale = UiScale{3, 2};
        
        // No outer frame: on this panel the space is better spent between the cards
        p.chrome.height = NavBar::DASHBOARD_HEIGHT;
        p.chrome.framed = false;
        p.chrome.margin = NavBar::MARGIN;
        p.chrome.barGap = NavBar::MARGIN;
        p.chrome.cardGap = NavBar::MARGIN;
        return p;
    }
    
    // Settings this device adds to the shared config file
    inline std::set<std::string> configKeys() {
        return {"display", "touch", "console_tty", "console_hook", "touch_swap_xy",
                "touch_min_x", "touch_max_x", "touch_min_y", "touch_max_y"};
    }
}
