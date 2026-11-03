#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
namespace dt
{
struct TopSite
{
    std::wstring title, url, domain;
    int64_t visits = 0;
    std::vector<uint8_t> icon;
};
struct SiteHistory
{
    std::vector<TopSite> sites;
    std::wstring notice;
};
SiteHistory dia_top_sites(const std::filesystem::path &profile);
std::wstring site_domain(const std::wstring &url);
std::filesystem::path dia_history_profile();
} // namespace dt
