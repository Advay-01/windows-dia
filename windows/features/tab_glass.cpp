#include "tab_glass.h"
#include "literal_override.h"
#include "session.h"
#include "url_tabs.h"
#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>

namespace dt
{
using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace Controls;
using namespace Media;

struct TabGlass::Impl
{
    ComPtr<IXamlDiagnostics> diagnostics;
    ComPtr<IVisualTreeService> service;

    struct Entry
    {
        IXamlDiagnostics *diagnostics;
        IVisualTreeService *service;
        weak_ref<FrameworkElement> element;
        DependencyProperty property;
        const wchar_t *name;
        std::optional<int64_t> changed;
        std::unique_ptr<LiteralOverride<Brush>> lease;
        SolidColorBrush glass;
        bool updating = false, unsupported = false;

        Entry(IXamlDiagnostics *d, IVisualTreeService *s, const FrameworkElement &target,
              const DependencyProperty &dp, const wchar_t *name,
              winrt::Windows::UI::Color color)
            : diagnostics(d), service(s), element(make_weak(target)), property(dp), name(name),
              glass(color)
        {
        }

        ~Entry()
        {
            try
            {
                if (auto target = element.get(); target && changed)
                    target.UnregisterPropertyChangedCallback(property, *changed);
            }
            catch (...)
            {
            }
        }

        void watch(const std::shared_ptr<Entry> &self, const FrameworkElement &target) noexcept
        {
            try
            {
                changed = target.RegisterPropertyChangedCallback(
                    property, [weak = std::weak_ptr<Entry>(self)](auto &&, auto &&) noexcept {
                        if (auto entry = weak.lock())
                            entry->refresh();
                    });
            }
            catch (...)
            {
                // The normal scan can still refresh this surface.
            }
        }

        void refresh() noexcept
        {
            if (updating)
                return;
            updating = true;
            unsupported = false;
            try
            {
                if (auto target = element.get())
                    update_fill(target);
            }
            catch (...)
            {
                unsupported = true;
            }
            updating = false;
        }

        void update_fill(const FrameworkElement &target)
        {
            auto background = target.GetValue(property).try_as<Brush>();
            if (background == glass)
                return;
            if (lease)
            {
                // An active animation can mask our local value temporarily.
                // Keep its base value in place so the glass returns with the state.
                if (target.ReadLocalValue(property).try_as<Brush>() == glass)
                    return;
                lease.reset();
            }
            auto solid = background.try_as<SolidColorBrush>();
            if (!solid)
                return;
            auto c = solid.Color();
            auto low = std::min({c.R, c.G, c.B}), high = std::max({c.R, c.G, c.B});
            if (c.A < 96 || high - low > 48)
                return;

            auto replacement = std::make_unique<LiteralOverride<Brush>>(
                diagnostics, service, target, property, name);
            replacement->apply(glass);
            lease = std::move(replacement);
        }
    };

    std::map<InstanceHandle, std::shared_ptr<Entry>> entries;
    size_t selected = 0, unsupported = 0;

    Impl(IXamlDiagnostics *d, IVisualTreeService *s) : diagnostics(d), service(s) {}

    void observe(const FrameworkElement &target, const DependencyProperty &property,
                 const wchar_t *name, winrt::Windows::UI::Color color,
                 std::set<InstanceHandle> &present)
    {
        try
        {
            InstanceHandle handle = 0;
            check(diagnostics->GetHandleFromIInspectable(
                reinterpret_cast<::IInspectable *>(get_abi(target)), &handle));
            auto it = entries.find(handle);
            if (it == entries.end())
            {
                auto entry = std::make_shared<Entry>(diagnostics.Get(), service.Get(), target,
                                                     property, name, color);
                entry->watch(entry, target);
                it = entries.emplace(handle, std::move(entry)).first;
            }
            present.insert(handle);
            auto &entry = *it->second;
            if (entry.glass.Color() != color)
                entry.glass.Color(color);
            entry.refresh();
        }
        catch (...)
        {
            ++unsupported;
        }
    }

    void surface(const FrameworkElement &row, const DependencyObject &object,
                 winrt::Windows::UI::Color color, std::set<InstanceHandle> &present,
                 int depth = 0)
    {
        if (depth > 8)
            return;
        if (auto e = object.try_as<FrameworkElement>())
        {
            DependencyProperty property{nullptr};
            const wchar_t *name = L"Background";
            double width = e.ActualWidth(), height = e.ActualHeight();
            if (e.try_as<Border>())
                property = Border::BackgroundProperty();
            else if (e.try_as<Panel>())
                property = Panel::BackgroundProperty();
            else if (auto shape = e.try_as<Shapes::Shape>())
            {
                property = Shapes::Shape::FillProperty();
                name = L"Fill";
                // Dia's tab pills are paths on a Canvas, with zero layout size.
                if (auto path = e.try_as<Shapes::Path>(); path && path.Data())
                {
                    auto bounds = path.Data().Bounds();
                    width = std::max(width, double(bounds.Width));
                    height = std::max(height, double(bounds.Height));
                }
            }
            // Hover changes visibility and opacity independently of selection.
            // Observe both pills for as long as the row remains selected.
            if (property && width >= row.ActualWidth() * .8 && height >= row.ActualHeight() * .65 &&
                height <= row.ActualHeight() * 1.25)
                observe(e, property, name, color, present);
        }
        for (int i = 0; i < VisualTreeHelper::GetChildrenCount(object); ++i)
            surface(row, VisualTreeHelper::GetChild(object, i), color, present, depth + 1);
    }
};

TabGlass::TabGlass(IXamlDiagnostics *d, IVisualTreeService *s) : impl_(std::make_unique<Impl>(d, s))
{
}
TabGlass::~TabGlass() = default;
void TabGlass::restore()
{
    impl_->entries.clear();
    impl_->selected = 0;
    impl_->unsupported = 0;
}
void TabGlass::tick(const std::vector<XamlRoot> &roots, bool enabled)
{
    if (!enabled)
    {
        restore();
        return;
    }
    std::set<InstanceHandle> present;
    std::vector<XamlRoot> seen;
    impl_->selected = 0;
    impl_->unsupported = 0;
    for (auto root : roots)
    {
        if (std::find(seen.begin(), seen.end(), root) != seen.end())
            continue;
        seen.push_back(root);
        try
        {
            for (auto &tab : dia_tabs(root.Content().as<FrameworkElement>()))
            {
                if (!dia_tab_selected(tab))
                    continue;
                if (auto row = tab.element.get())
                {
                    ++impl_->selected;
                    const auto color = row.ActualTheme() == ElementTheme::Light
                                           ? winrt::Windows::UI::Color{20, 0, 0, 0}
                                           : winrt::Windows::UI::Color{32, 255, 255, 255};
                    impl_->surface(row, row, color, present);
                }
            }
        }
        catch (...)
        {
        }
    }
    std::erase_if(impl_->entries, [&](const auto &entry) { return !present.contains(entry.first); });
}
std::wstring TabGlass::status() const
{
    const auto patched = std::count_if(impl_->entries.begin(), impl_->entries.end(),
                                       [](const auto &entry) { return bool(entry.second->lease); });
    const auto unsupported = std::count_if(impl_->entries.begin(), impl_->entries.end(),
                                           [](const auto &entry) { return entry.second->unsupported; });
    const auto watched = std::count_if(impl_->entries.begin(), impl_->entries.end(),
                                       [](const auto &entry) { return entry.second->changed.has_value(); });
    return L"tab glass: selected=" + std::to_wstring(impl_->selected) + L" surfaces=" +
           std::to_wstring(patched) + L" watched=" + std::to_wstring(watched) +
           L" unsupported=" + std::to_wstring(impl_->unsupported + unsupported);
}
} // namespace dt
