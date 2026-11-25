#pragma once
#include <string>

// The text console that shares the display. While the dashboard is showing, the
// virtual terminal is in graphics mode, so the kernel draws neither text nor a
// cursor over it; a login session on the terminal keeps running underneath.
// Handing the display over restores text mode and the kernel repaints the console.
class Console {
public:
    // tty is the virtual terminal shown on the display, e.g. /dev/tty1
    explicit Console(const std::string& tty);
    ~Console();
    
    // Dashboard owns the screen. Call before every frame: something else (a login,
    // an X server starting and stopping) can put the terminal back in text mode.
    // Returns true if the mode had to be changed, meaning the console may have
    // drawn on the screen.
    bool takeDisplay();
    
    // Console owns the screen: switch to the terminal, make sure it has a login
    // prompt, then run hook (a shell command, may be empty) to dress it up
    void giveDisplay(const std::string& hook);
    
private:
    // Run one ioctl on the terminal. It is opened each time because a login
    // starting on the terminal hangs up every handle that was open before.
    bool control(unsigned long request, long argument);
    bool mode(int& value);
    
    std::string tty_;          // "/dev/tty1"
    std::string name_;         // "tty1"
    int vtNumber_ = 0;
};
