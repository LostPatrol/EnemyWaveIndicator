// Offline tests for prediction geometry and countdown gating; no game process or Unreal object is used.
#include "../mods/EnemyWaveNativeProbe/NaturalWavePrediction.h"
#include <cstring>
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

    // Keep the three-hop pointer layout locked to the audited stock selector sequence.
    alignas(void*) unsigned char world[WorldNavigationOwnerOffset + sizeof(void*)]{};
    alignas(void*) unsigned char owner[NavigationOffset + sizeof(void*)]{};
    alignas(void*) unsigned char navigation[PathfinderOffset + sizeof(void*)]{};
    int pathfinder = 0;
    void* pointer = owner; std::memcpy(world + WorldNavigationOwnerOffset, &pointer, sizeof(pointer));
    pointer = navigation; std::memcpy(owner + NavigationOffset, &pointer, sizeof(pointer));
    pointer = &pathfinder; std::memcpy(navigation + PathfinderOffset, &pointer, sizeof(pointer));
    REQUIRE(resolvePathfinder(world) == &pathfinder);
    pointer = nullptr; std::memcpy(owner + NavigationOffset, &pointer, sizeof(pointer));
    REQUIRE(resolvePathfinder(world) == nullptr && resolvePathfinder(nullptr) == nullptr);

    alignas(void*) unsigned char actor[ActorRootOffset + sizeof(void*)]{};
    alignas(void*) unsigned char root[SceneTranslationOffset + sizeof(Position)]{};
    pointer = root; std::memcpy(actor + ActorRootOffset, &pointer, sizeof(pointer));
    const Position actorPoint{7, 8, 9}; std::memcpy(root + SceneTranslationOffset, &actorPoint, sizeof(actorPoint));
    Position readPoint{};
    REQUIRE(readActorPosition(actor, readPoint) && readPoint.x == 7 && readPoint.y == 8 && readPoint.z == 9);
    pointer = nullptr; std::memcpy(actor + ActorRootOffset, &pointer, sizeof(pointer));
    REQUIRE(!readActorPosition(actor, readPoint) && !readActorPosition(nullptr, readPoint));

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
    std::puts("PASS: player/root geometry, stock navigation chain and one-shot T-5 countdown gate.");
}
