#include "../examples/BasicSettings.hpp"
#include <cassert>
#include <cstdio>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<BasicSettings>);
static_assert(!std::is_move_constructible_v<BasicSettings>);

int main() {
    assert(GetModuleHandleA("SleekConfig.asi") == nullptr);
    BasicSettings settings;
    assert(settings.GetConfig().enabled);
    assert(settings.GetConfig().strength == 1.0f);
    for (int i = 0; i < 100; ++i) assert(!settings.Attach());
    settings.Detach();
    assert(settings.GetConfig().enabled);
    assert(settings.GetConfig().strength == 1.0f);
    std::puts("PASS: tutorial adapter compiles, retains defaults and safely retries without SleekConfig.");
}
