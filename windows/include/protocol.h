#pragma once
#include "common.h"
#include <cstdint>

namespace dt {
// One controller owns the request slot. Interlocked state transitions publish
// payloads across processes; neither side accesses a payload owned by the other.
enum class Command : LONG { Tree = 1, Inspect, Pulse, Restore, Settings, SetSidebar, SetFeature, Features, Layout, Overlay };
enum Phase : LONG { Idle, Pending, Working, Complete };
inline constexpr DWORD ProtocolVersion = 5;
inline constexpr DWORD AutoSettings = 1;
inline constexpr size_t ResponseCapacity = 256 * 1024;
struct Request { Command command; uint64_t handle; DWORD seconds; };
struct Shared {
    DWORD version;
    DWORD controller;
    DWORD target;
    DWORD options;
    uint32_t initialFeatureMask;
    volatile LONG currentFeatureMask;
    volatile LONG ready;
    volatile LONG stop;
    volatile LONG phase;
    HRESULT startup;
    Request request;
    HRESULT result;
    wchar_t response[ResponseCapacity];
};
inline LONG read_flag(volatile LONG& flag) { return InterlockedCompareExchange(&flag, 0, 0); }
}
