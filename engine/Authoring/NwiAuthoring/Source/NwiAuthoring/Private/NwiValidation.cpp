// Headless integration tests follow UE's EngineAutomationTests transient UWorld lifecycle.
#include "NwiValidation.h"
#include "NwiWaveTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/WorldSettings.h"
#include "Components/StaticMeshComponent.h"
#include "Blueprint/UserWidget.h"
#include "Curves/CurveFloat.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "Misc/App.h"
#include "UObject/Stack.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/SpinBox.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SOverlay.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateNullRendererModule.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"

// Test failures log and unwind normally; they must not deliberately crash the editor.
#define NWI_REQUIRE(Condition) do { if (!(Condition)) { UE_LOG(LogTemp, Error, TEXT("NWI validation failed: %s"), TEXT(#Condition)); return false; } } while (false)

namespace
{
struct FTestWorld
{
    UWorld* World = nullptr;
    FTestWorld()
    {
        World = UWorld::CreateWorld(EWorldType::Game, false);
        if (World)
        {
            GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            World->InitializeActorsForPlay(FURL());
            World->BeginPlay();
            // This test has no GameMode to dispatch BeginPlay on its behalf.
            World->GetWorldSettings()->NotifyBeginPlay();
        }
    }
    ~FTestWorld()
    {
        if (World)
        {
            GEngine->DestroyWorldContext(World);
            World->DestroyWorld(false);
        }
    }
};

bool Initialize(AActor* Actor, UObject* Material, UObject* Scale, UObject* Alpha)
{
    auto* Function = Actor->FindFunction(TEXT("InitializeVisual"));
    NWI_REQUIRE(Function);
    FStructOnScope Parameters(Function);
    const TCHAR* Names[] = { TEXT("Material"), TEXT("Scale"), TEXT("Alpha") };
    UObject* Inputs[] = { Material, Scale, Alpha };
    for (int32 Index = 0; Index < 3; ++Index)
    {
        auto* Property = FindFProperty<FObjectPropertyBase>(Function, Names[Index]);
        NWI_REQUIRE(Property && Property->HasAnyPropertyFlags(CPF_Parm));
        Property->SetObjectPropertyValue_InContainer(Parameters.GetStructMemory(), Inputs[Index]);
    }
    Actor->ProcessEvent(Function, Parameters.GetStructMemory());
    return true;
}

bool ValidateInWorld(UClass* PulseClass)
{
    FTestWorld Test;
    NWI_REQUIRE(Test.World);
    UE_LOG(LogTemp, Display, TEXT("NWI_TEST world initialized; spawning directly through UWorld"));
    auto* Actor = Test.World->SpawnActor<AStaticMeshActor>(PulseClass, FVector::ZeroVector, FRotator::ZeroRotator);
    NWI_REQUIRE(Actor);
    auto* Mesh = Actor->GetStaticMeshComponent();
    NWI_REQUIRE(Mesh && Mesh->GetStaticMesh());
    UE_LOG(LogTemp, Display, TEXT("NWI_TEST registered mesh profile=%s collision=%d tick=%d hidden=%d"),
        *Mesh->GetCollisionProfileName().ToString(), static_cast<int32>(Mesh->GetCollisionEnabled()), Actor->IsActorTickEnabled(), Actor->IsHidden());
    NWI_REQUIRE(Mesh->GetStaticMesh()->GetPathName() == TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    NWI_REQUIRE(Mesh->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
    NWI_REQUIRE(!Mesh->GetGenerateOverlapEvents() && !Mesh->CastShadow && !Mesh->CanEverAffectNavigation());
    NWI_REQUIRE(!Actor->GetIsReplicated() && Actor->IsHidden() && !Actor->IsActorTickEnabled());
    NWI_REQUIRE(Initialize(Actor, nullptr, nullptr, nullptr));
    NWI_REQUIRE(Actor->IsHidden() && !Actor->IsActorTickEnabled());

    auto* Material = LoadObject<UMaterial>(nullptr, TEXT("/Game/EnemyWaveIndicator/M_NwiRedPulse.M_NwiRedPulse"));
    auto* Scale = LoadObject<UCurveFloat>(nullptr, TEXT("/Game/NwiValidation/CF_TestScale.CF_TestScale"));
    auto* Alpha = LoadObject<UCurveFloat>(nullptr, TEXT("/Game/NwiValidation/CF_TestAlpha.CF_TestAlpha"));
    NWI_REQUIRE(Material && Scale && Alpha);
    FLinearColor Tint;
    NWI_REQUIRE(Material->GetVectorParameterValue(FMaterialParameterInfo(TEXT("Tint")), Tint));
    NWI_REQUIRE(Tint.Equals(FLinearColor(1.0f, 0.01f, 0.005f, 1.0f)) && !Material->TwoSided);
    NWI_REQUIRE(Material->GetShadingModels().HasShadingModel(MSM_Unlit));
    NWI_REQUIRE(Initialize(Actor, Material, Scale, Alpha));
    NWI_REQUIRE(!Actor->IsHidden() && Actor->IsActorTickEnabled() && Actor->PrimaryActorTick.IsTickFunctionRegistered());
    constexpr float Delta = 1.0f / 60.0f; // Synthetic engine timestep, not a measured display refresh rate.
    ++GFrameCounter;
    Test.World->Tick(LEVELTICK_All, Delta);
    NWI_REQUIRE(Actor->GetActorScale3D().Equals(FVector(1.875f), 0.00001f));
    auto* Property = FindFProperty<FObjectPropertyBase>(PulseClass, TEXT("PulseMaterial"));
    NWI_REQUIRE(Property);
    auto* Mid = Cast<UMaterialInstanceDynamic>(Property->GetObjectPropertyValue_InContainer(Actor));
    float Opacity = 0.0f;
    NWI_REQUIRE(Mid && Mid->GetScalarParameterValue(FMaterialParameterInfo(TEXT("Alpha")), Opacity));
    NWI_REQUIRE(FMath::IsNearlyEqual(Opacity, 0.25f, 0.00001f));
    UE_LOG(LogTemp, Display, TEXT("NWI_TEST valid input, registered World Tick, scale and Alpha passed"));

    auto* Deactivate = Actor->FindFunction(TEXT("DeactivateVisual"));
    NWI_REQUIRE(Deactivate);
    Actor->ProcessEvent(Deactivate, nullptr);
    NWI_REQUIRE(Actor->IsHidden() && !Actor->IsActorTickEnabled());
    // A varying curve proves repeated World ticks update the saved Blueprint, not just initialization.
    auto* Ramp = NewObject<UCurveFloat>();
    Ramp->FloatCurve.AddKey(0.0f, 0.0f);
    Ramp->FloatCurve.AddKey(1.0f, 1.0f);
    NWI_REQUIRE(Initialize(Actor, Material, Ramp, Alpha));
    const float Start = Actor->GetGameTimeSinceCreation();
    int32 ChangedFrames = 0;
    float Previous = -1.0f;
    for (int32 Index = 0; Index < 150; ++Index)
    {
        ++GFrameCounter;
        Test.World->Tick(LEVELTICK_All, Delta);
        const float Expected = 3.75f * Ramp->GetFloatValue(FMath::Fmod(Actor->GetGameTimeSinceCreation() - Start, 2.0f));
        const float Actual = Actor->GetActorScale3D().X;
        NWI_REQUIRE(FMath::IsNearlyEqual(Actual, Expected, 0.0001f));
        ChangedFrames += !FMath::IsNearlyEqual(Previous, Actual, 0.0001f);
        Previous = Actual;
    }
    NWI_REQUIRE(ChangedFrames > 80);
    NWI_REQUIRE(Initialize(Actor, Material, nullptr, Alpha));
    NWI_REQUIRE(Actor->IsHidden() && !Actor->IsActorTickEnabled());
    NWI_REQUIRE(Actor->Destroy());
    UE_LOG(LogTemp, Display, TEXT("NWI_TEST 150 World ticks, phase wrap, reactivation, invalid reinit and destruction passed"));
    return true;
}

// Drive stock latent actions in a headless world; blocking flush is test-only, never shipped graph code.
bool ValidateResources(UClass* ResourceClass, UClass* PulseClass)
{
    FTestWorld Test;
    NWI_REQUIRE(Test.World);
    const TCHAR* Names[] = { TEXT("Material"), TEXT("Scale"), TEXT("Alpha") };
    const TCHAR* Paths[] = { TEXT("/Game/NwiValidation/M_TestAlpha.M_TestAlpha"),
        TEXT("/Game/NwiValidation/CurveContainer.CurveContainer:CurveFloat_0"),
        TEXT("/Game/NwiValidation/CurveContainer.CurveContainer:CurveFloat_1") };
    auto* Prepare = ResourceClass->FindFunctionByName(TEXT("PrepareResources"));
    auto* Attempted = FindFProperty<FBoolProperty>(ResourceClass, TEXT("Attempted"));
    auto* Finished = FindFProperty<FBoolProperty>(ResourceClass, TEXT("Finished"));
    auto* Ready = FindFProperty<FBoolProperty>(ResourceClass, TEXT("Ready"));
    NWI_REQUIRE(Prepare && Attempted && Finished && Ready);
    FStrProperty* PathProperties[3];
    FObjectPropertyBase* Resources[3];
    for (int32 Index = 0; Index < 3; ++Index)
    {
        PathProperties[Index] = FindFProperty<FStrProperty>(ResourceClass, *(FString(Names[Index]) + TEXT("Path")));
        Resources[Index] = FindFProperty<FObjectPropertyBase>(ResourceClass, Names[Index]);
        NWI_REQUIRE(PathProperties[Index] && Resources[Index]);
    }
    auto TickLoading = [&Test](int32 Count) {
        for (int32 Index = 0; Index < Count; ++Index)
        {
            FlushAsyncLoading();
            ++GFrameCounter;
            Test.World->Tick(LEVELTICK_All, 1.0f / 60.0f);
        }
    };
    for (int32 Mode = 0; Mode < 3; ++Mode)
    {
        auto* Provider = Test.World->SpawnActor<AActor>(ResourceClass);
        NWI_REQUIRE(Provider && !Provider->IsActorTickEnabled() && !Provider->GetIsReplicated());
        NWI_REQUIRE(!Attempted->GetPropertyValue_InContainer(Provider) && !Ready->GetPropertyValue_InContainer(Provider));
        for (int32 Index = 0; Index < 3; ++Index)
            PathProperties[Index]->SetPropertyValue_InContainer(Provider, Paths[Mode == 1 ? 0 : Index]);
        Provider->ProcessEvent(Prepare, nullptr);
        NWI_REQUIRE(Attempted->GetPropertyValue_InContainer(Provider));
        if (Mode == 2)
        {
            // Destroy while async work is outstanding: latent actions must not revive the provider.
            NWI_REQUIRE(Provider->Destroy());
            TickLoading(4);
            NWI_REQUIRE(Provider->IsActorBeingDestroyed());
            continue;
        }
        TickLoading(8);
        NWI_REQUIRE(Finished->GetPropertyValue_InContainer(Provider));
        NWI_REQUIRE(Ready->GetPropertyValue_InContainer(Provider) == (Mode == 0));
        if (Mode == 0)
        {
            UObject* Loaded[3];
            for (int32 Index = 0; Index < 3; ++Index)
            {
                Loaded[Index] = Resources[Index]->GetObjectPropertyValue_InContainer(Provider);
                NWI_REQUIRE(Loaded[Index] && Loaded[Index]->GetPathName() == Paths[Index]);
                PathProperties[Index]->SetPropertyValue_InContainer(Provider, TEXT(""));
            }
            // Repeated preparation must use the cache, including while a caller changes path variables.
            Provider->ProcessEvent(Prepare, nullptr);
            NWI_REQUIRE(Ready->GetPropertyValue_InContainer(Provider));
            for (int32 Index = 0; Index < 3; ++Index)
                NWI_REQUIRE(Resources[Index]->GetObjectPropertyValue_InContainer(Provider) == Loaded[Index]);
            auto* Pulse = Test.World->SpawnActor<AStaticMeshActor>(PulseClass);
            NWI_REQUIRE(Pulse && Initialize(Pulse, Loaded[0], Loaded[1], Loaded[2]));
            NWI_REQUIRE(Pulse->IsActorTickEnabled() && !Pulse->IsHidden());
            NWI_REQUIRE(Pulse->Destroy());
        }
        else NWI_REQUIRE(Resources[1]->GetObjectPropertyValue_InContainer(Provider) == nullptr);
        NWI_REQUIRE(Provider->Destroy());
    }
    UE_LOG(LogTemp, Display, TEXT("NWI_TEST async embedded resources, cached reuse, pulse handoff, wrong type and pending destruction passed; 20 resource World ticks"));
    return true;
}
}

// Execute the serialized Blueprint's actual placement event with independent geometric expectations.
bool ValidatePlacement(UClass* WidgetClass)
{
    auto* Widget = NewObject<UUserWidget>(GetTransientPackage(), WidgetClass);
    auto* Update = WidgetClass->FindFunctionByName(TEXT("UpdatePlacement"));
    auto* Position = FindFProperty<FStructProperty>(WidgetClass, TEXT("MarkerPosition"));
    auto* Edge = FindFProperty<FBoolProperty>(WidgetClass, TEXT("AtEdge"));
    auto* Angle = FindFProperty<FFloatProperty>(WidgetClass, TEXT("ArrowAngle"));
    NWI_REQUIRE(Widget && Update && Position && Edge && Angle);
    int32 Cases = 0;
    auto Evaluate = [&](FVector2D Projected, bool Front, FVector Camera, FVector2D Viewport, FVector2D Label) {
        FStructOnScope Params(Update);
        for (const auto* Name : { TEXT("Projected"), TEXT("Viewport"), TEXT("LabelSize"), TEXT("CameraDelta"), TEXT("Front") })
            if (!Update->FindPropertyByName(Name)) return false;
        *FindFProperty<FStructProperty>(Update, TEXT("Projected"))->ContainerPtrToValuePtr<FVector2D>(Params.GetStructMemory()) = Projected;
        *FindFProperty<FStructProperty>(Update, TEXT("Viewport"))->ContainerPtrToValuePtr<FVector2D>(Params.GetStructMemory()) = Viewport;
        *FindFProperty<FStructProperty>(Update, TEXT("LabelSize"))->ContainerPtrToValuePtr<FVector2D>(Params.GetStructMemory()) = Label;
        *FindFProperty<FStructProperty>(Update, TEXT("CameraDelta"))->ContainerPtrToValuePtr<FVector>(Params.GetStructMemory()) = Camera;
        FindFProperty<FBoolProperty>(Update, TEXT("Front"))->SetPropertyValue_InContainer(Params.GetStructMemory(), Front);
        Widget->ProcessEvent(Update, Params.GetStructMemory()); ++Cases;
        const auto Actual = *Position->ContainerPtrToValuePtr<FVector2D>(Widget);
        return FMath::IsFinite(Actual.X) && FMath::IsFinite(Actual.Y) && FMath::IsFinite(Angle->GetPropertyValue_InContainer(Widget))
            && Actual.X >= 0 && Actual.Y >= 0 && Actual.X <= Viewport.X && Actual.Y <= Viewport.Y;
    };
    const FVector2D View(1280, 720), Label(180, 24);
    for (auto Point : { FVector2D(640, 360), FVector2D(750, 250), FVector2D(300, 500) })
    {
        NWI_REQUIRE(Evaluate(Point, true, FVector(100, 0, 0), View, Label));
        NWI_REQUIRE(!Edge->GetPropertyValue_InContainer(Widget));
        NWI_REQUIRE(Position->ContainerPtrToValuePtr<FVector2D>(Widget)->Equals(Point, 0.001f));
    }
    struct FCase { FVector2D Projected; bool Front; FVector Camera; FVector2D Expected; float Degrees; };
    const FCase Tests[] = {
        {{2000,360},true,{100,100,0},{1166,360},0}, {{-500,360},true,{100,-100,0},{114,360},180},
        {{640,-500},true,{100,0,100},{640,56},-90}, {{640,1500},true,{100,0,-100},{640,664},90},
        {{0,0},false,{-100,100,0},{1166,360},0}, {{0,0},false,{-100,-100,0},{114,360},180},
        {{0,0},false,{-100,0,100},{640,56},-90}, {{0,0},false,{-100,0,-100},{640,664},90},
        {{0,0},false,{-100,0,0},{640,664},90}
    };
    for (const auto& C : Tests)
    {
        NWI_REQUIRE(Evaluate(C.Projected, C.Front, C.Camera, View, Label));
        NWI_REQUIRE(Edge->GetPropertyValue_InContainer(Widget));
        NWI_REQUIRE(Position->ContainerPtrToValuePtr<FVector2D>(Widget)->Equals(C.Expected, 0.01f));
        NWI_REQUIRE(FMath::IsNearlyEqual(Angle->GetPropertyValue_InContainer(Widget), C.Degrees, 0.01f));
    }
    // Sweep both hemispheres across standard, ultrawide, portrait and small DPI-local viewports.
    for (auto Size : { FVector2D(1920,1080), FVector2D(2560,1080), FVector2D(720,1280), FVector2D(320,180) })
    {
        for (int32 I = 0; I < 360; ++I)
        {
            const float R = FMath::DegreesToRadians(static_cast<float>(I));
            const FVector Camera(FMath::Cos(R) * 100, FMath::Sin(R) * 100, 25);
            const bool Front = Camera.X > 0.001f;
            const FVector2D Projected = Front ? FVector2D(Size.X * 0.5f + Camera.Y / Camera.X * Size.X * 0.5f,
                Size.Y * 0.5f - Camera.Z / Camera.X * Size.X * 0.5f) : FVector2D::ZeroVector;
            NWI_REQUIRE(Evaluate(Projected, Front, Camera, Size, Label));
        }
    }
    UE_LOG(LogTemp, Display, TEXT("NWI_TEST %d serialized HUD placement cases: in-view preserved, four edges, rear, aspect ratios and viewport resize"), Cases);
    return true;
}

// Exercise serialized Blueprint region updates without binding any native test function.
namespace {
// Restore the commandlet language even when an assertion returns early.
struct FScopedLanguage
{
    FString Original = FInternationalization::Get().GetCurrentLanguage()->GetName();
    ~FScopedLanguage() { FInternationalization::Get().SetCurrentLanguage(Original); }
    bool Set(const TCHAR* Language) { return FInternationalization::Get().SetCurrentLanguage(Language); }
};

FString TextOutput(UObject* Object, const TCHAR* FunctionName, const TCHAR* OutputName)
{
    auto* Function = Object ? Object->GetClass()->FindFunctionByName(FunctionName) : nullptr;
    auto* Text = Function ? FindFProperty<FTextProperty>(Function, OutputName) : nullptr;
    if (!Function || !Text) return FString();
    FStructOnScope Params(Function); Object->ProcessEvent(Function, Params.GetStructMemory());
    return Text->GetPropertyValue_InContainer(Params.GetStructMemory()).ToString();
}

bool ValidateAutomatic(UClass* PulseClass, UClass* WidgetClass)
{
    // Commandlets have no Slate application; use a real widget tree with a non-rendering backend.
    if (!FSlateApplication::IsInitialized()) {
        auto& NullRenderer = FModuleManager::LoadModuleChecked<ISlateNullRendererModule>(TEXT("SlateNullRenderer"));
        FSlateApplication::InitializeAsStandaloneApplication(NullRenderer.CreateSlateNullRenderer());
    }
    FTestWorld Test;
    auto* Class = LoadClass<AActor>(nullptr, TEXT("/Game/EnemyWaveIndicator/BP_NwiAuto.BP_NwiAuto_C"));
    NWI_REQUIRE(Test.World && Class);
    auto* Controller = Test.World->SpawnActor<AActor>(Class);
    auto* HubModInterface = LoadClass<UInterface>(nullptr, TEXT("/Game/_ModHub/IHubMod.IHubMod_C"));
    NWI_REQUIRE(HubModInterface && Class->ImplementsInterface(HubModInterface));
    TArray<AActor*> DiscoveredMods;
    UGameplayStatics::GetAllActorsWithInterface(Test.World, HubModInterface, DiscoveredMods);
    NWI_REQUIRE(DiscoveredMods.Contains(Controller));
    NWI_REQUIRE(Class->FindFunctionByName(TEXT("NwiPoll")));
    auto* Service = Class->FindFunctionByName(TEXT("ServiceRegions"));
    auto* Attempted = FindFProperty<FBoolProperty>(Class, TEXT("PoolAttempted"));
    NWI_REQUIRE(Controller && Service && Attempted);
    NWI_REQUIRE(Controller->IsActorTickEnabled() && Controller->GetIsReplicated());
    ++GFrameCounter; Test.World->Tick(LEVELTICK_All, 0.1f);
    NWI_REQUIRE(!Attempted->GetPropertyValue_InContainer(Controller));
    auto* Material = LoadObject<UMaterial>(nullptr, TEXT("/Game/EnemyWaveIndicator/M_NwiRedPulse.M_NwiRedPulse"));
    auto* Scale = LoadObject<UCurveFloat>(nullptr, TEXT("/Game/NwiValidation/CF_TestScale.CF_TestScale"));
    auto* Alpha = LoadObject<UCurveFloat>(nullptr, TEXT("/Game/NwiValidation/CF_TestAlpha.CF_TestAlpha"));
    NWI_REQUIRE(Material && Scale && Alpha);
    AStaticMeshActor* Pulses[8]{}; UUserWidget* Huds[8]{}; UObject* Mids[8]{};
    auto* Mid = FindFProperty<FObjectPropertyBase>(PulseClass, TEXT("PulseMaterial"));
    auto* Started = FindFProperty<FFloatProperty>(PulseClass, TEXT("StartedAt"));
    NWI_REQUIRE(Mid && Started);
    for (int32 I = 0; I < 8; ++I) {
        auto* Point = FindFProperty<FStructProperty>(Class, *FString::Printf(TEXT("RegionPoint%d"), I));
        auto* Serial = FindFProperty<FIntProperty>(Class, *FString::Printf(TEXT("RegionSerial%d"), I));
        auto* Visible = FindFProperty<FIntProperty>(Class, *FString::Printf(TEXT("RegionVisible%d"), I));
        auto* Pulse = FindFProperty<FObjectPropertyBase>(Class, *FString::Printf(TEXT("AutoPulse%d"), I));
        auto* Hud = FindFProperty<FObjectPropertyBase>(Class, *FString::Printf(TEXT("AutoHud%d"), I));
        NWI_REQUIRE(Point && Point->Struct == TBaseStructure<FVector>::Get() && Serial && Visible && Pulse && Hud);
        Pulses[I] = Test.World->SpawnActor<AStaticMeshActor>(PulseClass);
        Huds[I] = NewObject<UUserWidget>(GetTransientPackage(), WidgetClass);
        NWI_REQUIRE(Huds[I]->Initialize());
        NWI_REQUIRE(Pulses[I] && Huds[I] && Initialize(Pulses[I], Material, Scale, Alpha));
        Pulses[I]->ProcessEvent(PulseClass->FindFunctionByName(TEXT("DeactivateVisual")), nullptr);
        Huds[I]->SetVisibility(ESlateVisibility::Collapsed);
        Pulse->SetObjectPropertyValue_InContainer(Controller, Pulses[I]); Hud->SetObjectPropertyValue_InContainer(Controller, Huds[I]);
        *Point->ContainerPtrToValuePtr<FVector>(Controller) = FVector(I * 1000, I * 100, 200);
        Visible->SetPropertyValue_InContainer(Controller, 1);
        Serial->SetPropertyValue_InContainer(Controller, 1);
        FindFProperty<FFloatProperty>(Class, *FString::Printf(TEXT("RegionExpires%d"), I))->SetPropertyValue_InContainer(Controller, Test.World->GetTimeSeconds()+8.f);
        Mids[I] = Mid->GetObjectPropertyValue_InContainer(Pulses[I]);
    }
    Controller->ProcessEvent(Service, nullptr);
    float StartTimes[8]{};
    for (int32 I = 0; I < 8; ++I) {
        NWI_REQUIRE(Pulses[I]->GetActorLocation().Equals(FVector(I * 1000, I * 100, 200)));
        NWI_REQUIRE(!Pulses[I]->IsHidden() && Pulses[I]->IsActorTickEnabled() && Huds[I]->GetVisibility() == ESlateVisibility::HitTestInvisible);
        StartTimes[I] = Started->GetPropertyValue_InContainer(Pulses[I]);
    }
    for (int32 J = 0; J < 100; ++J) {
        for (int32 I = 0; I < 8; ++I) FindFProperty<FIntProperty>(Class, *FString::Printf(TEXT("RegionSerial%d"), I))->SetPropertyValue_InContainer(Controller, J+2);
        Controller->ProcessEvent(Service, nullptr);
    }
    for (int32 I = 0; I < 8; ++I) {
        NWI_REQUIRE(Mid->GetObjectPropertyValue_InContainer(Pulses[I]) == Mids[I]);
        NWI_REQUIRE(Started->GetPropertyValue_InContainer(Pulses[I]) == StartTimes[I]);
    }
    // Independent timer expiry must hide the pool even if no further spawn events arrive.
    for (int32 J = 0; J < 90; ++J) { ++GFrameCounter; Test.World->Tick(LEVELTICK_All, 0.1f); }
    for (int32 I = 0; I < 8; ++I) NWI_REQUIRE(Pulses[I]->IsHidden() && !Pulses[I]->IsActorTickEnabled() && Huds[I]->GetVisibility() == ESlateVisibility::Collapsed);
    for (int32 I = 0; I < 8; ++I) {
        FindFProperty<FIntProperty>(Class, *FString::Printf(TEXT("RegionSerial%d"), I))->SetPropertyValue_InContainer(Controller, 200);
        FindFProperty<FIntProperty>(Class, *FString::Printf(TEXT("RegionVisible%d"), I))->SetPropertyValue_InContainer(Controller, 1);
        FindFProperty<FFloatProperty>(Class, *FString::Printf(TEXT("RegionExpires%d"), I))->SetPropertyValue_InContainer(Controller, Test.World->GetTimeSeconds()+8.f);
    }
    Controller->ProcessEvent(Service, nullptr);
    NWI_REQUIRE(!Pulses[0]->IsHidden());
    for (int32 I = 0; I < 8; ++I) FindFProperty<FIntProperty>(Class, *FString::Printf(TEXT("RegionVisible%d"), I))->SetPropertyValue_InContainer(Controller, 0);
    for (int32 I = 0; I < 8; ++I) FindFProperty<FIntProperty>(Class, *FString::Printf(TEXT("RegionSerial%d"), I))->SetPropertyValue_InContainer(Controller, 201);
    Controller->ProcessEvent(Service, nullptr);
    for (int32 I = 0; I < 8; ++I) NWI_REQUIRE(Pulses[I]->IsHidden() && Huds[I]->GetVisibility() == ESlateVisibility::Collapsed);
    // Exercise the serialized page's actual button delegate, disk round trip and pool update.
    auto* Settings = FindFProperty<FObjectPropertyBase>(Class, TEXT("Settings"))->GetObjectPropertyValue_InContainer(Controller);
    FFloatProperty* SphereChannels[] = {
        Settings ? FindFProperty<FFloatProperty>(Settings->GetClass(), TEXT("Red")) : nullptr,
        Settings ? FindFProperty<FFloatProperty>(Settings->GetClass(), TEXT("Green")) : nullptr,
        Settings ? FindFProperty<FFloatProperty>(Settings->GetClass(), TEXT("Blue")) : nullptr
    };
    auto* SettingsRevision = Settings ? FindFProperty<FIntProperty>(Settings->GetClass(), TEXT("Revision")) : nullptr;
    NWI_REQUIRE(SphereChannels[0] && SphereChannels[1] && SphereChannels[2] && SettingsRevision);
    NWI_REQUIRE(SphereChannels[0]->GetPropertyValue_InContainer(Settings) == 1.f);
    // Persisted settings may contain HDR-era values; runtime must normalize them before opening the page.
    for (auto* Channel : SphereChannels) Channel->SetPropertyValue_InContainer(Settings, 1.5f);
    SettingsRevision->SetPropertyValue_InContainer(Settings, SettingsRevision->GetPropertyValue_InContainer(Settings) + 1);
    Controller->ProcessEvent(Class->FindFunctionByName(TEXT("RefreshSettings")), nullptr);
    FLinearColor ClampedSphereTint;
    NWI_REQUIRE(Cast<UMaterialInstanceDynamic>(Mids[0])->GetVectorParameterValue(FMaterialParameterInfo(TEXT("Tint")), ClampedSphereTint));
    NWI_REQUIRE(ClampedSphereTint.Equals(FLinearColor::White));
    auto* PageClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/EnemyWaveIndicator/WBP_NwiSettings.WBP_NwiSettings_C"));
    FScopedLanguage Language;
    NWI_REQUIRE(Language.Set(TEXT("en")));
    auto* Page = NewObject<UUserWidget>(GetTransientPackage(), PageClass);
    NWI_REQUIRE(Settings && Page && Page->Initialize());
    FindFProperty<FObjectPropertyBase>(PageClass, TEXT("Settings"))->SetObjectPropertyValue_InContainer(Page, Settings);
    const FString Slot = TEXT("NwiValidationSettings_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    FindFProperty<FStrProperty>(PageClass, TEXT("SaveSlot"))->SetPropertyValue_InContainer(Page, Slot);
    Page->ProcessEvent(PageClass->FindFunctionByName(TEXT("Construct")), nullptr);
    auto* LabelInput = Cast<UEditableTextBox>(Page->WidgetTree->FindWidget(TEXT("InputLabel")));
    auto* RadiusInput = Cast<USpinBox>(Page->WidgetTree->FindWidget(TEXT("InputRadius")));
    auto* TimeInput = Cast<USpinBox>(Page->WidgetTree->FindWidget(TEXT("InputDuration")));
    auto* Button = Cast<UButton>(Page->WidgetTree->FindWidget(TEXT("ApplyButton")));
    NWI_REQUIRE(LabelInput && RadiusInput && TimeInput && Button);
    NWI_REQUIRE(Cast<UTextBlock>(Page->WidgetTree->FindWidget(TEXT("Title")))->GetText().ToString() == TEXT("Enemy Wave Indicator  |  0.9.1"));
    NWI_REQUIRE(Cast<UTextBlock>(Page->WidgetTree->FindWidget(TEXT("WaveName2")))->GetText().ToString() == TEXT("Egg hunt ambush"));
    auto* SettingsPanel = Cast<UVerticalBox>(Page->WidgetTree->FindWidget(TEXT("SettingsPanel")));
    auto* WaveGrid = Cast<UGridPanel>(Page->WidgetTree->FindWidget(TEXT("WaveGrid")));
    auto* TextGrid = Cast<UGridPanel>(Page->WidgetTree->FindWidget(TEXT("TextGrid")));
    auto* SphereGrid = Cast<UGridPanel>(Page->WidgetTree->FindWidget(TEXT("SphereGrid")));
    NWI_REQUIRE(SettingsPanel && WaveGrid && TextGrid && SphereGrid);
    auto* PageTitle = Cast<UTextBlock>(Page->WidgetTree->FindWidget(TEXT("Title")));
    auto* SectionTitle = Cast<UTextBlock>(Page->WidgetTree->FindWidget(TEXT("WaveSection")));
    auto* WaveBody = Cast<UTextBlock>(Page->WidgetTree->FindWidget(TEXT("WaveName2")));
    auto* ControlBody = Cast<UTextBlock>(Page->WidgetTree->FindWidget(TEXT("Caption9")));
    auto* PreviewText = Cast<UTextBlock>(Page->WidgetTree->FindWidget(TEXT("TextPreview")));
    auto* ApplyText = Cast<UTextBlock>(Page->WidgetTree->FindWidget(TEXT("ApplyText")));
    NWI_REQUIRE(PageTitle && SectionTitle && WaveBody && ControlBody && PreviewText && ApplyText);
    NWI_REQUIRE(PageTitle->Font.Size == 21 && SectionTitle->Font.Size == 19);
    NWI_REQUIRE(WaveBody->Font.Size == 13 && ControlBody->Font.Size == 13);
    NWI_REQUIRE(PreviewText->Font.Size == 18 && ApplyText->Font.Size == 16);
    NWI_REQUIRE(PageTitle->Font.Size > SectionTitle->Font.Size && SectionTitle->Font.Size > WaveBody->Font.Size);
    NWI_REQUIRE(SettingsPanel->GetChildAt(2) == WaveGrid && SettingsPanel->GetChildAt(4) == TextGrid && SettingsPanel->GetChildAt(8) == SphereGrid);
    NWI_REQUIRE(WaveGrid->GetChildrenCount() == nwi::WaveTypeCount * 3 && TextGrid->GetChildrenCount() == 16 && SphereGrid->GetChildrenCount() == 12);
    NWI_REQUIRE(TextOutput(Page, TEXT("GetPageInfo"), TEXT("PageName")) == TEXT("Indicator settings"));
    NWI_REQUIRE(TextOutput(Controller, TEXT("GetModInfo"), TEXT("ModName")) == TEXT("Enemy Wave Indicator"));
    NWI_REQUIRE(Language.Set(TEXT("zh-CN")));
    auto* ChinesePage = NewObject<UUserWidget>(GetTransientPackage(), PageClass);
    NWI_REQUIRE(ChinesePage && ChinesePage->Initialize());
    FindFProperty<FObjectPropertyBase>(PageClass, TEXT("Settings"))->SetObjectPropertyValue_InContainer(ChinesePage, Settings);
    ChinesePage->ProcessEvent(PageClass->FindFunctionByName(TEXT("Construct")), nullptr);
    NWI_REQUIRE(Cast<UTextBlock>(ChinesePage->WidgetTree->FindWidget(TEXT("Title")))->GetText().ToString() == TEXT("敌潮指示器  |  0.9.1"));
    NWI_REQUIRE(Cast<UTextBlock>(ChinesePage->WidgetTree->FindWidget(TEXT("WaveSection")))->GetText().ToString() == TEXT("虫潮播报"));
    NWI_REQUIRE(Cast<UTextBlock>(ChinesePage->WidgetTree->FindWidget(TEXT("WaveName0")))->GetText().ToString() == TEXT("自然潮"));
    NWI_REQUIRE(Cast<UTextBlock>(ChinesePage->WidgetTree->FindWidget(TEXT("WaveName2")))->GetText().ToString() == TEXT("虫蛋伏击"));
    NWI_REQUIRE(Cast<UTextBlock>(ChinesePage->WidgetTree->FindWidget(TEXT("WaveName25")))->GetText().ToString() == TEXT("搜救行动：矿骡伏击"));
    NWI_REQUIRE(Cast<UTextBlock>(ChinesePage->WidgetTree->FindWidget(TEXT("WaveName26")))->GetText().ToString() == TEXT("搜救行动：据点防守"));
    NWI_REQUIRE(Cast<UTextBlock>(ChinesePage->WidgetTree->FindWidget(TEXT("WaveName27")))->GetText().ToString() == TEXT("搜救行动：撤离"));
    NWI_REQUIRE(Cast<UTextBlock>(ChinesePage->WidgetTree->FindWidget(TEXT("WaveName29")))->GetText().ToString() == TEXT("无畏异虫潮"));
    NWI_REQUIRE(Cast<UTextBlock>(ChinesePage->WidgetTree->FindWidget(TEXT("TextSection")))->GetText().ToString() == TEXT("播报警示文本"));
    NWI_REQUIRE(Cast<UTextBlock>(ChinesePage->WidgetTree->FindWidget(TEXT("SphereSection")))->GetText().ToString() == TEXT("警示球体"));
    NWI_REQUIRE(Cast<UTextBlock>(ChinesePage->WidgetTree->FindWidget(TEXT("PreviewCaption")))->GetText().ToString() == TEXT("实时文字预览"));
    NWI_REQUIRE(Cast<UTextBlock>(ChinesePage->WidgetTree->FindWidget(TEXT("SaveStatus")))->GetText().IsEmpty());
    // Discovery-facing metadata stays stable; only the actual page contents are localized.
    NWI_REQUIRE(TextOutput(ChinesePage, TEXT("GetPageInfo"), TEXT("PageName")) == TEXT("Indicator settings"));
    NWI_REQUIRE(TextOutput(Controller, TEXT("GetModInfo"), TEXT("ModName")) == TEXT("Enemy Wave Indicator"));
    NWI_REQUIRE(Cast<UEditableTextBox>(ChinesePage->WidgetTree->FindWidget(TEXT("InputLabel")))->GetText().ToString() == TEXT("[!] NATURAL WAVE"));
    NWI_REQUIRE(Cast<UEditableTextBox>(ChinesePage->WidgetTree->FindWidget(TEXT("InputLabelType2")))->GetText().ToString() == TEXT("[!] Egg hunt ambush"));
    const FString ChineseSlot = Slot + TEXT("_zh-CN");
    FindFProperty<FStrProperty>(PageClass, TEXT("SaveSlot"))->SetPropertyValue_InContainer(ChinesePage, ChineseSlot);
    Cast<UButton>(ChinesePage->WidgetTree->FindWidget(TEXT("ApplyButton")))->OnClicked.Broadcast();
    NWI_REQUIRE(Cast<UTextBlock>(ChinesePage->WidgetTree->FindWidget(TEXT("SaveStatus")))->GetText().ToString() == TEXT("已应用并保存。"));
    NWI_REQUIRE(UGameplayStatics::DeleteGameInSlot(ChineseSlot, 0));
    NWI_REQUIRE(Language.Set(TEXT("en")));
    NWI_REQUIRE(Cast<UCheckBox>(Page->WidgetTree->FindWidget(TEXT("InputNaturalEnabled")))->IsChecked());
    for (uint32 I=1; I<nwi::WaveTypeCount; ++I) {
        auto* Toggle=Cast<UCheckBox>(Page->WidgetTree->FindWidget(*FString::Printf(TEXT("InputEnabledType%u"),I)));
        auto* Text=Cast<UEditableTextBox>(Page->WidgetTree->FindWidget(*FString::Printf(TEXT("InputLabelType%u"),I)));
        NWI_REQUIRE(Toggle && Text && Toggle->IsChecked()==(I!=1 && I!=6 && I!=32 && I!=34));
        Toggle->SetIsChecked((I%2)==1);Text->SetText(FText::FromString(FString::Printf(TEXT("TYPE %u"),I)));
    }
    NWI_REQUIRE(Cast<USpinBox>(Page->WidgetTree->FindWidget(TEXT("InputTextAR")))->GetValue()==1.f);
    NWI_REQUIRE(Cast<USpinBox>(Page->WidgetTree->FindWidget(TEXT("InputTextAG")))->GetValue()==1.f);
    NWI_REQUIRE(Cast<USpinBox>(Page->WidgetTree->FindWidget(TEXT("InputTextAB")))->GetValue()==0.f);
    NWI_REQUIRE(Cast<USpinBox>(Page->WidgetTree->FindWidget(TEXT("InputTextBR")))->GetValue()==1.f);
    NWI_REQUIRE(Cast<USpinBox>(Page->WidgetTree->FindWidget(TEXT("InputTextBG")))->GetValue()==0.f);
    NWI_REQUIRE(Cast<USpinBox>(Page->WidgetTree->FindWidget(TEXT("InputTextBB")))->GetValue()==0.f);
    for (const TCHAR* Name : {TEXT("InputRed"), TEXT("InputGreen"), TEXT("InputBlue")}) {
        auto* Channel = Cast<USpinBox>(Page->WidgetTree->FindWidget(Name));
        NWI_REQUIRE(Channel && Channel->GetMaxValue() == 1.f && Channel->GetMaxSliderValue() == 1.f);
        NWI_REQUIRE(Channel->GetValue() == 1.f); // Construct clamps legacy saved values before displaying them.
        Channel->SetValue(1.1f);
    }
    LabelInput->SetText(FText::FromString(TEXT("WATCH OUT"))); RadiusInput->SetValue(6.f); TimeInput->SetValue(12.f);
    auto* OpacityInput=Cast<USpinBox>(Page->WidgetTree->FindWidget(TEXT("InputOpacity")));auto* HzInput=Cast<USpinBox>(Page->WidgetTree->FindWidget(TEXT("InputBlinkHz")));NWI_REQUIRE(OpacityInput && HzInput);
    OpacityInput->SetValue(.65f);HzInput->SetValue(2.f);
    auto* PageTick=PageClass->FindFunctionByName(TEXT("Tick"));NWI_REQUIRE(PageTick);
    FStructOnScope TickParams(PageTick);Page->ProcessEvent(PageTick,TickParams.GetStructMemory());
    auto* Swatch=Cast<UImage>(Page->WidgetTree->FindWidget(TEXT("SpherePreview")));NWI_REQUIRE(Swatch && Swatch->ColorAndOpacity.A==.65f);
    NWI_REQUIRE(FindFProperty<FFloatProperty>(Settings->GetClass(),TEXT("Opacity"))->GetPropertyValue_InContainer(Settings)==.4f); // Preview has no save side effects.
    Button->OnClicked.Broadcast();
    NWI_REQUIRE(FindFProperty<FStrProperty>(Settings->GetClass(), TEXT("Label"))->GetPropertyValue_InContainer(Settings) == TEXT("WATCH OUT"));
    for (auto* Channel : SphereChannels) NWI_REQUIRE(Channel->GetPropertyValue_InContainer(Settings) == 1.f);
    NWI_REQUIRE(Cast<UTextBlock>(Page->WidgetTree->FindWidget(TEXT("SaveStatus")))->GetText().ToString() == TEXT("Applied and saved."));
    auto* Reloaded = UGameplayStatics::LoadGameFromSlot(Slot, 0);
    NWI_REQUIRE(Reloaded && UGameplayStatics::DeleteGameInSlot(Slot, 0));
    NWI_REQUIRE(FindFProperty<FFloatProperty>(Reloaded->GetClass(), TEXT("Duration"))->GetPropertyValue_InContainer(Reloaded) == 12.f);
    NWI_REQUIRE(FindFProperty<FFloatProperty>(Reloaded->GetClass(),TEXT("Opacity"))->GetPropertyValue_InContainer(Reloaded)==.65f);
    for (uint32 I=1; I<nwi::WaveTypeCount; ++I) {
        NWI_REQUIRE(FindFProperty<FBoolProperty>(Reloaded->GetClass(),*FString::Printf(TEXT("EnabledType%u"),I))->GetPropertyValue_InContainer(Reloaded)==((I%2)==1));
        NWI_REQUIRE(FindFProperty<FStrProperty>(Reloaded->GetClass(),*FString::Printf(TEXT("LabelType%u"),I))->GetPropertyValue_InContainer(Reloaded)==FString::Printf(TEXT("TYPE %u"),I));
    }
    Controller->ProcessEvent(Class->FindFunctionByName(TEXT("RefreshSettings")), nullptr);
    float SavedOpacity=0; NWI_REQUIRE(Cast<UMaterialInstanceDynamic>(Mids[0])->GetScalarParameterValue(FMaterialParameterInfo(TEXT("Opacity")),SavedOpacity) && SavedOpacity==.65f);
    NWI_REQUIRE(FindFProperty<FFloatProperty>(Class, TEXT("DurationSec"))->GetPropertyValue_InContainer(Controller) == 12.f);
    for (int32 I = 0; I < 8; ++I) NWI_REQUIRE(FindFProperty<FFloatProperty>(PulseClass, TEXT("RadiusScale"))->GetPropertyValue_InContainer(Pulses[I]) == 6.f && Mid->GetObjectPropertyValue_InContainer(Pulses[I]) == Mids[I]);
    auto* Pc = Test.World->SpawnActor<APlayerController>(); auto* Pawn = Test.World->SpawnActor<ADefaultPawn>();
    NWI_REQUIRE(Pc && Pawn); auto* Instance = NewObject<UGameInstance>(GEngine); Test.World->SetGameInstance(Instance);
    auto* Local = NewObject<ULocalPlayer>(GEngine); Pc->Player = Local; Local->PlayerController = Pc;
    Pc->Possess(Pawn); Huds[0]->SetOwningPlayer(Pc);
    NWI_REQUIRE(Pawn->SetActorLocation(FVector(0, 0, 0), false, nullptr, ETeleportType::TeleportPhysics));
    *FindFProperty<FStructProperty>(WidgetClass, TEXT("WorldLocation"))->ContainerPtrToValuePtr<FVector>(Huds[0]) = FVector(300, 400, 0);
    Huds[0]->ProcessEvent(WidgetClass->FindFunctionByName(TEXT("UpdateWarning")), nullptr);
    auto* MarkerText = Cast<UTextBlock>(Huds[0]->WidgetTree->FindWidget(TEXT("MarkerText")));
    NWI_REQUIRE(MarkerText && MarkerText->GetText().ToString() == TEXT("WATCH OUT  |  5 m"));
    NWI_REQUIRE(MarkerText->GetRenderOpacity()==1.f);
    NWI_REQUIRE(MarkerText->ColorAndOpacity.GetSpecifiedColor()==FLinearColor::Red || MarkerText->ColorAndOpacity.GetSpecifiedColor()==FLinearColor::Yellow);
    NWI_REQUIRE(FindFProperty<FFloatProperty>(WidgetClass,TEXT("BlinkHz"))->GetPropertyValue_InContainer(Huds[0])==2.f);
    FindFProperty<FBoolProperty>(WidgetClass, TEXT("BlinkEnabled"))->SetPropertyValue_InContainer(Huds[0], false);
    Huds[0]->ProcessEvent(WidgetClass->FindFunctionByName(TEXT("UpdateWarning")), nullptr);
    NWI_REQUIRE(MarkerText->GetRenderOpacity() == 1.f && MarkerText->ColorAndOpacity.GetSpecifiedColor()==FLinearColor::Yellow);
    auto* PagesFunction = Class->FindFunctionByName(TEXT("GetModPages")); NWI_REQUIRE(PagesFunction);
    FStructOnScope PageParams(PagesFunction); Controller->ProcessEvent(PagesFunction, PageParams.GetStructMemory());
    auto* PagesProperty = FindFProperty<FArrayProperty>(PagesFunction, TEXT("HubPages")); NWI_REQUIRE(PagesProperty);
    FScriptArrayHelper Pages(PagesProperty, PagesProperty->ContainerPtrToValuePtr<void>(PageParams.GetStructMemory()));
    NWI_REQUIRE(Pages.Num() == 1 && CastField<FInterfaceProperty>(PagesProperty->Inner));
    auto* HubPage = Cast<UUserWidget>(reinterpret_cast<FScriptInterface*>(Pages.GetRawPtr(0))->GetObject());
    NWI_REQUIRE(HubPage && HubPage->GetClass() == PageClass);
    NWI_REQUIRE(FindFProperty<FObjectPropertyBase>(PageClass, TEXT("Settings"))->GetObjectPropertyValue_InContainer(HubPage) == Settings);
    Controller->ProcessEvent(PagesFunction, PageParams.GetStructMemory());
    NWI_REQUIRE(reinterpret_cast<FScriptInterface*>(Pages.GetRawPtr(0))->GetObject() == HubPage);
    // Exercise every replicated source ID through actual saved Blueprint display branches.
    for (uint32 I=1; I<nwi::WaveTypeCount; ++I) {
        FindFProperty<FIntProperty>(Class,TEXT("RegionType0"))->SetPropertyValue_InContainer(Controller,I);
        FindFProperty<FIntProperty>(Class,TEXT("RegionSerial0"))->SetPropertyValue_InContainer(Controller,1000+I);
        FindFProperty<FIntProperty>(Class,TEXT("RegionVisible0"))->SetPropertyValue_InContainer(Controller,1);
        FindFProperty<FFloatProperty>(Class,TEXT("RegionExpires0"))->SetPropertyValue_InContainer(Controller,Test.World->GetTimeSeconds()+8.f);
        Controller->ProcessEvent(Service,nullptr);
        NWI_REQUIRE((Huds[0]->GetVisibility()==ESlateVisibility::HitTestInvisible)==((I%2)==1));
        if(I%2) NWI_REQUIRE(FindFProperty<FTextProperty>(WidgetClass,TEXT("BaseLabel"))->GetPropertyValue_InContainer(Huds[0]).ToString()==FString::Printf(TEXT("TYPE %u"),I));
        NWI_REQUIRE(FindFProperty<FIntProperty>(Class,*FString::Printf(TEXT("NativeEnabled%u"),I))->GetPropertyValue_InContainer(Controller)==int32(I%2));
    }
    UE_LOG(LogTemp,Display,TEXT("NWI_TEST Mod Hub interface discovery, stable metadata, explicit 21/19/13 heading-body typography, compact English/zh-CN page contents and unchanged English marker defaults passed"));
    UE_LOG(LogTemp,Display,TEXT("NWI_TEST 36 wave types: all default on except IDs 1/6/32/34, yellow-red text defaults, independent persistence and replicated source selection passed"));
    // Real Slate attachment matters: visibility alone cannot recover a viewport cleared after Init.
    auto* Viewport = NewObject<UGameViewportClient>(GEngine);
    auto Overlay = SNew(SOverlay);
    Viewport->SetViewportOverlayWidget(TSharedPtr<SWindow>(), Overlay);
    GEngine->GetWorldContextFromWorldChecked(Test.World).GameViewport = Viewport;
    Huds[0]->AddToViewport();
    UE_LOG(LogTemp,Display,TEXT("NWI_TEST real viewport attached=%d slots=%d before simulated HUD cleanup"),Huds[0]->IsInViewport(),Overlay->GetChildren()->Num());
    NWI_REQUIRE(Huds[0]->IsInViewport() && Overlay->GetChildren()->Num() == 1);
    Huds[0]->RemoveFromParent();
    NWI_REQUIRE(!Huds[0]->IsInViewport() && !Pulses[0]->IsHidden());
    const auto SerialBefore = FindFProperty<FIntProperty>(Class,TEXT("AppliedSerial0"))->GetPropertyValue_InContainer(Controller);
    const auto PulseStartBefore = Started->GetPropertyValue_InContainer(Pulses[0]);
    Controller->ProcessEvent(Service, nullptr); // No new spawn/serial: recovery must still run.
    NWI_REQUIRE(Huds[0]->IsInViewport() && Overlay->GetChildren()->Num() == 1);
    for (int32 I=0; I<10; ++I) Controller->ProcessEvent(Service, nullptr);
    NWI_REQUIRE(Overlay->GetChildren()->Num() == 1);
    NWI_REQUIRE(FindFProperty<FIntProperty>(Class,TEXT("AppliedSerial0"))->GetPropertyValue_InContainer(Controller) == SerialBefore);
    NWI_REQUIRE(Started->GetPropertyValue_InContainer(Pulses[0]) == PulseStartBefore);
    Huds[0]->RemoveFromParent();
    UE_LOG(LogTemp,Display,TEXT("NWI_TEST removed viewport HUD reattached without a new serial, duplicate Slate slot or pulse restart"));
    NWI_REQUIRE(Pawn->Destroy() && Pc->Destroy());
    UE_LOG(LogTemp, Display, TEXT("NWI_TEST settings button/save/reload, pooled radius update, actual pawn 3-4-5 distance text and blink disable passed; Mod Hub H discovery still requires game testing"));
    NWI_REQUIRE(Controller->Destroy());
    for (auto* Pulse : Pulses) NWI_REQUIRE(Pulse->IsActorBeingDestroyed());
    UE_LOG(LogTemp, Display, TEXT("NWI_TEST automatic pool: Blueprint region events, 8 placements, 100 reuses without MID allocation/phase reset, timer expiry, explicit hide and EndPlay cleanup; 91 World ticks"));
    return true;
}
}

#include "NwiCaptureValidation.inl"

// Substitute only transient test CDO paths; the next cook process still reads the saved game paths.
struct FTestAutomaticCurves
{
    UObject* Defaults;
    FString Scale, Alpha;
    explicit FTestAutomaticCurves(UClass* Class) : Defaults(Class->GetDefaultObject())
    {
        auto* S = FindFProperty<FStrProperty>(Class, TEXT("ScalePath"));
        auto* A = FindFProperty<FStrProperty>(Class, TEXT("AlphaPath"));
        Scale = S->GetPropertyValue_InContainer(Defaults); Alpha = A->GetPropertyValue_InContainer(Defaults);
        S->SetPropertyValue_InContainer(Defaults, TEXT("/Game/NwiValidation/CF_TestScale.CF_TestScale"));
        A->SetPropertyValue_InContainer(Defaults, TEXT("/Game/NwiValidation/CF_TestAlpha.CF_TestAlpha"));
    }
    ~FTestAutomaticCurves()
    {
        FindFProperty<FStrProperty>(Defaults->GetClass(), TEXT("ScalePath"))->SetPropertyValue_InContainer(Defaults, Scale);
        FindFProperty<FStrProperty>(Defaults->GetClass(), TEXT("AlphaPath"))->SetPropertyValue_InContainer(Defaults, Alpha);
    }
};

bool ValidateNwiPresentation()
{
    auto* PulseClass = LoadClass<AStaticMeshActor>(nullptr, TEXT("/Game/EnemyWaveIndicator/BP_NwiPulse.BP_NwiPulse_C"));
    auto* WidgetClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/EnemyWaveIndicator/WBP_NwiMarker.WBP_NwiMarker_C"));
    auto* ResourceClass = LoadClass<AActor>(nullptr, TEXT("/Game/EnemyWaveIndicator/BP_NwiResources.BP_NwiResources_C"));
    auto* VisualClass = LoadClass<AActor>(nullptr, TEXT("/Game/EnemyWaveIndicator/BP_NwiVisualTest.BP_NwiVisualTest_C"));
    NWI_REQUIRE(PulseClass && WidgetClass && ResourceClass && VisualClass);
    auto* AutomaticClass = LoadClass<AActor>(nullptr, TEXT("/Game/EnemyWaveIndicator/BP_NwiAuto.BP_NwiAuto_C"));
    NWI_REQUIRE(AutomaticClass);
    FTestAutomaticCurves TestCurves(AutomaticClass);
    NWI_REQUIRE(ValidatePlacement(WidgetClass));
    UE_LOG(LogTemp, Display, TEXT("NWI_TEST saved Blueprint classes loaded"));
    const int32 Before = GEngine->GetWorldContexts().Num();
    NWI_REQUIRE(ValidateAutomatic(PulseClass, WidgetClass));
    NWI_REQUIRE(ValidateCapture());
    NWI_REQUIRE(GEngine->GetWorldContexts().Num() == Before);
    {
        // No local controller: finite initialization retries and F5 must allocate no visual pair.
        FTestWorld Test;
        NWI_REQUIRE(Test.World);
        auto* Visual = Test.World->SpawnActor<AActor>(VisualClass);
        auto* Run = VisualClass->FindFunctionByName(TEXT("RunVisualTest"));
        auto* Tries = FindFProperty<FIntProperty>(VisualClass, TEXT("SetupTries"));
        auto* Attempted = FindFProperty<FBoolProperty>(VisualClass, TEXT("SetupAttempted"));
        auto* Pulse = FindFProperty<FObjectPropertyBase>(VisualClass, TEXT("Pulse"));
        auto* Hud = FindFProperty<FObjectPropertyBase>(VisualClass, TEXT("Hud"));
        NWI_REQUIRE(Visual && Run && Tries && Attempted && Pulse && Hud);
        NWI_REQUIRE(!Visual->IsActorTickEnabled() && !Visual->GetIsReplicated());
        NWI_REQUIRE(Tries->GetPropertyValue_InContainer(Visual) == 1);
        // Stay below WorldSettings' maximum delta clamp, so timers receive forty game seconds.
        for (int32 I = 0; I < 400; ++I)
        {
            if (I < 80) Visual->ProcessEvent(Run, nullptr);
            ++GFrameCounter;
            Test.World->Tick(LEVELTICK_All, 0.1f);
        }
        UE_LOG(LogTemp, Display, TEXT("NWI_TEST initialization attempts=%d time=%.2f"), Tries->GetPropertyValue_InContainer(Visual), Test.World->GetTimeSeconds());
        NWI_REQUIRE(Tries->GetPropertyValue_InContainer(Visual) == 30);
        NWI_REQUIRE(!Attempted->GetPropertyValue_InContainer(Visual));
        NWI_REQUIRE(!Pulse->GetObjectPropertyValue_InContainer(Visual) && !Hud->GetObjectPropertyValue_InContainer(Visual));
        NWI_REQUIRE(Visual->Destroy());
        UE_LOG(LogTemp, Display, TEXT("NWI_TEST visual test: 30 finite setup attempts, 80 guarded requests without local controller, no visual allocation"));
    }
    NWI_REQUIRE(GEngine->GetWorldContexts().Num() == Before);
    NWI_REQUIRE(ValidateResources(ResourceClass, PulseClass));
    NWI_REQUIRE(GEngine->GetWorldContexts().Num() == Before);
    NWI_REQUIRE(ValidateInWorld(PulseClass));
    NWI_REQUIRE(GEngine->GetWorldContexts().Num() == Before);
    NWI_REQUIRE(ValidateInWorld(PulseClass));
    NWI_REQUIRE(GEngine->GetWorldContexts().Num() == Before);
    UE_LOG(LogTemp, Display, TEXT("NWI_TEST six isolated worlds cleaned up; runtime graph checks passed"));
    return true;
}
