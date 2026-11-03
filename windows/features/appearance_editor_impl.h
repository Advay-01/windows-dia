#pragma once
#include "appearance.h"
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Xaml.Media.Animation.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>
namespace dt
{
struct AppearanceEditor::Impl : std::enable_shared_from_this<AppearanceEditor::Impl>
{
    using Canvas = winrt::Microsoft::UI::Xaml::Controls::Canvas;
    using Button = winrt::Microsoft::UI::Xaml::Controls::Button;
    using Ellipse = winrt::Microsoft::UI::Xaml::Shapes::Ellipse;
    FeatureChanged changed;
    Canvas board;
    winrt::Microsoft::UI::Xaml::Controls::StackPanel surface;
    winrt::Microsoft::UI::Xaml::Controls::Border frame;
    int panelAppearance = -1;
    std::array<Button, 3> handles;
    std::array<Ellipse, 3> dots;
    std::array<winrt::Microsoft::UI::Xaml::Media::TranslateTransform, 3> positions;
    winrt::Windows::Foundation::Point grabOffset{};
    std::array<Button, 3> modes;
    Button plus, minus, harmony;
    winrt::Microsoft::UI::Xaml::Controls::ScrollViewer pages;
    winrt::Microsoft::UI::Xaml::Controls::Flyout exact{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::ColorPicker exactPicker{nullptr};
    winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer exactTimer{nullptr};
    ThemeModel exactPending;
    bool exactDirty = false, exactPreview = false, exactUpdating = false;
    ULONGLONG exactChangedAt = 0;
    winrt::Microsoft::UI::Xaml::Media::Animation::Storyboard motion{nullptr};
    bool dragging = false;
    int page = 0;
    explicit Impl(FeatureChanged c) : changed(std::move(c)) {}
    void canvas();
    void appearance(bool light);
    void palette(const winrt::Microsoft::UI::Xaml::Controls::StackPanel &);
    void render(bool animate = false);
    void apply(ThemeModel, bool animate = false);
    void choose_exact();
    void flush_exact(bool persist);
    void activate();
};
} // namespace dt
