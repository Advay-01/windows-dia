#pragma once
#include <string>
#include <vector>
namespace dt
{
struct UrlAction
{
    std::wstring title, label, url;
};
std::vector<UrlAction> url_actions(std::wstring text);
} // namespace dt
