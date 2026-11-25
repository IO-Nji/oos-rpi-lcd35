#include "hw/console.hpp"
#include <fcntl.h>
#include <linux/kd.h>
#include <sys/ioctl.h>
#include <unistd.h>

Console::~Console() {
    giveDisplay();
    if (fd_ >= 0) ::close(fd_);
}

bool Console::open(const std::string& tty) {
    fd_ = ::open(tty.c_str(), O_RDWR | O_NOCTTY);
    return fd_ >= 0;
}

void Console::takeDisplay() {
    if (fd_ >= 0 && !graphics_ && ioctl(fd_, KDSETMODE, KD_GRAPHICS) == 0) graphics_ = true;
}

void Console::giveDisplay() {
    if (fd_ < 0 || !graphics_) return;
    ioctl(fd_, KDSETMODE, KD_TEXT);
    graphics_ = false;
    // Ask the terminal to repaint what the dashboard drew over
    const char redraw[] = "\033[?25h\033[2J\033[H";
    if (write(fd_, redraw, sizeof(redraw) - 1) < 0) return;
}
