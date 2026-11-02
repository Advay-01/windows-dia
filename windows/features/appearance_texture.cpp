#include "appearance.h"
#include "appearance_controls.h"
#include "appearance_drag.h"
#include "appearance_math.h"
#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>
#include <winrt/Windows.Foundation.Collections.h>
namespace dt
{
using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace Controls;
using namespace Media;
StackPanel appearance_texture(std::function<void()> changed)
{
    StackPanel root;
    root.Width(76);
    root.Height(76);
    Grid host;
    Canvas dial;
    dial.Width(76);
    dial.Height(76);
    dial.Background(SolidColorBrush(winrt::Windows::UI::Color{0, 0, 0, 0}));
    for (int i = 0; i < 16; ++i)
    {
        Shapes::Ellipse dot;
        dot.Width(3.2);
        dot.Height(3.2);
        dot.Fill(SolidColorBrush(winrt::Windows::UI::Color{255, 63, 65, 65}));
        double a = i * 2 * std::numbers::pi / 16 - std::numbers::pi / 2;
        Canvas::SetLeft(dot, 36.4 + 31 * std::cos(a));
        Canvas::SetTop(dot, 36.4 + 31 * std::sin(a));
        dial.Children().Append(dot);
    }
    Shapes::Ellipse face;
    face.Width(38);
    face.Height(38);
    LinearGradientBrush faceBrush;
    faceBrush.StartPoint({0, 0});
    faceBrush.EndPoint({1, 1});
    GradientStop top, bottom;
    top.Color(winrt::Windows::UI::Color{255, 44, 46, 46});
    bottom.Color(winrt::Windows::UI::Color{255, 27, 29, 29});
    bottom.Offset(1);
    faceBrush.GradientStops().Append(top);
    faceBrush.GradientStops().Append(bottom);
    face.Fill(faceBrush);
    face.Stroke(SolidColorBrush(winrt::Windows::UI::Color{255, 61, 63, 63}));
    face.StrokeThickness(.8);
    Canvas::SetLeft(face, 19);
    Canvas::SetTop(face, 19);
    dial.Children().Append(face);
    Shapes::Rectangle tick;
    tick.Width(5);
    tick.Height(11);
    tick.RadiusX(1.5);
    tick.RadiusY(1.5);
    tick.Fill(SolidColorBrush(winrt::Windows::UI::Color{255, 219, 221, 221}));
    RotateTransform rotate;
    tick.RenderTransform(rotate);
    tick.RenderTransformOrigin({0, 0});
    rotate.CenterX(2.5);
    rotate.CenterY(36.5);
    Canvas::SetLeft(tick, 35.5);
    Canvas::SetTop(tick, 1.5);
    dial.Children().Append(tick);
    auto position = [rotate](int value) { rotate.Angle(value * 360. / 16); };
    auto dragging = std::make_shared<bool>(false);
    Slider slider;
    slider.Minimum(0);
    slider.Maximum(15);
    slider.StepFrequency(1);
    slider.Value(appearance_values().texture);
    slider.Opacity(0);
    slider.IsHitTestVisible(false);
    Automation::AutomationProperties::SetName(slider, L"Grain, sixteen positions");
    Border focus;
    focus.CornerRadius({38, 38, 38, 38});
    focus.BorderThickness({1, 1, 1, 1});
    focus.BorderBrush(SolidColorBrush(winrt::Windows::UI::Color{150, 255, 255, 255}));
    focus.Visibility(Visibility::Collapsed);
    focus.IsHitTestVisible(false);
    slider.GotFocus([focus](auto &&sender, auto &&) noexcept {
        focus.Visibility(sender.template as<Slider>().FocusState() == FocusState::Keyboard
                             ? Visibility::Visible
                             : Visibility::Collapsed);
    });
    slider.LostFocus(
        [focus](auto &&, auto &&) noexcept { focus.Visibility(Visibility::Collapsed); });
    slider.ValueChanged([position, changed, dragging](auto &&sender, auto &&) noexcept {
        try
        {
            auto s = appearance_values();
            int v = int(sender.template as<Slider>().Value());
            change_appearance(s.palette, s.opacity, v, !*dragging);
            position(v);
            changed();
        }
        catch (...)
        {
        }
    });
    position(appearance_values().texture);
    auto weak = make_weak(dial);

    auto move = [weak, slider](const Input::PointerRoutedEventArgs &e) {
        if (auto d = weak.get())
        {
            auto p = e.GetCurrentPoint(d).Position();
            slider.Value(texture_detent(p.X - 38, p.Y - 38, int(slider.Value())));
        }
    };
    dial.PointerPressed(
        [weak, slider, dragging, move](auto &&, const Input::PointerRoutedEventArgs &e) noexcept {
            try
            {
                if (auto d = weak.get())
                {
                    slider.Focus(FocusState::Pointer);
                    *dragging = d.CapturePointer(e.Pointer());
                    move(e);
                    e.Handled(true);
                }
            }
            catch (...)
            {
            }
        });
    dial.PointerMoved([dragging, move](auto &&, const Input::PointerRoutedEventArgs &e) noexcept {
        try
        {
            if (*dragging)
                move(e);
        }
        catch (...)
        {
        }
    });
    dial.PointerReleased([weak, dragging](auto &&, auto &&) noexcept {
        try
        {
            finish_appearance_drag(dragging);
            if (auto d = weak.get())
                d.ReleasePointerCaptures();
        }
        catch (...)
        {
        }
    });
    dial.PointerCaptureLost(
        [dragging](auto &&, auto &&) noexcept { finish_appearance_drag(dragging); });
    host.Children().Append(dial);
    host.Children().Append(slider);
    host.Children().Append(focus);
    root.Children().Append(host);
    auto weakHost = make_weak(host);
    host.ActualThemeChanged([weakHost, top, bottom, tick, face, focus, dial](auto &&,
                                                                             auto &&) noexcept {
        try
        {
            if (auto h = weakHost.get())
            {
                bool light = h.ActualTheme() == ElementTheme::Light;
                top.Color(winrt::Windows::UI::Color{255, uint8_t(light ? 237 : 44),
                                                    uint8_t(light ? 239 : 46),
                                                    uint8_t(light ? 239 : 46)});
                bottom.Color(winrt::Windows::UI::Color{255, uint8_t(light ? 211 : 27),
                                                       uint8_t(light ? 213 : 29),
                                                       uint8_t(light ? 213 : 29)});
                tick.Fill(SolidColorBrush(winrt::Windows::UI::Color{255, uint8_t(light ? 55 : 219),
                                                                    uint8_t(light ? 57 : 221),
                                                                    uint8_t(light ? 57 : 221)}));
                face.Stroke(SolidColorBrush(winrt::Windows::UI::Color{
                    255, uint8_t(light ? 180 : 61), uint8_t(light ? 183 : 63),
                    uint8_t(light ? 183 : 63)}));
                for (unsigned i = 0; i < 16; ++i)
                    dial.Children().GetAt(i).as<Shapes::Ellipse>().Fill(
                        SolidColorBrush(winrt::Windows::UI::Color{255, uint8_t(light ? 175 : 63),
                                                                  uint8_t(light ? 177 : 65),
                                                                  uint8_t(light ? 177 : 65)}));
                focus.BorderBrush(SolidColorBrush(winrt::Windows::UI::Color{
                    150, uint8_t(light ? 45 : 255), uint8_t(light ? 48 : 255),
                    uint8_t(light ? 48 : 255)}));
            }
        }
        catch (...)
        {
        }
    });
    return root;
}
} // namespace dt
