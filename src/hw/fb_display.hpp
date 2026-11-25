#pragma once
#include "core/framebuffer.hpp"
#include <cstdint>
#include <string>
#include <vector>

// A Linux framebuffer device (/dev/fbN), as provided for SPI displays by the fbtft
// and DRM panel drivers. Only rows that changed are written, because every write
// to such a display ends up as SPI traffic.
class FbDisplay {
public:
    ~FbDisplay();
    
    // Open device, or with an empty name the first framebuffer of the given size.
    // On failure error() says why.
    bool open(const std::string& device, int width, int height);
    void close();
    const std::string& error() const { return error_; }
    const std::string& device() const { return device_; }
    
    // Copy fb to the display. Returns the number of rows written.
    int present(const Framebuffer& fb);
    
    // Forget what is on the display, so the next present() writes everything
    void invalidate() { shown_.clear(); }
    
private:
    bool openDevice(const std::string& device, int width, int height);
    
    int fd_ = -1;
    uint8_t* memory_ = nullptr;
    size_t memorySize_ = 0;
    int width_ = 0, height_ = 0, stride_ = 0, bytesPerPixel_ = 0;
    int redShift_ = 0, greenShift_ = 0, blueShift_ = 0;
    int redBits_ = 0, greenBits_ = 0, blueBits_ = 0;
    std::vector<uint16_t> shown_;
    std::string device_, error_;
};
