#include <windows.h>

#include <cstdio>

__declspec(noinline) void wef_breakpoint_fixture() {
    volatile unsigned long long marker = 0x1122334455667788ULL;
    const char ascii[] = "wef stable ascii string";
    const wchar_t wide[] = L"wef stable wide string";
    const void* pointer0 = const_cast<const unsigned long long*>(&marker);
    const void* pointer1 = ascii;
    const void* pointer2 = wide;

    std::printf(
        "marker=%p ascii=%p wide=%p pointers=%p,%p,%p\n",
        const_cast<const unsigned long long*>(&marker),
        static_cast<const void*>(ascii),
        static_cast<const void*>(wide),
        pointer0,
        pointer1,
        pointer2);
    DebugBreak();
}

int main() {
    wef_breakpoint_fixture();
    return 0;
}
