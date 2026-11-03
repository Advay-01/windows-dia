#include "appearance.h"
#include "appearance_math.h"
#include "literal_override.h"
#include "session.h"
#include "tab_glass.h"
#include <algorithm>
#include <robuffer.h>
#include <set>
#include <winrt/Microsoft.UI.Content.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.UI.ViewManagement.h>
namespace dt
{
using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Media;
static thread_local std::vector<weak_ref<ImageBrush>> opacityBrushes;
void preview_appearance_opacity(double value) noexcept
{
    try
    {
        for (const auto &weak : opacityBrushes)
            if (auto brush = weak.get())
                brush.Opacity(std::clamp(value / 100., 0., 1.));
    }
    catch (...)
    {
    }
}
static bool same_theme(const ThemeModel &a, const ThemeModel &b)
{
    return a.x == b.x && a.y == b.y && a.lightness == b.lightness && a.hue == b.hue &&
           a.explicitLightness == b.explicitLightness && a.grayscale == b.grayscale &&
           a.count == b.count && a.harmony == b.harmony && a.mode == b.mode;
}
static ImageBrush texture_brush(const AppearanceValues &s, int width, int height, bool light,
                                std::vector<uint32_t> &gradient, bool reset, bool preview)
{
    // Keep pointer feedback responsive; restore full-resolution grain on release.
    const int limit = preview ? 128 : 4096;
    width = std::clamp(width, 1, limit);
    height = std::clamp(height, 1, limit);
    Imaging::WriteableBitmap bitmap(width, height);
    auto access = bitmap.PixelBuffer().as<::Windows::Storage::Streams::IBufferByteAccess>();
    BYTE *bytes = nullptr;
    check(access->Buffer(&bytes));
    const ThemeRender render(s.theme, light);
    reset = reset || gradient.size() != size_t(width) * height;
    gradient.resize(size_t(width) * height);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            const auto index = size_t(y) * width + x;
            if (reset)
                gradient[index] = render.pixel(double(x) / width, double(y) / height);
            const auto colorPixel = gradient[index];
            // Preview the colors without magnifying noise into coarse patches.
            const int grain = preview ? 0 : fine_grain(x, y, s.texture);
            auto offset = static_cast<size_t>(y * width + x) * 4;
            for (int c = 0; c < 3; ++c)
            {
                int color = std::clamp(int((colorPixel >> (c * 8)) & 255) + grain, 0, 255);
                bytes[offset + c] = static_cast<BYTE>(color);
            }
            bytes[offset + 3] = 255;
        }
    bitmap.Invalidate();
    ImageBrush brush;
    brush.ImageSource(bitmap);
    brush.Stretch(Stretch::Fill);
    brush.Opacity(s.opacity / 100.);
    std::erase_if(opacityBrushes, [](const auto &weak) { return !weak.get(); });
    opacityBrushes.push_back(make_weak(brush));
    return brush;
}
struct AppearanceRuntime::Impl
{
    ComPtr<IXamlDiagnostics> diagnostics;
    ComPtr<IVisualTreeService> service;
    struct Entry
    {
        std::unique_ptr<LiteralOverride<Brush>> lease;
        uint64_t revision = 0;
        int width = 0, height = 0;
        ElementTheme theme = ElementTheme::Default;
        ThemeModel model;
        int texture = -1, opacity = -1;
        bool preview = false;
        ImageBrush brush{nullptr};
        std::vector<uint32_t> gradient;
    };
    std::map<uint64_t, Entry> entries;
    std::unique_ptr<TabGlass> tabs;
    std::set<uint64_t> unsupported;
    std::set<InstanceHandle> unsupportedModes;
    std::map<InstanceHandle, std::unique_ptr<LiteralOverride<ElementTheme>>> modes;
    struct CaptionSurface
    {
        using ColorRef = winrt::Windows::Foundation::IReference<winrt::Windows::UI::Color>;
        winrt::Microsoft::UI::Windowing::AppWindowTitleBar bar{nullptr};
        ColorRef before[2]{nullptr, nullptr}, applied{nullptr};
        explicit CaptionSurface(const winrt::Microsoft::UI::Windowing::AppWindowTitleBar &value)
            : bar(value)
        {
            before[0] = bar.ButtonBackgroundColor();
            before[1] = bar.ButtonInactiveBackgroundColor();
        }
        void sync(bool light)
        {
            auto native = bar.BackgroundColor();
            const uint8_t neutral = light ? 248 : 32;
            auto value = native && native.Value().A == 255
                             ? native.Value()
                             : winrt::Windows::UI::Color{255, neutral, neutral, neutral};
            applied = box_value(value).as<ColorRef>();
            auto active = bar.ButtonBackgroundColor(),
                 inactive = bar.ButtonInactiveBackgroundColor();
            if (!active || active.Value() != value)
                bar.ButtonBackgroundColor(applied);
            if (!inactive || inactive.Value() != value)
                bar.ButtonInactiveBackgroundColor(applied);
        }
        ~CaptionSurface()
        {
            try
            {
                auto value = bar.ButtonBackgroundColor();
                if (applied && value && value.Value() == applied.Value())
                    bar.ButtonBackgroundColor(before[0]);
            }
            catch (...)
            {
            }
            try
            {
                auto value = bar.ButtonInactiveBackgroundColor();
                if (applied && value && value.Value() == applied.Value())
                    bar.ButtonInactiveBackgroundColor(before[1]);
            }
            catch (...)
            {
            }
        }
    };
    std::map<uint64_t, std::unique_ptr<CaptionSurface>> captions;
    Impl(IXamlDiagnostics *d, IVisualTreeService *s) : diagnostics(d), service(s) {}
};
AppearanceRuntime::AppearanceRuntime(IXamlDiagnostics *d, IVisualTreeService *s)
    : impl_(std::make_unique<Impl>(d, s))
{
}
AppearanceRuntime::~AppearanceRuntime() = default;
bool AppearanceRuntime::active() const
{
    return !impl_->entries.empty() || !impl_->modes.empty();
}
void AppearanceRuntime::restore()
{
    impl_->entries.clear();
    impl_->unsupported.clear();
    impl_->unsupportedModes.clear();
    impl_->modes.clear();
    if (impl_->tabs)
        impl_->tabs->restore();
    impl_->captions.clear();
}
void AppearanceRuntime::tick(const std::vector<Node> &nodes, bool on, FeatureState &state)
{
    if (!on || appearance_values().theme.count == 0)
        restore();
    on = on && appearance_values().theme.count > 0;
    state.detail = L"Custom color and grain on the sidebar and browser frame";
    std::set<uint64_t> present;
    const auto settings = appearance_values();
    std::vector<XamlRoot> browserRoots;
    std::set<InstanceHandle> frameHandles;
    for (const auto &n : nodes)
    {
        if (n.name != L"SidebarBorder" && n.name != L"SharedSurface")
            continue;
        try
        {
            ComPtr<::IInspectable> raw;
            check(impl_->diagnostics->GetIInspectableFromHandle(n.handle, &raw));
            winrt::Windows::Foundation::IInspectable object{nullptr};
            copy_from_abi(object, raw.Get());
            auto root = object.as<FrameworkElement>().XamlRoot();
            if (root)
                browserRoots.push_back(root);
            for (auto parent = Media::VisualTreeHelper::GetParent(object.as<FrameworkElement>());
                 parent; parent = Media::VisualTreeHelper::GetParent(parent))
            {
                auto presenter = parent.try_as<ContentPresenter>();
                if (!presenter || presenter.Name() != L"ClientAreaPresenter")
                    continue;
                auto frame = presenter.Content().try_as<Grid>();
                if (!frame)
                    for (int i = 0; i < Media::VisualTreeHelper::GetChildrenCount(presenter); ++i)
                        if ((frame =
                                 Media::VisualTreeHelper::GetChild(presenter, i).try_as<Grid>()))
                            break;
                if (!frame)
                    break;
                InstanceHandle handle;
                check(impl_->diagnostics->GetHandleFromIInspectable(
                    reinterpret_cast<::IInspectable *>(winrt::get_abi(frame)), &handle));
                frameHandles.insert(handle);
                break;
            }
        }
        catch (...)
        {
        }
    }
    if (on && std::any_of(nodes.begin(), nodes.end(),
                          [](const Node &n) { return n.name == L"SidebarBorder"; }))
    {
        try
        {
            if (winrt::Windows::UI::ViewManagement::AccessibilitySettings{}.HighContrast())
            {
                restore();
                state.detail = L"Custom backgrounds are disabled in system high contrast";
                return;
            }
        }
        catch (...)
        {
        }
    }
    if (!impl_->tabs)
        impl_->tabs = std::make_unique<TabGlass>(impl_->diagnostics.Get(), impl_->service.Get());
    impl_->tabs->tick(browserRoots, on);
    if (on)
        state.detail += L"; " + impl_->tabs->status();
    std::set<InstanceHandle> themedRoots;
    std::set<uint64_t> captionWindows;
    if (on)
        for (const auto &root : browserRoots)
        {
            try
            {
                auto id = root.ContentIslandEnvironment().AppWindowId();
                if (!id.Value)
                    continue;
                auto bar =
                    winrt::Microsoft::UI::Windowing::AppWindow::GetFromWindowId(id).TitleBar();
                if (!bar)
                    continue;
                captionWindows.insert(id.Value);
                auto &surface = impl_->captions[id.Value];
                if (!surface)
                    surface = std::make_unique<Impl::CaptionSurface>(bar);
                surface->sync(root.Content().as<FrameworkElement>().ActualTheme() ==
                              ElementTheme::Light);
            }
            catch (...)
            {
            }
        }
    for (auto it = impl_->captions.begin(); it != impl_->captions.end();)
        if (!captionWindows.contains(it->first))
            it = impl_->captions.erase(it);
        else
            ++it;
    if (!on || settings.theme.mode == ThemeMode::Auto)
        impl_->modes.clear();
    else
        for (const auto &root : browserRoots)
        {
            InstanceHandle handle = 0;
            try
            {
                auto content = root.Content().as<FrameworkElement>();
                check(impl_->diagnostics->GetHandleFromIInspectable(
                    reinterpret_cast<::IInspectable *>(winrt::get_abi(content)), &handle));
                themedRoots.insert(handle);
                if (impl_->unsupportedModes.contains(handle))
                    continue;
                auto &lease = impl_->modes[handle];
                if (!lease)
                    lease = std::make_unique<LiteralOverride<ElementTheme>>(
                        impl_->diagnostics.Get(), impl_->service.Get(), content,
                        FrameworkElement::RequestedThemeProperty(), L"RequestedTheme");
                const auto wanted = settings.theme.mode == ThemeMode::Light ? ElementTheme::Light
                                                                            : ElementTheme::Dark;
                if (content.RequestedTheme() != wanted)
                    lease->apply(wanted);
            }
            catch (...)
            {
                if (handle)
                    impl_->unsupportedModes.insert(handle);
                state.detail = L"Custom colors applied; Dia controls its own foreground appearance";
            }
        }
    for (auto it = impl_->modes.begin(); it != impl_->modes.end();)
        if (!themedRoots.contains(it->first))
            it = impl_->modes.erase(it);
        else
            ++it;
    for (const auto &n : nodes)
    {
        if (!frameHandles.contains(n.handle) &&
            (n.type != L"Microsoft.UI.Xaml.Controls.Border" ||
             (n.name != L"SidebarBorder" && n.name != L"SidebarFloatingSurface" &&
              n.name != L"BackgroundSurface")))
            continue;
        try
        {
            if (impl_->unsupported.contains(n.id))
                continue;
            ComPtr<::IInspectable> raw;
            check(impl_->diagnostics->GetIInspectableFromHandle(n.handle, &raw));
            winrt::Windows::Foundation::IInspectable object{nullptr};
            copy_from_abi(object, raw.Get());
            auto border = object.as<FrameworkElement>();
            if (std::find(browserRoots.begin(), browserRoots.end(), border.XamlRoot()) ==
                browserRoots.end())
                continue;
            present.insert(n.id);
            state.available = true;
            if (!on)
                continue;
            if (!border.IsLoaded() || border.ActualWidth() <= 0 || border.ActualHeight() <= 0)
                continue;
            auto &entry = impl_->entries[n.id];
            const auto scale = std::clamp(border.XamlRoot().RasterizationScale(), .5, 4.);
            int w = static_cast<int>(std::ceil(border.ActualWidth() * scale)),
                h = static_cast<int>(std::ceil(border.ActualHeight() * scale));
            if (!entry.lease)
                entry.lease = std::make_unique<LiteralOverride<Brush>>(
                    impl_->diagnostics.Get(), impl_->service.Get(), border,
                    frameHandles.contains(n.handle) ? Panel::BackgroundProperty()
                                                    : Border::BackgroundProperty(),
                    L"Background");
            if (entry.revision != settings.revision || entry.width != w || entry.height != h ||
                entry.theme != border.ActualTheme())
            {
                const bool colorChanged = !entry.brush || entry.width != w || entry.height != h ||
                                          entry.theme != border.ActualTheme() ||
                                          !same_theme(entry.model, settings.theme);
                const bool preview = settings.preview && (colorChanged || entry.preview);
                if (colorChanged || preview != entry.preview || entry.texture != settings.texture)
                {
                    auto brush = texture_brush(
                        settings, w, h, border.ActualTheme() == ElementTheme::Light, entry.gradient,
                        colorChanged || preview != entry.preview, preview);
                    entry.lease->apply(brush);
                    entry.brush = brush;
                }
                else if (entry.opacity != settings.opacity)
                    entry.brush.Opacity(settings.opacity / 100.);
                entry.revision = settings.revision;
                entry.width = w;
                entry.height = h;
                entry.theme = border.ActualTheme();
                entry.model = settings.theme;
                entry.texture = settings.texture;
                entry.opacity = settings.opacity;
                entry.preview = preview;
            }
            state.applied = true;
        }
        catch (...)
        {
            if (impl_->entries.contains(n.id))
            {
                impl_->unsupported.insert(n.id);
                impl_->entries.erase(n.id);
            }
            state.detail = L"This background has a binding or an unsupported property source";
        }
    }
    for (auto it = impl_->entries.begin(); it != impl_->entries.end();)
        if (!present.contains(it->first))
            it = impl_->entries.erase(it);
        else
            ++it;
}
} // namespace dt
