#include "ttd.hpp"

#include "config.hpp"

#include <string>
#include <vector>

namespace wef {

namespace {

std::string jsQuote(std::string_view text) {
    std::string quoted = "\"";
    for (const char ch : text) {
        if (ch == '\\' || ch == '"') {
            quoted += '\\';
        }
        quoted += ch;
    }
    quoted += '"';
    return quoted;
}

bool evalAddress(DbgSession& session, const std::string& text, ULONG64& value) {
    return session.evaluate(text, value) || parseNumber(text, value);
}

std::string takeSuffix(ULONG64 count) {
    return ".Take(" + std::to_string(count) + ")";
}

std::string dmlColor(std::string_view text, std::string_view color) {
    return "<col fg=\"" + std::string(color) + "\">" + dmlEscape(text) + "</col>";
}

HRESULT executeDx(DbgSession& session, const Output& out, std::string_view label, const std::string& expression, ULONG recurse = 2) {
    const std::string command = "dx -g -r" + std::to_string(recurse) + " " + expression;
    out.dmlLine("  [" + std::string(label) + "]", "  " + dmlCommandLink(label, command));
    return session.control()->Execute(DEBUG_OUTCTL_ALL_CLIENTS, command.c_str(), DEBUG_EXECUTE_DEFAULT);
}

void usage(const Output& out) {
    out.heading("TTD Event Explorer");
    out.line("usage:");
    out.line("  !wef.ttd calls <module!function|pattern> [L<count>]");
    out.line("  !wef.ttd exceptions [L<count>]");
    out.line("  !wef.ttd memory <addr> L<size> [-r|-w|-x|-rw|-all] [-take <count>]");
    out.line("  !wef.ttd allocs [addr] [L<size>|L<count>]");
    out.line("  !wef.ttd timeline [L<count>]");
    out.line("  !wef.ttd gui|grid");
    out.blank();
    out.line("These commands query the WinDbg TTD data model and require a loaded TTD trace.");
}

ULONG64 parseCount(const std::vector<std::string>& args, size_t start, ULONG64 fallback) {
    ULONG64 count = fallback;
    for (size_t i = start; i < args.size(); ++i) {
        ULONG64 parsed = 0;
        if (parseLengthToken(args[i], parsed)) {
            count = parsed;
        }
    }
    return count;
}

HRESULT runCalls(DbgSession& session, const Output& out, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        out.line("usage: !wef.ttd calls <module!function|pattern> [L<count>]");
        return S_OK;
    }

    const ULONG64 count = parseCount(args, 2, configGetNumber("ttd.max_events", 200));
    return executeDx(session, out, "calls  " + args[1], "@$cursession.TTD.Calls(" + jsQuote(args[1]) + ")" + takeSuffix(count), 3);
}

HRESULT runExceptions(DbgSession& session, const Output& out, const std::vector<std::string>& args) {
    const ULONG64 count = parseCount(args, 1, configGetNumber("ttd.max_events", 200));
    return executeDx(session, out, "exceptions", "@$cursession.TTD.Events.Where(e => e.Type == \"Exception\")" + takeSuffix(count), 3);
}

HRESULT runMemory(DbgSession& session, const Output& out, const std::vector<std::string>& args) {
    if (args.size() < 3) {
        out.line("usage: !wef.ttd memory <addr> L<size> [-r|-w|-x|-rw|-all] [-take <count>]");
        return S_OK;
    }

    ULONG64 address = 0;
    if (!evalAddress(session, args[1], address)) {
        out.error("could not evaluate address: " + args[1] + "\n");
        return S_OK;
    }

    ULONG64 size = 0;
    if (!parseLengthToken(args[2], size) && !evalAddress(session, args[2], size)) {
        out.error("could not parse memory range size: " + args[2] + "\n");
        return S_OK;
    }

    std::string access = "rw";
    ULONG64 count = configGetNumber("ttd.max_events", 200);
    for (size_t i = 3; i < args.size(); ++i) {
        if (args[i] == "-r") {
            access = "r";
        } else if (args[i] == "-w") {
            access = "w";
        } else if (args[i] == "-x") {
            access = "x";
        } else if (args[i] == "-rw") {
            access = "rw";
        } else if (args[i] == "-all") {
            access = "rwx";
        } else if (args[i] == "-take" && i + 1 < args.size()) {
            ULONG64 parsed = 0;
            if (parseNumber(args[i + 1], parsed)) {
                count = parsed;
            }
            ++i;
        }
    }

    return executeDx(
        session,
        out,
        "memory  " + formatAddress(address) + " L" + formatHex(size) + " " + access,
        "@$cursession.TTD.Memory(0x" + formatHex(address) + ", 0x" + formatHex(size) + ", " + jsQuote(access) + ")" + takeSuffix(count),
        3);
}

HRESULT runAllocs(DbgSession& session, const Output& out, const std::vector<std::string>& args) {
    ULONG64 address = 0;
    bool hasAddress = args.size() > 1 && evalAddress(session, args[1], address);
    ULONG64 count = parseCount(args, hasAddress ? 2 : 1, configGetNumber("ttd.max_events", 200));
    ULONG64 size = session.pointerSize();
    if (hasAddress) {
        for (size_t i = 2; i < args.size(); ++i) {
            ULONG64 parsed = 0;
            if (parseLengthToken(args[i], parsed)) {
                size = parsed;
            }
        }
    }

    if (hasAddress) {
        out.heading("TTD Allocation Lifetime");
        HRESULT hr = executeDx(
            session,
            out,
            "memory writes  " + formatAddress(address),
            "@$cursession.TTD.Memory(0x" + formatHex(address) + ", 0x" + formatHex(size) + ", \"w\")" + takeSuffix(count),
            3);
        if (FAILED(hr)) {
            return hr;
        }
        out.line("  allocation/free APIs around the same trace");
    } else {
        out.heading("TTD Allocation Calls");
    }

    constexpr const char* patterns[] = {
        "ntdll!RtlAllocateHeap",
        "ntdll!RtlFreeHeap",
        "kernelbase!HeapAlloc",
        "kernelbase!HeapFree",
        "kernel32!HeapAlloc",
        "kernel32!HeapFree",
        "kernelbase!VirtualAlloc",
        "kernelbase!VirtualFree",
        "kernel32!VirtualAlloc",
        "kernel32!VirtualFree",
    };

    for (const char* pattern : patterns) {
        const HRESULT hr = executeDx(session, out, pattern, "@$cursession.TTD.Calls(" + jsQuote(pattern) + ")" + takeSuffix(count), 2);
        if (FAILED(hr)) {
            return hr;
        }
    }
    return S_OK;
}

HRESULT runTimeline(DbgSession& session, const Output& out, const std::vector<std::string>& args) {
    const ULONG64 count = parseCount(args, 1, configGetNumber("ttd.max_events", 200));
    out.heading("TTD Timeline");
    out.line("  Exceptions");
    HRESULT hr = executeDx(session, out, "exceptions", "@$cursession.TTD.Events.Where(e => e.Type == \"Exception\")" + takeSuffix(count), 3);
    if (FAILED(hr)) {
        return hr;
    }
    out.line("  Heap and virtual allocation APIs");
    return runAllocs(session, out, {"allocs", "L" + std::to_string(count)});
}

HRESULT runGui(DbgSession&, const Output& out) {
    out.heading("TTD Event Explorer GUI");
    out.line("  The native WEF command exposes the same timeline sources as searchable dx grids.");
    out.line("  Use WinDbg Preview's grid filtering on the emitted dx tables for an interactive GUI-like explorer.");
    out.dmlLine(
        "  buttons: [events] [exceptions] [allocs] [calls template] [memory template]",
        "  " + dmlColor("buttons", "gray") + ": " +
            dmlCommandLink("events", "dx -g @$cursession.TTD.Events") + " " +
            dmlCommandLink("exceptions", "!wef.ttd exceptions") + " " +
            dmlCommandLink("allocs", "!wef.ttd allocs") + " " +
            dmlCommandLink("calls template", "!wef.ttd calls ntdll!RtlAllocateHeap") + " " +
            dmlCommandLink("memory template", "dx -g @$cursession.TTD.Memory(@rsp, 0x80, \"rw\")"));
    return S_OK;
}

}

HRESULT runTtd(DbgSession& session, const Output& out, const std::vector<std::string>& args) {
    if (args.empty() || args[0] == "help" || args[0] == "-h" || args[0] == "/?") {
        usage(out);
        return S_OK;
    }

    if (args[0] == "calls") {
        return runCalls(session, out, args);
    }
    if (args[0] == "exceptions") {
        return runExceptions(session, out, args);
    }
    if (args[0] == "memory") {
        return runMemory(session, out, args);
    }
    if (args[0] == "allocs") {
        return runAllocs(session, out, args);
    }
    if (args[0] == "timeline") {
        return runTimeline(session, out, args);
    }
    if (args[0] == "gui" || args[0] == "grid") {
        return runGui(session, out);
    }

    usage(out);
    return S_OK;
}

}
