# WEF

WEF is a native C++ WinDbg extension DLL that adds GEF-like debugger ergonomics while preserving normal WinDbg command behavior.

**Note: Currently only support amd64/x86-64 based systems and binaries!**

## Build

Requirements:

- Visual Studio 2026 with **Desktop development with C++**
- Debugging Tools for Windows

Open `wef.sln` and build `Debug|x64` or `Release|x64`.

Output:

```text
x64\Debug\wef.dll
x64\Release\wef.dll
```

The project expects the DbgEng SDK here:

```text
C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\sdk\
```

If your SDK is installed elsewhere, open `wef.vcxproj` and change the `DbgSdkDir` user macro.

## Load

### From source build

Build `Release|x64`, then copy:

```text
x64\Release\wef.dll
```

to:

```text
%LOCALAPPDATA%\Dbg\EngineExtensions\wef.dll
```

### From GitHub release

Download the compiled `wef.dll` from the GitHub release, then copy it to:

```text
%LOCALAPPDATA%\Dbg\EngineExtensions\wef.dll
```

You can also keep it anywhere and load it by absolute path:

```text
.load C:\dev\wef\x64\Release\wef.dll
!wef
```

Restart WinDbg and run:

```text
!wef
```

If your WinDbg build does not auto-load it by module name, run `.load wef` once in that session.

UserExtensions gallery files are also available under `manifest/`, but the simple `EngineExtensions` install is usually better for this native DLL.

Aliases are opt-in:

```text
!wef.install
ctx
telescope @rsp L8
!wef.uninstall
```

Canonical commands like `!wef.ctx` always work, even without aliases.

## Commands

```text
!wef
!wef.install
!wef.uninstall
!wef.aliases
!wef.ctx
!wef.telescope [addr] L<count>
!wef.hexdump <addr> L<size>
!wef.vmmap
!wef.checksec
!wef.heaps [L<count>|heap-address]       (alias: wef-heap)
!wef.vis [L<count>|heap-address]
!vis [L<count>|heap-address]
!wef.config get [key]
!wef.config set <key> <value>
```

Aliases installed by `!wef.install`:

```text
ctx
telescope
hexdump
vmmap
checksec
wef-heap
vis
wef-config
```

## Config

Config is in-memory only and resets when WinDbg exits or the extension reloads.

```text
!wef.config get
!wef.config set output.use_dml false
```

Defaults:

```text
output.use_dml = true
ctx.stack.count = 20
ctx.code.lines = 8
ctx.regs.show_flags = true
dereference.depth = 3
heap.max_count = 80
heap.vis.max_chunks = 160
heap.vis.max_segments = 16
telescope.count = 20
hexdump.size = 100
```

`output.use_dml` controls colored DML output.
