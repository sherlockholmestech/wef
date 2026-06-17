#include "command_router.hpp"

#include <windows.h>
#include <dbgeng.h>

#define WEF_EXPORT extern "C" __declspec(dllexport)
#define WEF_ALIAS_TARGET extern "C"

#if defined(_MSC_VER)
#pragma comment(linker, "/EXPORT:wef=wef_entry")
#endif

WEF_EXPORT HRESULT CALLBACK DebugExtensionInitialize(PULONG Version, PULONG Flags) {
    *Version = (1u << 16) | 0u;
    *Flags = 0;
    return S_OK;
}

WEF_EXPORT void CALLBACK DebugExtensionUninitialize(void) {}

WEF_ALIAS_TARGET HRESULT CALLBACK wef_entry(IDebugClient* client, PCSTR args) {
    return wef::commands::WefHelp(client, args);
}

WEF_EXPORT HRESULT CALLBACK install(IDebugClient* client, PCSTR args) {
    return wef::commands::Install(client, args);
}

WEF_EXPORT HRESULT CALLBACK uninstall(IDebugClient* client, PCSTR args) {
    return wef::commands::Uninstall(client, args);
}

WEF_EXPORT HRESULT CALLBACK aliases(IDebugClient* client, PCSTR args) {
    return wef::commands::Aliases(client, args);
}

WEF_EXPORT HRESULT CALLBACK ctx(IDebugClient* client, PCSTR args) {
    return wef::commands::Ctx(client, args);
}

WEF_EXPORT HRESULT CALLBACK telescope(IDebugClient* client, PCSTR args) {
    return wef::commands::Telescope(client, args);
}

WEF_EXPORT HRESULT CALLBACK hexdump(IDebugClient* client, PCSTR args) {
    return wef::commands::Hexdump(client, args);
}

WEF_EXPORT HRESULT CALLBACK vmmap(IDebugClient* client, PCSTR args) {
    return wef::commands::Vmmap(client, args);
}

WEF_EXPORT HRESULT CALLBACK checksec(IDebugClient* client, PCSTR args) {
    return wef::commands::Checksec(client, args);
}

WEF_EXPORT HRESULT CALLBACK heaps(IDebugClient* client, PCSTR args) {
    return wef::commands::Heaps(client, args);
}

WEF_EXPORT HRESULT CALLBACK vis(IDebugClient* client, PCSTR args) {
    return wef::commands::Vis(client, args);
}

WEF_EXPORT HRESULT CALLBACK config(IDebugClient* client, PCSTR args) {
    return wef::commands::Config(client, args);
}
