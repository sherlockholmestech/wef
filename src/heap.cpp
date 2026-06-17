#include "heap.hpp"

#include "config.hpp"

#include <algorithm>
#include <vector>

namespace wef {

namespace {

struct HeapSummary {
    ULONG index = 0;
    ULONG64 address = 0;
    ULONG flags = 0;
    ULONG forceFlags = 0;
    UCHAR frontEndType = 0;
    bool gotFlags = false;
    bool gotForceFlags = false;
    bool gotFrontEnd = false;
    MEMORY_BASIC_INFORMATION64 region = {};
    bool gotRegion = false;
};

template <typename T>
bool readValue(DbgSession& session, ULONG64 address, T& value) {
    ULONG bytesRead = 0;
    const HRESULT hr = session.readVirtual(address, &value, sizeof(T), &bytesRead);
    return SUCCEEDED(hr) && bytesRead == sizeof(T);
}

bool fieldOffset(DbgSession& session, PCSTR type, PCSTR field, ULONG& offset) {
    const std::string qualified(type);
    const size_t bang = qualified.find('!');
    if (bang == std::string::npos || bang == 0 || bang + 1 >= qualified.size()) {
        return false;
    }

    const std::string moduleName = qualified.substr(0, bang);
    const std::string typeName = qualified.substr(bang + 1);

    ULONG64 moduleBase = 0;
    if (FAILED(session.symbols()->GetModuleByModuleName(moduleName.c_str(), 0, nullptr, &moduleBase))) {
        return false;
    }

    ULONG typeId = 0;
    if (FAILED(session.symbols()->GetTypeId(moduleBase, typeName.c_str(), &typeId))) {
        return false;
    }

    return SUCCEEDED(session.symbols()->GetFieldOffset(moduleBase, typeId, field, &offset));
}

bool readFieldUlong(DbgSession& session, ULONG64 base, PCSTR type, PCSTR field, ULONG& value) {
    ULONG offset = 0;
    if (!fieldOffset(session, type, field, offset)) {
        return false;
    }
    return readValue(session, base + offset, value);
}

bool readFieldUchar(DbgSession& session, ULONG64 base, PCSTR type, PCSTR field, UCHAR& value) {
    ULONG offset = 0;
    if (!fieldOffset(session, type, field, offset)) {
        return false;
    }
    return readValue(session, base + offset, value);
}

bool readPebHeapList(DbgSession& session, ULONG64& heapsAddress, ULONG& heapCount) {
    ULONG64 peb = 0;
    if (!session.evaluate("@$peb", peb) || peb == 0) {
        return false;
    }

    ULONG countOffset = 0;
    ULONG heapsOffset = 0;
    if (!fieldOffset(session, "ntdll!_PEB", "NumberOfHeaps", countOffset) ||
        !fieldOffset(session, "ntdll!_PEB", "ProcessHeaps", heapsOffset)) {
        return false;
    }

    if (!readValue(session, peb + countOffset, heapCount)) {
        return false;
    }
    return session.readPointer(peb + heapsOffset, heapsAddress);
}

std::string heapKind(UCHAR frontEndType) {
    switch (frontEndType) {
    case 0:
        return "backend";
    case 1:
        return "lookaside";
    case 2:
        return "lfh";
    default:
        return "type-" + formatHex(frontEndType);
    }
}

HeapSummary inspectHeap(DbgSession& session, ULONG index, ULONG64 heapAddress) {
    HeapSummary heap;
    heap.index = index;
    heap.address = heapAddress;
    heap.gotFlags = readFieldUlong(session, heapAddress, "ntdll!_HEAP", "Flags", heap.flags);
    heap.gotForceFlags = readFieldUlong(session, heapAddress, "ntdll!_HEAP", "ForceFlags", heap.forceFlags);
    heap.gotFrontEnd = readFieldUchar(session, heapAddress, "ntdll!_HEAP", "FrontEndHeapType", heap.frontEndType);
    heap.gotRegion = session.queryVirtual(heapAddress, heap.region);
    return heap;
}

void printHeapRow(const Output& out, const HeapSummary& heap) {
    std::string line = "  ";
    line += formatHex(heap.index, 2);
    line += "   ";
    line += formatAddress(heap.address);
    line += "  ";
    line += heap.gotFlags ? "0x" + formatHex(heap.flags, 8) : "unavail   ";
    line += "  ";
    line += heap.gotForceFlags ? "0x" + formatHex(heap.forceFlags, 8) : "unavail   ";
    line += "  ";
    line += heap.gotFrontEnd ? heapKind(heap.frontEndType) : "unavail";
    line += "  ";
    if (heap.gotRegion) {
        line += stateToString(heap.region.State) + " " + protectionToString(heap.region.Protect) + " " + typeToString(heap.region.Type);
    } else {
        line += "unknown";
    }
    out.line(line);
}

std::vector<HeapSummary> collectHeaps(DbgSession& session, const Output& out, ULONG64 limit, ULONG64 singleHeap) {
    std::vector<HeapSummary> heaps;
    if (singleHeap != 0) {
        heaps.push_back(inspectHeap(session, 0, singleHeap));
        return heaps;
    }

    ULONG64 heapsAddress = 0;
    ULONG heapCount = 0;
    if (!readPebHeapList(session, heapsAddress, heapCount)) {
        out.error("could not read PEB heap list; ntdll symbols may be unavailable\n");
        return heaps;
    }

    if (heapCount == 0 || heapsAddress == 0) {
        out.warning("PEB reports no process heaps\n");
        return heaps;
    }

    const ULONG count = static_cast<ULONG>(heapCount > limit ? limit : heapCount);
    heaps.reserve(count);
    for (ULONG i = 0; i < count; ++i) {
        ULONG64 heapAddress = 0;
        if (!session.readPointer(heapsAddress + (i * session.pointerSize()), heapAddress)) {
            out.line("  " + formatHex(i, 2) + "   <read failed>");
            continue;
        }
        heaps.push_back(inspectHeap(session, i, heapAddress));
    }

    if (heapCount > limit) {
        out.warning("heap output truncated; use !wef.wef-heap L<count> to show more\n");
    }
    return heaps;
}

bool parseHeapArgs(DbgSession& session, const Output& out, const std::vector<std::string>& args, ULONG64& singleHeap, ULONG64& limit) {
    singleHeap = 0;
    limit = configGetNumber("heap.max_count", 80);
    for (const auto& arg : args) {
        ULONG64 parsed = 0;
        if (parseLengthToken(arg, parsed)) {
            limit = parsed;
            continue;
        }
        if (singleHeap != 0 || (!session.evaluate(arg, parsed) && !parseNumber(arg, parsed))) {
            out.line("usage:");
            out.line("  !wef.wef-heap");
            out.line("  !wef.wef-heap L<count>");
            out.line("  !wef.wef-heap <heap-address>");
            out.line("  short form: !wef-heap [L<count>|heap-address]");
            out.line("  !vis [L<count>|heap-address]");
            return false;
        }
        singleHeap = parsed;
    }
    return true;
}

void usage(const Output& out) {
    out.line("usage:");
    out.line("  !wef.wef-heap");
    out.line("  !wef.wef-heap L<count>");
    out.line("  !wef.wef-heap <heap-address>");
    out.line("  short form: !wef-heap [L<count>|heap-address]");
}

}

HRESULT runHeap(DbgSession& session, const Output& out, const std::vector<std::string>& args) {
    const TargetInfo target = session.targetInfo();
    if (!target.userMode) {
        out.error("heap requires a user-mode target\n");
        return S_OK;
    }

    ULONG64 singleHeap = 0;
    ULONG64 limit = 0;
    if (!parseHeapArgs(session, out, args, singleHeap, limit)) {
        return S_OK;
    }

    out.heading("Windows Heaps");
    out.line("  #    heap               flags       force      frontend  region");
    out.line("  --   -----------------  ----------  ----------  --------  ------------------------------");

    const auto heaps = collectHeaps(session, out, limit, singleHeap);
    for (const auto& heap : heaps) {
        printHeapRow(out, heap);
    }
    return S_OK;
}

HRESULT runHeapVis(DbgSession& session, const Output& out, const std::vector<std::string>& args) {
    const TargetInfo target = session.targetInfo();
    if (!target.userMode) {
        out.error("vis requires a user-mode target\n");
        return S_OK;
    }

    ULONG64 singleHeap = 0;
    ULONG64 limit = 0;
    if (!parseHeapArgs(session, out, args, singleHeap, limit)) {
        return S_OK;
    }

    auto heaps = collectHeaps(session, out, limit, singleHeap);
    std::sort(heaps.begin(), heaps.end(), [](const HeapSummary& left, const HeapSummary& right) {
        return left.address < right.address;
    });

    out.heading("Heap Visualization");
    if (heaps.empty()) {
        return S_OK;
    }

    for (const auto& heap : heaps) {
        out.line("  [" + formatHex(heap.index, 2) + "] " + formatAddress(heap.address));
        const std::string frontend = heap.gotFrontEnd ? heapKind(heap.frontEndType) : "unavailable";
        const std::string flags = heap.gotFlags ? "0x" + formatHex(heap.flags, 8) : "unavailable";
        const std::string forceFlags = heap.gotForceFlags ? "0x" + formatHex(heap.forceFlags, 8) : "unavailable";
        out.line("      frontend : " + frontend);
        out.line("      flags    : " + flags);
        out.line("      force    : " + forceFlags);
        if (!heap.gotRegion) {
            out.line("      region   : unavailable");
            out.blank();
            continue;
        }

        const ULONG64 base = heap.region.BaseAddress;
        const ULONG64 end = base + heap.region.RegionSize;
        out.line("      region   : " + formatAddress(base) + " - " + formatAddress(end) +
                 " (0x" + formatHex(heap.region.RegionSize) + ")");
        out.line("      memory   : [" + stateToString(heap.region.State) + "] [" +
                 protectionToString(heap.region.Protect) + "] [" + typeToString(heap.region.Type) + "]");
        out.line("      layout   : | HEAP HEADER | SEGMENTS / LFH / BACKEND ALLOCATIONS |");
        out.blank();
    }
    return S_OK;
}

}
