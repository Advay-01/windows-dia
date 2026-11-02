#pragma once
#include "appearance.h"
namespace dt
{
class ThemesMenu
{
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    ThemesMenu(IXamlDiagnostics *, FeatureChanged);
    ~ThemesMenu();
    void tick(const std::vector<Node> &, const FeatureState &);
    void remove();
    std::wstring debug_status() const;
};
} // namespace dt
