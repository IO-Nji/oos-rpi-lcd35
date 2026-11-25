#pragma once
#include "ui/touch.hpp"
#include <string>
#include <vector>

// A touchscreen exposed by the kernel as an input event device (/dev/input/eventN),
// as the ads7846 driver does for the XPT2046.
class TouchDevice {
public:
    struct Event {
        enum class Kind { Press, Move, Release } kind;
        RawPoint raw;
    };
    
    ~TouchDevice();
    
    // Open device, or with an empty name the first input device that reports
    // absolute X/Y and touch contact. On failure error() says why.
    bool open(const std::string& device);
    void close();
    const std::string& error() const { return error_; }
    const std::string& device() const { return device_; }
    const std::string& name() const { return name_; }
    
    // Raw axis ranges reported by the driver: the calibration to use until the
    // panel has been calibrated
    TouchCalibration defaultCalibration() const { return defaults_; }
    
    // Wait up to timeoutMs for input and return what happened, oldest first
    std::vector<Event> poll(int timeoutMs);
    
private:
    bool openDevice(const std::string& device);
    
    int fd_ = -1;
    RawPoint position_;
    bool touching_ = false, pendingPress_ = false, pendingRelease_ = false, moved_ = false;
    TouchCalibration defaults_;
    std::string device_, name_, error_;
};
