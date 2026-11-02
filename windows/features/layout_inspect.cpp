#include "native_features_impl.h"
#include "themes_selector.h"
#include <winrt/Windows.Foundation.Collections.h>
#include <sstream>
namespace dt {
using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
static void thickness(std::wostream& out, const Thickness& t) { out << t.Left << L"," << t.Top << L"," << t.Right << L"," << t.Bottom; }
static void walk(const DependencyObject& node, std::wostream& out, unsigned depth = 0) {
    if (!node || depth > 5) return;
    if (auto e = node.try_as<FrameworkElement>()) {
        out << std::wstring(depth * 2, L' ') << get_class_name(e).c_str() << L" name=" << e.Name().c_str()
            << L" size=" << e.ActualWidth() << L"x" << e.ActualHeight() << L" margin=";
        thickness(out, e.Margin());
        out << L" row=" << Grid::GetRow(e) << L" col=" << Grid::GetColumn(e)
            << L" span=" << Grid::GetRowSpan(e) << L"," << Grid::GetColumnSpan(e);
        if (auto parent = Media::VisualTreeHelper::GetParent(e).try_as<UIElement>()) {
            auto p = e.TransformToVisual(parent).TransformPoint({0, 0}); out << L" xy=" << p.X << L"," << p.Y;
        }
        if (auto grid = e.try_as<Grid>()) { out << L" padding="; thickness(out, grid.Padding());
            out << L" rows=";
            for (const auto& row : grid.RowDefinitions()) out << row.Height().Value << L":" << static_cast<int>(row.Height().GridUnitType) << L"/" << row.ActualHeight() << L";";
        }
        if (auto border = e.try_as<Border>()) { auto c = border.CornerRadius(); out << L" corner=" << c.TopLeft << L"," << c.TopRight << L"," << c.BottomRight << L"," << c.BottomLeft; }
        if (auto menu = e.try_as<MenuFlyoutPresenter>()) {
            auto source = menu.ItemsSource();
            if (source) out << L" menu-source=" << get_class_name(source).c_str();
            auto items = source.try_as<winrt::Windows::Foundation::Collections::IVector<MenuFlyoutItemBase>>();
            if (items && items.Size() <= 64) {
                std::vector<SidebarMenuRole> roles; unsigned themes = 0;
                for (auto item : items) {
                    if (item.Name() == L"DiaTweaksThemesItem") ++themes;
                    auto command = item.try_as<MenuFlyoutItem>();
                    roles.push_back(command ? sidebar_menu_role(command.Text().c_str()) : SidebarMenuRole::Other);
                }
                out << L" menu-count=" << items.Size() << L" sidebar-menu=" << is_sidebar_menu(roles) << L" themes=" << themes;
            }
        }
        // Only recognized control roles are emitted, never arbitrary text.
        if (e.try_as<Button>()) {
            auto role = Automation::AutomationProperties::GetName(e);
            if (role == L"Chats" || role == L"Recent tabs" || role == L"Search tabs" || role == L"Tab search" || role == L"Chat history") out << L" role=" << role.c_str();
        }
        out << L"\n";
    }
    for (int i = 0, count = Media::VisualTreeHelper::GetChildrenCount(node); i < count; ++i)
        walk(Media::VisualTreeHelper::GetChild(node, i), out, depth + 1);
}
std::wstring NativeFeatures::layout(InstanceHandle handle) {
    auto e = impl_->element(handle);
    std::wostringstream out;
    if (impl_->themesMenu) out << L"Theme menu: " << impl_->themesMenu->debug_status() << L"\n";
    if (!e) return L"Not a FrameworkElement.\n";
    out << L"Ancestors (nearest first):\n";
    auto parent = Media::VisualTreeHelper::GetParent(e);
    for (int i = 0; parent && i < 6; ++i) {
        if (auto p = parent.try_as<FrameworkElement>()) out << get_class_name(p).c_str() << L" name=" << p.Name().c_str() << L" size=" << p.ActualWidth() << L"x" << p.ActualHeight() << L"\n";
        parent = Media::VisualTreeHelper::GetParent(parent);
    }
    out << L"Subtree (no content text or URLs):\n"; walk(e, out); return out.str();
}
}
