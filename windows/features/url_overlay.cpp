#include "url_overlay.h"
#include "session.h"
#include "url_contrast.h"
#include "url_icons.h"
#include "url_logic.h"
#include "url_sites.h"
#include "url_style.h"
#include "url_surface.h"
#include "url_tabs.h"
#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <sstream>
#include <winrt/Microsoft.UI.Content.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.ViewManagement.h>
namespace dt
{
using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Controls::Primitives;
using namespace winrt::Microsoft::UI::Xaml::Media;
static SolidColorBrush color(uint32_t rgb, uint8_t alpha = 255)
{
    return SolidColorBrush(
        winrt::Windows::UI::Color{alpha, uint8_t(rgb >> 16), uint8_t(rgb >> 8), uint8_t(rgb)});
}
static uint8_t opacity(double value)
{
    return uint8_t(std::clamp(value, 0., 1.) * 255 + .5);
}
static std::wstring lower(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](wchar_t c) { return wchar_t(towlower(c)); });
    return value;
}
// Subsequence matching supports short fragments without sending queries anywhere.
static int match_score(const std::wstring &text, const std::wstring &query)
{
    auto value = lower(text), needle = lower(query);
    if (value.starts_with(needle))
        return 300;
    if (value.find(needle) != std::wstring::npos)
        return 200;
    size_t next = 0;
    for (auto c : value)
        if (next < needle.size() && c == needle[next])
            ++next;
    return next == needle.size() ? 100 : 0;
}
struct UrlOverlay::Impl : std::enable_shared_from_this<Impl>
{
    ComPtr<IXamlDiagnostics> diagnostics;
    HWND dispatcher;
    weak_ref<Grid> root;
    PageContrast contrast, inputContrast;
    bool contrastReading = false;
    ULONGLONG nextContrast = 0;
    uint64_t contrastGeneration = 0;
    uint32_t foregroundRgb = 0;
    SolidColorBrush primary{nullptr}, secondary{nullptr}, iconForeground{nullptr}, edge{nullptr};
    SolidColorBrush inputText{nullptr}, inputPlaceholder{nullptr};
    UrlSurface popup{nullptr};
    Border panel{nullptr};
    TextBox input{nullptr};
    weak_ref<ScrollViewer> inputContent;
    StackPanel results{nullptr};
    StackPanel list{nullptr};
    ScrollViewer scroll{nullptr};
    Border selectionMarker{nullptr};
    Animation::Storyboard selectionMotion{nullptr};
    TextBlock error{nullptr};
    static void set_brush(SolidColorBrush &brush, uint32_t rgb, uint8_t alpha)
    {
        const winrt::Windows::UI::Color value{alpha, uint8_t(rgb >> 16), uint8_t(rgb >> 8),
                                               uint8_t(rgb)};
        if (brush)
            brush.Color(value);
        else
            brush = SolidColorBrush(value);
    }
    void set_foreground(bool darkText)
    {
        foregroundRgb = darkText ? 0 : 0xffffff;
        set_brush(primary, foregroundRgb, 255);
        set_brush(secondary, foregroundRgb, opacity(std::max(.55, style.secondaryOpacity)));
        if (!iconForeground)
            iconForeground = color(0x8a8e92);
        // Foreground only: theme changes also restyle acrylic, hover fills and
        // native input surfaces, so never change RequestedTheme here.
        if (error)
            error.Foreground(color(darkText ? 0x8b1e1e : 0xffb4a8));
    }
    void set_input_foreground(bool darkText)
    {
        const uint32_t rgb = darkText ? 0 : 0xffffff;
        set_brush(inputPlaceholder, rgb, opacity(std::max(.65, style.secondaryOpacity)));
        if (inputText && inputText.Color().R == uint8_t(rgb))
            return;
        // RichEdit caches the glyph color. A new foreground value invalidates
        // inherited text formatting, whereas mutating the brush can leave it cached.
        inputText = color(rgb);
        if (input)
            apply_input_text_brush();
    }
    static winrt::fire_and_forget read_contrast(
        std::weak_ptr<Impl> weak, winrt::Microsoft::UI::Dispatching::DispatcherQueue queue,
        HWND browser, RECT surface, RECT header, LONG headerInset, uint64_t generation)
    {
        try
        {
            co_await winrt::resume_background();
            GlassLuminance luminance;
            try
            {
                luminance = sample_glass_luminance(browser, surface, header, headerInset);
            }
            catch (...)
            {
            }
            queue.TryEnqueue([weak, luminance, generation]() noexcept {
                try
                {
                    if (auto s = weak.lock(); s && !s->disposed)
                    {
                        s->contrastReading = false;
                        if (generation != s->contrastGeneration || !s->popup || !s->popup.IsOpen())
                            return;
                        if (s->contrast.update(luminance.panel))
                            s->set_foreground(*s->contrast.light);
                        if (s->inputContrast.update(luminance.input))
                            s->set_input_foreground(*s->inputContrast.light);
                    }
                }
                catch (...)
                {
                }
            });
        }
        catch (...)
        {
        }
    }
    void poll_contrast()
    {
        if (disposed || contrastReading || GetTickCount64() < nextContrast || !popup ||
            !popup.IsOpen() || closing)
            return;
        nextContrast = GetTickCount64() + 350;
        try
        {
            const auto browser = GetAncestor(GetForegroundWindow(), GA_ROOTOWNER);
            DWORD process = 0;
            GetWindowThreadProcessId(browser, &process);
            RECT area{};
            if (process != GetCurrentProcessId() || !popup.bounds(browser, area) ||
                panel.ActualHeight() <= 0 || panel.ActualWidth() <= 0)
                return;
            RECT header = area;
            const double headerFraction =
                std::min(1., url_style::HeaderHeight / panel.ActualHeight());
            header.bottom = area.top + LONG(std::lround((area.bottom - area.top) * headerFraction));
            const auto headerInset =
                std::max(1L, LONG(std::lround(14. * (area.right - area.left) / panel.ActualWidth())));
            contrastReading = true;
            read_contrast(weak_from_this(), panel.DispatcherQueue(), browser, area, header,
                          headerInset, contrastGeneration);
        }
        catch (...)
        {
        }
    }
    struct Result
    {
        std::wstring title, subtitle, label, url, section;
        ImageSource icon{nullptr};
        std::vector<uint8_t> iconBytes;
        int tab = -1;
    };
    std::vector<Result> actions;
    std::vector<DiaTab> tabs;
    struct Recent
    {
        weak_ref<FrameworkElement> element;
        uint64_t order;
    };
    std::vector<Recent> recent;
    weak_ref<FrameworkElement> lastActive;
    uint64_t recentOrder = 0;
    ULONGLONG nextTabCheck = 0;
    void remember_tab(const DiaTab &tab)
    {
        auto element = tab.element.get();
        if (!element || element == lastActive.get())
            return;
        lastActive = make_weak(element);
        std::erase_if(recent, [](const Recent &r) { return !r.element.get(); });
        for (auto &r : recent)
            if (r.element.get() == element)
            {
                r.order = ++recentOrder;
                return;
            }
        recent.push_back({make_weak(element), ++recentOrder});
    }
    void track_tabs()
    {
        if (GetTickCount64() < nextTabCheck)
            return;
        nextTabCheck = GetTickCount64() + 500;
        for (auto &tab : tabs)
            if (dia_tab_selected(tab))
            {
                remember_tab(tab);
                break;
            }
    }
    void refresh_tabs(const FrameworkElement &browser)
    {
        tabs = dia_tabs(browser);
        nextTabCheck = 0;
        track_tabs();
        auto rank = [&](const DiaTab &tab) {
            for (auto &r : recent)
                if (r.element.get() == tab.element.get())
                    return r.order;
            return uint64_t(0);
        };
        std::stable_sort(tabs.begin(), tabs.end(),
                         [&](const DiaTab &a, const DiaTab &b) { return rank(a) > rank(b); });
    }
    SiteHistory history;
    bool loadingHistory = false, disposed = false;
    ULONGLONG lastHistory = 0, nextStyle = 0;
    UrlStyle style;
    std::filesystem::file_time_type styleTime{};
    bool styleLoaded = false;
    std::vector<Button> rows;
    std::vector<Border> arrows;
    std::vector<Animation::Storyboard> arrowMotion;
    std::vector<bool> arrowVisible;
    int hoveredRow = -1;
    bool keyboardSelection = false;
    void update_arrows()
    {
        for (size_t i = 0; i < arrows.size(); ++i)
        {
            bool show = hoveredRow == int(i) || (keyboardSelection && i == selected);
            if (show == arrowVisible[i])
                continue;
            arrowVisible[i] = show;
            auto arrow = arrows[i];
            auto transform = arrow.RenderTransform().as<TranslateTransform>();
            auto fromOpacity = arrow.Opacity(), fromX = transform.X();
            if (arrowMotion[i])
                arrowMotion[i].Stop();
            arrow.Opacity(show ? 1 : 0);
            transform.X(show ? 0 : 5);
            if (!winrt::Windows::UI::ViewManagement::UISettings{}.AnimationsEnabled())
                continue;
            Animation::Storyboard revealMotion;
            for (int property = 0; property < 2; ++property)
            {
                Animation::DoubleAnimation a;
                a.From(property ? fromX : fromOpacity);
                a.To(property ? (show ? 0. : 5.) : (show ? 1. : 0.));
                a.Duration(
                    Duration{std::chrono::milliseconds(show ? 140 : 100), DurationType::TimeSpan});
                Animation::CubicEase ease;
                ease.EasingMode(Animation::EasingMode::EaseOut);
                a.EasingFunction(ease);
                Animation::Storyboard::SetTarget(a, property ? transform.as<DependencyObject>()
                                                             : arrow.as<DependencyObject>());
                Animation::Storyboard::SetTargetProperty(a, property ? L"X" : L"Opacity");
                revealMotion.Children().Append(a);
            }
            arrowMotion[i] = revealMotion;
            revealMotion.Begin();
        }
    }
    size_t selected = 0;
    bool registered = false, closing = false, navigating = false;
    std::wstring shortcut, message = L"Waiting for Dia browser root";
    winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer closeTimer{nullptr};
    Animation::Storyboard motion{nullptr};
    Impl(IXamlDiagnostics *d, HWND w) : diagnostics(d), dispatcher(w) {}
    ~Impl()
    {
        shutdown();
    }
    void shutdown() noexcept
    {
        disposed = true;
        if (registered)
            UnregisterHotKey(dispatcher, HotkeyId);
        registered = false;
        try
        {
            if (popup)
                popup.IsOpen(false);
            if (closeTimer)
                closeTimer.Stop();
        }
        catch (...)
        {
        }
    }
    void apply_style()
    {
        if (!panel)
            return;
        panel.CornerRadius({style.radius, style.radius, style.radius, style.radius});
        set_foreground(contrast.light.value_or(foregroundRgb == 0));
        set_input_foreground(inputContrast.light.value_or(foregroundRgb == 0));
        if (!edge)
            edge = color(0xffffff, opacity(style.borderOpacity));
        else
            edge.Color(color(0xffffff, opacity(style.borderOpacity)).Color());
        panel.BorderBrush(edge);
        popup.material(style);
        if (auto xaml = popup.XamlRoot())
        {
            panel.Width(std::max(240., std::min(style.width, xaml.Size().Width - 48.)));
            popup.HorizontalOffset((xaml.Size().Width - panel.Width()) / 2);
        }
    }
    void refresh_style()
    {
        if (GetTickCount64() < nextStyle)
            return;
        nextStyle = GetTickCount64() + 500;
        try
        {
            auto path = url_style_path();
            std::error_code ec;
            auto stamp = std::filesystem::last_write_time(path, ec);
            if (styleLoaded && !ec && stamp == styleTime)
                return;
            style = read_url_style(path);
            styleTime = std::filesystem::last_write_time(path, ec);
            styleLoaded = true;
            if (panel)
            {
                apply_style();
                render(true);
            }
        }
        catch (...)
        {
        }
    }
    static winrt::fire_and_forget read_history(
        std::weak_ptr<Impl> weak, winrt::Microsoft::UI::Dispatching::DispatcherQueue queue)
    {
        try
        {
            SiteHistory data;
            co_await winrt::resume_background();
            try
            {
                data = dia_top_sites(dia_history_profile());
            }
            catch (...)
            {
                data.notice = L"Dia history is temporarily unavailable";
            }
            queue.TryEnqueue([weak, data = std::move(data)]() mutable noexcept {
                try
                {
                    if (auto s = weak.lock(); s && !s->disposed)
                    {
                        s->history = std::move(data);
                        s->loadingHistory = false;
                        s->lastHistory = GetTickCount64();
                        if (s->popup && s->popup.IsOpen())
                            s->render(true);
                    }
                }
                catch (...)
                {
                }
            });
        }
        catch (...)
        {
        } // The UI dispatcher may disappear while Dia is quitting.
    }
    void refresh_history()
    {
        if (loadingHistory || (lastHistory && GetTickCount64() - lastHistory < 60000))
            return;
        loadingHistory = true;
        read_history(weak_from_this(), panel.DispatcherQueue());
    }
    static winrt::fire_and_forget decode_icon(Image image, FontIcon fallback,
                                              std::vector<uint8_t> bytes)
    {
        try
        {
            using namespace winrt::Windows::Storage::Streams;
            InMemoryRandomAccessStream stream;
            DataWriter writer(stream);
            writer.WriteBytes(bytes);
            co_await writer.StoreAsync();
            writer.DetachStream();
            stream.Seek(0);
            Imaging::BitmapImage bitmap;
            bitmap.DecodePixelWidth(32);
            co_await bitmap.SetSourceAsync(stream);
            image.Source(bitmap);
            fallback.Visibility(winrt::Microsoft::UI::Xaml::Visibility::Collapsed);
        }
        catch (...)
        {
        }
    }
    void select(size_t index)
    {
        if (rows.empty())
            return;
        selected = index % rows.size();
        update_arrows();
        if (selectionMarker && rows[selected].ActualHeight() > 0)
        {
            auto y = rows[selected].TransformToVisual(list).TransformPoint({0, 0}).Y;
            auto transform = selectionMarker.RenderTransform().as<TranslateTransform>();
            auto old = transform.Y();
            if (selectionMotion)
                selectionMotion.Stop();
            selectionMarker.Width(rows[selected].ActualWidth());
            selectionMarker.Background(color(style.accent, opacity(style.selectionOpacity)));
            transform.Y(y);
            if (winrt::Windows::UI::ViewManagement::UISettings{}.AnimationsEnabled() &&
                std::abs(old - y) > .5)
            {
                selectionMotion = Animation::Storyboard{};
                Animation::DoubleAnimation a;
                a.From(old);
                a.To(y);
                a.Duration(Duration{std::chrono::milliseconds(110), DurationType::TimeSpan});
                Animation::CubicEase ease;
                ease.EasingMode(Animation::EasingMode::EaseOut);
                a.EasingFunction(ease);
                Animation::Storyboard::SetTarget(a, transform);
                Animation::Storyboard::SetTargetProperty(a, L"Y");
                selectionMotion.Children().Append(a);
                selectionMotion.Begin();
            }
            selectionMarker.Opacity(1);
        }
        rows[selected].StartBringIntoView();
    }
    void style_input_parts(const DependencyObject &node)
    {
        if (auto e = node.try_as<FrameworkElement>())
        {
            if (e.Name() == L"ContentElement")
            {
                e.VerticalAlignment(VerticalAlignment::Center);
                if (auto content = e.try_as<ScrollViewer>())
                {
                    inputContent = make_weak(content);
                    content.Foreground(inputText);
                }
            }
            else if (e.Name() == L"PlaceholderTextContentPresenter")
            {
                e.VerticalAlignment(VerticalAlignment::Center);
                if (auto hint = e.try_as<TextBlock>())
                    hint.Foreground(inputPlaceholder);
            }
        }
        for (int i = 0; i < VisualTreeHelper::GetChildrenCount(node); ++i)
            style_input_parts(VisualTreeHelper::GetChild(node, i));
    }
    void apply_input_text_brush()
    {
        input.Foreground(inputText);
        if (auto content = inputContent.get())
            content.Foreground(inputText);
        try
        {
            for (auto key : {L"TextControlForeground", L"TextControlForegroundFocused",
                             L"TextControlForegroundPointerOver"})
                input.Resources().Insert(box_value(key), inputText);
        }
        catch (...)
        {
        }
        if (input.IsLoaded())
            style_input_states(input);
    }
    void style_input_states(const TextBox &edit)
    {
        if (VisualTreeHelper::GetChildrenCount(edit) == 0)
            return;
        auto templateRoot = VisualTreeHelper::GetChild(edit, 0).try_as<FrameworkElement>();
        if (!templateRoot)
            return;
        // Dia's built-in template can replace the local foreground when focused.
        // Change the foreground on this input's visual states before they run.
        auto brush_for = [&](std::wstring_view target) -> SolidColorBrush {
            if (target == L"ContentElement")
                return inputText;
            if (target == L"PlaceholderTextContentPresenter")
                return inputPlaceholder;
            return nullptr;
        };
        for (const auto &group : VisualStateManager::GetVisualStateGroups(templateRoot))
        {
            if (group.Name() != L"CommonStates")
                continue;
            auto current = group.CurrentState();
            auto currentName = current ? current.Name() : hstring{};
            const bool refresh = currentName == L"Focused" || currentName == L"PointerOver";
            if (refresh)
                VisualStateManager::GoToState(edit, L"Normal", false);
            try
            {
                for (const auto &state : group.States())
                {
                    if (state.Name() != L"Focused" && state.Name() != L"PointerOver")
                        continue;
                    if (auto storyboard = state.Storyboard())
                        for (const auto &timeline : storyboard.Children())
                            if (auto keyframes = timeline.try_as<Animation::ObjectAnimationUsingKeyFrames>();
                                keyframes && Animation::Storyboard::GetTargetProperty(timeline) == L"Foreground")
                                if (auto brush = brush_for(Animation::Storyboard::GetTargetName(timeline)))
                                    for (const auto &frame : keyframes.KeyFrames())
                                        frame.Value(brush);
                    for (const auto &base : state.Setters())
                    {
                        auto setter = base.try_as<Setter>();
                        auto target = setter ? setter.Target() : nullptr;
                        if (!target || !target.Path())
                            continue;
                        auto path = target.Path().Path();
                        std::wstring_view property = path;
                        if (property.ends_with(L".Foreground"))
                        {
                            property.remove_suffix(std::wstring_view(L".Foreground").size());
                            if (auto brush = brush_for(property))
                                setter.Value(brush);
                        }
                        else if (property == L"Foreground")
                        {
                            auto targetValue = target.Target();
                            auto element = targetValue.try_as<FrameworkElement>();
                            auto name = element ? element.Name()
                                                : unbox_value_or<hstring>(targetValue, {});
                            if (auto brush = brush_for(name))
                                setter.Value(brush);
                        }
                    }
                }
            }
            catch (...)
            {
                // Keep the direct foreground and resource overrides as fallback.
            }
            if (refresh)
                VisualStateManager::GoToState(edit, currentName, false);
            break;
        }
    }
    void animate(bool open)
    {
        if (motion)
            motion.Stop();
        panel.Opacity(open ? 1 : 0);
        if (!winrt::Windows::UI::ViewManagement::UISettings{}.AnimationsEnabled())
            return;
        motion = Animation::Storyboard{};
        auto transform = panel.RenderTransform().as<CompositeTransform>();
        for (int i = 0; i < 3; ++i)
        {
            Animation::DoubleAnimation a;
            a.From(i == 0 ? (open ? 0. : 1.) : (open ? .982 : 1.));
            a.To(i == 0 ? (open ? 1. : 0.) : (open ? 1. : .99));
            a.Duration(Duration{std::chrono::milliseconds(open ? url_style::OpenMilliseconds
                                                               : url_style::CloseMilliseconds),
                                DurationType::TimeSpan});
            Animation::CubicEase ease;
            ease.EasingMode(Animation::EasingMode::EaseOut);
            a.EasingFunction(ease);
            Animation::Storyboard::SetTarget(a, i == 0 ? panel.as<DependencyObject>()
                                                       : transform.as<DependencyObject>());
            Animation::Storyboard::SetTargetProperty(a, i == 0   ? L"Opacity"
                                                        : i == 1 ? L"ScaleX"
                                                                 : L"ScaleY");
            motion.Children().Append(a);
        }
        motion.Begin();
    }
    void hide()
    {
        if (!popup || !popup.IsOpen() || closing)
            return;
        closing = true;
        animate(false);
        if (!closeTimer)
        {
            closeTimer = panel.DispatcherQueue().CreateTimer();
            closeTimer.IsRepeating(false);
            closeTimer.Interval(std::chrono::milliseconds(125));
            auto weak = weak_from_this();
            closeTimer.Tick([weak](auto &&, auto &&) noexcept {
                try
                {
                    if (auto s = weak.lock())
                    {
                        s->popup.IsOpen(false);
                        s->closing = false;
                    }
                }
                catch (...)
                {
                }
            });
        }
        closeTimer.Start();
    }
    winrt::fire_and_forget activate()
    {
        if (closing || navigating || selected >= actions.size())
            co_return;
        auto lifetime = shared_from_this();
        navigating = true;
        try
        {
            const auto action = actions[selected];
            if (action.tab >= 0)
            {
                if (size_t(action.tab) < tabs.size() && select_dia_tab(tabs[action.tab]))
                {
                    remember_tab(tabs[action.tab]);
                    message = L"Dia tab selected";
                    hide();
                }
                else
                {
                    message = L"Dia tab selection unavailable";
                    error.Text(L"This tab no longer exposes a selection action. Reopen the popup "
                               L"to refresh it.");
                }
                navigating = false;
                co_return;
            }
            message = L"Activating Dia protocol handler";
            winrt::Windows::Foundation::Uri uri(action.url);
            winrt::Windows::System::LauncherOptions options;
            options.TargetApplicationPackageFamilyName(L"TheBrowserCompany.Dia_ttt1ap7aakyb4");
            const bool opened =
                co_await winrt::Windows::System::Launcher::LaunchUriAsync(uri, options);
            if (disposed)
                co_return;
            if (opened)
            {
                message = L"Dia protocol activation succeeded";
                hide();
            }
            else
            {
                message = L"Dia protocol activation was refused";
                error.Text(
                    L"Windows could not open this URL in Dia. Your default browser was not used.");
            }
        }
        catch (...)
        {
            std::wostringstream details;
            details << L"Dia protocol activation failed: 0x" << std::hex
                    << uint32_t(winrt::to_hresult());
            message = details.str();
            error.Text(message);
        }
        navigating = false;
    }
    void render(bool preserve = false)
    {
        surfaceStage = L"populate URL results";
        auto previous = selected;
        actions.clear();
        std::wstring query(input.Text());
        while (!query.empty() && iswspace(query.front()))
            query.erase(query.begin());
        while (!query.empty() && iswspace(query.back()))
            query.pop_back();
        const bool empty = query.empty();
        // A typed URL or search action remains first; tab matches never hijack Enter.
        for (auto &a : url_actions(query))
            actions.push_back({a.title, site_domain(a.url), a.label, a.url, L""});
        std::vector<std::pair<int, Result>> matches;
        for (size_t i = 0; !empty && i < std::min<size_t>(3, tabs.size()); ++i)
        {
            auto &tab = tabs[i];
            auto score = std::max(match_score(tab.title, query), match_score(tab.url, query));
            if (!score)
                continue;
            Result row{tab.title, site_domain(tab.url), L"Switch to Tab", tab.url, L"Matching tabs"};
            row.tab = int(i);
            row.icon = tab.icon;
            matches.emplace_back(score, std::move(row));
        }
        std::stable_sort(matches.begin(), matches.end(),
                         [](const auto &a, const auto &b) { return a.first > b.first; });
        for (size_t i = 0; i < matches.size(); ++i)
            actions.push_back(std::move(matches[i].second));
        for (auto &site : history.sites)
        {
            if (empty ||
                !std::max(match_score(site.title, query), match_score(site.domain, query)))
                continue;
            Result row{site.title, site.domain, L"Open", site.url, L"Top sites"};
            row.iconBytes = site.icon;
            actions.push_back(std::move(row));
        }
        selected = preserve && previous < actions.size() ? previous : 0;
        rows.clear();
        arrows.clear();
        for (auto &animation : arrowMotion)
            if (animation)
                animation.Stop();
        arrowMotion.clear();
        arrowVisible.clear();
        hoveredRow = -1;
        list.Children().Clear();
        if (selectionMarker)
            selectionMarker.Opacity(0);
        error.Text(L"");
        results.Visibility(empty ? winrt::Microsoft::UI::Xaml::Visibility::Collapsed
                                 : winrt::Microsoft::UI::Xaml::Visibility::Visible);
        if (empty)
            return;
        auto note = [&](const std::wstring &text, bool heading) {
            TextBlock hint;
            hint.Text(text);
            hint.FontSize(heading ? 12 : 14);
            hint.FontWeight({uint16_t(heading ? 600 : 400)});
            hint.Foreground(secondary);
            hint.Margin({24, heading ? 9. : 6., 24, heading ? 5. : 8.});
            list.Children().Append(hint);
        };
        std::wstring section;
        auto weak = weak_from_this();
        for (size_t i = 0; i < actions.size(); ++i)
        {
            if (actions[i].section != section)
            {
                section = actions[i].section;
                if (!section.empty())
                    note(section, true);
            }
            Button row;
            row.Name(L"DiaTweaksUrlResult");
            row.MinWidth(0);
            row.MinHeight(0);
            row.Resources().Insert(box_value(L"ButtonBackgroundPointerOver"), color(0xffffff, 15));
            row.Resources().Insert(box_value(L"ButtonBackgroundPressed"), color(0xffffff, 23));
            row.Height(url_style::RowHeight);
            row.HorizontalAlignment(HorizontalAlignment::Stretch);
            row.HorizontalContentAlignment(HorizontalAlignment::Stretch);
            row.Padding({16, 0, 16, 0});
            row.Margin({8, 2, 8, 0});
            row.BorderThickness({0});
            row.CornerRadius({10, 10, 10, 10});
            row.IsTabStop(false);
            BrushTransition transition;
            transition.Duration(std::chrono::milliseconds(
                winrt::Windows::UI::ViewManagement::UISettings{}.AnimationsEnabled() ? 110 : 0));
            row.Background(color(0, 0));
            Grid content;
            for (auto width :
                 {GridLength{24, GridUnitType::Pixel}, GridLength{1, GridUnitType::Star},
                  GridLength{1, GridUnitType::Auto}})
            {
                ColumnDefinition col;
                col.Width(width);
                content.ColumnDefinitions().Append(col);
            }
            Grid iconHost;
            iconHost.Width(20);
            iconHost.Height(20);
            iconHost.HorizontalAlignment(HorizontalAlignment::Left);
            iconHost.VerticalAlignment(VerticalAlignment::Center);
            FontIcon fallback;
            fallback.FontFamily(FontFamily(L"Segoe Fluent Icons"));
            fallback.Glyph(actions[i].label == L"Search in Dia" ? L"\uE721" : L"\uE774");
            fallback.FontSize(18);
            fallback.Foreground(iconForeground);
            iconHost.Children().Append(fallback);
            if (actions[i].icon || !actions[i].iconBytes.empty())
            {
                Image icon;
                icon.Width(20);
                icon.Height(20);
                icon.Stretch(Stretch::Uniform);
                if (actions[i].icon)
                    icon.Source(actions[i].icon);
                else
                    decode_icon(icon, fallback, actions[i].iconBytes);
                iconHost.Children().Append(icon);
                // Keep the fallback only until a cached bitmap is available.
                icon.ImageOpened([fallback](auto &&, auto &&) {
                    fallback.Visibility(winrt::Microsoft::UI::Xaml::Visibility::Collapsed);
                });
                if (actions[i].icon)
                    fallback.Visibility(winrt::Microsoft::UI::Xaml::Visibility::Collapsed);
            }
            content.Children().Append(iconHost);
            StackPanel text;
            text.VerticalAlignment(VerticalAlignment::Center);
            text.Margin({12, 0, 16, 0});
            TextBlock title;
            title.Text(actions[i].title);
            title.FontSize(14);
            title.FontWeight({550});
            title.Foreground(primary);
            title.TextTrimming(TextTrimming::CharacterEllipsis);
            title.VerticalAlignment(VerticalAlignment::Center);
            text.Children().Append(title);
            if (!actions[i].subtitle.empty())
            {
                TextBlock domain;
                domain.Text(actions[i].subtitle);
                domain.FontSize(12);
                domain.Foreground(secondary);
                domain.TextTrimming(TextTrimming::CharacterEllipsis);
                domain.Margin({0, 2, 0, 0});
                text.Children().Append(domain);
            }
            Grid::SetColumn(text, 1);
            content.Children().Append(text);
            StackPanel action;
            action.Orientation(Orientation::Horizontal);
            action.Spacing(8);
            action.VerticalAlignment(VerticalAlignment::Center);
            TextBlock label;
            label.Text(actions[i].tab >= 0                    ? L"Switch"
                       : actions[i].label == L"Search in Dia" ? L"Search"
                                                              : L"Open");
            label.FontSize(12);
            label.Foreground(secondary);
            label.VerticalAlignment(VerticalAlignment::Center);
            action.Children().Append(label);
            Border arrow;
            arrow.Height(26);
            arrow.BackgroundTransition(transition);
            arrow.Opacity(0);
            arrow.IsHitTestVisible(false);
            TranslateTransform reveal;
            reveal.X(5);
            arrow.RenderTransform(reveal);
            auto glyph = url_vector_icon(iconForeground, false);
            glyph.HorizontalAlignment(HorizontalAlignment::Center);
            glyph.VerticalAlignment(VerticalAlignment::Center);
            action.Children().Append(glyph);
            arrow.Child(action);
            Grid::SetColumn(arrow, 2);
            content.Children().Append(arrow);
            row.Content(content);
            row.Loaded([weak, i](auto &&, auto &&) noexcept {
                try
                {
                    if (auto s = weak.lock(); s && s->selected == i)
                        s->select(i);
                }
                catch (...)
                {
                }
            });
            row.Click([weak, i](auto &&, auto &&) noexcept {
                try
                {
                    if (auto s = weak.lock())
                    {
                        s->select(i);
                        s->activate();
                    }
                }
                catch (...)
                {
                }
            });
            row.PointerEntered([weak, i](auto &&, auto &&) noexcept {
                try
                {
                    if (auto s = weak.lock())
                    {
                        s->hoveredRow = int(i);
                        s->keyboardSelection = false;
                        s->select(i);
                    }
                }
                catch (...)
                {
                }
            });
            row.PointerExited([weak, i](auto &&, auto &&) noexcept {
                try
                {
                    if (auto s = weak.lock(); s && s->hoveredRow == int(i))
                    {
                        s->hoveredRow = -1;
                        s->update_arrows();
                    }
                }
                catch (...)
                {
                }
            });
            list.Children().Append(row);
            rows.push_back(row);
            arrows.push_back(arrow);
            arrowMotion.emplace_back(nullptr);
            arrowVisible.push_back(false);
        }
        if (!rows.empty())
            select(selected);
    }
    void build()
    {
        popup = UrlSurface{};
        surfaceStage = L"create search panel";
        panel = Border{};
        panel.Name(L"DiaTweaksUrlOverlay");
        panel.RequestedTheme(ElementTheme::Dark);
        edge = color(0xffffff, opacity(style.borderOpacity));
        set_foreground(true);
        set_input_foreground(true);
        panel.BorderThickness({1, 1, 1, 1});
        panel.Shadow(ThemeShadow{});
        panel.Translation({0, 0, 24});
        panel.RenderTransform(CompositeTransform{});
        panel.RenderTransformOrigin({.5f, .25f});
        StackPanel body;
        surfaceStage = L"create search layout";
        body.Resources().Insert(box_value(L"ContentControlThemeFontFamily"),
                                box_value(FontFamily(L"Segoe UI Variable")));
        Grid glass;
        Border tintLayer;
        tintLayer.IsHitTestVisible(false);
        glass.Children().Append(tintLayer);
        glass.Children().Append(body);
        panel.Child(glass);
        Grid header;
        header.Height(url_style::HeaderHeight);
        header.Padding({24, 0, 24, 0});
        ColumnDefinition iconCol, textCol;
        iconCol.Width({24, GridUnitType::Pixel});
        textCol.Width({1, GridUnitType::Star});
        header.ColumnDefinitions().Append(iconCol);
        header.ColumnDefinitions().Append(textCol);
        surfaceStage = L"create search icon";
        auto icon = url_vector_icon(iconForeground, true);
        icon.Width(20);
        icon.Height(20);
        icon.HorizontalAlignment(HorizontalAlignment::Left);
        icon.VerticalAlignment(VerticalAlignment::Center);
        header.Children().Append(icon);
        surfaceStage = L"create search input";
        input = TextBox{};
        surfaceStage = L"configure search input";
        input.Name(L"DiaTweaksUrlInput");
        input.PlaceholderText(L"Search or enter URL");
        input.FontSize(url_style::InputFontSize);
        input.FontFamily(FontFamily(L"Segoe UI Variable"));
        input.Background(color(0, 0));
        input.BorderThickness({0});
        input.Padding({0, 0, 0, 0});
        input.Margin({12, 0, 0, 0});
        input.VerticalAlignment(VerticalAlignment::Center);
        input.VerticalContentAlignment(VerticalAlignment::Center);
        input.MinHeight(0);
        input.Height(40);
        apply_input_text_brush();
        try
        {
            for (auto key : {L"TextControlPlaceholderForeground",
                             L"TextControlPlaceholderForegroundFocused",
                             L"TextControlPlaceholderForegroundPointerOver"})
                input.Resources().Insert(box_value(key), inputPlaceholder);
        }
        catch (...)
        {
        }
        surfaceStage = L"hook loaded input styling";
        auto weak = weak_from_this();
        input.Loaded([weak](auto &&sender, auto &&) noexcept {
            auto s = weak.lock();
            if (!s)
                return;
            auto edit = sender.template try_as<TextBox>();
            if (!edit)
                return;
            try
            {
                edit.PlaceholderForeground(s->inputPlaceholder);
            }
            catch (...)
            {
            }
            try
            {
                s->style_input_parts(edit);
                s->style_input_states(edit);
            }
            catch (...)
            {
            }
            try
            {
                edit.Resources().Insert(box_value(L"TextControlBorderBrushFocused"), color(0, 0));
                edit.Resources().Insert(box_value(L"TextControlBorderThicknessFocused"),
                                        box_value(Thickness{0}));
                for (auto key : {L"TextControlBackground", L"TextControlBackgroundPointerOver",
                                 L"TextControlBackgroundFocused", L"TextControlBorderBrush",
                                 L"TextControlBorderBrushPointerOver"})
                    edit.Resources().Insert(box_value(key), color(0, 0));
                for (auto key :
                     {L"TextControlButtonForeground", L"TextControlButtonForegroundPointerOver",
                      L"TextControlButtonForegroundPressed"})
                    edit.Resources().Insert(box_value(key), s->iconForeground);
            }
            catch (...)
            {
            }
        });
        surfaceStage = L"attach search input";
        input.MaxLength(8192);
        Grid::SetColumn(input, 1);
        header.Children().Append(input);
        body.Children().Append(header);
        results = StackPanel{};
        results.Visibility(winrt::Microsoft::UI::Xaml::Visibility::Collapsed);
        body.Children().Append(results);
        Border divider;
        divider.Height(1);
        divider.Background(edge);
        results.Children().Append(divider);
        surfaceStage = L"create results container";
        list = StackPanel{};
        list.Padding({0, 7, 0, 10});
        scroll = ScrollViewer{};
        scroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
        scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Hidden);
        scroll.HorizontalScrollMode(ScrollMode::Disabled);
        Grid resultRoot;
        selectionMarker = Border{};
        selectionMarker.Height(url_style::RowHeight);
        selectionMarker.HorizontalAlignment(HorizontalAlignment::Left);
        selectionMarker.VerticalAlignment(VerticalAlignment::Top);
        selectionMarker.Margin({8, 0, 0, 0});
        selectionMarker.CornerRadius({10, 10, 10, 10});
        selectionMarker.RenderTransform(TranslateTransform{});
        selectionMarker.IsHitTestVisible(false);
        resultRoot.Children().Append(selectionMarker);
        resultRoot.Children().Append(list);
        scroll.Content(resultRoot);
        results.Children().Append(scroll);
        error = TextBlock{};
        error.FontSize(13);
        error.Foreground(color(0x8b1e1e));
        error.Margin({24, 0, 24, 8});
        error.TextWrapping(TextWrapping::Wrap);
        results.Children().Append(error);
        surfaceStage = L"attach search content";
        popup.Child(panel, tintLayer);
        apply_style();
        input.TextChanged([weak](auto &&, auto &&) noexcept {
            try
            {
                if (auto s = weak.lock())
                    s->render();
            }
            catch (...)
            {
            }
        });
        input.AddHandler(
            UIElement::PreviewKeyDownEvent(),
            box_value(Input::KeyEventHandler([weak](auto &&,
                                                    const Input::KeyRoutedEventArgs &e) noexcept {
                try
                {
                    if (auto s = weak.lock())
                    {
                        using K = winrt::Windows::System::VirtualKey;
                        if (e.Key() == K::Escape)
                        {
                            s->hide();
                            e.Handled(true);
                        }
                        else if (e.Key() == K::Enter)
                        {
                            s->activate();
                            e.Handled(true);
                        }
                        else if (e.Key() == K::Down || e.Key() == K::Up || e.Key() == K::Tab)
                        {
                            bool reverse = e.Key() == K::Up ||
                                           (e.Key() == K::Tab && (GetKeyState(VK_SHIFT) & 0x8000));
                            if (!s->rows.empty())
                            {
                                s->keyboardSelection = true;
                                s->hoveredRow = -1;
                                s->select((s->selected + s->rows.size() + (reverse ? -1 : 1)) %
                                          s->rows.size());
                            }
                            e.Handled(true);
                        }
                    }
                }
                catch (...)
                {
                }
            })),
            true);
        render();
    }
};
UrlOverlay::UrlOverlay(IXamlDiagnostics *d, HWND dispatcher)
    : impl_(std::make_shared<Impl>(d, dispatcher))
{
}
UrlOverlay::~UrlOverlay()
{
    impl_->shutdown();
}
void UrlOverlay::tick(const std::vector<Node> &nodes)
{
    auto s = impl_;
    s->refresh_style();
    s->track_tabs();
    s->poll_contrast();
    if (auto existing = s->root.get(); existing && existing.IsLoaded())
        return;
    for (const auto &n : nodes)
        if (n.name == L"SharedSurface")
            try
            {
                ComPtr<::IInspectable> raw;
                check(s->diagnostics->GetIInspectableFromHandle(n.handle, &raw));
                winrt::Windows::Foundation::IInspectable object{nullptr};
                copy_from_abi(object, raw.Get());
                auto e = object.as<FrameworkElement>();
                for (auto p = Media::VisualTreeHelper::GetParent(e); p;
                     p = Media::VisualTreeHelper::GetParent(p))
                    if (auto presenter = p.try_as<ContentPresenter>();
                        presenter && presenter.Name() == L"ClientAreaPresenter")
                    {
                        Grid root{nullptr};
                        for (int i = 0; i < Media::VisualTreeHelper::GetChildrenCount(p); ++i)
                            if ((root = Media::VisualTreeHelper::GetChild(p, i).try_as<Grid>()))
                                break;
                        if (!root || !root.IsLoaded())
                            break;
                        s->root = make_weak(root);
                        if (!s->registered)
                        {
                            s->registered = RegisterHotKey(s->dispatcher, HotkeyId,
                                                           MOD_ALT | MOD_NOREPEAT, VK_SPACE) != 0;
                            s->shortcut = L"Alt+Space";
                            if (!s->registered)
                            {
                                s->registered = RegisterHotKey(s->dispatcher, HotkeyId,
                                                               MOD_CONTROL | MOD_ALT | MOD_NOREPEAT,
                                                               VK_SPACE) != 0;
                                s->shortcut = L"Ctrl+Alt+Space";
                            }
                        }
                        s->message =
                            s->registered
                                ? L"Ready: " + s->shortcut
                                : L"Hotkey unavailable: Alt+Space and Ctrl+Alt+Space are occupied";
                        return;
                    }
            }
            catch (...)
            {
            }
}
void UrlOverlay::toggle()
{
    try
    {
        auto s = impl_;
        auto root = s->root.get();
        if (!root || !root.IsLoaded())
            return;
        if (s->popup && s->popup.IsOpen() && !s->closing)
        {
            s->hide();
            return;
        }
        if (!s->popup)
            s->build();
        if (s->closeTimer)
            s->closeTimer.Stop();
        s->closing = false;
        if (s->popup.IsOpen())
            s->popup.IsOpen(false);
        auto xaml = root.XamlRoot();
        auto windowId = xaml.ContentIslandEnvironment().AppWindowId();
        auto window = winrt::Microsoft::UI::Windowing::AppWindow::GetFromWindowId(windowId);
        window.Show(true);
        s->contrast = {};
        s->inputContrast = {};
        ++s->contrastGeneration;
        s->nextContrast = 0;
        s->set_foreground(root.ActualTheme() == ElementTheme::Light);
        s->popup.XamlRoot(xaml);
        s->apply_style();
        s->popup.VerticalOffset(std::max(24., xaml.Size().Height * .20));
        s->scroll.MaxHeight(std::max(100., xaml.Size().Height - s->popup.VerticalOffset() -
                                               url_style::HeaderHeight - 48.));
        s->refresh_tabs(root);
        s->refresh_history();
        s->keyboardSelection = false;
        s->hoveredRow = -1;
        s->input.Text(L"");
        s->render();
        s->popup.IsOpen(true);
        s->input.Focus(FocusState::Programmatic);
        s->animate(true);
        s->message = L"Open: " + s->shortcut;
    }
    catch (...)
    {
        const auto hr = winrt::to_hresult();
        std::wstring detail;
        try
        {
            throw;
        }
        catch (const winrt::hresult_error &e)
        {
            detail = e.message();
        }
        catch (...)
        {
        }
        std::wostringstream message;
        message << L"Could not open Dia URL overlay: 0x" << std::hex << uint32_t(hr) << L" at "
                << surfaceStage << L"; " << detail;
        impl_->message = message.str();
        try
        {
            if (impl_->popup)
                impl_->popup.IsOpen(false);
        }
        catch (...)
        {
        }
        impl_->popup = nullptr;
    }
}
std::wstring UrlOverlay::status() const
{
    auto s = impl_;
    auto switchable = std::count_if(s->tabs.begin(), s->tabs.end(),
                                    [](const DiaTab &tab) { return tab.pattern != 0; });
    auto inputBrush = std::wstring(L"unavailable");
    if (auto content = s->inputContent.get())
        if (auto brush = content.Foreground().try_as<SolidColorBrush>())
        {
            auto rgb = brush.Color();
            inputBrush = rgb.R < 128 && rgb.G < 128 && rgb.B < 128 ? L"dark" : L"light";
        }
    return s->message + L"; tabs=" + std::to_wstring(s->tabs.size()) + L" selectable=" +
           std::to_wstring(switchable) + L" top-sites=" + std::to_wstring(s->history.sites.size()) +
           L" material=" + (s->popup ? s->popup.status() : L"not-built") + L" contrast=" +
           (!s->contrast.light   ? L"unavailable"
            : *s->contrast.light ? L"light-glass/black-text"
                                 : L"dark-glass/white-text") +
           L" input-contrast=" +
           (!s->inputContrast.light   ? L"unavailable"
            : *s->inputContrast.light ? L"black-text"
                                      : L"white-text") +
           L" input-brush=" + inputBrush;
}
} // namespace dt
