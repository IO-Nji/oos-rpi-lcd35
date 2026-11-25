#include "hw/fb_display.hpp"
#include "core/color.hpp"
#include <algorithm>
#include <cstring>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

FbDisplay::~FbDisplay() {
    close();
}

bool FbDisplay::open(const std::string& device, int width, int height) {
    if (!device.empty()) return openDevice(device, width, height);
    
    std::string firstError;
    for (int i = 0; i < 8; i++) {
        if (openDevice("/dev/fb" + std::to_string(i), width, height)) return true;
        if (firstError.empty() || error_.find("No such file") == std::string::npos) firstError = error_;
    }
    error_ = "no " + std::to_string(width) + "x" + std::to_string(height) +
             " framebuffer found (last problem: " + firstError + ")";
    return false;
}

bool FbDisplay::openDevice(const std::string& device, int width, int height) {
    close();
    fd_ = ::open(device.c_str(), O_RDWR);
    if (fd_ < 0) {
        error_ = device + ": " + std::strerror(errno);
        return false;
    }
    
    struct fb_var_screeninfo var{};
    struct fb_fix_screeninfo fix{};
    if (ioctl(fd_, FBIOGET_VSCREENINFO, &var) != 0 || ioctl(fd_, FBIOGET_FSCREENINFO, &fix) != 0) {
        error_ = device + ": not a framebuffer";
        close();
        return false;
    }
    
    if (static_cast<int>(var.xres) != width || static_cast<int>(var.yres) != height) {
        error_ = device + " is " + std::to_string(var.xres) + "x" + std::to_string(var.yres) +
                 ", need " + std::to_string(width) + "x" + std::to_string(height) +
                 " (check the display's rotate setting)";
        close();
        return false;
    }
    if (var.bits_per_pixel != 16 && var.bits_per_pixel != 32) {
        error_ = device + ": " + std::to_string(var.bits_per_pixel) + " bits per pixel is not supported";
        close();
        return false;
    }
    
    width_ = width;
    height_ = height;
    stride_ = fix.line_length;
    bytesPerPixel_ = var.bits_per_pixel / 8;
    redShift_ = var.red.offset;     redBits_ = var.red.length;
    greenShift_ = var.green.offset; greenBits_ = var.green.length;
    blueShift_ = var.blue.offset;   blueBits_ = var.blue.length;
    
    memorySize_ = static_cast<size_t>(stride_) * height_;
    void* map = mmap(nullptr, memorySize_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (map == MAP_FAILED) {
        error_ = device + ": cannot map: " + std::strerror(errno);
        memory_ = nullptr;
        close();
        return false;
    }
    memory_ = static_cast<uint8_t*>(map);
    device_ = device;
    shown_.clear();
    return true;
}

void FbDisplay::close() {
    if (memory_) {
        munmap(memory_, memorySize_);
        memory_ = nullptr;
    }
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

void FbDisplay::clear() {
    if (memory_) std::memset(memory_, 0, memorySize_);
    shown_.clear();
}

int FbDisplay::present(const Framebuffer& fb) {
    if (!memory_ || fb.deviceWidth() != width_ || fb.deviceHeight() != height_) return 0;
    
    const size_t pixelCount = static_cast<size_t>(width_) * height_;
    bool all = shown_.size() != pixelCount;
    if (all) shown_.assign(pixelCount, 0);
    
    int rows = 0;
    for (int y = 0; y < height_; y++) {
        const uint16_t* src = fb.pixels() + static_cast<size_t>(y) * width_;
        uint16_t* seen = shown_.data() + static_cast<size_t>(y) * width_;
        if (!all && std::equal(src, src + width_, seen)) continue;
        
        uint8_t* dst = memory_ + static_cast<size_t>(y) * stride_;
        for (int x = 0; x < width_; x++) {
            // Framebuffer colours are RGB565 with the bytes swapped
            uint16_t c = swapEndian16(src[x]);
            uint32_t r = (c >> 11) & 0x1F, g = (c >> 5) & 0x3F, b = c & 0x1F;
            if (bytesPerPixel_ == 2 && redBits_ == 5 && greenBits_ == 6 && blueBits_ == 5) {
                uint16_t out = static_cast<uint16_t>((r << redShift_) | (g << greenShift_) | (b << blueShift_));
                std::memcpy(dst + x * 2, &out, 2);
            } else {
                // Widen each channel to the device's depth
                uint32_t r8 = (r << 3) | (r >> 2), g8 = (g << 2) | (g >> 4), b8 = (b << 3) | (b >> 2);
                uint32_t out = ((r8 >> (8 - redBits_)) << redShift_) |
                               ((g8 >> (8 - greenBits_)) << greenShift_) |
                               ((b8 >> (8 - blueBits_)) << blueShift_);
                std::memcpy(dst + x * bytesPerPixel_, &out, bytesPerPixel_);
            }
        }
        std::copy(src, src + width_, seen);
        rows++;
    }
    return rows;
}
