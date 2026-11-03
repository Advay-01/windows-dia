#pragma once
#include <memory>
#include <string>
#include <vector>
#include <windows.h>
struct IXamlDiagnostics;
namespace dt
{
struct Node;
class UrlOverlay
{
    struct Impl;
    std::shared_ptr<Impl> impl_;

  public:
    static constexpr int HotkeyId = 0xD1A;
    UrlOverlay(IXamlDiagnostics *, HWND dispatcher);
    ~UrlOverlay();
    void tick(const std::vector<Node> &);
    void toggle();
    std::wstring status() const;
};
} // namespace dt
