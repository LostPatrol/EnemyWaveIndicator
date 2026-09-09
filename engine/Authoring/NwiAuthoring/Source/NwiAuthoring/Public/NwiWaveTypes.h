// Stable stock wave IDs shared by the DLL and editor generator. Append entries; never reorder saved IDs.
#pragma once
#include <cstdint>
namespace nwi {
struct WaveType { const wchar_t* key; const wchar_t* title; const wchar_t* classPath; };
static constexpr WaveType WaveTypes[] = {
    {L"Natural", L"Natural wave", L""},
    {L"EWC_DeepScan_Drillevator", L"Drillevator", L"/Game/Enemies/Waves/WaveControllers/EWC_DeepScan_Drillevator.EWC_DeepScan_Drillevator_C"},
    {L"EWC_EggHunt_Ambush", L"Egg hunt ambush", L"/Game/Enemies/Waves/WaveControllers/EWC_EggHunt_Ambush.EWC_EggHunt_Ambush_C"},
    {L"EWC_EndMission", L"Extraction", L"/Game/Enemies/Waves/WaveControllers/EWC_EndMission.EWC_EndMission_C"},
    {L"EWC_EndMission_MotherLode", L"Motherlode extraction", L"/Game/Enemies/Waves/WaveControllers/EWC_EndMission_MotherLode.EWC_EndMission_MotherLode_C"},
    {L"EWC_EndMission_Tutorial", L"Tutorial extraction", L"/Game/Enemies/Waves/WaveControllers/EWC_EndMission_Tutorial.EWC_EndMission_Tutorial_C"},
    {L"EWC_Escort_DigPhase", L"Escort: drilling", L"/Game/Enemies/Waves/WaveControllers/EWC_Escort_DigPhase.EWC_Escort_DigPhase_C"},
    {L"EWC_Escort_EndDefense", L"Escort: heartstone defense", L"/Game/Enemies/Waves/WaveControllers/EWC_Escort_EndDefense.EWC_Escort_EndDefense_C"},
    {L"EWC_Escort_EndMission", L"Escort: extraction", L"/Game/Enemies/Waves/WaveControllers/EWC_Escort_EndMission.EWC_Escort_EndMission_C"},
    {L"EWC_Escort_Refueling", L"Escort: refueling", L"/Game/Enemies/Waves/WaveControllers/EWC_Escort_Refueling.EWC_Escort_Refueling_C"},
    {L"EWC_Excavation_ExcavationPhase", L"Excavation: mining", L"/Game/Enemies/Waves/WaveControllers/EWC_Excavation_ExcavationPhase.EWC_Excavation_ExcavationPhase_C"},
    {L"EWC_Excavation_LaunchPhase", L"Excavation: launch", L"/Game/Enemies/Waves/WaveControllers/EWC_Excavation_LaunchPhase.EWC_Excavation_LaunchPhase_C"},
    {L"EWC_Generic", L"Announced / generic swarm", L"/Game/Enemies/Waves/WaveControllers/EWC_Generic.EWC_Generic_C"},
    {L"EWC_OktoberFest_BeerAmbush", L"Oktoberfest beer ambush", L"/Game/Enemies/Waves/WaveControllers/EWC_OktoberFest_BeerAmbush.EWC_OktoberFest_BeerAmbush_C"},
    {L"EWC_OverloadShieldGenerator_Facility", L"Industrial sabotage: power station", L"/Game/Enemies/Waves/WaveControllers/EWC_OverloadShieldGenerator_Facility.EWC_OverloadShieldGenerator_Facility_C"},
    {L"EWC_PlagueMeteorDefence", L"Plague meteor defense", L"/Game/Enemies/Waves/WaveControllers/EWC_PlagueMeteorDefence.EWC_PlagueMeteorDefence_C"},
    {L"EWC_PumpSequence_ConstantPresure_Refinery", L"Refinery: constant pressure", L"/Game/Enemies/Waves/WaveControllers/EWC_PumpSequence_ConstantPresure_Refinery.EWC_PumpSequence_ConstantPresure_Refinery_C"},
    {L"EWC_PumpSequence_Wave_Refinery", L"Refinery: pumping swarm", L"/Game/Enemies/Waves/WaveControllers/EWC_PumpSequence_Wave_Refinery.EWC_PumpSequence_Wave_Refinery_C"},
    {L"EWC_Refinery_BokenPipe_LocalWave", L"Refinery: broken pipe", L"/Game/Enemies/Waves/WaveControllers/EWC_Refinery_BokenPipe_LocalWave.EWC_Refinery_BokenPipe_LocalWave_C"},
    {L"EWC_Refinery_End", L"Refinery: extraction", L"/Game/Enemies/Waves/WaveControllers/EWC_Refinery_End.EWC_Refinery_End_C"},
    {L"EWC_SW_Grunts", L"Special swarm: grunts", L"/Game/Enemies/Waves/WaveControllers/EWC_SW_Grunts.EWC_SW_Grunts_C"},
    {L"EWC_SW_Macteras", L"Special swarm: mactera", L"/Game/Enemies/Waves/WaveControllers/EWC_SW_Macteras.EWC_SW_Macteras_C"},
    {L"EWC_SW_Plague_RockpoxInfectedEnemies", L"Special swarm: rockpox", L"/Game/Enemies/Waves/WaveControllers/EWC_SW_Plague_RockpoxInfectedEnemies.EWC_SW_Plague_RockpoxInfectedEnemies_C"},
    {L"EWC_SW_Pretorians", L"Special swarm: praetorians", L"/Game/Enemies/Waves/WaveControllers/EWC_SW_Pretorians.EWC_SW_Pretorians_C"},
    {L"EWC_SW_Swarmers", L"Special swarm: swarmers", L"/Game/Enemies/Waves/WaveControllers/EWC_SW_Swarmers.EWC_SW_Swarmers_C"},
    {L"EWC_Salvage_Ambush", L"Salvage: mini-MULE ambush", L"/Game/Enemies/Waves/WaveControllers/EWC_Salvage_Ambush.EWC_Salvage_Ambush_C"},
    {L"EWC_Salvage_Defend", L"Salvage: defense", L"/Game/Enemies/Waves/WaveControllers/EWC_Salvage_Defend.EWC_Salvage_Defend_C"},
    {L"EWC_Salvage_End", L"Salvage: extraction", L"/Game/Enemies/Waves/WaveControllers/EWC_Salvage_End.EWC_Salvage_End_C"},
    {L"EWC_ShieledGenerator_DronePresure_Facility", L"Industrial sabotage: drones", L"/Game/Enemies/Waves/WaveControllers/EWC_ShieledGenerator_DronePresure_Facility.EWC_ShieledGenerator_DronePresure_Facility_C"},
    {L"EWC_Spiders_Boss", L"Dreadnought wave", L"/Game/Enemies/Waves/WaveControllers/EWC_Spiders_Boss.EWC_Spiders_Boss_C"},
    {L"EWC_Spiders_Motherlode", L"Motherlode wave", L"/Game/Enemies/Waves/WaveControllers/EWC_Spiders_Motherlode.EWC_Spiders_Motherlode_C"},
    {L"EWC_TutorialGrunts", L"Tutorial grunts", L"/Game/Enemies/Waves/WaveControllers/EWC_TutorialGrunts.EWC_TutorialGrunts_C"},
    {L"EWC_CoreRift", L"Core stone event", L"/Game/GameElements/GameEvents/CoreRift/EWC_CoreRift.EWC_CoreRift_C"},
    {L"EWC_BombEvent", L"Rival communications event", L"/Game/GameElements/GameEvents/RivalBombEvent/EWC_BombEvent.EWC_BombEvent_C"},
    {L"EWC_CoreCorruption", L"Core corruption warning", L"/Game/GameElements/Missions/Warnings/CoreCorruption/EWC_CoreCorruption.EWC_CoreCorruption_C"},
    {L"EWC_HackBuilding", L"Hacking defense", L"/Game/GameElements/Objectives/HackBuilding/EWC_HackBuilding.EWC_HackBuilding_C"},
    {L"BP_ExplosiveBarrelsEvent", L"Tritilyte Deposit", L"/Game/GameElements/GameEvents/ExplosiveBarrelsEvent/BP_ExplosiveBarrelsEvent.BP_ExplosiveBarrelsEvent_C"},
    {L"BP_RockEnemiesEvent", L"Ebonite Mutation", L"/Game/GameElements/GameEvents/RockEnemies/BP_RockEnemiesEvent.BP_RockEnemiesEvent_C"},
    {L"BP_AmberEvent", L"Kursite Infection", L"/Game/GameElements/GameEvents/AmberEvent/BP_AmberEvent.BP_AmberEvent_C"}
};
static constexpr uint32_t WaveTypeCount = sizeof(WaveTypes) / sizeof(WaveTypes[0]);
// Some event-owned spawns share an existing user-facing setting instead of adding a duplicate control.
struct WaveSourceAlias { uint32_t type; const wchar_t* classPath; };
static constexpr WaveSourceAlias WaveSourceAliases[] = {
    {32, L"/Game/GameElements/GameEvents/CoreRift/BP_RiftCrystal.BP_RiftCrystal_C"}
};
// High byte identifies source type; remaining bits distinguish concurrent requests without time heuristics.
inline uint32_t waveType(uint64_t wave) noexcept { return static_cast<uint32_t>(wave >> 56); }
inline uint64_t waveIdentity(uint64_t serial, uint32_t type) noexcept { return (serial & 0x00ffffffffffffffULL) | (uint64_t(type) << 56); }
}
