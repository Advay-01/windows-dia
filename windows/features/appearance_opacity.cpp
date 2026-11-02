#include "appearance.h"
#include "appearance_controls.h"
#include "appearance_drag.h"
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
Grid appearance_opacity(std::function<void()> changed)
{
    Grid host;
    host.Width(250);
    host.Height(76);
    Canvas canvas;
    canvas.Width(250);
    canvas.Height(76);
    canvas.IsHitTestVisible(false);
    Shapes::Rectangle track;
    track.Width(230);
    track.Height(4);
    track.RadiusX(2);
    track.RadiusY(2);
    track.Fill(SolidColorBrush(winrt::Windows::UI::Color{255, 70, 73, 73}));
    Canvas::SetLeft(track, 10);
    Canvas::SetTop(track, 36);
    canvas.Children().Append(track);
    Shapes::Path wave;
    wave.StrokeThickness(4.5);
    wave.StrokeStartLineCap(PenLineCap::Round);
    wave.StrokeEndLineCap(PenLineCap::Round);
    LinearGradientBrush fill;
    fill.StartPoint({0, 0});
    fill.EndPoint({1, 0});
    GradientStop filled, remaining;
    filled.Color(winrt::Windows::UI::Color{255, 162, 164, 164});
    remaining.Color(winrt::Windows::UI::Color{255, 70, 73, 73});
    fill.GradientStops().Append(filled);
    fill.GradientStops().Append(remaining);
    wave.Stroke(fill);
    canvas.Children().Append(wave);
    Border thumb;
    thumb.Background(SolidColorBrush(winrt::Windows::UI::Color{255, 255, 255, 255}));
    thumb.CornerRadius({20, 20, 20, 20});
    canvas.Children().Append(thumb);
    PathGeometry geometry;
    PathFigure figure;
    std::array<BezierSegment, 10> segments;
    figure.StartPoint({10, 38});
    for (int i = 0; i < 10; ++i)
    {
        auto b = segments[i];
        double x = 10 + i * 23, sign = i % 2 == 0 ? -1. : 1.;
        b.Point1({float(x + 7), float(38 + sign * 21 * 1.33)});
        b.Point2({float(x + 16), float(38 + sign * 21 * 1.33)});
        b.Point3({float(x + 23), 38});
        figure.Segments().Append(b);
    }
    geometry.Figures().Append(figure);
    wave.Data(geometry);
    wave.Width(250);
    wave.Height(76);
    CompositeTransform position;
    position.CenterX(12.5);
    position.CenterY(27.5);
    thumb.Width(25);
    thumb.Height(55);
    thumb.RenderTransform(position);
    thumb.UseLayoutRounding(false);
    auto draw = [segments, position, filled, remaining](double value) {
        double t = value / 100.;
        filled.Offset(t);
        remaining.Offset(std::min(1., t + .005));
        // Reuse the curve segments, keeping stroke thickness constant as the
        // amplitude changes. Explicit bounds prevent changes to layout.
        for (int i = 0; i < 10; ++i)
        {
            double x = 10 + i * 23, a = (i % 2 == 0 ? -1. : 1.) * 21 * t * 1.33;
            segments[i].Point1({float(x + 7), float(38 + a)});
            segments[i].Point2({float(x + 16), float(38 + a)});
        }
        position.ScaleX((10 + 15 * t) / 25);
        position.ScaleY((40 + 15 * t) / 55);
        position.TranslateX(10 + 230 * t - 12.5);
        position.TranslateY(38 - 27.5);
    };
    Slider slider;
    slider.Width(250);
    slider.Height(76);
    slider.Minimum(0);
    slider.Maximum(100);
    slider.StepFrequency(.1);
    slider.SmallChange(1);
    slider.Value(appearance_values().opacity);
    slider.Opacity(0);
    Automation::AutomationProperties::SetName(slider, L"Theme opacity");
    Border focus;
    focus.BorderThickness({1, 1, 1, 1});
    focus.BorderBrush(SolidColorBrush(winrt::Windows::UI::Color{160, 255, 255, 255}));
    focus.CornerRadius({12, 12, 12, 12});
    focus.Visibility(Visibility::Collapsed);
    focus.IsHitTestVisible(false);
    slider.GotFocus([focus](auto &&sender, auto &&) noexcept {
        focus.Visibility(sender.template as<Slider>().FocusState() == FocusState::Keyboard
                             ? Visibility::Visible
                             : Visibility::Collapsed);
    });
    slider.LostFocus(
        [focus](auto &&, auto &&) noexcept { focus.Visibility(Visibility::Collapsed); });
    auto dragging = std::make_shared<bool>(false);
    slider.ValueChanged([draw, changed, dragging](auto &&sender, auto &&) noexcept {
        try
        {
            auto value = sender.template as<Slider>().Value();
            draw(value);
            preview_appearance_opacity(value);
            auto s = appearance_values();
            change_appearance(s.palette, int(std::lround(value)), s.texture, !*dragging);
            changed();
        }
        catch (...)
        {
        }
    });
    slider.AddHandler(
        UIElement::PointerPressedEvent(),
        box_value(Input::PointerEventHandler([dragging, focus](auto &&, auto &&) noexcept {
            *dragging = true;
            focus.Visibility(Visibility::Collapsed);
        })),
        true);
    slider.AddHandler(UIElement::PointerReleasedEvent(),
                      box_value(Input::PointerEventHandler([dragging](auto &&, auto &&) noexcept {
                          finish_appearance_drag(dragging);
                      })),
                      true);
    slider.AddHandler(UIElement::PointerCaptureLostEvent(),
                      box_value(Input::PointerEventHandler([dragging](auto &&, auto &&) noexcept {
                          finish_appearance_drag(dragging);
                      })),
                      true);
    draw(slider.Value());
    host.Children().Append(canvas);
    host.Children().Append(slider);
    host.Children().Append(focus);
    auto weakHost = make_weak(host);
    host.ActualThemeChanged([weakHost, track, thumb, filled, remaining, focus](auto &&,
                                                                               auto &&) noexcept {
        try
        {
            if (auto h = weakHost.get())
            {
                bool light = h.ActualTheme() == ElementTheme::Light;
                track.Fill(SolidColorBrush(winrt::Windows::UI::Color{255, uint8_t(light ? 194 : 70),
                                                                     uint8_t(light ? 197 : 73),
                                                                     uint8_t(light ? 197 : 73)}));
                thumb.Background(SolidColorBrush(winrt::Windows::UI::Color{
                    255, uint8_t(light ? 45 : 255), uint8_t(light ? 48 : 255),
                    uint8_t(light ? 48 : 255)}));
                filled.Color(winrt::Windows::UI::Color{255, uint8_t(light ? 100 : 162),
                                                       uint8_t(light ? 103 : 164),
                                                       uint8_t(light ? 103 : 164)});
                remaining.Color(winrt::Windows::UI::Color{255, uint8_t(light ? 194 : 70),
                                                          uint8_t(light ? 197 : 73),
                                                          uint8_t(light ? 197 : 73)});
                focus.BorderBrush(SolidColorBrush(winrt::Windows::UI::Color{
                    160, uint8_t(light ? 45 : 255), uint8_t(light ? 48 : 255),
                    uint8_t(light ? 48 : 255)}));
            }
        }
        catch (...)
        {
        }
    });
    return host;
}
} // namespace dt
