#include "url_contrast.h"
#include <iostream>
#include <stdexcept>
int main()
{
    try
    {
        auto require = [](bool value) {
            if (!value)
                throw std::runtime_error("Contrast policy check failed");
        };
        require(dt::relative_luminance(0) == 0 && dt::relative_luminance(0xffffff) > .999);
        require(!dt::page_luminance({0, 0xffffff}));
        require(*dt::page_luminance({0xffffff, 0xffffff, 0xffffff, 0, 0}) > .99);
        require(*dt::page_luminance({0, 0, 0, 0xffffff, 0xffffff}) == 0);
        dt::PageContrast c;
        require(c.update(dt::page_luminance({0xffffff, 0xffffff, 0xffffff})) && *c.light);
        require(!c.update(.18) && *c.light);
        require(!c.update(.01) && *c.light);
        require(c.update(.01) && !*c.light);
        require(!c.update(.19) && !*c.light);
        require(!c.update(.8) && !*c.light);
        require(c.update(.8) && *c.light);
        require(!c.update({}) && *c.light);
        dt::PageContrast dark;
        require(dark.update(.01) && !*dark.light);
        std::cout << "PASS: light/dark contrast, mixed-background median, unavailable samples and "
                     "hysteresis\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
