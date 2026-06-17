#include "pe_security.hpp"

#include <windows.h>

#include <sstream>
#include <type_traits>

namespace wef {

namespace {

template <typename T>
bool readStruct(DbgSession& session, ULONG64 address, T& value) {
    ULONG bytesRead = 0;
    const HRESULT hr = session.readVirtual(address, &value, sizeof(T), &bytesRead);
    return SUCCEEDED(hr) && bytesRead == sizeof(T);
}

std::string machineName(USHORT machine) {
    switch (machine) {
    case IMAGE_FILE_MACHINE_I386:
        return "x86";
    case IMAGE_FILE_MACHINE_AMD64:
        return "x64";
    case IMAGE_FILE_MACHINE_ARM64:
        return "arm64";
    default:
        return "0x" + formatHex(machine);
    }
}

std::string subsystemName(USHORT subsystem) {
    switch (subsystem) {
    case IMAGE_SUBSYSTEM_NATIVE:
        return "native";
    case IMAGE_SUBSYSTEM_WINDOWS_GUI:
        return "windows-gui";
    case IMAGE_SUBSYSTEM_WINDOWS_CUI:
        return "windows-cui";
    case IMAGE_SUBSYSTEM_EFI_APPLICATION:
        return "efi-application";
    default:
        return "0x" + formatHex(subsystem);
    }
}

std::string yesNo(bool value) {
    return value ? "yes" : "no";
}

bool moduleBase(DbgSession& session, const std::vector<std::string>& args, ULONG& index, ULONG64& base) {
    if (args.empty()) {
        return SUCCEEDED(session.symbols()->GetModuleByIndex(0, &base));
    }
    return SUCCEEDED(session.symbols()->GetModuleByModuleName(args[0].c_str(), 0, &index, &base));
}

std::string moduleName(DbgSession& session, ULONG index, ULONG64 base) {
    char module[512] = {};
    if (SUCCEEDED(session.symbols()->GetModuleNames(index, base, nullptr, 0, nullptr, module, sizeof(module), nullptr, nullptr, 0, nullptr))) {
        return module;
    }
    return "<unknown>";
}

template <typename OptionalHeader, typename LoadConfig>
HRESULT reportImage(DbgSession& session, const Output& out, ULONG index, ULONG64 base, const IMAGE_FILE_HEADER& fileHeader, const OptionalHeader& optional) {
    const USHORT dll = optional.DllCharacteristics;
    const bool dynamicBase = (dll & IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE) != 0;
    const bool nxCompat = (dll & IMAGE_DLLCHARACTERISTICS_NX_COMPAT) != 0;
    const bool guardCf = (dll & IMAGE_DLLCHARACTERISTICS_GUARD_CF) != 0;
    const bool highEntropy = (dll & IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA) != 0;

    out.heading("WEF Checksec");
    out.field("Module", moduleName(session, index, base));
    out.field("ImageBase", formatAddress(base));
    out.field("PreferredImageBase", formatAddress(optional.ImageBase));
    out.field("Machine", machineName(fileHeader.Machine));
    out.field("Subsystem", subsystemName(optional.Subsystem));
    out.field("Timestamp", "0x" + formatHex(fileHeader.TimeDateStamp));
    out.field("DYNAMICBASE / ASLR", yesNo(dynamicBase));
    out.field("NX compatible", yesNo(nxCompat));
    out.field("High entropy VA", yesNo(highEntropy));

    const auto& loadConfigDir = optional.DataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG];
    LoadConfig loadConfig = {};
    bool loadConfigRead = false;
    if (loadConfigDir.VirtualAddress != 0 && loadConfigDir.Size >= sizeof(ULONG)) {
        loadConfigRead = readStruct(session, base + loadConfigDir.VirtualAddress, loadConfig);
    }

    if (guardCf) {
        if (loadConfigRead) {
            out.field("Control Flow Guard", "yes (GuardFlags=0x" + formatHex(loadConfig.GuardFlags) + ")");
        } else {
            out.field("Control Flow Guard", "yes (metadata unreadable)");
        }
    } else {
        out.field("Control Flow Guard", "no");
    }

    if constexpr (std::is_same_v<LoadConfig, IMAGE_LOAD_CONFIG_DIRECTORY32>) {
        if ((dll & IMAGE_DLLCHARACTERISTICS_NO_SEH) != 0) {
            out.field("SafeSEH", "not applicable (/NOSEH)");
        } else if (loadConfigRead && loadConfig.SEHandlerCount != 0) {
            out.field("SafeSEH", "yes");
        } else {
            out.field("SafeSEH", "no or unavailable");
        }
    } else {
        out.field("SafeSEH", "not applicable");
    }

    return S_OK;
}

}

HRESULT runChecksec(DbgSession& session, const Output& out, const std::vector<std::string>& args) {
    if (args.size() > 1) {
        out.line("usage: !wef.checksec [module]");
        return S_OK;
    }

    ULONG index = 0;
    ULONG64 base = 0;
    if (!moduleBase(session, args, index, base)) {
        out.error("could not locate module\n");
        return S_OK;
    }

    IMAGE_DOS_HEADER dos = {};
    if (!readStruct(session, base, dos) || dos.e_magic != IMAGE_DOS_SIGNATURE) {
        out.error("could not read valid DOS header at " + formatAddress(base) + "\n");
        return S_OK;
    }

    const ULONG64 ntAddress = base + static_cast<ULONG>(dos.e_lfanew);
    DWORD signature = 0;
    if (!readStruct(session, ntAddress, signature) || signature != IMAGE_NT_SIGNATURE) {
        out.error("could not read valid NT headers at " + formatAddress(ntAddress) + "\n");
        return S_OK;
    }

    IMAGE_FILE_HEADER fileHeader = {};
    if (!readStruct(session, ntAddress + sizeof(DWORD), fileHeader)) {
        out.error("could not read PE file header\n");
        return S_OK;
    }

    USHORT magic = 0;
    const ULONG64 optionalAddress = ntAddress + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER);
    if (!readStruct(session, optionalAddress, magic)) {
        out.error("could not read PE optional header magic\n");
        return S_OK;
    }

    if (magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        IMAGE_OPTIONAL_HEADER64 optional = {};
        if (!readStruct(session, optionalAddress, optional)) {
            out.error("could not read PE32+ optional header\n");
            return S_OK;
        }
        return reportImage<IMAGE_OPTIONAL_HEADER64, IMAGE_LOAD_CONFIG_DIRECTORY64>(session, out, index, base, fileHeader, optional);
    }

    if (magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        IMAGE_OPTIONAL_HEADER32 optional = {};
        if (!readStruct(session, optionalAddress, optional)) {
            out.error("could not read PE32 optional header\n");
            return S_OK;
        }
        return reportImage<IMAGE_OPTIONAL_HEADER32, IMAGE_LOAD_CONFIG_DIRECTORY32>(session, out, index, base, fileHeader, optional);
    }

    out.error("unsupported PE optional header magic: 0x" + formatHex(magic) + "\n");
    return S_OK;
}

}
