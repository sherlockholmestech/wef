#include "memory.hpp"

#include "config.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <vector>

namespace wef {

namespace {

bool evalAddress(DbgSession& session, std::string_view text, ULONG64& value) {
    return session.evaluate(text, value) || parseNumber(text, value);
}

bool isPrintableAscii(unsigned char ch) {
    return ch >= 0x20 && ch <= 0x7e;
}

void telescopeUsage(const Output& out) {
    out.line("usage: !wef.telescope [<addr>] [L<count>]");
}

void hexdumpUsage(const Output& out) {
    out.line("usage: !wef.hexdump <addr> [L<size>] [-force]");
}

std::string byteToHex(unsigned char value) {
    std::ostringstream stream;
    stream << std::hex << std::nouppercase << std::setfill('0') << std::setw(2) << static_cast<unsigned>(value);
    return stream.str();
}

std::string padRight(std::string text, size_t width) {
    if (text.size() >= width) {
        return text;
    }
    text.append(width - text.size(), ' ');
    return text;
}

}

std::string previewString(DbgSession& session, ULONG64 address) {
    if (address == 0) {
        return {};
    }

    unsigned char buffer[96] = {};
    ULONG bytesRead = 0;
    if (FAILED(session.readVirtual(address, buffer, sizeof(buffer), &bytesRead)) || bytesRead < 4) {
        return {};
    }

    std::string ascii;
    for (ULONG i = 0; i < bytesRead; ++i) {
        if (buffer[i] == 0) {
            break;
        }
        if (!isPrintableAscii(buffer[i])) {
            ascii.clear();
            break;
        }
        ascii.push_back(static_cast<char>(buffer[i]));
        if (ascii.size() >= 48) {
            break;
        }
    }
    if (ascii.size() >= 4) {
        return "\"" + ascii + "\"";
    }

    std::string wide;
    for (ULONG i = 0; i + 1 < bytesRead; i += 2) {
        const unsigned char lo = buffer[i];
        const unsigned char hi = buffer[i + 1];
        if (lo == 0 && hi == 0) {
            break;
        }
        if (hi != 0 || !isPrintableAscii(lo)) {
            wide.clear();
            break;
        }
        wide.push_back(static_cast<char>(lo));
        if (wide.size() >= 48) {
            break;
        }
    }
    if (wide.size() >= 4) {
        return "L\"" + wide + "\"";
    }

    return {};
}

std::string describePointer(DbgSession& session, ULONG64 value) {
    if (value == 0) {
        return "NULL";
    }

    const std::string symbol = session.resolveSymbol(value);
    if (!symbol.empty()) {
        return symbol;
    }

    const std::string stringPreview = previewString(session, value);
    if (!stringPreview.empty()) {
        return stringPreview;
    }

    MEMORY_BASIC_INFORMATION64 info = {};
    if (session.queryVirtual(value, info)) {
        std::string region = stateToString(info.State);
        const std::string protection = protectionToString(info.Protect);
        const std::string type = typeToString(info.Type);
        if (!protection.empty()) {
            region += " " + protection;
        }
        if (!type.empty()) {
            region += " " + type;
        }

        const std::string moduleName = session.moduleNameForOffset(value);
        if (!moduleName.empty()) {
            region += " " + moduleName;
        }
        return region;
    }

    return "unmapped or unreadable";
}

std::string describePointerChain(DbgSession& session, ULONG64 value, ULONG64 depth) {
    std::string result = describePointer(session, value);
    ULONG64 current = value;

    for (ULONG64 i = 0; i < depth; ++i) {
        ULONG64 next = 0;
        if (!session.readPointer(current, next) || next == 0 || next == current) {
            break;
        }

        result += " -> " + formatAddress(next);
        const std::string final = describePointer(session, next);
        if (!final.empty() && final != "unmapped or unreadable") {
            result += " (" + final + ")";
            break;
        }
        current = next;
    }

    return result;
}

HRESULT runTelescope(DbgSession& session, const Output& out, const std::vector<std::string>& args) {
    ULONG64 address = 0;
    if (!session.readRegister("rsp", address) && !session.readRegister("esp", address)) {
        out.error("default stack pointer is unavailable; provide an explicit address\n");
        return S_OK;
    }

    ULONG64 count = configGetNumber("telescope.count", 20);
    bool gotAddress = false;
    for (const auto& arg : args) {
        ULONG64 parsed = 0;
        if (parseLengthToken(arg, parsed)) {
            count = parsed;
            continue;
        }
        if (gotAddress) {
            telescopeUsage(out);
            return S_OK;
        }
        if (!evalAddress(session, arg, parsed)) {
            out.error("could not evaluate address: " + arg + "\n");
            return S_OK;
        }
        address = parsed;
        gotAddress = true;
    }

    const ULONG ptrSize = session.pointerSize();
    out.heading("Telescope");
    out.line("  #    address             value               dereference");
    out.line("  --   -----------------   -----------------   ----------------------------------------");
    for (ULONG64 i = 0; i < count; ++i) {
        const ULONG64 current = address + (i * ptrSize);
        ULONG64 value = 0;
        const std::string slot = formatHex(i, 2);
        const std::string currentText = formatAddress(current);
        std::string line = "  " + padRight(slot, 4) + " " + currentText + "  ";
        if (!session.readPointer(current, value)) {
            line += "<read failed>";
            out.dmlLine(
                line,
                std::string("  <col fg=\"gray\">") + dmlEscape(padRight(slot, 4)) + "</col> " +
                    "<col fg=\"yellow\">" + dmlEscape(currentText) + "</col>  " +
                    "<col fg=\"red\">&lt;read failed&gt;</col>");
            continue;
        }
        const std::string desc = describePointerChain(session, value, configGetNumber("dereference.depth", 3));
        const std::string valueText = formatAddress(value);
        line += valueText + "  " + desc;
        const std::string dml =
            std::string("  <col fg=\"gray\">") + dmlEscape(padRight(slot, 4)) + "</col> " +
            "<col fg=\"yellow\">" + dmlEscape(currentText) + "</col>  " +
            "<col fg=\"white\">" + dmlEscape(valueText) + "</col>  " +
            "<col fg=\"cyan\">" + dmlEscape(desc) + "</col>";
        out.dmlLine(
            line,
            dml);
    }
    return S_OK;
}

HRESULT runHexdump(DbgSession& session, const Output& out, const std::vector<std::string>& args) {
    if (args.empty()) {
        hexdumpUsage(out);
        return S_OK;
    }

    ULONG64 address = 0;
    ULONG64 size = configGetNumber("hexdump.size", 0x100);
    bool force = false;
    bool gotAddress = false;

    for (const auto& arg : args) {
        if (arg == "-force") {
            force = true;
            continue;
        }

        ULONG64 parsed = 0;
        if (parseLengthToken(arg, parsed)) {
            size = parsed;
            continue;
        }

        if (gotAddress) {
            hexdumpUsage(out);
            return S_OK;
        }
        if (!evalAddress(session, arg, parsed)) {
            out.error("could not evaluate address: " + arg + "\n");
            return S_OK;
        }
        address = parsed;
        gotAddress = true;
    }

    if (!gotAddress) {
        hexdumpUsage(out);
        return S_OK;
    }

    if (!force && size > 0x1000) {
        out.error("read size exceeds 0x1000 bytes; pass -force to override\n");
        return S_OK;
    }

    out.heading("Hexdump");
    for (ULONG64 offset = 0; offset < size; offset += 16) {
        const ULONG lineSize = static_cast<ULONG>(std::min<ULONG64>(16, size - offset));
        unsigned char bytes[16] = {};
        ULONG bytesRead = 0;
        const HRESULT hr = session.readVirtual(address + offset, bytes, lineSize, &bytesRead);

        std::ostringstream line;
        line << formatAddress(address + offset) << "  ";
        for (ULONG i = 0; i < 16; ++i) {
            if (i < bytesRead) {
                line << byteToHex(bytes[i]);
            } else if (i < lineSize) {
                line << "??";
            } else {
                line << "  ";
            }
            line << ' ';
        }

        line << " ";
        for (ULONG i = 0; i < bytesRead; ++i) {
            line << (isPrintableAscii(bytes[i]) ? static_cast<char>(bytes[i]) : '.');
        }
        if (FAILED(hr) || bytesRead < lineSize) {
            line << "  <read failed at " << formatAddress(address + offset + bytesRead)
                 << " for 0x" << formatHex(lineSize - bytesRead) << " bytes>";
        }
        const std::string plain = line.str();
        const std::string dml =
            std::string("<col fg=\"yellow\">") + dmlEscape(formatAddress(address + offset)) + "</col>  " +
            dmlEscape(plain.substr(20));
        out.dmlLine(
            plain,
            dml);
    }
    return S_OK;
}

}
