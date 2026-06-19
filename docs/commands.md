# Command Reference

This page lists WEF commands by category. Canonical commands use the `!wef.` namespace and do not require aliases.

## Top-Level Commands

```text
!wef
!wef.install
!wef.uninstall
!wef.aliases
!wef.config get [key]
!wef.config set <key> <value>
```

### `!wef`

Shows the command overview and quick-action DML buttons.

### `!wef.install`

Installs optional text replacement aliases for shorter commands.

### `!wef.uninstall`

Removes WEF aliases installed by `!wef.install`.

### `!wef.aliases`

Displays aliases and their canonical targets.

## Context Commands

```text
!wef.ctx [-full] [-regs] [-stack L<count>] [-code L<count>]
!wef.telescope [addr] L<count>
!wef.hexdump <addr> L<size> [-force]
```

### `!wef.ctx`

Displays:

- Target summary
- Current thread
- Registers
- Disassembly near the instruction pointer
- Stack entries with dereference annotations

Useful examples:

```text
!wef.ctx
!wef.ctx -full
!wef.ctx -stack L40
!wef.ctx -code L16
```

### `!wef.telescope`

Reads pointer-sized slots and annotates values as symbols, strings, memory regions, or pointer chains.

```text
!wef.telescope @rsp L20
!wef.telescope poi(@rsp) L8
```

### `!wef.hexdump`

Shows hex bytes and printable ASCII.

```text
!wef.hexdump @rsp L80
!wef.hexdump 00000000`0019f000 L200 -force
```

Reads larger than `0x1000` require `-force`.

## Memory Commands

```text
!wef.vmmap [-all|-free] [-commit|-reserve] [-x|-w|-rwx] [-image|-mapped|-private] [-module <name>] [-contains <addr>] [L<count>]
!wef.search_pattern <text|addr|hex:bytes> [start] [L<size>] [-max <count>] [-x|-w|-rwx] [-image|-mapped|-private]
```

### `!wef.vmmap`

Lists memory regions with base, end, size, state, protection, type, and module.

Examples:

```text
!wef.vmmap
!wef.vmmap -x
!wef.vmmap -w -private
!wef.vmmap -image -module ntdll
!wef.vmmap -contains @rsp
!wef.vmmap L20
```

Rows include DML buttons for dumping, searching, and focusing on a region.

### `!wef.search_pattern`

Searches committed readable memory. Supported pattern forms:

- Plain text token, for example `password`
- Explicit string token, for example `str:password`
- Hex bytes, for example `hex:41424344`
- Numeric expression or pointer-like value, for example `0x41414141` or `@rsp`

Examples:

```text
!wef.search_pattern hex:41414141
!wef.search_pattern str:password
!wef.search_pattern 0x4141414141414141
!wef.search_pattern hex:cc 00007ff6`00000000 L100000 -x
!wef.search_pattern token @rsp L1000 -max 10
```

Results include buttons for:

- `dump`, hexdump at the match
- `tel`, telescope at the match
- `chunk`, inspect containing heap chunk

## Pattern Commands

```text
!wef.pattern create <length>
!wef.pattern offset <value|ascii> [L<max-length>]
```

### `!wef.pattern create`

Creates a GEF/metasploit-style cyclic pattern:

```text
!wef.pattern create 512
```

### `!wef.pattern offset`

Finds the offset of an ASCII token or numeric value inside the cyclic pattern.

```text
!wef.pattern offset Aa3A L8192
!wef.pattern offset 0x41336141 L8192
```

## PE Security

```text
!wef.checksec [module]
```

Inspects PE security properties for the main module or a specified module.

```text
!wef.checksec
!wef.checksec ntdll
```

## Heap Commands

```text
!wef.heaps [L<count>|heap-address]
!wef.heaps flags [L<count>|heap-address]
!wef.heaps segments [L<count>|heap-address]
!wef.heaps chunks [-busy|-free] [-contains <addr>] [L<count>|heap-address]
!wef.heaps chunk <addr>
!wef.heaps find <addr|text> [L<count>|heap-address]
!wef.heaps validate [L<count>|heap-address]
!wef.chunk <addr>
!wef.vis [L<count>|heap-address]
!vis [L<count>|heap-address]
```

See [Heap Workflows](heap.md) for detailed examples.

## TTD Commands

```text
!wef.ttd calls <module!function|pattern> [L<count>]
!wef.ttd exceptions [L<count>]
!wef.ttd memory <addr> L<size> [-r|-w|-x|-rw|-all] [-take <count>]
!wef.ttd allocs [addr] [L<size>|L<count>]
!wef.ttd timeline [L<count>]
!wef.ttd gui|grid
```

See [TTD Event Explorer](ttd.md) for detailed usage.
