#include "context.hpp"

#include "config.hpp"
#include "memory.hpp"

#include <sstream>

namespace wef {

namespace {

std::string padRight(std::string text, size_t width) {
    if (text.size() >= width) {
        return text;
    }
    text.append(width - text.size(), ' ');
    return text;
}

void usage(const Output& out) {
    out.line("usage: !wef.ctx [-full] [-regs] [-stack L<count>] [-code L<count>]");
}

bool nextLength(const std::vector<std::string>& args, size_t& index, ULONG64& value) {
    if (index + 1 >= args.size()) {
        return false;
    }
    ++index;
    return parseLengthToken(args[index], value);
}

void printRegisters(DbgSession& session, const Output& out) {
    out.heading("Registers");
    const auto regs = session.readCommonRegisters();
    if (regs.empty()) {
        out.line("  unavailable: register interface returned no common registers");
        return;
    }

    std::string line = "  ";
    size_t columns = 0;
    for (const auto& reg : regs) {
        const std::string item = padRight(reg.name, 4) + " " + formatAddress(reg.value);
        line += padRight(item, 27);
        ++columns;
        if (columns == 3) {
            out.line(line);
            line = "  ";
            columns = 0;
        }
    }
    if (columns != 0) {
        out.line(line);
    }
}

void printCode(DbgSession& session, const Output& out, ULONG64 ip, ULONG64 codeLines) {
    out.heading("Code");
    if (ip == 0) {
        out.line("  unavailable: instruction pointer is unavailable");
        return;
    }

    const std::string symbol = session.resolveSymbol(ip);
    if (!symbol.empty()) {
        out.dmlLine(
            "  symbol: " + symbol,
            std::string("  <col fg=\"green\">symbol:</col> <col fg=\"cyan\">") + dmlEscape(symbol) + "</col>");
    }

    ULONG64 current = ip;
    for (ULONG64 i = 0; i < codeLines; ++i) {
        ULONG64 next = current;
        const std::string disasm = session.disassembleLine(current, next);
        if (disasm.empty()) {
            out.line("  " + formatAddress(current) + "  disassembly unavailable");
            break;
        }
        const std::string marker = i == 0 ? "=> " : "   ";
        out.normal(marker);
        out.normal(disasm);
        if (!disasm.empty() && disasm.back() != '\n') {
            out.blank();
        }
        if (next == current) {
            break;
        }
        current = next;
    }
}

void printStack(DbgSession& session, const Output& out, std::string_view spName, ULONG64 sp, ULONG64 count) {
    out.heading("Stack");
    if (sp == 0) {
        out.line("  unavailable: stack pointer is unavailable");
        return;
    }

    const ULONG ptrSize = session.pointerSize();
    out.line("  #    offset        address             value               dereference");
    out.line("  --   ------------  -----------------   -----------------   ----------------------------------------");
    for (ULONG64 i = 0; i < count; ++i) {
        const ULONG64 current = sp + (i * ptrSize);
        ULONG64 value = 0;
        const std::string slot = formatHex(i, 2);
        const std::string offset = std::string(spName) + "+0x" + formatHex(i * ptrSize, 4);
        const std::string paddedSlot = padRight(slot, 4);
        const std::string paddedOffset = padRight(offset, 12);
        std::ostringstream line;
        line << "  " << paddedSlot << " " << paddedOffset << "  ";
        line << formatAddress(current) << "  ";
        if (!session.readPointer(current, value)) {
            line << "<read failed>";
            out.line(line.str());
            continue;
        }
        const std::string desc = describePointerChain(session, value, configGetNumber("dereference.depth", 3));
        line << formatAddress(value) << "  " << desc;
        out.line(line.str());
    }
}

}

HRESULT runCtx(DbgSession& session, const Output& out, const std::vector<std::string>& args) {
    ULONG64 stackCount = configGetNumber("ctx.stack.count", 20);
    ULONG64 codeLines = configGetNumber("ctx.code.lines", 8);

    for (size_t i = 0; i < args.size(); ++i) {
        const auto& arg = args[i];
        if (arg == "-full" || arg == "-regs") {
            continue;
        }
        if (arg == "-stack") {
            if (!nextLength(args, i, stackCount)) {
                usage(out);
                return S_OK;
            }
            continue;
        }
        if (arg == "-code") {
            if (!nextLength(args, i, codeLines)) {
                usage(out);
                return S_OK;
            }
            continue;
        }
        usage(out);
        return S_OK;
    }

    out.heading("WEF Context");
    const TargetInfo target = session.targetInfo();
    out.field("Target", target.description);
    out.field("Thread", session.currentThreadText());
    out.blank();

    ULONG64 ip = 0;
    ULONG64 sp = 0;
    std::string_view spName = "rsp";
    if (!session.readRegister("rip", ip)) {
        session.readRegister("eip", ip);
    }
    if (!session.readRegister("rsp", sp)) {
        if (session.readRegister("esp", sp)) {
            spName = "esp";
        }
    }

    printRegisters(session, out);
    out.blank();
    printCode(session, out, ip, codeLines);
    out.blank();
    printStack(session, out, spName, sp, stackCount);
    return S_OK;
}

}
