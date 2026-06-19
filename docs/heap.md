# Heap Workflows

WEF heap commands provide GEF-adjacent heap inspection for Windows process heaps. They rely on PEB heap lists and `ntdll` heap type symbols where available.

## Quick Workflow

```text
!wef.heaps
!wef.vis
!wef.chunk <addr>
!wef.heaps find <addr|text>
!wef.heaps validate
```

## List Process Heaps

```text
!wef.heaps
!wef.heaps L20
!wef.heaps <heap-address>
```

The table includes:

- Heap index
- Heap address
- `_HEAP.Flags`
- `_HEAP.ForceFlags`
- Front-end heap type, for example backend, lookaside, or LFH
- Virtual memory region summary

## Visualize Heaps

```text
!wef.vis
!wef.vis L4
!wef.vis <heap-address>
```

`!wef.vis` is intentionally compact. It shows:

- Heap address
- Chunk count
- Busy/free counts and byte totals
- A compact map
- A small table of largest chunks

Map legend:

```text
A = allocated or busy
F = free
V = virtual allocation chunk
M = mixed bucket
```

Use DML buttons from the visual view:

- `[chunks]` opens the full chunk table
- `[segments]` lists segments
- `[flags]` decodes heap flags
- `[validate]` runs lightweight structural checks

## Inspect One Chunk

```text
!wef.chunk <addr>
!wef.heaps chunk <addr>
```

The address can point at the chunk header or user data. WEF walks backend chunks and reports the chunk containing that address.

Output includes:

- Requested address
- Heap address
- Segment address
- Header address
- User-data address
- Total chunk size
- Usable size
- Previous size units
- Next chunk address
- Flags
- User-byte preview

Useful follow-up buttons:

- `[hexdump user]`
- `[telescope user]`
- `[heap chunks]`
- `[validate heap]`

## List Chunks

```text
!wef.heaps chunks
!wef.heaps chunks <heap-address>
!wef.heaps chunks -busy <heap-address>
!wef.heaps chunks -free <heap-address>
!wef.heaps chunks -contains <addr>
```

The chunk table includes:

- Header address
- User address
- Size
- Usable size
- Decoded flags

`-contains <addr>` is useful when you want to find the owning chunk but still see surrounding heap context.

## Search Heap Contents

```text
!wef.heaps find <addr|text>
!wef.heaps find password
!wef.heaps find 0x4141414141414141
!wef.heaps find token <heap-address>
```

Numeric values are searched as pointer-sized little-endian values. Text values are searched as raw bytes.

Search limits are controlled by:

```text
heap.find.max_hits
heap.find.max_chunk_read
heap.chunk.max_search_chunks
```

## Show Segments

```text
!wef.heaps segments
!wef.heaps segments <heap-address>
```

Segment output includes:

- Owning heap
- Segment address
- First chunk entry
- Last valid entry
- Segment span
- Virtual memory region details

This is useful when a chunk walk stops early or when checking whether an address belongs to a backend segment.

## Decode Heap Flags

```text
!wef.heaps flags
!wef.heaps flags <heap-address>
```

Displays `_HEAP.Flags`, `_HEAP.ForceFlags`, decoded flag names, and front-end type. This is useful for identifying debugging options, validation flags, execute-enabled heaps, and front-end heap behavior.

## Validate Heap Structure

```text
!wef.heaps validate
!wef.heaps validate <heap-address>
```

Validation is intentionally lightweight and read-only. It checks walked backend chunks for obvious structural problems such as:

- Zero or misaligned sizes
- Overlapping chunks
- Walk truncation due to limits or unreadable metadata

It does not call process-mutating heap validation APIs.

## LFH Notes

Modern Windows often uses LFH. Some LFH state is not represented as simple backend chunks, and private symbols vary between Windows versions. WEF reports what it can walk safely and warns when LFH or missing symbols may hide backend chunks.

## Common Heap Triage Recipes

### Find the chunk for a crash pointer

```text
!wef.chunk @rax
```

### Search all heaps for a marker

```text
!wef.heaps find marker
```

### Visualize the first few heaps

```text
!wef.vis L4
```

### Show only busy chunks in one heap

```text
!wef.heaps chunks -busy <heap-address>
```

### Check a heap before digging deeper

```text
!wef.heaps flags <heap-address>
!wef.heaps segments <heap-address>
!wef.heaps validate <heap-address>
```
