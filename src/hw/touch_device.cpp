#include "hw/touch_device.hpp"
#include <cstring>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>

static bool hasBit(const unsigned long* bits, int bit) {
    return bits[bit / (8 * sizeof(long))] & (1UL << (bit % (8 * sizeof(long))));
}

TouchDevice::~TouchDevice() {
    close();
}

bool TouchDevice::open(const std::string& device) {
    if (!device.empty()) return openDevice(device);
    
    for (int i = 0; i < 16; i++) {
        if (openDevice("/dev/input/event" + std::to_string(i))) return true;
    }
    error_ = "no touchscreen found among /dev/input/event*";
    return false;
}

bool TouchDevice::openDevice(const std::string& device) {
    close();
    fd_ = ::open(device.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd_ < 0) {
        error_ = device + ": " + std::strerror(errno);
        return false;
    }
    
    unsigned long absBits[ABS_MAX / (8 * sizeof(long)) + 1] = {};
    unsigned long keyBits[KEY_MAX / (8 * sizeof(long)) + 1] = {};
    struct input_absinfo absX{}, absY{};
    if (ioctl(fd_, EVIOCGBIT(EV_ABS, sizeof(absBits)), absBits) < 0 ||
        ioctl(fd_, EVIOCGBIT(EV_KEY, sizeof(keyBits)), keyBits) < 0 ||
        !hasBit(absBits, ABS_X) || !hasBit(absBits, ABS_Y) || !hasBit(keyBits, BTN_TOUCH) ||
        ioctl(fd_, EVIOCGABS(ABS_X), &absX) < 0 || ioctl(fd_, EVIOCGABS(ABS_Y), &absY) < 0) {
        error_ = device + ": not a touchscreen";
        close();
        return false;
    }
    
    char name[128] = "";
    ioctl(fd_, EVIOCGNAME(sizeof(name) - 1), name);
    name_ = name;
    device_ = device;
    
    defaults_ = TouchCalibration();
    if (absX.maximum > absX.minimum) { defaults_.minX = absX.minimum; defaults_.maxX = absX.maximum; }
    if (absY.maximum > absY.minimum) { defaults_.minY = absY.minimum; defaults_.maxY = absY.maximum; }
    touching_ = pendingPress_ = pendingRelease_ = moved_ = false;
    return true;
}

void TouchDevice::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

std::vector<TouchDevice::Event> TouchDevice::poll(int timeoutMs) {
    std::vector<Event> events;
    if (fd_ < 0) return events;
    
    struct pollfd pfd{fd_, POLLIN, 0};
    if (::poll(&pfd, 1, timeoutMs) <= 0) return events;
    
    struct input_event batch[32];
    ssize_t n;
    while ((n = read(fd_, batch, sizeof(batch))) > 0) {
        for (size_t i = 0; i < n / sizeof(batch[0]); i++) {
            const struct input_event& e = batch[i];
            if (e.type == EV_ABS && e.code == ABS_X) {
                position_.x = e.value;
                moved_ = true;
            } else if (e.type == EV_ABS && e.code == ABS_Y) {
                position_.y = e.value;
                moved_ = true;
            } else if (e.type == EV_KEY && e.code == BTN_TOUCH) {
                (e.value ? pendingPress_ : pendingRelease_) = true;
            } else if (e.type == EV_SYN && e.code == SYN_REPORT) {
                // One report is complete: the position belongs with the contact change
                if (pendingPress_) {
                    touching_ = true;
                    events.push_back({Event::Kind::Press, position_});
                } else if (touching_ && moved_ && !pendingRelease_) {
                    events.push_back({Event::Kind::Move, position_});
                }
                if (pendingRelease_ && touching_) {
                    touching_ = false;
                    events.push_back({Event::Kind::Release, position_});
                }
                pendingPress_ = pendingRelease_ = moved_ = false;
            }
        }
    }
    return events;
}
