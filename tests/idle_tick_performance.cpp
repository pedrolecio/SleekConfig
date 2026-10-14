#include "Registry.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <new>

static size_t allocations = 0;
static bool counting = false;
void* operator new(size_t size) {
    if (counting) ++allocations;
    if (void* memory = std::malloc(size ? size : 1)) return memory;
    throw std::bad_alloc();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, size_t) noexcept { std::free(memory); }

int main(int argc, char**) {
    auto& registry = sc::Registry::Instance();
    ScModDesc descriptor{};
    descriptor.size = sizeof descriptor;
    descriptor.required_api = SC_API_VERSION;
    descriptor.id = "test.idle";
    descriptor.name = "Idle benchmark";
    descriptor.persistence = SC_EXTERNAL;
    ScMod mod{};
    assert(registry.Register(&descriptor, &mod) == SC_OK);
    registry.Tick();
    const auto revision = registry.Revision(mod);
    constexpr unsigned ticks = 100000;
    counting = true;
    const auto start = std::chrono::steady_clock::now();
    for (unsigned i = 0; i < ticks; ++i) registry.Tick();
    const auto end = std::chrono::steady_clock::now();
    counting = false;
    std::printf("%u idle ticks: %zu allocations, %.3f ms\n", ticks, allocations,
        std::chrono::duration<double, std::milli>(end - start).count());
    assert(registry.Revision(mod) == revision);
    // Baseline mode records the old implementation before the optimization.
    if (argc == 1) assert(allocations == 0);
    assert(registry.Unregister(mod) == SC_OK);
    descriptor.id = "test.late";
    assert(registry.Register(&descriptor, &mod) == SC_OK);
    assert(registry.Mods().at(mod).startupPending);
    registry.Tick();
    assert(!registry.Mods().at(mod).startupPending);
    assert(registry.Unregister(mod) == SC_OK);
}
