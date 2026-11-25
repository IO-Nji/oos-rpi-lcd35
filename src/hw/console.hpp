#pragma once
#include <string>

// The text console that shares the display. While the dashboard is showing, the
// virtual terminal is put in graphics mode so the kernel does not draw text or a
// cursor over it; handing the display back restores text mode.
class Console {
public:
    ~Console();
    
    // tty is the virtual terminal shown on the display, e.g. /dev/tty1. Returns false
    // if it cannot be opened; the dashboard still works, the console just is not managed.
    bool open(const std::string& tty);
    
    void takeDisplay();    // dashboard owns the screen
    void giveDisplay();    // console owns the screen
    
private:
    int fd_ = -1;
    bool graphics_ = false;
};
