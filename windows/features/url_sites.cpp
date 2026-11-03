#include "url_sites.h"
#include <algorithm>
#include <cwctype>
#include <fstream>
#include <map>
#include <objbase.h>
#include <stdexcept>
#include <windows.h>
#include <winrt/Windows.Data.Json.h>
namespace dt
{
namespace
{
using Db = void;
using Stmt = void;
struct Sqlite
{
    struct Library
    {
        HMODULE handle = LoadLibraryExW(L"winsqlite3.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        ~Library()
        {
            if (handle)
                FreeLibrary(handle);
        }
    } module;
    template <class T> T function(const char *name)
    {
        auto address = module.handle ? GetProcAddress(module.handle, name) : nullptr;
        if (!address)
            throw std::runtime_error("SQLite unavailable");
        return reinterpret_cast<T>(address);
    }
    int(__cdecl *open)(const char *, Db **, int,
                       const char *) = function<decltype(open)>("sqlite3_open_v2");
    int(__cdecl *close)(Db *) = function<decltype(close)>("sqlite3_close");
    int(__cdecl *timeout)(Db *, int) = function<decltype(timeout)>("sqlite3_busy_timeout");
    int(__cdecl *prepare)(Db *, const char *, int, Stmt **,
                          const char **) = function<decltype(prepare)>("sqlite3_prepare_v2");
    int(__cdecl *step)(Stmt *) = function<decltype(step)>("sqlite3_step");
    int(__cdecl *finish)(Stmt *) = function<decltype(finish)>("sqlite3_finalize");
    const unsigned char *(__cdecl *text)(Stmt *,
                                         int) = function<decltype(text)>("sqlite3_column_text");
    int64_t(__cdecl *integer)(Stmt *, int) = function<decltype(integer)>("sqlite3_column_int64");
    const void *(__cdecl *blob)(Stmt *, int) = function<decltype(blob)>("sqlite3_column_blob");
    int(__cdecl *bytes)(Stmt *, int) = function<decltype(bytes)>("sqlite3_column_bytes");
    int(__cdecl *bind)(Stmt *, int, const char *, int,
                       void(__cdecl *)(void *)) = function<decltype(bind)>("sqlite3_bind_text");
};
std::string utf8(const std::wstring &s)
{
    int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0, nullptr, nullptr);
    std::string out(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, s.data(), int(s.size()), out.data(), n, nullptr, nullptr);
    return out;
}
std::wstring wide(const unsigned char *s)
{
    if (!s)
        return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char *>(s), -1, nullptr, 0);
    std::wstring out(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char *>(s), -1, out.data(), n);
    if (!out.empty())
        out.pop_back();
    return out;
}
struct Database
{
    Sqlite &api;
    Db *db = nullptr;
    Database(Sqlite &a, const std::filesystem::path &p) : api(a)
    {
        auto path = utf8(p.wstring());
        int result = api.open(path.c_str(), &db, 1, nullptr);
        if (result)
        {
            if (db)
                api.close(db);
            db = nullptr;
            throw std::runtime_error("Cannot read history snapshot");
        }
        api.timeout(db, 100);
    }
    ~Database()
    {
        if (db)
            api.close(db);
    }
};
struct Query
{
    Sqlite &api;
    Stmt *statement = nullptr;
    Query(Database &db, const char *sql) : api(db.api)
    {
        if (api.prepare(db.db, sql, -1, &statement, nullptr))
            throw std::runtime_error("History schema unavailable");
    }
    ~Query()
    {
        if (statement)
            api.finish(statement);
    }
};
struct Snapshot
{
    std::filesystem::path dir;
    Snapshot()
    {
        wchar_t temp[32768]{};
        if (!GetTempPathW(32768, temp))
            throw std::runtime_error("No temporary directory");
        GUID id;
        if (FAILED(CoCreateGuid(&id)))
            throw std::runtime_error("No snapshot ID");
        wchar_t name[64]{};
        StringFromGUID2(id, name, 64);
        dir = std::filesystem::path(temp) / (L"DiaTweaks-UrlHistory-" + std::wstring(name));
        if (!std::filesystem::create_directory(dir))
            throw std::runtime_error("Cannot create snapshot");
    }
    ~Snapshot()
    {
        for (auto file : {L"History", L"History-wal", L"History-shm", L"History-journal",
                          L"Favicons", L"Favicons-wal", L"Favicons-shm", L"Favicons-journal"})
        {
            std::error_code e;
            std::filesystem::remove(dir / file, e);
        }
        std::error_code e;
        std::filesystem::remove(dir, e);
    }
    void copy(const std::filesystem::path &profile, const wchar_t *name)
    {
        auto source = profile / name;
        if (!std::filesystem::exists(source))
            return;
        for (int attempt = 0; attempt < 3; ++attempt)
        {
            auto stamp = [](const std::filesystem::path &p) {
                return std::filesystem::exists(p)
                           ? std::pair{std::filesystem::last_write_time(p),
                                       std::filesystem::file_size(p)}
                           : std::pair{std::filesystem::file_time_type{}, uintmax_t(0)};
            };
            auto wal = source;
            wal += L"-wal";
            auto journal = source;
            journal += L"-journal";
            auto before = stamp(source), walBefore = stamp(wal), journalBefore = stamp(journal);
            std::filesystem::copy_file(source, dir / name,
                                       std::filesystem::copy_options::overwrite_existing);
            for (auto suffix : {L"-wal", L"-journal"})
            {
                auto extra = source;
                extra += suffix;
                auto dest = dir / (std::wstring(name) + suffix);
                if (std::filesystem::exists(extra))
                    std::filesystem::copy_file(extra, dest,
                                               std::filesystem::copy_options::overwrite_existing);
                else
                {
                    std::error_code e;
                    std::filesystem::remove(dest, e);
                }
            }
            if (before == stamp(source) && walBefore == stamp(wal) &&
                journalBefore == stamp(journal))
                return;
        }
        throw std::runtime_error("History is changing");
    }
};
} // namespace
std::wstring site_domain(const std::wstring &url)
{
    auto start = url.find(L"://");
    if (start == std::wstring::npos)
        return {};
    auto domain = url.substr(start + 3, url.find_first_of(L"/?#", start + 3) - (start + 3));
    if (domain.find(L'@') != std::wstring::npos)
        return {};
    std::transform(domain.begin(), domain.end(), domain.begin(),
                   [](wchar_t c) { return wchar_t(towlower(c)); });
    if (domain.starts_with(L"www."))
        domain.erase(0, 4);
    return domain;
}
std::filesystem::path dia_history_profile()
{
    wchar_t local[32768]{};
    if (!GetEnvironmentVariableW(L"LOCALAPPDATA", local, 32768))
        throw std::runtime_error("No app data");
    auto base = std::filesystem::path(local) / L"Dia" / L"User Data";
    if (!std::filesystem::exists(base))
        base = std::filesystem::path(local) / L"Packages" / L"TheBrowserCompany.Dia_ttt1ap7aakyb4" /
               L"LocalCache" / L"Local" / L"Dia" / L"User Data";
    std::wstring profile = L"Default";
    try
    {
        auto state = base / L"Local State";
        if (std::filesystem::file_size(state) <= 16 * 1024 * 1024)
        {
            std::ifstream in(state, std::ios::binary);
            std::string json(std::istreambuf_iterator<char>(in), {});
            auto root = winrt::Windows::Data::Json::JsonObject::Parse(winrt::to_hstring(json));
            auto name = root.GetNamedObject(L"profile").GetNamedString(L"last_used", L"Default");
            std::filesystem::path candidate(name.c_str());
            if (!candidate.empty() && candidate == candidate.filename() && candidate != L"." &&
                candidate != L"..")
                profile = name;
        }
    }
    catch (...)
    {
    }
    return base / profile;
}
SiteHistory dia_top_sites(const std::filesystem::path &profile)
{
    SiteHistory result;
    try
    {
        Sqlite api;
        Snapshot snapshot;
        snapshot.copy(profile, L"History");
        std::map<std::wstring, TopSite> byDomain;
        {
            Database db(api, snapshot.dir / L"History");
            Query query(db, "SELECT url,title,visit_count FROM urls WHERE hidden=0 AND "
                            "visit_count>0 AND (url LIKE 'https://%' OR url LIKE 'http://%') ORDER "
                            "BY visit_count DESC,last_visit_time DESC");
            int step;
            while ((step = api.step(query.statement)) == 100)
            {
                auto url = wide(api.text(query.statement, 0));
                auto domain = site_domain(url);
                if (domain.empty())
                    continue;
                auto count = api.integer(query.statement, 2);
                auto [it, inserted] = byDomain.try_emplace(domain);
                auto &site = it->second;
                if (inserted)
                {
                    site.url = url;
                    site.title = wide(api.text(query.statement, 1));
                    site.domain = domain;
                    if (site.title.empty())
                        site.title = domain;
                }
                site.visits += count;
            }
            if (step != 101)
                throw std::runtime_error("Cannot read history");
        }
        for (auto &[domain, site] : byDomain)
            result.sites.push_back(std::move(site));
        std::stable_sort(result.sites.begin(), result.sites.end(),
                         [](const TopSite &a, const TopSite &b) { return a.visits > b.visits; });
        if (result.sites.size() > 6)
            result.sites.resize(6);
        try
        {
            snapshot.copy(profile, L"Favicons");
            Database db(api, snapshot.dir / L"Favicons");
            for (auto &site : result.sites)
            {
                Query icon(db, "SELECT b.image_data FROM icon_mapping m JOIN favicon_bitmaps b ON "
                               "b.icon_id=m.icon_id WHERE m.page_url=? AND length(b.image_data)>0 "
                               "AND b.width>0 AND b.height>0 ORDER BY "
                               "ABS(b.width-32),ABS(b.width-b.height) LIMIT 1");
                auto url = utf8(site.url);
                api.bind(icon.statement, 1, url.c_str(), int(url.size()), nullptr);
                if (api.step(icon.statement) == 100)
                {
                    int bytes = api.bytes(icon.statement, 0);
                    auto data = static_cast<const uint8_t *>(api.blob(icon.statement, 0));
                    if (data && bytes > 0 && bytes <= 262144)
                        site.icon.assign(data, data + bytes);
                }
            }
        }
        catch (...)
        {
        } // A missing cached favicon never prevents a site result.
        result.notice = result.sites.empty() ? L"No visited sites yet" : L"";
    }
    catch (...)
    {
        result.notice = L"Dia history is temporarily unavailable";
    }
    return result;
}
} // namespace dt
