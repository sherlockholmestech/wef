#include "modules.hpp"

namespace wef {

namespace {

std::string padRight(std::string text, size_t width) {
    if (text.size() >= width) {
        return text;
    }
    text.append(width - text.size(), ' ');
    return text;
}

std::string padLeft(std::string text, size_t width) {
    if (text.size() >= width) {
        return text;
    }
    return std::string(width - text.size(), ' ') + text;
}

std::string sizeText(ULONG64 size) {
    return "0x" + formatHex(size, 8);
}

}

HRESULT runVmmap(DbgSession& session, const Output& out, const std::vector<std::string>& args) {
    if (!args.empty()) {
        out.line("usage: !wef.vmmap");
        return S_OK;
    }

    const TargetInfo target = session.targetInfo();
    if (target.kernelMode) {
        out.error("vmmap is unsupported for kernel targets in this milestone\n");
        return S_OK;
    }
    if (!target.userMode) {
        out.error("vmmap requires a user-mode target in this milestone\n");
        return S_OK;
    }
    if (target.dump) {
        out.warning("dump target detected; memory region information may be partial\n");
    }

    out.heading("VM Map");
    out.line("  base               end                size        state     protect      type      module");
    out.line("  -----------------  -----------------  ----------  --------  -----------  --------  ----------------");

    const ULONG64 maxAddress = session.pointerSize() == 8 ? 0x0000800000000000ULL : 0xffffffffULL;
    ULONG64 address = 0;
    ULONG64 regions = 0;
    bool printed = false;

    while (address < maxAddress && regions < 200000) {
        MEMORY_BASIC_INFORMATION64 info = {};
        if (!session.queryVirtual(address, info)) {
            if (address == 0) {
                out.error("QueryVirtual failed at " + formatAddress(address) + "\n");
                return S_OK;
            }
            out.warning("QueryVirtual stopped at " + formatAddress(address) + "\n");
            break;
        }

        const ULONG64 base = info.BaseAddress;
        const ULONG64 size = info.RegionSize;
        const ULONG64 end = size == 0 ? base : base + size;
        if (info.State != MEM_FREE) {
            const std::string baseText = formatAddress(base);
            const std::string endText = formatAddress(end);
            const std::string regionSize = sizeText(size);
            const std::string state = stateToString(info.State);
            const std::string protection = protectionToString(info.Protect);
            const std::string type = typeToString(info.Type);
            const std::string moduleName = session.moduleNameForOffset(base);
            const std::string line =
                "  " +
                padRight(baseText, 17) + "  " +
                padRight(endText, 17) + "  " +
                padLeft(regionSize, 10) + "  " +
                padRight(state, 8) + "  " +
                padRight(protection, 11) + "  " +
                padRight(type, 8) + "  " +
                moduleName;
            const std::string dml =
                std::string("  <col fg=\"yellow\">") + dmlEscape(padRight(baseText, 17)) + "</col>  " +
                "<col fg=\"yellow\">" + dmlEscape(padRight(endText, 17)) + "</col>  " +
                "<col fg=\"white\">" + dmlEscape(padLeft(regionSize, 10)) + "</col>  " +
                "<col fg=\"green\">" + dmlEscape(padRight(state, 8)) + "</col>  " +
                "<col fg=\"cyan\">" + dmlEscape(padRight(protection, 11)) + "</col>  " +
                "<col fg=\"magenta\">" + dmlEscape(padRight(type, 8)) + "</col>  " +
                "<col fg=\"white\">" + dmlEscape(moduleName) + "</col>";
            out.dmlLine(
                line,
                dml);
            printed = true;
        }

        if (size == 0 || end <= address) {
            address += 0x1000;
        } else {
            address = end;
        }
        ++regions;
    }

    if (!printed) {
        out.warning("no committed or reserved regions were reported\n");
    }
    return S_OK;
}

}
