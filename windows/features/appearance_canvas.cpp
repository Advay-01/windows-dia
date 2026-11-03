#include "appearance_editor_impl.h"
#include "theme_button.h"
#include <robuffer.h>
#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.ViewManagement.h>
namespace dt
{
using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace Controls;
using namespace Media;
static SolidColorBrush brush(uint32_t c, uint8_t a = 255)
{
    return SolidColorBrush(
        winrt::Windows::UI::Color{a, uint8_t(c >> 16), uint8_t(c >> 8), uint8_t(c)});
}
static Button icon(const wchar_t *path, const wchar_t *name)
{
    Button b;
    theme_button(b);
    b.Width(30);
    b.Height(30);
    b.MinWidth(0);
    b.MinHeight(0);
    b.Padding({5, 5, 5, 5});
    b.BorderThickness({0, 0, 0, 0});
    b.CornerRadius({9, 9, 9, 9});
    b.Background(brush(0, 0));
    std::wstring x =
        L"<Path xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" Data=\"" +
        std::wstring(path) +
        L"\" Stroke=\"#EBEDED\" StrokeThickness=\"1.5\" StrokeLineJoin=\"Round\" "
        L"Stretch=\"None\" Width=\"24\" Height=\"24\"/>";
    Viewbox bounds;
    bounds.Width(18);
    bounds.Height(18);
    bounds.Child(Markup::XamlReader::Load(x).as<UIElement>());
    b.Content(bounds);
    Automation::AutomationProperties::SetName(b, name);
    ToolTipService::SetToolTip(b, box_value(name));
    return b;
}
static ImageBrush canvas_background(bool light)
{
    Imaging::WriteableBitmap bitmap(720, 720);
    BYTE *data = nullptr;
    check_hresult(
        bitmap.PixelBuffer().as<::Windows::Storage::Streams::IBufferByteAccess>()->Buffer(&data));
    for (int y = 0; y < 720; ++y)
        for (int x = 0; x < 720; ++x)
        {
            double px = x / 2., py = y / 2.;
            double dx = std::max({18 - px, px - 342, 0.}), dy = std::max({18 - py, py - 342, 0.});
            bool inside = dx * dx + dy * dy <= 18 * 18;
            auto i = (y * 720 + x) * 4;
            double dot = std::hypot(std::fmod(px, 6.) - 3, std::fmod(py, 6.) - 3);
            uint8_t c = uint8_t(light ? 229 - std::clamp(1. - dot, 0., 1.) * 15
                                      : 44 + std::clamp(1. - dot, 0., 1.) * 10);
            data[i] = data[i + 1] = data[i + 2] = inside ? c : 0;
            data[i + 3] = inside ? 255 : 0;
        }
    bitmap.Invalidate();
    ImageBrush background;
    background.ImageSource(bitmap);
    background.Stretch(Stretch::Fill);
    return background;
}
void AppearanceEditor::Impl::canvas()
{
    board.Width(360);
    board.Height(360);
    board.Name(L"DiaTweaksColorCanvas");
    RectangleGeometry clip;
    clip.Rect({0, 0, 360, 360});
    board.Clip(clip);
    board.Background(canvas_background(false));
    auto weak = weak_from_this();
    const wchar_t *paths[] = {
        L"M12,0 L15,9 L24,12 L15,15 L12,24 L9,15 L0,12 L9,9 Z M3,0 L4,3 L7,4 L4,5 L3,8 L2,5 L0,4 "
        L"L2,3 Z",
        L"M12,6 A6,6 0 1 1 11.99,6 Z M12,0 V3 M12,21 V24 M0,12 H3 M21,12 H24 M3.5,3.5 L5.5,5.5 "
        L"M18.5,18.5 L20.5,20.5 M3.5,20.5 L5.5,18.5 M18.5,5.5 L20.5,3.5",
        L"M14,2 A10,10 0 1 0 22,16 A9,9 0 0 1 14,2 Z"};
    const wchar_t *names[] = {L"System appearance", L"Light appearance", L"Dark appearance"};
    for (int i = 0; i < 3; ++i)
    {
        modes[i] = icon(paths[i], names[i]);
        if (i != 1)
            modes[i].Content().as<Viewbox>().Child().as<Shapes::Path>().Fill(brush(0xebeded));
        Canvas::SetLeft(modes[i], 115 + i * 50);
        Canvas::SetTop(modes[i], 14);
        board.Children().Append(modes[i]);
        modes[i].Click([weak, i](auto &&, auto &&) noexcept {
            try
            {
                if (auto s = weak.lock())
                {
                    auto m = appearance_values().theme;
                    m.mode = static_cast<ThemeMode>(i);
                    s->apply(m);
                }
            }
            catch (...)
            {
            }
        });
    }
    plus = icon(L"M12,3 V21 M3,12 H21", L"Add color");
    minus = icon(L"M3,12 H21", L"Remove color");
    harmony =
        icon(L"M12,2 A3,3 0 1 1 11.99,2 Z M5,14 A3,3 0 1 1 4.99,14 Z M19,14 A3,3 0 1 1 18.99,14 Z",
             L"Cycle color harmony");
    std::array<Button, 3> actions{plus, minus, harmony};
    for (int i = 0; i < 3; ++i)
    {
        Canvas::SetLeft(actions[i], 115 + i * 50);
        Canvas::SetTop(actions[i], 316);
        board.Children().Append(actions[i]);
        actions[i].Click([weak, i](auto &&, auto &&) noexcept {
            try
            {
                if (auto s = weak.lock())
                {
                    auto m = appearance_values().theme;
                    if (i == 0)
                        m.count = std::min(3, m.count + 1);
                    else if (i == 1)
                        m.count = std::max(0, m.count - 1);
                    else
                        m.harmony = (m.harmony + 1) % (m.count == 2 ? 2 : 3);
                    s->apply(m, true);
                }
            }
            catch (...)
            {
            }
        });
    }
    for (int i = 0; i < 3; ++i)
    {
        auto &b = handles[i];
        b.Name(i == 0 ? L"DiaTweaksPrimaryColor" : L"DiaTweaksCompanionColor");
        b.UseLayoutRounding(false);
        dots[i].UseLayoutRounding(false);
        b.RenderTransform(positions[i]);
        b.MinWidth(0);
        b.MinHeight(0);
        b.Padding({0, 0, 0, 0});
        b.BorderThickness({0, 0, 0, 0});
        b.Background(brush(0, 0));
        b.CornerRadius({30, 30, 30, 30});
        b.IsHitTestVisible(i == 0);
        b.IsTabStop(i == 0);
        dots[i].Stroke(brush(0xffffff));
        b.Content(dots[i]);
        board.Children().Append(b);
    }
    Automation::AutomationProperties::SetName(
        handles[0], L"Theme color. Arrow keys change color; double-click for RGB and hex");
    handles[0].DoubleTapped([weak](auto &&, auto &&) noexcept {
        try
        {
            if (auto s = weak.lock())
                s->choose_exact();
        }
        catch (...)
        {
        }
    });
    handles[0].KeyDown([weak](auto &&, const Input::KeyRoutedEventArgs &e) noexcept {
        try
        {
            if (auto s = weak.lock())
            {
                auto m = appearance_values().theme;
                using K = winrt::Windows::System::VirtualKey;
                switch (e.Key())
                {
                case K::Enter:
                case K::Space:
                    s->choose_exact();
                    e.Handled(true);
                    return;
                case K::Left:
                    m.x -= .015;
                    break;
                case K::Right:
                    m.x += .015;
                    break;
                case K::Up:
                    m.y -= .015;
                    break;
                case K::Down:
                    m.y += .015;
                    break;
                default:
                    return;
                }
                move_theme(m, m.x, m.y);
                s->apply(m);
                e.Handled(true);
            }
        }
        catch (...)
        {
        }
    });
    auto move = [weak](const Input::PointerRoutedEventArgs &e, bool animate) {
        if (auto s = weak.lock())
        {
            auto p = e.GetCurrentPoint(s->board).Position();
            auto m = appearance_values().theme;
            if (!m.count)
                m.count = 1;
            move_theme(m, (p.X - s->grabOffset.X - 180) / 180, (p.Y - s->grabOffset.Y - 180) / 180);
            s->apply(m, animate);
        }
    };
    board.AddHandler(
        UIElement::PointerPressedEvent(),
        box_value(Input::PointerEventHandler(
            [weak, move](auto &&, const Input::PointerRoutedEventArgs &e) noexcept {
                try
                {
                    if (auto s = weak.lock())
                    {
                        bool onHandle = false;
                        for (auto p = e.OriginalSource().try_as<DependencyObject>();
                             p && p != s->board; p = VisualTreeHelper::GetParent(p))
                        {
                            if (auto b = p.try_as<Button>(); b && b != s->handles[0])
                                return;
                            onHandle |= p == s->handles[0];
                        }
                        if (!e.GetCurrentPoint(s->board).Properties().IsLeftButtonPressed())
                            return;
                        s->dragging = onHandle ? s->handles[0].CapturePointer(e.Pointer())
                                               : s->board.CapturePointer(e.Pointer());
                        auto point = e.GetCurrentPoint(s->board).Position();
                        s->grabOffset =
                            onHandle
                                ? winrt::Windows::Foundation::Point{float(point.X -
                                                                          s->positions[0].X() - 25),
                                                                    float(point.Y -
                                                                          s->positions[0].Y() - 25)}
                                : winrt::Windows::Foundation::Point{};
                        s->handles[0].Focus(FocusState::Pointer);
                        move(e, !onHandle);
                        e.Handled(true);
                    }
                }
                catch (...)
                {
                }
            })),
        true);
    board.AddHandler(UIElement::PointerMovedEvent(),
                     box_value(Input::PointerEventHandler(
                         [weak, move](auto &&, const Input::PointerRoutedEventArgs &e) noexcept {
                             try
                             {
                                 if (auto s = weak.lock(); s && s->dragging)
                                     move(e, false);
                             }
                             catch (...)
                             {
                             }
                         })),
                     true);
    board.AddHandler(UIElement::PointerReleasedEvent(),
                     box_value(Input::PointerEventHandler([weak](auto &&, auto &&) noexcept {
                         try
                         {
                             if (auto s = weak.lock())
                             {
                                 s->dragging = false;
                                 change_theme(appearance_values().theme);
                                 s->board.ReleasePointerCaptures();
                                 s->handles[0].ReleasePointerCaptures();
                             }
                         }
                         catch (...)
                         {
                         }
                     })),
                     true);
    board.PointerCaptureLost([weak](auto &&, auto &&) noexcept {
        if (auto s = weak.lock(); s && s->dragging)
        {
            s->dragging = false;
            try
            {
                change_theme(appearance_values().theme);
            }
            catch (...)
            {
            }
        }
    });
}
void AppearanceEditor::Impl::render(bool animate)
{
    auto m = appearance_values().theme;
    if (!dragging)
    {
        surface.RequestedTheme(m.mode == ThemeMode::Auto    ? ElementTheme::Default
                               : m.mode == ThemeMode::Light ? ElementTheme::Light
                                                            : ElementTheme::Dark);
        appearance(surface.ActualTheme() == ElementTheme::Light);
    }
    auto colors = theme_dots(m);
    std::array<std::pair<double, double>, 3> previous;
    for (int i = 0; i < 3; ++i)
        previous[i] = {positions[i].X(), positions[i].Y()};
    if (animate)
        animate = winrt::Windows::UI::ViewManagement::UISettings{}.AnimationsEnabled();
    if (motion)
    {
        motion.Stop();
        motion = nullptr;
    }
    if (animate)
        motion = Animation::Storyboard{};
    for (int i = 0; i < 3; ++i)
    {
        auto b = handles[i];
        auto visibility = i < m.count ? Visibility::Visible : Visibility::Collapsed;
        if (b.Visibility() != visibility)
            b.Visibility(visibility);
        if (i >= m.count)
            continue;
        double size = i == 0 ? 50 : 24;
        double x = 180 + colors[i].x * 180 - size / 2, y = 180 + colors[i].y * 180 - size / 2;
        auto [oldX, oldY] = previous[i];
        if (b.Width() != size)
        {
            b.Width(size);
            b.Height(size);
            dots[i].Width(size);
            dots[i].Height(size);
        }
        dots[i].StrokeThickness(i == 0 ? 5 : 2.5);
        dots[i].Fill(brush(colors[i].color));
        positions[i].X(x);
        positions[i].Y(y);
        if (animate && std::isfinite(oldX) && std::isfinite(oldY))
        {
            for (int k = 0; k < 2; ++k)
            {
                Animation::DoubleAnimation a;
                a.From(k ? oldY : oldX);
                a.To(k ? y : x);
                a.Duration(Duration{std::chrono::milliseconds(400), DurationType::TimeSpan});
                Animation::BackEase ease;
                ease.Amplitude(.3);
                ease.EasingMode(Animation::EasingMode::EaseOut);
                a.EasingFunction(ease);
                Animation::Storyboard::SetTarget(a, positions[i]);
                Animation::Storyboard::SetTargetProperty(a, k ? L"Y" : L"X");
                motion.Children().Append(a);
            }
        }
    }
    if (!dragging)
        for (int i = 0; i < 3; ++i)
            modes[i].Background(
                brush(i == static_cast<int>(m.mode) ? (panelAppearance ? 0xd4d6d6 : 0x383a3a) : 0,
                      i == static_cast<int>(m.mode) ? 255 : 0));
    plus.IsEnabled(m.count < 3);
    minus.IsEnabled(m.count > 0);
    harmony.IsEnabled(m.count > 1);
    if (!dragging)
        harmony.Background(brush(panelAppearance ? 0xdfe1e1 : 0x333535, m.count > 1 ? 255 : 0));
    if (animate && motion.Children().Size())
        motion.Begin();
}
void AppearanceEditor::Impl::appearance(bool light)
{
    if (panelAppearance == int(light))
        return;
    panelAppearance = int(light);
    frame.Background(brush(light ? 0xfafafa : 0x181a1c));
    board.Background(canvas_background(light));
    for (auto button : {modes[0], modes[1], modes[2], plus, minus, harmony})
    {
        auto path = button.Content().as<Viewbox>().Child().as<Shapes::Path>();
        path.Stroke(brush(light ? 0x303333 : 0xebeded));
        if (path.Fill())
            path.Fill(brush(light ? 0x303333 : 0xebeded));
    }
}
void AppearanceEditor::Impl::choose_exact()
{
    if (exact)
    {
        auto colors = theme_dots(appearance_values().theme);
        uint32_t c = colors.empty() ? 0xffffff : colors[0].color;
        exactUpdating = true;
        exactPicker.Color(
            winrt::Windows::UI::Color{255, uint8_t(c >> 16), uint8_t(c >> 8), uint8_t(c)});
        exactUpdating = false;
        exact.ShowAt(handles[0]);
        return;
    }
    ColorPicker picker;
    exactPicker = picker;
    picker.IsAlphaEnabled(false);
    picker.IsMoreButtonVisible(false);
    picker.IsHexInputVisible(true);
    auto colors = theme_dots(appearance_values().theme);
    uint32_t c = colors.empty() ? 0xffffff : colors[0].color;
    picker.Color(winrt::Windows::UI::Color{255, uint8_t(c >> 16), uint8_t(c >> 8), uint8_t(c)});
    auto weak = weak_from_this();
    exactTimer = board.DispatcherQueue().CreateTimer();
    exactTimer.Interval(std::chrono::milliseconds(33));
    exactTimer.Tick([weak](auto &&, auto &&) noexcept {
        try
        {
            if (auto s = weak.lock())
            {
                if (s->exactDirty)
                    s->flush_exact(false);
                else if (s->exactPreview && GetTickCount64() - s->exactChangedAt >= 160)
                    s->flush_exact(true);
            }
        }
        catch (...)
        {
        }
    });
    picker.ColorChanged([weak](auto &&, const ColorChangedEventArgs &e) noexcept {
        try
        {
            if (auto s = weak.lock())
            {
                if (s->exactUpdating)
                    return;
                auto c = e.NewColor();
                auto old = appearance_values().theme;
                auto m = theme_from_rgb((uint32_t(c.R) << 16) | (uint32_t(c.G) << 8) | c.B);
                m.count = std::max(1, old.count);
                m.harmony = old.harmony;
                m.mode = old.mode;
                s->exactPending = m;
                s->exactDirty = true;
                s->exactChangedAt = GetTickCount64();
                if (!s->exactTimer.IsRunning())
                    s->exactTimer.Start();
            }
        }
        catch (...)
        {
        }
    });
    exact = Flyout{};
    exact.Content(picker);
    exact.Closed([weak](auto &&, auto &&) noexcept {
        try
        {
            if (auto s = weak.lock())
                s->flush_exact(true);
        }
        catch (...)
        {
        }
    });
    exact.ShowAt(handles[0]);
}
void AppearanceEditor::Impl::flush_exact(bool persist)
{
    if (!exactDirty && !(persist && exactPreview))
        return;
    change_theme(exactPending, persist);
    exactDirty = false;
    exactPreview = !persist;
    activate();
    render(false);
    if (persist && exactTimer)
        exactTimer.Stop();
}
} // namespace dt
