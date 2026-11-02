#include "appearance_math.h"
#include "theme_model.h"
#include "themes_selector.h"
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>
int main()
{
    try
    {
        auto check = [](bool value) {
            if (!value)
                throw std::runtime_error("appearance check failed");
        };
        // Exact RGB entry must survive HSL conversion, including fully saturated
        // colors whose canvas radius is zero and therefore needs a retained hue.
        for (uint32_t color : {0xff0000u, 0x00ff00u, 0x0000ffu, 0xffff00u, 0x00ffffu, 0xff00ffu,
                               0xffffffu, 0u, 0x808080u, 0x244c68u, 0xf0b8cdu})
            check(dt::theme_dots(dt::theme_from_rgb(color))[0].color == color);
        for (int i = 0; i < dt::ThemePresetCount; ++i)
            check(dt::theme_dots(dt::theme_preset(i))[0].color == dt::theme_preset_color(i));
        auto model = dt::theme_from_rgb(0x00ffff);
        model.count = 3;
        model.harmony = 2;
        auto colors = dt::theme_dots(model);
        check(colors.size() == 3 && colors[1].color == 0xff00ff && colors[2].color == 0xffff00);
        dt::move_theme(model, 3, 4);
        check(std::abs(std::hypot(model.x, model.y) - 1) < 1e-10);
        model.x = std::numeric_limits<double>::quiet_NaN();
        model.y = std::numeric_limits<double>::infinity();
        model.lightness = -1;
        model.count = 99;
        model.harmony = 999;
        model.mode = static_cast<dt::ThemeMode>(99);
        dt::sanitize_theme(model);
        check(model.x == 0 && model.y == 0 && model.lightness == 0 && model.count == 3 &&
              model.harmony == 2 && model.mode == dt::ThemeMode::Auto);
        model = dt::theme_from_rgb(0x244c68);
        dt::ThemeRender solid(model, false);
        check(solid.pixel(0, 0) == 0x244c68 && solid.pixel(1, 1) == 0x244c68);
        model.count = 0;
        check(dt::theme_dots(model).empty());
        model = dt::theme_preset(9);
        dt::ThemeRender gradient(model, false);
        check(gradient.pixel(0, 0) != gradient.pixel(1, 1));
        std::cout << "PASS: exact RGB and palette colors, harmonies, bounds, reset and gradients\n";
        check(dt::texture_detent(0, -40, 8) == 0);
        check(dt::texture_detent(40, 0, 0) == 4);
        check(dt::texture_detent(0, 40, 0) == 8);
        check(dt::texture_detent(-40, 0, 0) == 12);
        check(dt::texture_detent(-0.1, -40, 8) == 0);
        check(dt::texture_detent(0, 0, 7) == 7);
        for (int i = 0; i < 16; ++i)
        {
            double a = i * 2 * std::numbers::pi / 16 - std::numbers::pi / 2;
            check(dt::texture_detent(40 * std::cos(a), 40 * std::sin(a), 0) == i);
        }
        std::cout << "PASS: sixteen clockwise detents, wrap and center dead zone\n";
        double sum = 0, squares = 0, horizontal = 0, vertical = 0;
        for (unsigned y = 0; y < 256; ++y)
            for (unsigned x = 0; x < 256; ++x)
            {
                int n = dt::fine_grain(x, y, 15);
                check(dt::fine_grain(x, y, 0) == 0 && std::abs(n) <= 24);
                sum += n;
                squares += n * n;
                horizontal += n * dt::fine_grain(x + 1, y, 15);
                vertical += n * dt::fine_grain(x, y + 1, 15);
            }
        check(std::abs(sum / 65536) < .2 && squares / 65536 > 60 && squares / 65536 < 110);
        check(std::abs(horizontal / squares) < .03 && std::abs(vertical / squares) < .03);
        std::cout << "PASS: grain is centered, bounded and spatially uncorrelated\n";
        const std::array sidebar{
            dt::sidebar_menu_role(L"New Tab"), dt::sidebar_menu_role(L"New Group"),
            dt::sidebar_menu_role(L"Show Tabs in Sidebar"),
            dt::sidebar_menu_role(L"Auto-Hide Tabs"), dt::sidebar_menu_role(L"Close all tabs")};
        check(dt::is_sidebar_menu(sidebar));
        check(dt::sidebar_menu_role(L"&New tab") == dt::SidebarMenuRole::NewTab);
        check(!dt::is_sidebar_menu({}));
        check(!dt::is_sidebar_menu(std::span(sidebar).first(4)));
        const std::array tabMenu{dt::sidebar_menu_role(L"New Tab"),
                                 dt::sidebar_menu_role(L"Duplicate Tab"),
                                 dt::sidebar_menu_role(L"Close all tabs")};
        check(!dt::is_sidebar_menu(tabMenu));
        check(dt::sidebar_menu_role(L"My page: Show Tabs in Sidebar") ==
              dt::SidebarMenuRole::Other);
        std::cout
            << "PASS: sidebar menu signature; reject tab menus, partial matches and page titles\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
