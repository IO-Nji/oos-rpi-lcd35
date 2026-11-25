#include "hw/console.hpp"
#include "data/provider_base.hpp"
#include <fcntl.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <sys/ioctl.h>
#include <unistd.h>

Console::Console(const std::string& tty) : tty_(tty) {
    size_t slash = tty.rfind('/');
    name_ = slash == std::string::npos ? tty : tty.substr(slash + 1);
    size_t digits = name_.find_first_of("0123456789");
    vtNumber_ = digits == std::string::npos ? 0 : std::atoi(name_.c_str() + digits);
}

Console::~Console() {
    control(KDSETMODE, KD_TEXT);
}

bool Console::control(unsigned long request, long argument) {
    int fd = ::open(tty_.c_str(), O_RDWR | O_NOCTTY);
    if (fd < 0) return false;
    bool ok = ioctl(fd, request, argument) == 0;
    ::close(fd);
    return ok;
}

bool Console::mode(int& value) {
    int fd = ::open(tty_.c_str(), O_RDWR | O_NOCTTY);
    if (fd < 0) return false;
    bool ok = ioctl(fd, KDGETMODE, &value) == 0;
    ::close(fd);
    return ok;
}

bool Console::takeDisplay() {
    int current = KD_TEXT;
    if (!mode(current) || current == KD_GRAPHICS) return false;
    return control(KDSETMODE, KD_GRAPHICS);
}

void Console::giveDisplay(const std::string& hook) {
    control(KDSETMODE, KD_TEXT);
    if (vtNumber_ > 0) control(VT_ACTIVATE, vtNumber_);
    
    // A login prompt, unless one is already there
    runCommand("systemctl start getty@" + name_ + ".service 2>/dev/null");
    if (!hook.empty()) runCommand(hook + " >/dev/null 2>&1");
}
