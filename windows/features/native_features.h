#pragma once
#include "feature_state.h"
#include "visibility.h"
#include <memory>
namespace dt {
struct Node;
class NativeFeatures {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    NativeFeatures(IXamlDiagnostics*, IVisualTreeService*, FeatureChanged = {});
    ~NativeFeatures();
    FeatureStates tick(const std::vector<Node>&, uint32_t requested);
    void restore();
    bool active() const;
    std::wstring layout(InstanceHandle);
};
}
