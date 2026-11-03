#pragma once
#include "url_glass.h"
#include "url_style.h"
#include <algorithm>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>

namespace dt
{
inline thread_local std::wstring surfaceStage;

class UrlSurface
{
    using Popup = winrt::Microsoft::UI::Xaml::Controls::Primitives::Popup;
    using Element = winrt::Microsoft::UI::Xaml::FrameworkElement;
    using Root = winrt::Microsoft::UI::Xaml::XamlRoot;

    Popup popup_{nullptr};
    Element content_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::Border tintLayer_{nullptr};
    UrlStyle style_;
    bool windowed_ = false, painted_ = false, graded_ = false;
    HRESULT desktopResult_ = S_OK;

    winrt::Windows::UI::Color tint() const
    {
        return {255, uint8_t(style_.tint >> 16), uint8_t(style_.tint >> 8), uint8_t(style_.tint)};
    }

    void apply_backdrop()
    {
        using namespace winrt::Microsoft::UI::Xaml;
        auto panel = content_.as<Controls::Border>();
        graded_ = false;
        if (!style_.blur || windowed_)
        {
            panel.Background(Media::SolidColorBrush({0, 0, 0, 0}));
            return;
        }
        const auto color = tint();
        if (IsOpen())
            if (auto frost = frosted_backdrop(panel, color, style_.luminosityOpacity))
            {
                try
                {
                    panel.Background(frost);
                    graded_ = true;
                    return;
                }
                catch (const winrt::hresult_error &)
                {
                }
            }
        Media::AcrylicBrush acrylic;
        acrylic.TintColor(color);
        acrylic.TintOpacity(0);
        acrylic.TintLuminosityOpacity(style_.luminosityOpacity);
        acrylic.FallbackColor(color);
        panel.Background(acrylic);
    }

    static Popup make_popup(bool windowed)
    {
        using namespace winrt::Microsoft::UI::Xaml;
        Popup popup;
        if (windowed)
            popup.ShouldConstrainToRootBounds(false);
        popup.IsLightDismissEnabled(true);
        popup.LightDismissOverlayMode(Controls::LightDismissOverlayMode::Off);
        return popup;
    }

    void use_windowed_popup(bool windowed)
    {
        if (windowed == windowed_)
            return;
        auto replacement = make_popup(windowed);
        replacement.XamlRoot(popup_.XamlRoot());
        replacement.HorizontalOffset(popup_.HorizontalOffset());
        replacement.VerticalOffset(popup_.VerticalOffset());
        popup_.Child(nullptr);
        try
        {
            replacement.Child(content_);
            if (windowed)
            {
                // WinUI clips its backdrop to the rounded child, including during resize.
                using winrt::Microsoft::UI::Xaml::Media::DesktopAcrylicBackdrop;
                replacement.SystemBackdrop(DesktopAcrylicBackdrop{});
            }
        }
        catch (...)
        {
            replacement.Child(nullptr);
            popup_.Child(content_);
            throw;
        }
        popup_ = replacement;
        windowed_ = windowed;
        paint_material();
    }

    void paint_material()
    {
        using namespace winrt::Microsoft::UI::Xaml::Media;
        surfaceStage = L"set search panel material";
        const double innerRadius = std::max(0., style_.radius - 1.);
        tintLayer_.CornerRadius({innerRadius, innerRadius, innerRadius, innerRadius});
        LinearGradientBrush glaze;
        auto add_stop = [&](double offset, double density, bool light) {
            auto color = tint();
            color.A = uint8_t(std::min(1., style_.tintOpacity * density) * 255 + .5);
            if (light)
            {
                color.R += uint8_t((255 - color.R) * .08);
                color.G += uint8_t((255 - color.G) * .08);
                color.B += uint8_t((255 - color.B) * .08);
            }
            GradientStop stop;
            stop.Offset(offset);
            stop.Color(color);
            glaze.GradientStops().Append(stop);
        };
        if (windowed_)
        {
            glaze.StartPoint({0, 0});
            glaze.EndPoint({0, 1});
            add_stop(0., .13, true);
            add_stop(1., .23, false);
        }
        else
        {
            glaze.StartPoint({0, .5f});
            glaze.EndPoint({1, .5f});
            add_stop(0., .85, true);
            add_stop(.04, 1.28, false);
            add_stop(.96, 1.28, false);
            add_stop(1., .85, true);
        }
        tintLayer_.Background(glaze);
        apply_backdrop();
        painted_ = true;
    }

  public:
    UrlSurface(std::nullptr_t) {}
    UrlSurface() : popup_(make_popup(false)) {}
    explicit operator bool() const { return bool(popup_); }
    void Child(const Element &content,
               const winrt::Microsoft::UI::Xaml::Controls::Border &tintLayer)
    {
        content_ = content;
        tintLayer_ = tintLayer;
        popup_.Child(content);
    }
    auto XamlRoot() const { return popup_.XamlRoot(); }
    void XamlRoot(const Root &value)
    {
        if (popup_.XamlRoot() == value)
            return;
        popup_.XamlRoot(value);
        if (!windowed_)
            painted_ = graded_ = false;
    }
    void HorizontalOffset(double x) { popup_.HorizontalOffset(x); }
    double VerticalOffset() const { return popup_.VerticalOffset(); }
    void VerticalOffset(double y) { popup_.VerticalOffset(y); }
    bool IsOpen() const { return popup_ && popup_.IsOpen(); }
    void IsOpen(bool open)
    {
        if (!open)
        {
            popup_.IsOpen(false);
            return;
        }
        desktopResult_ = S_OK;
        if (style_.blur && style_.backdrop == UrlBackdrop::System)
        {
            try
            {
                surfaceStage = L"open windowed search popup";
                use_windowed_popup(true);
                popup_.IsOpen(true);
                return;
            }
            catch (...)
            {
                desktopResult_ = winrt::to_hresult();
                popup_.IsOpen(false);
            }
        }
        use_windowed_popup(false);
        surfaceStage = L"open in-app search popup";
        popup_.IsOpen(true);
        if (style_.blur && !graded_)
            apply_backdrop();
    }
    bool bounds(HWND browser, RECT &area) const
    {
        POINT origin{};
        if (!IsOpen() || !ClientToScreen(browser, &origin) || content_.ActualHeight() <= 0)
            return false;
        auto root = popup_.XamlRoot();
        auto local = content_.TransformToVisual(root.Content()).TransformBounds(
            {0, 0, float(content_.ActualWidth()), float(content_.ActualHeight())});
        auto scale = root.RasterizationScale();
        area = {origin.x + LONG(local.X * scale), origin.y + LONG(local.Y * scale),
                origin.x + LONG((local.X + local.Width) * scale),
                origin.y + LONG((local.Y + local.Height) * scale)};
        return true;
    }
    void material(const UrlStyle &style)
    {
        const bool changed = !painted_ || style.radius != style_.radius ||
                             style.tint != style_.tint || style.tintOpacity != style_.tintOpacity ||
                             style.luminosityOpacity != style_.luminosityOpacity ||
                             style.blur != style_.blur;
        style_ = style;
        if (changed)
            paint_material();
    }
    std::wstring status() const
    {
        auto name = windowed_ ? L"WinUI desktop acrylic"
                              : graded_ ? L"graded compositor backdrop"
                              : style_.blur ? L"in-app acrylic" : L"tint";
        return std::wstring(name) + L"; visible=" + std::to_wstring(IsOpen()) +
               (FAILED(desktopResult_) ? L"; host HRESULT=" + std::to_wstring(desktopResult_)
                                       : std::wstring{});
    }
};
} // namespace dt
