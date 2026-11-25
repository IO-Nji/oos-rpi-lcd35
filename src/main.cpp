#include "device.hpp"
#include "hw/console.hpp"
#include "hw/fb_display.hpp"
#include "hw/touch_device.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

#ifndef OOS_VERSION
#define OOS_VERSION "dev"
#endif

static std::atomic<bool> running{true};

static void signalHandler(int) {
    running = false;
}

static void printUsage(const char* program) {
    std::cout << "Usage: " << program << " [options]\n"
              << "  --config <file>   Settings file (default: " << device::DEFAULT_CONFIG << ")\n"
              << "  --calibrate       Calibrate the touch panel and print the settings to use\n"
              << "  --version         Print the version\n"
              << "  --help            Show this help\n";
}

static std::string setting(const Config& config, const char* key, const char* fallback = "") {
    auto it = config.deviceSettings.find(key);
    return it == config.deviceSettings.end() ? fallback : it->second;
}

// Calibration from the settings file, starting from the ranges the driver reports
static TouchCalibration calibrationFrom(const Config& config, TouchCalibration c) {
    std::string swap = setting(config, "touch_swap_xy");
    if (!swap.empty()) c.swapXY = swap == "yes" || swap == "true" || swap == "1";
    
    const std::pair<const char*, int*> ranges[] = {
        {"touch_min_x", &c.minX}, {"touch_max_x", &c.maxX},
        {"touch_min_y", &c.minY}, {"touch_max_y", &c.maxY},
    };
    for (const auto& r : ranges) {
        std::string value = setting(config, r.first);
        if (!value.empty()) *r.second = std::atoi(value.c_str());
    }
    return c;
}

// Show three targets in turn, read where the panel says each tap landed, and print
// the settings that make touches line up with the picture.
static int calibrate(FbDisplay& display, TouchDevice& touch) {
    const float inset = 0.1f;
    const DisplayProfile profile = device::display();
    Framebuffer fb(profile.width, profile.height, profile.scale);
    const float targets[3][2] = {{inset, inset}, {1 - inset, inset}, {inset, 1 - inset}};
    RawPoint taps[3];
    
    for (int i = 0; i < 3 && running; i++) {
        int x = static_cast<int>(targets[i][0] * profile.width);
        int y = static_cast<int>(targets[i][1] * profile.height);
        fb.clear(Theme::BG);
        fb.drawTextCentered(profile.width / 2, profile.height / 2 - 12, "TOUCH CALIBRATION", Theme::TITLE, Font::Title);
        fb.drawTextCentered(profile.width / 2, profile.height / 2 + 4,
                            "Tap the target (" + std::to_string(i + 1) + " of 3)", Theme::TEXT);
        fb.fillCircle(x, y, 7, Theme::TITLE);
        fb.fillCircle(x, y, 4, Theme::BG);
        fb.fillCircle(x, y, 1, Theme::VALUE);
        display.present(fb);
        
        // The reading at release is the steadiest
        bool done = false;
        while (!done && running) {
            for (const auto& e : touch.poll(200)) {
                if (e.kind == TouchDevice::Event::Kind::Release) {
                    taps[i] = e.raw;
                    done = true;
                }
            }
        }
    }
    if (!running) return 1;
    
    TouchCalibration c = TouchCalibration::solve(taps[0], taps[1], taps[2], inset);
    std::cout << "# Add these lines to " << device::DEFAULT_CONFIG << "\n"
              << "touch_swap_xy = " << (c.swapXY ? "yes" : "no") << "\n"
              << "touch_min_x = " << c.minX << "\n"
              << "touch_max_x = " << c.maxX << "\n"
              << "touch_min_y = " << c.minY << "\n"
              << "touch_max_y = " << c.maxY << std::endl;
    return 0;
}

int main(int argc, char* argv[]) {
    std::string configPath = device::DEFAULT_CONFIG;
    bool configGiven = false, calibrateMode = false;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            configPath = argv[++i];
            configGiven = true;
        } else if (arg == "--calibrate") {
            calibrateMode = true;
        } else if (arg == "--version") {
            std::cout << device::SERVICE_NAME << " " << OOS_VERSION << std::endl;
            return 0;
        } else if (arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return 2;
        }
    }
    
    Config config = Config::defaultsFor(device::SERVICE_NAME);
    config.deviceKeys = device::configKeys();
    std::vector<std::string> warnings;
    if (!config.load(configPath, warnings) && configGiven) {
        std::cerr << "Cannot read " << configPath << std::endl;
        return 2;
    }
    for (const auto& w : warnings) {
        std::cerr << configPath << ": " << w << std::endl;
    }
    
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    FbDisplay display;
    if (!display.open(setting(config, "display"), device::PIXEL_WIDTH, device::PIXEL_HEIGHT)) {
        std::cerr << "Cannot open the display: " << display.error() << std::endl;
        return 1;
    }
    
    // The dashboard is still useful without touch; it just rotates on its own
    TouchDevice touch;
    bool haveTouch = touch.open(setting(config, "touch"));
    if (!haveTouch) std::cerr << "Touch disabled: " << touch.error() << std::endl;
    
    Console console(setting(config, "console_tty", "/dev/tty1"));
    
    if (calibrateMode) {
        console.takeDisplay();
        if (!haveTouch) return 1;
        return calibrate(display, touch);
    }
    
    const TouchCalibration calibration = calibrationFrom(config, touch.defaultCalibration());
    const DisplayProfile profile = device::display();
    
    Dashboard dashboard(config, profile);
    dashboard.startCollectors();
    NavBar navBar;
    GestureRecognizer gestures;
    
    std::cout << device::SERVICE_NAME << " " << OOS_VERSION << ": " << dashboard.screens().pageCount()
              << " pages on " << display.device()
              << (haveTouch ? ", touch on " + touch.device() + " (" + touch.name() + ")" : "")
              << std::endl;
    
    const auto frameInterval = 250ms;
    auto nextFrame = std::chrono::steady_clock::now();
    bool consoleHasDisplay = false;
    
    while (running) {
        auto now = std::chrono::steady_clock::now();
        if (!consoleHasDisplay && now >= nextFrame) {
            if (console.takeDisplay()) display.clear();
            dashboard.renderFrame();
            navBar.draw(dashboard.framebuffer(), dashboard.screens().hold());
            display.present(dashboard.framebuffer());
            nextFrame = now + frameInterval;
        }
        
        // Wait for touch until the next frame is due
        int waitMs = consoleHasDisplay ? 250 : static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(nextFrame - std::chrono::steady_clock::now()).count());
        if (!haveTouch) {
            std::this_thread::sleep_for(std::chrono::milliseconds(std::max(0, waitMs)));
            continue;
        }
        
        for (const auto& e : touch.poll(std::max(0, waitMs))) {
            int x, y;
            calibration.map(e.raw, profile.width, profile.height, x, y);
            if (e.kind == TouchDevice::Event::Kind::Press) {
                gestures.press(x, y);
                continue;
            }
            if (e.kind == TouchDevice::Event::Kind::Move) {
                gestures.move(x, y);
                continue;
            }
            
            Gesture gesture = gestures.release();
            if (consoleHasDisplay) {
                // Any tap brings the dashboard back
                if (gesture.kind == Gesture::Kind::Tap) {
                    consoleHasDisplay = false;
                    display.clear();
                    nextFrame = std::chrono::steady_clock::now();
                }
                continue;
            }
            
            NavBar::Action action = NavBar::actionFor(gesture);
            if (action == NavBar::Action::Console) {
                consoleHasDisplay = true;
                display.clear();
                console.giveDisplay(setting(config, "console_hook"));
            } else if (action != NavBar::Action::None) {
                NavBar::apply(action, dashboard.screens());
                nextFrame = std::chrono::steady_clock::now();   // show the result at once
            }
        }
    }
    
    // Leave the console in charge of the screen; its destructor restores text mode
    dashboard.stopCollectors();
    display.clear();
    return 0;
}
