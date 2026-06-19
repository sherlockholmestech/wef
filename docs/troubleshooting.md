# Troubleshooting

This page lists common WEF problems and likely fixes.

## `!wef` Is Not Found

Load the extension explicitly:

```text
.load C:\path\to\wef.dll
!wef
```

If using the automatic engine extension path, confirm the DLL is here:

```text
%LOCALAPPDATA%\Dbg\EngineExtensions\wef.dll
```

Restart WinDbg after copying the DLL.

## Aliases Do Not Work

Aliases are opt-in. Install them:

```text
!wef.install
```

Canonical commands always work:

```text
!wef.ctx
!wef.vmmap
!wef.heaps
```

Show mappings:

```text
!wef.aliases
```

Remove aliases:

```text
!wef.uninstall
```

## DML Buttons Do Not Render

Make sure DML is enabled:

```text
!wef.config set output.use_dml true
```

Some debugger frontends or logs may show DML markup as text. Disable DML for plain output:

```text
!wef.config set output.use_dml false
```

## Heap Commands Cannot Read Heap Metadata

Heap internals require user-mode state and useful `ntdll` type symbols.

Try:

```text
.reload /f ntdll.dll
!sym noisy
!wef.heaps
```

If symbols are unavailable or private layouts differ, WEF may still list heaps but fail to walk segments or chunks.

## `!wef.vis` Shows No Chunks

Common causes:

- The heap uses LFH metadata that does not appear as simple backend chunks.
- Symbols for `_HEAP`, `_HEAP_SEGMENT`, or `_HEAP_ENTRY` are missing.
- The segment walk hit configured limits.
- The selected heap address is not a backend heap.

Try:

```text
!wef.heaps flags <heap-address>
!wef.heaps segments <heap-address>
!wef.heaps chunks <heap-address>
```

You can raise limits:

```text
!wef.config set heap.vis.max_segments 32
!wef.config set heap.vis.max_chunks 320
!wef.config set heap.chunk.max_search_chunks 8192
```

## Search Is Slow

Whole-process memory searches can be expensive. Narrow the range or add filters:

```text
!wef.search_pattern hex:41414141 @rsp L4000
!wef.search_pattern marker -private -w -max 20
!wef.search_pattern hex:cc -x -max 20
```

## Search Does Not Find A String With Spaces

WinDbg extension arguments are split on whitespace. Use a marker without spaces, search a shorter token, or use hex bytes:

```text
!wef.search_pattern hex:77656620737461626c65
```

## TTD Commands Fail

TTD commands require a loaded TTD trace and WinDbg's TTD data model.

Start with:

```text
!wef.ttd gui
dx -g @$cursession.TTD.Events
```

If the raw `dx` query fails, the trace or debugger session does not expose the expected TTD model.

## TTD Call Grid Is Empty

Try related API layers. For heap allocations, any of these might be relevant:

```text
!wef.ttd calls ntdll!RtlAllocateHeap
!wef.ttd calls kernelbase!HeapAlloc
!wef.ttd calls kernel32!HeapAlloc
```

Forwarding and implementation details vary between Windows versions.

## Large Output Is Too Noisy

Use limits:

```text
!wef.vmmap L20
!wef.heaps L10
!wef.heaps chunks <heap-address>
!wef.search_pattern marker -max 10
!wef.ttd exceptions L20
```

Use DML buttons to drill into specific rows instead of dumping everything at once.

## Build Issues

WEF is a Windows native DLL project. Build it in Visual Studio with the Debugging Tools SDK installed. The project expects:

```text
C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\sdk\
```

If your SDK is elsewhere, update `DbgSdkDir` in `wef.vcxproj`.
