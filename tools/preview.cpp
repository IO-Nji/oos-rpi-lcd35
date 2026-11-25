// Preview tool for the 480x320 Raspberry Pi display; see core/tools/preview_app.hpp.
#include "preview_app.hpp"
#include "device.hpp"

int main(int argc, char* argv[]) {
    PreviewDevice pi{device::SERVICE_NAME, device::display(), device::configKeys(),
                     [](Dashboard& d) { NavBar().draw(d.framebuffer(), d.screens().hold()); }};
    return runPreview(argc, argv, pi);
}
