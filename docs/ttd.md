# TTD Event Explorer

WEF includes a lightweight Time Travel Debugging Event Explorer built around WinDbg's TTD data model. The commands emit `dx -g` grids, so results can be sorted, filtered, searched, and expanded in WinDbg Preview.

TTD commands require a loaded TTD trace.

## Commands

```text
!wef.ttd calls <module!function|pattern> [L<count>]
!wef.ttd exceptions [L<count>]
!wef.ttd memory <addr> L<size> [-r|-w|-x|-rw|-all] [-take <count>]
!wef.ttd allocs [addr] [L<size>|L<count>]
!wef.ttd timeline [L<count>]
!wef.ttd gui|grid
```

## GUI Entry Point

```text
!wef.ttd gui
!wef.ttd grid
```

The GUI entry point prints clickable DML buttons for common TTD grids:

- Events
- Exceptions
- Allocation-related APIs
- Calls template
- Memory template

These are command links around normal `dx -g` expressions and WEF TTD commands.

## Calls Timeline

```text
!wef.ttd calls ntdll!RtlAllocateHeap
!wef.ttd calls kernelbase!VirtualAlloc L50
!wef.ttd calls user32!MessageBoxW
```

This queries:

```text
@$cursession.TTD.Calls(...)
```

Use this when you know the API or symbol pattern you want to inspect.

Common useful call patterns:

```text
ntdll!RtlAllocateHeap
ntdll!RtlFreeHeap
kernelbase!HeapAlloc
kernelbase!HeapFree
kernelbase!VirtualAlloc
kernelbase!VirtualFree
kernelbase!WriteProcessMemory
kernelbase!CreateFileW
kernelbase!ReadFile
kernelbase!WriteFile
```

## Exceptions

```text
!wef.ttd exceptions
!wef.ttd exceptions L20
```

This shows exception events from the trace. It is useful for quickly jumping to crashes, first-chance exceptions, access violations, and breakpoint events.

## Memory Access Timeline

```text
!wef.ttd memory <addr> L<size> [-r|-w|-x|-rw|-all] [-take <count>]
```

Examples:

```text
!wef.ttd memory @rsp L80 -rw
!wef.ttd memory 00000000`0019f000 L100 -w
!wef.ttd memory <addr> L8 -all -take 50
```

Access filters:

```text
-r    reads
-w    writes
-x    execution
-rw   reads and writes
-all  reads, writes, and execution
```

This uses:

```text
@$cursession.TTD.Memory(address, size, access)
```

## Allocation Explorer

```text
!wef.ttd allocs
!wef.ttd allocs L20
!wef.ttd allocs <addr> L<size>
```

Without an address, this emits grids for allocation-related APIs:

- `ntdll!RtlAllocateHeap`
- `ntdll!RtlFreeHeap`
- `kernelbase!HeapAlloc`
- `kernelbase!HeapFree`
- `kernel32!HeapAlloc`
- `kernel32!HeapFree`
- `kernelbase!VirtualAlloc`
- `kernelbase!VirtualFree`
- `kernel32!VirtualAlloc`
- `kernel32!VirtualFree`

With an address, WEF first shows writes to that range and then emits allocation/free API grids for nearby manual correlation:

```text
!wef.ttd allocs <addr> L20
```

This is useful for reconstructing a heap object's lifetime in a trace.

## Timeline Overview

```text
!wef.ttd timeline
!wef.ttd timeline L50
```

The timeline command emits an exception grid followed by allocation API grids. It is intended as a first-pass triage view.

## Recommended TTD Workflows

### Start from a crash

```text
!wef.ttd exceptions
```

Open the relevant exception row, then inspect registers and memory at that position.

### Track writes to a corrupted address

```text
!wef.ttd memory <addr> L8 -w -take 100
```

### Track heap allocation APIs

```text
!wef.ttd allocs
```

### Track one suspected object

```text
!wef.chunk <addr>
!wef.ttd allocs <addr> L<size>
```

Use `!wef.chunk` first to determine a sensible user-data address and size.

## Notes and Limitations

- TTD commands depend on WinDbg's TTD data model.
- They are most useful in WinDbg Preview where `dx -g` grids are interactive.
- WEF does not currently install a custom data model provider. It emits standard `dx -g` queries and DML links.
- API names differ between Windows versions and import forwarding layers. If one API grid is empty, try related APIs such as `kernel32!HeapAlloc`, `kernelbase!HeapAlloc`, and `ntdll!RtlAllocateHeap`.
