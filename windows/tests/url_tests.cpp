#include "url_logic.h"
#include <iostream>
#include <stdexcept>
#include <winrt/Windows.Foundation.h>
int main()
{
    try
    {
        winrt::init_apartment();
        auto require = [](bool value) {
            if (!value)
                throw std::runtime_error("URL action check failed");
        };
        require(dt::url_actions(L"  ").empty());
        auto domain = dt::url_actions(L" example.com ");
        require(domain.size() == 2 && domain[0].label == L"Open in Dia");
        require(domain[0].url.starts_with(L"https://example.com"));
        require(dt::url_actions(L"https://example.com/a?x=1&y=2").size() == 2);
        require(dt::url_actions(L"http://localhost:3000/test").size() == 2);
        auto query = dt::url_actions(L"two words & more");
        require(query.size() == 1 && query[0].url.find(L"%20") != std::wstring::npos &&
                query[0].url.find(L"%26") != std::wstring::npos);
        for (auto unsafe : {L"javascript:alert(1)", L"file:///C:/Windows",
                            L"data:text/html,example", L"https://user:pass@example.com"})
            require(dt::url_actions(unsafe).size() == 1);
        std::cout
            << "PASS: Dia URL/search classification, URL normalization and safe query encoding\n";
        winrt::uninit_apartment();
        return 0;
    }
    catch (...)
    {
        std::cerr << "URL action checks failed\n";
        return 1;
    }
}
