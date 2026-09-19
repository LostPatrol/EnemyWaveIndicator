// Verify the saved native contract and real initializer ownership; native attribution is tested by the C++ harness.
namespace {
bool ValidateCapture()
{
    FTestWorld Test;
    auto* Class=LoadClass<AActor>(nullptr,TEXT("/Game/EnemyWaveIndicator/BP_EwiAuto.BP_EwiAuto_C"));EWI_REQUIRE(Class);
    EWI_REQUIRE(Class->FindFunctionByName(TEXT("EwiPoll")) && !Class->FindFunctionByName(TEXT("ObserveEnemy")));
    EWI_REQUIRE(FindFProperty<FIntProperty>(Class,TEXT("NativeAbi"))->GetPropertyValue_InContainer(Class->GetDefaultObject())==0x10000);
    for(int32 I=0;I<8;++I) for(const TCHAR* Prefix:{TEXT("RegionPoint"),TEXT("RegionSerial"),TEXT("RegionVisible"),TEXT("RegionExpires"),TEXT("RegionScale"),TEXT("RegionType")}) {
        auto* Field=FindFProperty<FProperty>(Class,*FString::Printf(TEXT("%s%d"),Prefix,I));EWI_REQUIRE(Field && Field->HasAnyPropertyFlags(CPF_Net));
    }
    // The actual native-loading entry assets deduplicate the controller across repeated initialization.
    AActor* OwningEntry = nullptr;
    for(const TCHAR* Path : {TEXT("/Game/EnemyWaveIndicator/InitCave.InitCave_C"), TEXT("/Game/EnemyWaveIndicator/InitSpacerig.InitSpacerig_C")}) {
        auto* InitClass=LoadClass<AActor>(nullptr,Path); EWI_REQUIRE(InitClass);
        auto* Entry = Test.World->SpawnActor<AActor>(InitClass); EWI_REQUIRE(Entry);
        if (!OwningEntry) OwningEntry = Entry;
        auto* Duplicate = Test.World->SpawnActor<AActor>(InitClass); EWI_REQUIRE(Duplicate && Duplicate->Destroy());
    }
    TArray<AActor*> Controllers; UGameplayStatics::GetAllActorsOfClass(Test.World,Class,Controllers); EWI_REQUIRE(Controllers.Num()==1);
    EWI_REQUIRE(OwningEntry->Destroy() && Controllers[0]->IsActorBeingDestroyed());
    Controllers.Reset(); UGameplayStatics::GetAllActorsOfClass(Test.World,Class,Controllers); EWI_REQUIRE(Controllers.Num()==0);
    UE_LOG(LogTemp, Display, TEXT("EWI_TEST native ABI, replicated field metadata, no approximate event observer, initializer ownership/deduplication passed; network transport requires two real game peers"));
    return true;
}
}
