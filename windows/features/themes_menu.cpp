#include "themes_menu.h"
#include "session.h"
#include "themes_panel.h"
#include "themes_selector.h"
#include <set>
#include <winrt/Windows.Foundation.Collections.h>
namespace dt
{
using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using MenuItems = winrt::Windows::Foundation::Collections::IVector<MenuFlyoutItemBase>;
static void refresh(const MenuFlyoutPresenter &presenter, const MenuItems &source)
{
    // WinUI owns this local ItemsSource. Keep its original collection and bindings.
    if (!presenter || presenter.ItemsSource() != source ||
        presenter.ReadLocalValue(ItemsControl::ItemsSourceProperty()) != source)
        return;
    presenter.ItemsSource(nullptr);
    try
    {
        presenter.ItemsSource(source);
    }
    catch (...)
    {
        if (!presenter.ItemsSource())
            presenter.ItemsSource(source);
        throw;
    }
}
struct MenuEntry
{
    MenuItems source{nullptr};
    weak_ref<MenuFlyoutPresenter> presenter;
    MenuFlyoutItem item{nullptr};
    ~MenuEntry()
    {
        try
        {
            uint32_t index = 0;
            if (source && item && source.IndexOf(item, index))
            {
                source.RemoveAt(index);
                if (auto p = presenter.get())
                    refresh(p, source);
            }
        }
        catch (...)
        {
        }
    }
};
struct ThemesMenu::Impl
{
    ComPtr<IXamlDiagnostics> diagnostics;
    FeatureChanged changed;
    std::map<uint64_t, std::unique_ptr<MenuEntry>> entries;
    std::shared_ptr<ThemesPanel> panel;
    Impl(IXamlDiagnostics *d, FeatureChanged c) : diagnostics(d), changed(std::move(c)) {}
    FrameworkElement element(const Node &n)
    {
        ComPtr<::IInspectable> raw;
        check(diagnostics->GetIInspectableFromHandle(n.handle, &raw));
        winrt::Windows::Foundation::IInspectable object{nullptr};
        copy_from_abi(object, raw.Get());
        return object.as<FrameworkElement>();
    }
    std::unique_ptr<MenuEntry> install(const MenuFlyoutPresenter &presenter,
                                       const MenuItems &source, const FrameworkElement &sidebar,
                                       uint32_t before)
    {
        auto entry = std::make_unique<MenuEntry>();
        entry->source = source;
        entry->presenter = make_weak(presenter);
        entry->item = MenuFlyoutItem{};
        entry->item.Name(L"DiaTweaksThemesItem");
        entry->item.Text(L"Themes");
        std::weak_ptr<ThemesPanel> weakPanel = panel;
        auto weakAnchor = make_weak(sidebar);
        auto weakMenu = make_weak(presenter);
        entry->item.Click([weakPanel, weakAnchor, weakMenu](auto &&, auto &&) noexcept {
            try
            {
                auto target = weakPanel.lock();
                auto anchor = weakAnchor.get();
                auto menu = weakMenu.get();
                if (target)
                    target->request_show(anchor, menu);
            }
            catch (...)
            {
            }
        });
        source.InsertAt(before, entry->item);
        refresh(presenter, source);
        return entry;
    }
};
ThemesMenu::ThemesMenu(IXamlDiagnostics *d, FeatureChanged c)
    : impl_(std::make_unique<Impl>(d, std::move(c)))
{
}
ThemesMenu::~ThemesMenu() = default;
std::wstring ThemesMenu::debug_status() const
{
    return impl_->panel ? impl_->panel->status() : L"Theme menu disconnected";
}
void ThemesMenu::remove()
{
    impl_->entries.clear();
    impl_->panel.reset();
}
void ThemesMenu::tick(const std::vector<Node> &nodes, const FeatureState &state)
{
    if (!impl_->panel)
        impl_->panel = std::make_shared<ThemesPanel>(impl_->changed);
    impl_->panel->sync(state);
    std::set<uint64_t> present;
    for (const auto &n : nodes)
    {
        if (n.type != L"Microsoft.UI.Xaml.Controls.MenuFlyoutPresenter")
            continue;
        present.insert(n.id);
        try
        {
            auto presenter = impl_->element(n).as<MenuFlyoutPresenter>();
            if (!presenter.IsLoaded())
                continue;
            auto object = presenter.ItemsSource();
            auto source = object.try_as<MenuItems>();
            if (!source || source.Size() > 64 ||
                presenter.ReadLocalValue(ItemsControl::ItemsSourceProperty()) != object)
                continue;
            auto &entry = impl_->entries[n.id];
            uint32_t existing = 0;
            if (entry && entry->source == source && source.IndexOf(entry->item, existing))
                continue;
            entry.reset();
            std::vector<SidebarMenuRole> roles;
            uint32_t before = source.Size();
            for (uint32_t i = 0; i < source.Size(); ++i)
            {
                auto item = source.GetAt(i).try_as<MenuFlyoutItem>();
                auto role = item ? sidebar_menu_role(item.Text().c_str()) : SidebarMenuRole::Other;
                roles.push_back(role);
                if (role == SidebarMenuRole::CloseAll)
                    before = i;
            }
            if (!is_sidebar_menu(roles))
                continue;
            if (before > 0 && source.GetAt(before - 1).try_as<MenuFlyoutSeparator>())
                --before;
            for (const auto &sidebar : nodes)
                if (sidebar.name == L"SidebarBorder")
                {
                    auto anchor = impl_->element(sidebar);
                    if (anchor.IsLoaded() && anchor.XamlRoot() == presenter.XamlRoot())
                    {
                        entry = impl_->install(presenter, source, anchor, before);
                        break;
                    }
                }
        }
        catch (...)
        {
        }
    }
    for (auto it = impl_->entries.begin(); it != impl_->entries.end();)
        if (!present.contains(it->first))
            it = impl_->entries.erase(it);
        else
            ++it;
}
} // namespace dt
