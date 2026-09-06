// Cold-loaded Blueprint tests use the game's delegate shape, not native replacement of Mod functions.
namespace {
bool ValidateCapture()
{
    FTestWorld Test;
    auto* Class = LoadClass<AActor>(nullptr, TEXT("/Game/NormalWaveIndicator/BP_NwiAuto.BP_NwiAuto_C"));
    auto* Controller = Test.World->SpawnActor<AActor>(Class); NWI_REQUIRE(Controller);
    auto* Wave = NewObject<UEnemyWaveManager>(Controller);
    auto* Manager = NewObject<UEnemySpawnManager>(Controller); Wave->SpawnManager = Manager;
    auto* AttachFunction = Class->FindFunctionByName(TEXT("AttachCapture")); NWI_REQUIRE(AttachFunction);
    auto Attach = [&](UEnemyWaveManager* W) {
        FStructOnScope P(AttachFunction); FindFProperty<FObjectPropertyBase>(AttachFunction, TEXT("Manager"))->SetObjectPropertyValue_InContainer(P.GetStructMemory(), W);
        Controller->ProcessEvent(AttachFunction, P.GetStructMemory());
    };
    auto Count = [&]() { return FindFProperty<FIntProperty>(Class, TEXT("CapturedSpawns"))->GetPropertyValue_InContainer(Controller); };
    auto Point = [&](int32 I) { return *FindFProperty<FStructProperty>(Class, *FString::Printf(TEXT("RegionPoint%d"), I))->ContainerPtrToValuePtr<FVector>(Controller); };
    auto Label = [&](int32 I) { return FindFProperty<FStrProperty>(Class, *FString::Printf(TEXT("RegionLabel%d"), I))->GetPropertyValue_InContainer(Controller); };
    auto Expires = [&](int32 I) { return FindFProperty<FFloatProperty>(Class, *FString::Printf(TEXT("RegionExpires%d"), I))->GetPropertyValue_InContainer(Controller); };
    auto* Enemy = Test.World->SpawnActor<ADefaultPawn>(); auto* Descriptor = NewObject<UEnemyDescriptor>(Controller); NWI_REQUIRE(Enemy);
    auto Emit = [&](float X) { Enemy->SetActorLocation(FVector(X, 0, 100), false, nullptr, ETeleportType::TeleportPhysics); Manager->OnEnemySpawned.Broadcast(Enemy, Descriptor); };
    Attach(Wave); Attach(Wave); // Rebinding must leave exactly one copy of our subscriber.
    NWI_REQUIRE(Manager->OnEnemySpawned.IsBound());
    Manager->OnEnemySpawned.Broadcast(nullptr, Descriptor); NWI_REQUIRE(Count() == 0);
    Emit(1000); NWI_REQUIRE(Count() == 1 && Point(0).Equals(FVector(1000,0,100)));
    NWI_REQUIRE(Label(0) == TEXT("Unknown / possible natural wave"));
    Emit(1799); NWI_REQUIRE(Count() == 2 && Point(0).Equals(FVector(1000,0,100)) && Expires(1) == 0.f);
    Emit(1801); NWI_REQUIRE(Count() == 3 && Point(1).Equals(FVector(1801,0,100)));
    // Context is read from the active controller class, and kept separate from nearby unknown spawns.
    auto* EventClass = LoadClass<UObject>(nullptr, TEXT("/Game/NwiValidation/EWC_EggHunt_Ambush.EWC_EggHunt_Ambush_C")); NWI_REQUIRE(EventClass);
    Wave->ActiveScriptedWaves.Add(NewObject<UEnemyWaveController>(Controller, EventClass));
    Emit(1000); NWI_REQUIRE(Count() == 4 && Label(2) == TEXT("Event context: EggHunt Ambush"));
    Wave->ActiveScriptedWaves.Add(NewObject<UEnemyWaveController>(Controller, EventClass));
    Emit(1000); NWI_REQUIRE(Label(3) == TEXT("Mixed events (first): EggHunt Ambush"));
    Wave->ActiveScriptedWaves.Reset(); Wave->ActiveScriptedWaves.Add(nullptr);
    Emit(1000); NWI_REQUIRE(Count() == 6 && Label(0) == TEXT("Unknown / possible natural wave")); Wave->ActiveScriptedWaves.Reset();
    // Fresh callback-time locations are recorded, while the original regional anchor remains stationary.
    NWI_REQUIRE(Enemy->SetActorLocation(FVector(99999,0,100))); NWI_REQUIRE(Point(0).Equals(FVector(1000,0,100)));
    for (int32 I=0; I<85; ++I) { ++GFrameCounter; Test.World->Tick(LEVELTICK_All, .1f); }
    Emit(20000); bool Found=false; for(int32 I=0; I<8; ++I) Found |= Point(I).Equals(FVector(20000,0,100)); NWI_REQUIRE(Found);
    for (int32 I=0; I<20; ++I) Emit(30000.f+I*2000.f);
    int32 Active=0; for(int32 I=0; I<8; ++I) Active += Expires(I)>Test.World->GetTimeSeconds(); NWI_REQUIRE(Active==8);
    // Switching managers unbinds the old manager; EndPlay must unbind the replacement as well.
    auto* OtherWave=NewObject<UEnemyWaveManager>(Controller); auto* OtherManager=NewObject<UEnemySpawnManager>(Controller); OtherWave->SpawnManager=OtherManager;
    Attach(OtherWave); const int32 Before=Count(); Emit(77777); NWI_REQUIRE(Count()==Before && !Manager->OnEnemySpawned.IsBound());
    OtherManager->OnEnemySpawned.Broadcast(Enemy, Descriptor); NWI_REQUIRE(Count()==Before+1);
    NWI_REQUIRE(Controller->Destroy() && !OtherManager->OnEnemySpawned.IsBound());
    // The actual native-loading entry assets deduplicate the controller across repeated initialization.
    AActor* OwningEntry = nullptr;
    for(const TCHAR* Path : {TEXT("/Game/NormalWaveIndicator/InitCave.InitCave_C"), TEXT("/Game/NormalWaveIndicator/InitSpacerig.InitSpacerig_C")}) {
        auto* InitClass=LoadClass<AActor>(nullptr,Path); NWI_REQUIRE(InitClass);
        auto* Entry = Test.World->SpawnActor<AActor>(InitClass); NWI_REQUIRE(Entry);
        if (!OwningEntry) OwningEntry = Entry;
        auto* Duplicate = Test.World->SpawnActor<AActor>(InitClass); NWI_REQUIRE(Duplicate && Duplicate->Destroy());
    }
    TArray<AActor*> Controllers; UGameplayStatics::GetAllActorsOfClass(Test.World,Class,Controllers); NWI_REQUIRE(Controllers.Num()==1);
    NWI_REQUIRE(OwningEntry->Destroy() && Controllers[0]->IsActorBeingDestroyed());
    Controllers.Reset(); UGameplayStatics::GetAllActorsOfClass(Test.World,Class,Controllers); NWI_REQUIRE(Controllers.Num()==0);
    UE_LOG(LogTemp, Display, TEXT("NWI_TEST content capture: real delegate -> saved Blueprint; null guard, repeated bind, 8m boundary, context/fallback/mixed labels, stable origins, expiry, 8-slot cap, manager replacement, EndPlay and native initializer ownership/deduplication passed"));
    return true;
}
}
