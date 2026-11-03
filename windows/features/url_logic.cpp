#include "url_logic.h"
#include <algorithm>
#include <cwctype>
#include <winrt/Windows.Foundation.h>
namespace dt
{
std::vector<UrlAction> url_actions(std::wstring text)
{
    while (!text.empty() && iswspace(text.front()))
        text.erase(text.begin());
    while (!text.empty() && iswspace(text.back()))
        text.pop_back();
    if (text.empty())
        return {};
    if (text.size() > 8192)
        text.resize(8192);
    std::wstring lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](wchar_t c) { return wchar_t(towlower(c)); });
    std::vector<UrlAction> actions;
    const bool http = lower.starts_with(L"https://") || lower.starts_with(L"http://");
    const auto hostEnd = lower.find_first_of(L"/?#");
    const auto host = lower.substr(0, hostEnd);
    const bool domain = host.find(L'.') != std::wstring::npos || host.starts_with(L"localhost");
    const auto colon = host.find(L':');
    const bool otherScheme = colon != std::wstring::npos &&
                             host.substr(0, colon).find(L'.') == std::wstring::npos &&
                             !host.starts_with(L"localhost:");
    if (std::none_of(text.begin(), text.end(), [](wchar_t c) { return iswspace(c); }) &&
        (http || (domain && !otherScheme)))
    {
        try
        {
            winrt::Windows::Foundation::Uri uri(http ? text : L"https://" + text);
            if ((uri.SchemeName() == L"https" || uri.SchemeName() == L"http") &&
                !uri.Host().empty() && uri.UserName().empty() && uri.Password().empty())
                actions.push_back({text, L"Open in Dia", std::wstring(uri.AbsoluteUri())});
        }
        catch (...)
        {
        }
    }
    actions.push_back({L"Search for \u201c" + text + L"\u201d", L"Search in Dia",
                       L"https://www.google.com/search?q=" +
                           std::wstring(winrt::Windows::Foundation::Uri::EscapeComponent(text))});
    return actions;
}
} // namespace dt
