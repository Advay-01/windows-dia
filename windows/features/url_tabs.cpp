#include "url_tabs.h"
#include <functional>
#include <winrt/Microsoft.UI.Xaml.Automation.Provider.h>
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Data.h>
#include <winrt/Windows.Foundation.Collections.h>
namespace dt
{
using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace Controls;
using namespace Automation::Peers;
static void walk(const DependencyObject &object,
                 const std::function<void(const FrameworkElement &)> &visit, int depth = 0)
{
    if (!object || depth > 24)
        return;
    if (auto element = object.try_as<FrameworkElement>())
        visit(element);
    for (int i = 0; i < Media::VisualTreeHelper::GetChildrenCount(object); ++i)
        walk(Media::VisualTreeHelper::GetChild(object, i), visit, depth + 1);
}
std::vector<DiaTab> dia_tabs(const FrameworkElement &root)
{
    std::vector<DiaTab> tabs;
    walk(root, [&](const FrameworkElement &scope) {
        if (scope.Name() != L"TabListPresenter")
            return;
        walk(scope, [&](const FrameworkElement &list) {
            if (list.Name() != L"RowsPanel" && list.Name() != L"DockHost")
                return;
            for (int i = 0; i < Media::VisualTreeHelper::GetChildrenCount(list); ++i)
            {
                auto canvas = Media::VisualTreeHelper::GetChild(list, i).try_as<Canvas>();
                if (!canvas)
                    continue;
                for (const auto &child : canvas.Children())
                {
                    if (tabs.size() >= 64)
                        break;
                    auto row = child.try_as<FrameworkElement>();
                    if (!row || row.ActualHeight() < 20 || row.ActualHeight() > 90)
                        continue;
                    DiaTab tab;
                    tab.element = make_weak(row);
                    walk(row, [&](const FrameworkElement &element) {
                        if (auto text = element.try_as<TextBlock>(); text && !text.Text().empty())
                        {
                            std::wstring candidate(text.Text());
                            if (candidate.size() > tab.title.size() && candidate.front() < 0xe000)
                                tab.title = candidate;
                        }
                        if (!tab.icon)
                            if (auto image = element.try_as<Image>(); image && image.Source())
                                tab.icon = image.Source();
                        try
                        {
                            auto context = element.DataContext();
                            auto properties = context.try_as<Data::ICustomPropertyProvider>();
                            if (!properties)
                                return;
                            tab.customModel = true;
                            if (tab.title.empty())
                                for (auto name : {L"Title", L"DisplayTitle"})
                                {
                                    if (auto property = properties.GetCustomProperty(name);
                                        property && property.CanRead())
                                    {
                                        auto text = unbox_value_or<hstring>(
                                            property.GetValue(context), L"");
                                        if (!text.empty())
                                        {
                                            tab.title = text;
                                            break;
                                        }
                                    }
                                }
                            if (tab.url.empty())
                                for (auto name : {L"Url", L"URL", L"Uri"})
                                {
                                    if (auto property = properties.GetCustomProperty(name);
                                        property && property.CanRead())
                                    {
                                        auto value = property.GetValue(context);
                                        auto text = unbox_value_or<hstring>(value, L"");
                                        if (auto uri =
                                                value.try_as<winrt::Windows::Foundation::Uri>())
                                            text = uri.AbsoluteUri();
                                        if (std::wstring_view(text).starts_with(L"https://") ||
                                            std::wstring_view(text).starts_with(L"http://"))
                                        {
                                            tab.url = text;
                                            break;
                                        }
                                    }
                                }
                            if (!tab.command)
                                for (auto name :
                                     {L"SelectCommand", L"ActivateCommand", L"SelectTabCommand"})
                                {
                                    if (auto property = properties.GetCustomProperty(name);
                                        property && property.CanRead())
                                    {
                                        if (auto command = property.GetValue(context)
                                                               .try_as<Input::ICommand>())
                                        {
                                            tab.command = command;
                                            tab.pattern = 3;
                                            break;
                                        }
                                    }
                                }
                        }
                        catch (...)
                        {
                        }
                    });
                    if (tab.title.empty())
                    {
                        try
                        {
                            auto peer = FrameworkElementAutomationPeer::CreatePeerForElement(row);
                            if (peer)
                                tab.title = peer.GetName();
                        }
                        catch (...)
                        {
                        }
                    }
                    if (tab.title.empty())
                        continue;
                    walk(row, [&](const FrameworkElement &element) {
                        try
                        {
                            auto peer =
                                FrameworkElementAutomationPeer::CreatePeerForElement(element);
                            auto help = Automation::AutomationProperties::GetHelpText(element);
                            if (std::wstring_view(help).starts_with(L"https://") ||
                                std::wstring_view(help).starts_with(L"http://"))
                                tab.url = help;
                            if (tab.pattern || !peer)
                                return;
                            if (peer.GetPattern(PatternInterface::SelectionItem)
                                    .try_as<Automation::Provider::ISelectionItemProvider>())
                            {
                                tab.peer = make_weak(peer);
                                tab.pattern = 1;
                            }
                            // An invoke provider must name the tab itself, never a close button.
                            else if (peer.GetName() == tab.title &&
                                     peer.GetPattern(PatternInterface::Invoke)
                                         .try_as<Automation::Provider::IInvokeProvider>())
                            {
                                tab.peer = make_weak(peer);
                                tab.pattern = 2;
                            }
                        }
                        catch (...)
                        {
                        }
                    });
                    tabs.push_back(std::move(tab));
                }
            }
        });
    });
    return tabs;
}
bool dia_tab_selected(const DiaTab &tab)
{
    try
    {
        if (auto peer = tab.peer.get())
            if (auto provider = peer.GetPattern(PatternInterface::SelectionItem)
                                    .try_as<Automation::Provider::ISelectionItemProvider>())
                return provider.IsSelected();
        if (auto row = tab.element.get())
        {
            bool selected = false;
            walk(row, [&](const FrameworkElement &e) {
                if (selected)
                    return;
                try
                {
                    auto context = e.DataContext();
                    if (auto properties = context.try_as<Data::ICustomPropertyProvider>())
                        for (auto name : {L"IsSelected", L"IsActive"})
                            if (auto p = properties.GetCustomProperty(name); p && p.CanRead())
                                selected =
                                    selected || unbox_value_or<bool>(p.GetValue(context), false);
                }
                catch (...)
                {
                }
            });
            return selected;
        }
    }
    catch (...)
    {
    }
    return false;
}
bool select_dia_tab(const DiaTab &tab)
{
    if (tab.pattern == 3 && tab.command)
    {
        if (!tab.command.CanExecute(tab.parameter))
            return false;
        tab.command.Execute(tab.parameter);
        return true;
    }
    auto peer = tab.peer.get();
    if (!peer || !peer.IsEnabled())
        return false;
    if (tab.pattern == 1)
    {
        auto provider = peer.GetPattern(PatternInterface::SelectionItem)
                            .try_as<Automation::Provider::ISelectionItemProvider>();
        if (!provider)
            return false;
        provider.Select();
        return true;
    }
    if (tab.pattern == 2)
    {
        auto provider = peer.GetPattern(PatternInterface::Invoke)
                            .try_as<Automation::Provider::IInvokeProvider>();
        if (!provider)
            return false;
        provider.Invoke();
        return true;
    }
    return false;
}
} // namespace dt
