#include "command_router.hpp"

#include "aliases.hpp"
#include "config.hpp"
#include "context.hpp"
#include "heap.hpp"
#include "memory.hpp"
#include "modules.hpp"
#include "pe_security.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace wef {

namespace {

HRESULT runHelp(DbgSession&, const Output& out, const std::vector<std::string>&) {
    out.heading("WEF WinDbg Extension");
    out.blank();
    out.heading("Canonical commands:");
    out.line("  !wef");
    out.line("  !wef.install");
    out.line("  !wef.uninstall");
    out.line("  !wef.aliases");
    out.line("  !wef.ctx [-full] [-regs] [-stack L<count>] [-code L<count>]");
    out.line("  !wef.telescope [<addr>] [L<count>]");
    out.line("  !wef.hexdump <addr> [L<size>] [-force]");
    out.line("  !wef.vmmap");
    out.line("  !wef.checksec [module]");
    out.line("  !wef.heaps [L<count>|heap-address]    (alias: wef-heap)");
    out.line("  !wef.vis [L<count>|heap-address]");
    out.line("  !vis [L<count>|heap-address]");
    out.line("  !wef.config get [key]");
    out.line("  !wef.config set <key> <value>");
    out.blank();
    out.line("Run !wef.install to add opt-in bare aliases: ctx, telescope, hexdump, vmmap, checksec, vis, wef-config.");
    return S_OK;
}

bool startsWithLengthPrefix(std::string_view token) {
    return !token.empty() && (token[0] == 'L' || token[0] == 'l');
}

}

HRESULT executeCommand(IDebugClient* client, PCSTR args, CommandHandler handler) {
    try {
        DbgSession session(client);
        Output out(session.control());
        return handler(session, out, splitArgs(args));
    } catch (const HResultError& error) {
        if (client != nullptr) {
            IDebugControl* rawControl = nullptr;
            if (SUCCEEDED(client->QueryInterface(__uuidof(IDebugControl), reinterpret_cast<void**>(&rawControl))) && rawControl != nullptr) {
                Output out(rawControl);
                out.error(std::string(error.what()) + ": " + formatHResult(error.hr()) + "\n");
                rawControl->Release();
            }
        }
        return error.hr();
    } catch (const std::exception& error) {
        if (client != nullptr) {
            IDebugControl* rawControl = nullptr;
            if (SUCCEEDED(client->QueryInterface(__uuidof(IDebugControl), reinterpret_cast<void**>(&rawControl))) && rawControl != nullptr) {
                Output out(rawControl);
                out.error(std::string("internal error: ") + error.what() + "\n");
                rawControl->Release();
            }
        }
        return S_OK;
    }
}

std::vector<std::string> splitArgs(PCSTR args) {
    std::vector<std::string> result;
    if (args == nullptr) {
        return result;
    }

    std::istringstream stream(args);
    std::string item;
    while (stream >> item) {
        result.push_back(item);
    }
    return result;
}

bool parseNumber(std::string_view text, ULONG64& value) {
    std::string cleaned;
    cleaned.reserve(text.size());
    for (const char ch : text) {
        if (ch != '`') {
            cleaned.push_back(ch);
        }
    }
    if (cleaned.empty()) {
        return false;
    }

    int base = 16;
    if (cleaned.size() > 2 && cleaned[0] == '0' && (cleaned[1] == 'x' || cleaned[1] == 'X')) {
        base = 16;
    }

    char* end = nullptr;
    value = std::strtoull(cleaned.c_str(), &end, base);
    return end != nullptr && *end == '\0';
}

bool parseLengthToken(std::string_view token, ULONG64& value) {
    if (!startsWithLengthPrefix(token)) {
        return false;
    }
    return parseNumber(token.substr(1), value);
}

std::string joinArgs(const std::vector<std::string>& args, size_t start, size_t end) {
    if (start >= args.size() || start >= end) {
        return {};
    }

    end = std::min(end, args.size());
    std::string joined;
    for (size_t i = start; i < end; ++i) {
        if (!joined.empty()) {
            joined += ' ';
        }
        joined += args[i];
    }
    return joined;
}

namespace commands {

HRESULT WefHelp(IDebugClient* client, PCSTR args) {
    return executeCommand(client, args, runHelp);
}

HRESULT Install(IDebugClient* client, PCSTR args) {
    return executeCommand(client, args, runInstall);
}

HRESULT Uninstall(IDebugClient* client, PCSTR args) {
    return executeCommand(client, args, runUninstall);
}

HRESULT Aliases(IDebugClient* client, PCSTR args) {
    return executeCommand(client, args, runAliases);
}

HRESULT Ctx(IDebugClient* client, PCSTR args) {
    return executeCommand(client, args, runCtx);
}

HRESULT Telescope(IDebugClient* client, PCSTR args) {
    return executeCommand(client, args, runTelescope);
}

HRESULT Hexdump(IDebugClient* client, PCSTR args) {
    return executeCommand(client, args, runHexdump);
}

HRESULT Vmmap(IDebugClient* client, PCSTR args) {
    return executeCommand(client, args, runVmmap);
}

HRESULT Checksec(IDebugClient* client, PCSTR args) {
    return executeCommand(client, args, runChecksec);
}

HRESULT Heaps(IDebugClient* client, PCSTR args) {
    return executeCommand(client, args, runHeap);
}

HRESULT Vis(IDebugClient* client, PCSTR args) {
    return executeCommand(client, args, runHeapVis);
}

HRESULT Config(IDebugClient* client, PCSTR args) {
    return executeCommand(client, args, runConfig);
}

}

}
