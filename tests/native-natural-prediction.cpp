// Offline tests for prediction geometry and countdown gating; no game process or Unreal object is used.
#include "../mods/EnemyWaveNativeProbe/NaturalWavePrediction.h"
#include <cmath>
#include <cstdio>

#define REQUIRE(x) do { if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

int main() {
    using namespace nwi::prediction;
    const Position grouped[]{{0,0,0},{100,0,0},{50,20,0},{50,-20,0}};
    const auto group = playerSphere(grouped, 4);
    REQUIRE(group.valid && group.center.x == 50 && group.center.y == 0 && group.radius == 50);

    const Position split[]{{0,0,0},{10,0,0},{20,0,0},{7000,0,0}};
    const auto lone = playerSphere(split, 4);
    REQUIRE(lone.valid && lone.center.x == 3500 && lone.radius == 3500);

    const Position solo[]{{12,-4,8}};
    const auto one = playerSphere(solo, 1);
    REQUIRE(one.valid && one.center.x == 12 && one.center.y == -4 && one.center.z == 8 && one.radius == 0);
    REQUIRE(!playerSphere(nullptr, 1).valid && !playerSphere(solo, 0).valid);
    Position bad = solo[0]; bad.x = NAN; REQUIRE(!playerSphere(&bad, 1).valid);

    CountdownGate gate;
    REQUIRE(!gate.sample(12, true, false));
    REQUIRE(!gate.sample(5.01f, true, false));
    REQUIRE(gate.sample(4.99f, true, false));
    REQUIRE(!gate.sample(4, true, false) && !gate.sample(1, true, false));
    REQUIRE(!gate.sample(20, true, false));
    REQUIRE(!gate.sample(5, true, true));
    REQUIRE(gate.sample(4.5f, true, false));
    REQUIRE(!gate.sample(30, true, false));
    REQUIRE(!gate.sample(4, false, false));
    REQUIRE(gate.sample(3, true, false));
    std::puts("PASS: player sphere and one-shot T-5 countdown gate.");
}
