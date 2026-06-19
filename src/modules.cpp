#include "modules.hpp"

#include <algorithm>
#include <cctype>
#include <string>

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

bool hasWriteProtect(ULONG protect) {
    const ULONG base = protect & 0xff;
    return base == PAGE_READWRITE || base == PAGE_WRITECOPY || base == PAGE_EXECUTE_READWRITE || base == PAGE_EXECUTE_WRITECOPY;
}

bool hasExecuteProtect(ULONG protect) {
    const ULONG base = protect & 0xff;
    return base == PAGE_EXECUTE || base == PAGE_EXECUTE_READ || base == PAGE_EXECUTE_READWRITE || base == PAGE_EXECUTE_WRITECOPY;
}

bool containsAddress(const MEMORY_BASIC_INFORMATION64& info, ULONG64 address) {
    return address >= info.BaseAddress && address - info.BaseAddress < info.RegionSize;
}

bool containsInsensitive(std::string text, std::string needle) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return text.find(needle) != std::string::npos;
}

struct VmmapFilter {
    bool includeFree = false;
    bool onlyCommit = false;
    bool onlyReserve = false;
    bool onlyWrite = false;
    bool onlyExecute = false;
    bool onlyImage = false;
    bool onlyMapped = false;
    bool onlyPrivate = false;
    bool hasContains = false;
    ULONG64 contains = 0;
    ULONG64 limit = 0;
    std::string module;
};

void vmmapUsage(const Output& out) {
    out.line("usage: !wef.vmmap [-all|-free] [-commit|-reserve] [-x|-w|-rwx] [-image|-mapped|-private] [-module <name>] [-contains <addr>] [L<count>]");
}

}

HRESULT runVmmap(DbgSession& session, const Output& out, const std::vector<std::string>& args) {
    VmmapFilter filter;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-all" || args[i] == "-free") {
            filter.includeFree = true;
        } else if (args[i] == "-commit") {
            filter.onlyCommit = true;
        } else if (args[i] == "-reserve") {
            filter.onlyReserve = true;
        } else if (args[i] == "-x") {
            filter.onlyExecute = true;
        } else if (args[i] == "-w") {
            filter.onlyWrite = true;
        } else if (args[i] == "-rwx") {
            filter.onlyExecute = true;
            filter.onlyWrite = true;
        } else if (args[i] == "-image") {
            filter.onlyImage = true;
        } else if (args[i] == "-mapped") {
            filter.onlyMapped = true;
        } else if (args[i] == "-private") {
            filter.onlyPrivate = true;
        } else if (args[i] == "-module" && i + 1 < args.size()) {
            filter.module = args[++i];
        } else if (args[i] == "-contains" && i + 1 < args.size()) {
            if (!session.evaluate(args[i + 1], filter.contains) && !parseNumber(args[i + 1], filter.contains)) {
                vmmapUsage(out);
                return S_OK;
            }
            filter.hasContains = true;
            ++i;
        } else {
            ULONG64 parsed = 0;
            if (parseLengthToken(args[i], parsed)) {
                filter.limit = parsed;
            } else {
                vmmapUsage(out);
                return S_OK;
            }
        }
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
    out.line("  base               end                size        state     protect      type      module            actions");
    out.line("  -----------------  -----------------  ----------  --------  -----------  --------  ----------------  ------------------------------");

    const ULONG64 maxAddress = session.pointerSize() == 8 ? 0x0000800000000000ULL : 0xffffffffULL;
    ULONG64 address = 0;
    ULONG64 regions = 0;
    ULONG64 printedCount = 0;
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
        const std::string moduleName = session.moduleNameForOffset(base);
        const bool matches =
            (filter.includeFree || info.State != MEM_FREE) &&
            (!filter.onlyCommit || info.State == MEM_COMMIT) &&
            (!filter.onlyReserve || info.State == MEM_RESERVE) &&
            (!filter.onlyWrite || hasWriteProtect(info.Protect)) &&
            (!filter.onlyExecute || hasExecuteProtect(info.Protect)) &&
            (!filter.onlyImage || info.Type == MEM_IMAGE) &&
            (!filter.onlyMapped || info.Type == MEM_MAPPED) &&
            (!filter.onlyPrivate || info.Type == MEM_PRIVATE) &&
            (!filter.hasContains || containsAddress(info, filter.contains)) &&
            (filter.module.empty() || containsInsensitive(moduleName, filter.module));
        if (matches) {
            const std::string baseText = formatAddress(base);
            const std::string endText = formatAddress(end);
            const std::string regionSize = sizeText(size);
            const std::string state = stateToString(info.State);
            const std::string protection = protectionToString(info.Protect);
            const std::string type = typeToString(info.Type);
            const std::string line =
                "  " +
                padRight(baseText, 17) + "  " +
                padRight(endText, 17) + "  " +
                padLeft(regionSize, 10) + "  " +
                padRight(state, 8) + "  " +
                padRight(protection, 11) + "  " +
                padRight(type, 8) + "  " +
                padRight(moduleName, 16) + "  " +
                "[dump] [search] [contains]";
            const std::string dml =
                std::string("  <col fg=\"yellow\">") + dmlEscape(padRight(baseText, 17)) + "</col>  " +
                "<col fg=\"yellow\">" + dmlEscape(padRight(endText, 17)) + "</col>  " +
                "<col fg=\"white\">" + dmlEscape(padLeft(regionSize, 10)) + "</col>  " +
                "<col fg=\"green\">" + dmlEscape(padRight(state, 8)) + "</col>  " +
                "<col fg=\"cyan\">" + dmlEscape(padRight(protection, 11)) + "</col>  " +
                "<col fg=\"magenta\">" + dmlEscape(padRight(type, 8)) + "</col>  " +
                "<col fg=\"white\">" + dmlEscape(padRight(moduleName, 16)) + "</col>  " +
                dmlCommandLink("dump", "!wef.hexdump " + baseText + " L80") + " " +
                dmlCommandLink("search", "!wef.search_pattern hex:4141 " + baseText + " L" + formatHex(size)) + " " +
                dmlCommandLink("contains", "!wef.vmmap -contains " + baseText);
            out.dmlLine(
                line,
                dml);
            printed = true;
            ++printedCount;
            if (filter.limit != 0 && printedCount >= filter.limit) {
                break;
            }
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
