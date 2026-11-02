#pragma once
#include <cwctype>
#include <span>
#include <string>
#include <string_view>
namespace dt
{
enum class SidebarMenuRole
{
    Other,
    NewTab,
    NewGroup,
    SidebarLayout,
    AutoHide,
    CloseAll
};
inline SidebarMenuRole sidebar_menu_role(std::wstring_view text)
{
    if (text.size() > 64)
        return SidebarMenuRole::Other;
    std::wstring label;
    for (auto c : text)
        if (c != L'&')
            label += static_cast<wchar_t>(std::towlower(c));
    if (label == L"new tab")
        return SidebarMenuRole::NewTab;
    if (label == L"new group")
        return SidebarMenuRole::NewGroup;
    if (label == L"show tabs in sidebar")
        return SidebarMenuRole::SidebarLayout;
    if (label == L"auto-hide tabs")
        return SidebarMenuRole::AutoHide;
    if (label == L"close all tabs")
        return SidebarMenuRole::CloseAll;
    return SidebarMenuRole::Other;
}
inline bool is_sidebar_menu(std::span<const SidebarMenuRole> roles)
{
    unsigned mask = 0;
    for (auto role : roles)
        if (role != SidebarMenuRole::Other)
            mask |= 1u << static_cast<unsigned>(role);
    return mask == 0x3e; // Require all five fixed commands, never tab titles or a single label.
}
} // namespace dt
