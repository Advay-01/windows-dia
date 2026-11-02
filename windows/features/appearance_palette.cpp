#include "appearance_editor_impl.h"
#include "theme_button.h"
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Windows.Foundation.Collections.h>
namespace dt
{
using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace Controls;
using namespace Media;
void AppearanceEditor::Impl::palette(const StackPanel &root)
{
    StackPanel row;
    row.Orientation(Orientation::Horizontal);
    row.Spacing(12);
    row.Height(44);
    row.HorizontalAlignment(HorizontalAlignment::Center);
    Button previous, next;
    previous.Content(Markup::XamlReader::Load(
        LR"(<Path xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" Data="M8,2 L4,6 L8,10" Width="12" Height="12" Stretch="None" Stroke="{ThemeResource ButtonForeground}" StrokeThickness="1.8" StrokeStartLineCap="Round" StrokeEndLineCap="Round"/>)"));
    next.Content(Markup::XamlReader::Load(
        LR"(<Path xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" Data="M4,2 L8,6 L4,10" Width="12" Height="12" Stretch="None" Stroke="{ThemeResource ButtonForeground}" StrokeThickness="1.8" StrokeStartLineCap="Round" StrokeEndLineCap="Round"/>)"));
    for (auto b : {previous, next})
    {
        theme_button(b);
        b.Width(20);
        b.MinWidth(0);
        b.Height(36);
        b.VerticalAlignment(VerticalAlignment::Center);
        b.HorizontalContentAlignment(HorizontalAlignment::Center);
        b.VerticalContentAlignment(VerticalAlignment::Center);
        b.MinHeight(0);
        b.Padding({0, 0, 0, 0});
        b.BorderThickness({0, 0, 0, 0});
        b.FontSize(25);
        b.Background(SolidColorBrush(winrt::Windows::UI::Color{0, 0, 0, 0}));
    }
    Automation::AutomationProperties::SetName(previous, L"Previous colors");
    Automation::AutomationProperties::SetName(next, L"Next colors");
    previous.IsEnabled(false);
    pages.Width(288);
    pages.HorizontalScrollMode(ScrollMode::Enabled);
    pages.VerticalScrollMode(ScrollMode::Disabled);
    pages.HorizontalScrollBarVisibility(ScrollBarVisibility::Hidden);
    pages.VerticalScrollBarVisibility(ScrollBarVisibility::Disabled);
    StackPanel strip;
    strip.Orientation(Orientation::Horizontal);
    auto weak = weak_from_this();
    for (int p = 0; p < 5; ++p)
    {
        StackPanel group;
        group.Orientation(Orientation::Horizontal);
        group.Spacing(4);
        group.Width(288);
        group.VerticalAlignment(VerticalAlignment::Center);
        for (int i = 0; i < 8; ++i)
        {
            int index = p * 8 + i;
            uint32_t c = theme_preset_color(index);
            Button swatch;
            theme_button(swatch);
            swatch.Width(32);
            swatch.Height(40);
            swatch.HorizontalContentAlignment(HorizontalAlignment::Center);
            swatch.VerticalContentAlignment(VerticalAlignment::Center);
            swatch.MinWidth(0);
            swatch.MinHeight(0);
            swatch.Padding({0, 0, 0, 0});
            swatch.BorderThickness({0, 0, 0, 0});
            swatch.Background(SolidColorBrush(winrt::Windows::UI::Color{0, 0, 0, 0}));
            swatch.CornerRadius({13, 13, 13, 13});
            Shapes::Ellipse dot;
            dot.Width(26);
            dot.Height(26);
            dot.UseLayoutRounding(false);
            dot.Fill(SolidColorBrush(
                winrt::Windows::UI::Color{255, uint8_t(c >> 16), uint8_t(c >> 8), uint8_t(c)}));
            swatch.Content(dot);
            Automation::AutomationProperties::SetName(
                swatch, hstring(L"Color preset " + std::to_wstring(index + 1)));
            swatch.Click([weak, index](auto &&, auto &&) noexcept {
                try
                {
                    if (auto s = weak.lock())
                    {
                        auto m = theme_preset(index);
                        m.mode = appearance_values().theme.mode;
                        s->apply(m, true);
                    }
                }
                catch (...)
                {
                }
            });
            group.Children().Append(swatch);
        }
        strip.Children().Append(group);
    }
    pages.Content(strip);
    row.Children().Append(previous);
    row.Children().Append(pages);
    row.Children().Append(next);
    root.Children().Append(row);
    auto wp = make_weak(previous), wn = make_weak(next);
    auto change = [weak, wp, wn](int d) {
        if (auto s = weak.lock())
        {
            s->page = std::clamp(s->page + d, 0, 4);
            s->pages.ChangeView(s->page * 288., nullptr, nullptr);
            if (auto p = wp.get())
                p.IsEnabled(s->page > 0);
            if (auto n = wn.get())
                n.IsEnabled(s->page < 4);
        }
    };
    previous.Click([change](auto &&, auto &&) noexcept {
        try
        {
            change(-1);
        }
        catch (...)
        {
        }
    });
    next.Click([change](auto &&, auto &&) noexcept {
        try
        {
            change(1);
        }
        catch (...)
        {
        }
    });
}
} // namespace dt
