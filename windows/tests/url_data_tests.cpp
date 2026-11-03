#include "url_sites.h"
#include "url_style.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <objbase.h>
#include <stdexcept>
#include <windows.h>
#include <winrt/base.h>
static void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}
static std::string read(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}
int main(int argc, char **argv)
{
    try
    {
        winrt::init_apartment();
        if (argc == 2 && std::string_view(argv[1]) == "--live-count")
        {
            auto data = dt::dia_top_sites(dt::dia_history_profile());
            std::cout << "sites=" << data.sites.size() << " unavailable=" << !data.notice.empty()
                      << "\n";
            return data.sites.empty() ? 1 : 0;
        }
        auto s = dt::parse_url_style("/* --tint-opacity: .9; */ :root {--tint-opacity: .25; "
                                     "--radius: 22px; --accent: #123abc; --blur: none;}");
        require(s.tintOpacity == .25 && s.radius == 22 && s.accent == 0x123abc && !s.blur,
                "Style variables");
        require(dt::parse_url_style("--blur: acrylic;").backdrop == dt::UrlBackdrop::System &&
                    dt::parse_url_style("--blur: in-app;").backdrop == dt::UrlBackdrop::InApp &&
                    dt::parse_url_style("--blur: in-app; --blur: system;").backdrop ==
                        dt::UrlBackdrop::System,
                "Backdrop modes");
        s = dt::parse_url_style("--tint-opacity: NaN; --radius: 800px; --width: 9999px; --accent: "
                                "#bad; --secondary-opacity: .3garbage;");
        require(s.tintOpacity == .60 && s.tint == 0x92979d && s.radius == 40 && s.width == 1200 &&
                    s.accent == 0xffffff && s.secondaryOpacity == .50,
                "Invalid and bounded styles");
        require(dt::site_domain(L"https://WWW.Example.com/path") == L"example.com",
                "Host normalization");
        require(dt::site_domain(L"https://user:pass@example.com").empty(), "Credentials excluded");
        // Synthetic history only. No real profile URLs or titles enter test output.
        GUID id;
        winrt::check_hresult(CoCreateGuid(&id));
        wchar_t name[64];
        StringFromGUID2(id, name, 64);
        auto dir =
            std::filesystem::temp_directory_path() / (L"DiaTweaks-UrlTest-" + std::wstring(name));
        std::filesystem::create_directory(dir);
        struct Fixture
        {
            std::filesystem::path dir;
            ~Fixture()
            {
                for (auto n : {L"History", L"style.css"})
                {
                    std::error_code e;
                    std::filesystem::remove(dir / n, e);
                }
                std::error_code e;
                std::filesystem::remove(dir, e);
            }
        } fixture{dir};
        HMODULE sqlite = LoadLibraryExW(L"winsqlite3.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        require(sqlite != nullptr, "Windows SQLite");
        struct Module
        {
            HMODULE value;
            ~Module()
            {
                FreeLibrary(value);
            }
        } module{sqlite};
        auto open = reinterpret_cast<int(__cdecl *)(const wchar_t *, void **)>(
            GetProcAddress(sqlite, "sqlite3_open16"));
        auto exec = reinterpret_cast<int(__cdecl *)(void *, const char *, void *, void *, void *)>(
            GetProcAddress(sqlite, "sqlite3_exec"));
        auto close =
            reinterpret_cast<int(__cdecl *)(void *)>(GetProcAddress(sqlite, "sqlite3_close"));
        require(open && exec && close, "SQLite exports");
        void *db = nullptr;
        require(open((dir / L"History").c_str(), &db) == 0, "Fixture DB");
        struct Connection
        {
            void *db;
            decltype(close) fn;
            ~Connection()
            {
                if (db)
                    fn(db);
            }
        } connection{db, close};
        require(exec(db,
                     "CREATE TABLE urls(url TEXT,title TEXT,visit_count INTEGER,last_visit_time "
                     "INTEGER,hidden INTEGER); INSERT INTO urls VALUES "
                     "('https://www.one.test/a','One',8,1,0),('https://one.test/"
                     "b','Other',7,2,0),('https://two.test/','Two',12,3,0),('https://hidden.test/"
                     "','Hidden',99,4,1),('file:///example','Local',100,5,0),('https://"
                     "user:pass@bad.test/','Credentials',200,6,0);",
                     nullptr, nullptr, nullptr) == 0,
                "Fixture schema");
        close(db);
        connection.db = nullptr;
        auto before = read(dir / L"History");
        auto history = dt::dia_top_sites(dir);
        require(history.notice.empty() && history.sites.size() == 2, "History filtering");
        require(history.sites[0].domain == L"one.test" && history.sites[0].visits == 15 &&
                    history.sites[1].domain == L"two.test",
                "Domain visit ranking");
        require(read(dir / L"History") == before, "Source history unchanged");
        auto path = dir / L"style.css";
        dt::read_url_style(path);
        {
            std::ofstream out(path);
            out << ":root {--tint-opacity: .27;}";
        }
        require(dt::read_url_style(path).tintOpacity == .27, "Live style reread");
        require(read(path) == ":root {--tint-opacity: .27;}", "User style not overwritten");
        std::cout << "PASS: history aggregation, filtering, read-only snapshot and editable style "
                     "variables\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    catch (...)
    {
        std::cerr << "URL data checks failed\n";
        return 1;
    }
}
