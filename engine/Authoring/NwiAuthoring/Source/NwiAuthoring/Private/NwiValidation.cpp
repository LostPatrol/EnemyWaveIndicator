// Headless integration tests follow UE's EngineAutomationTests transient UWorld lifecycle.
#include "NwiValidation.h"
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
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

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

    auto* Material = LoadObject<UMaterial>(nullptr, TEXT("/Game/NormalWaveIndicator/M_NwiRedPulse.M_NwiRedPulse"));
    auto* Scale = LoadObject<UCurveFloat>(nullptr, TEXT("/Game/NwiValidation/CF_TestScale.CF_TestScale"));
    auto* Alpha = LoadObject<UCurveFloat>(nullptr, TEXT("/Game/NwiValidation/CF_TestAlpha.CF_TestAlpha"));
    NWI_REQUIRE(Material && Scale && Alpha);
    FLinearColor Tint;
    NWI_REQUIRE(Material->GetVectorParameterValue(FMaterialParameterInfo(TEXT("Tint")), Tint));
    NWI_REQUIRE(Tint.Equals(FLinearColor(3.0f, 0.01f, 0.005f, 1.0f)) && !Material->TwoSided);
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

// Exercise the saved Blueprint caller after replacing ONLY its own zero-argument UFunction.
namespace {
int32 PollCalls = 0;
void TestNativePoll(UObject* Context, FFrame& Stack, void*)
{
    P_FINISH;
    ++PollCalls;
    for (int32 I = 0; I < 8; ++I) {
        const auto Name = FString::Printf(TEXT("NativeSerial%d"), I);
        auto* Serial = FindFProperty<FIntProperty>(Context->GetClass(), *Name);
        Serial->SetPropertyValue_InContainer(Context, PollCalls);
    }
}
struct FRestoreNative {
    UFunction* Function; FNativeFuncPtr Original; EFunctionFlags Flags;
    explicit FRestoreNative(UFunction* F) : Function(F), Original(F->GetNativeFunc()), Flags(F->FunctionFlags) {}
    ~FRestoreNative() { Function->SetNativeFunc(Original); Function->FunctionFlags = Flags; }
};
bool ValidateAutomatic(UClass* PulseClass, UClass* WidgetClass)
{
    FTestWorld Test;
    auto* Class = LoadClass<AActor>(nullptr, TEXT("/Game/NormalWaveIndicator/BP_NwiAuto.BP_NwiAuto_C"));
    NWI_REQUIRE(Test.World && Class);
    auto* Controller = Test.World->SpawnActor<AActor>(Class);
    auto* Poll = Class->FindFunctionByName(TEXT("NwiPoll"));
    auto* Service = Class->FindFunctionByName(TEXT("ServiceNative"));
    auto* Abi = FindFProperty<FIntProperty>(Class, TEXT("NativeAbi"));
    auto* Attempted = FindFProperty<FBoolProperty>(Class, TEXT("PoolAttempted"));
    NWI_REQUIRE(Controller && Poll && Service && Abi && Attempted && Poll->ParmsSize == 0);
    NWI_REQUIRE(Abi->GetPropertyValue_InContainer(Controller) == 393216);
    NWI_REQUIRE(Controller->IsActorTickEnabled() && !Controller->GetIsReplicated());
    ++GFrameCounter; Test.World->Tick(LEVELTICK_All, 0.1f);
    NWI_REQUIRE(!Attempted->GetPropertyValue_InContainer(Controller));
    auto* Material = LoadObject<UMaterial>(nullptr, TEXT("/Game/NormalWaveIndicator/M_NwiRedPulse.M_NwiRedPulse"));
    auto* Scale = LoadObject<UCurveFloat>(nullptr, TEXT("/Game/NwiValidation/CF_TestScale.CF_TestScale"));
    auto* Alpha = LoadObject<UCurveFloat>(nullptr, TEXT("/Game/NwiValidation/CF_TestAlpha.CF_TestAlpha"));
    NWI_REQUIRE(Material && Scale && Alpha);
    AStaticMeshActor* Pulses[8]{}; UUserWidget* Huds[8]{}; UObject* Mids[8]{};
    auto* Mid = FindFProperty<FObjectPropertyBase>(PulseClass, TEXT("PulseMaterial"));
    auto* Started = FindFProperty<FFloatProperty>(PulseClass, TEXT("StartedAt"));
    NWI_REQUIRE(Mid && Started);
    for (int32 I = 0; I < 8; ++I) {
        auto* Point = FindFProperty<FStructProperty>(Class, *FString::Printf(TEXT("NativePoint%d"), I));
        auto* Serial = FindFProperty<FIntProperty>(Class, *FString::Printf(TEXT("NativeSerial%d"), I));
        auto* Visible = FindFProperty<FIntProperty>(Class, *FString::Printf(TEXT("NativeVisible%d"), I));
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
        Mids[I] = Mid->GetObjectPropertyValue_InContainer(Pulses[I]);
    }
    FRestoreNative Restore(Poll);
    PollCalls = 0; Poll->SetNativeFunc(&TestNativePoll); Poll->FunctionFlags |= FUNC_Native;
    Controller->ProcessEvent(Service, nullptr);
    NWI_REQUIRE(PollCalls == 1);
    float StartTimes[8]{};
    for (int32 I = 0; I < 8; ++I) {
        NWI_REQUIRE(Pulses[I]->GetActorLocation().Equals(FVector(I * 1000, I * 100, 200)));
        NWI_REQUIRE(!Pulses[I]->IsHidden() && Pulses[I]->IsActorTickEnabled() && Huds[I]->GetVisibility() == ESlateVisibility::HitTestInvisible);
        StartTimes[I] = Started->GetPropertyValue_InContainer(Pulses[I]);
    }
    for (int32 J = 0; J < 100; ++J) Controller->ProcessEvent(Service, nullptr);
    NWI_REQUIRE(PollCalls == 101);
    for (int32 I = 0; I < 8; ++I) {
        NWI_REQUIRE(Mid->GetObjectPropertyValue_InContainer(Pulses[I]) == Mids[I]);
        NWI_REQUIRE(Started->GetPropertyValue_InContainer(Pulses[I]) == StartTimes[I]);
    }
    // Independent timer expiry must hide the pool even if the native producer stops completely.
    for (int32 J = 0; J < 90; ++J) { ++GFrameCounter; Test.World->Tick(LEVELTICK_All, 0.1f); }
    for (int32 I = 0; I < 8; ++I) NWI_REQUIRE(Pulses[I]->IsHidden() && !Pulses[I]->IsActorTickEnabled() && Huds[I]->GetVisibility() == ESlateVisibility::Collapsed);
    Controller->ProcessEvent(Service, nullptr);
    NWI_REQUIRE(!Pulses[0]->IsHidden());
    for (int32 I = 0; I < 8; ++I) FindFProperty<FIntProperty>(Class, *FString::Printf(TEXT("NativeVisible%d"), I))->SetPropertyValue_InContainer(Controller, 0);
    Controller->ProcessEvent(Service, nullptr);
    for (int32 I = 0; I < 8; ++I) NWI_REQUIRE(Pulses[I]->IsHidden() && Huds[I]->GetVisibility() == ESlateVisibility::Collapsed);
    // Exercise the serialized page's actual button delegate, disk round trip and pool update.
    auto* Settings = FindFProperty<FObjectPropertyBase>(Class, TEXT("Settings"))->GetObjectPropertyValue_InContainer(Controller);
    auto* PageClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/NormalWaveIndicator/WBP_NwiSettings.WBP_NwiSettings_C"));
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
    LabelInput->SetText(FText::FromString(TEXT("WATCH OUT"))); RadiusInput->SetValue(6.f); TimeInput->SetValue(12.f);
    Button->OnClicked.Broadcast();
    NWI_REQUIRE(FindFProperty<FStrProperty>(Settings->GetClass(), TEXT("Label"))->GetPropertyValue_InContainer(Settings) == TEXT("WATCH OUT"));
    auto* Reloaded = UGameplayStatics::LoadGameFromSlot(Slot, 0);
    NWI_REQUIRE(Reloaded && UGameplayStatics::DeleteGameInSlot(Slot, 0));
    NWI_REQUIRE(FindFProperty<FFloatProperty>(Reloaded->GetClass(), TEXT("Duration"))->GetPropertyValue_InContainer(Reloaded) == 12.f);
    Controller->ProcessEvent(Class->FindFunctionByName(TEXT("RefreshSettings")), nullptr);
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
    NWI_REQUIRE(MarkerText->GetRenderOpacity() >= .55f && MarkerText->GetRenderOpacity() <= 1.f);
    FindFProperty<FBoolProperty>(WidgetClass, TEXT("BlinkEnabled"))->SetPropertyValue_InContainer(Huds[0], false);
    Huds[0]->ProcessEvent(WidgetClass->FindFunctionByName(TEXT("UpdateWarning")), nullptr);
    NWI_REQUIRE(MarkerText->GetRenderOpacity() == 1.f);
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
    NWI_REQUIRE(Pawn->Destroy() && Pc->Destroy());
    UE_LOG(LogTemp, Display, TEXT("NWI_TEST settings button/save/reload, pooled radius update, actual pawn 3-4-5 distance text and blink disable passed; Mod Hub H discovery still requires game testing"));
    NWI_REQUIRE(Controller->Destroy());
    for (auto* Pulse : Pulses) NWI_REQUIRE(Pulse->IsActorBeingDestroyed());
    UE_LOG(LogTemp, Display, TEXT("NWI_TEST automatic pool: own native UFunction dispatch, 8 placements, 100 reuses without MID allocation/phase reset, timer expiry, native hide and EndPlay cleanup; 91 World ticks"));
    return true;
}
}

bool ValidateNwiPresentation()
{
    auto* PulseClass = LoadClass<AStaticMeshActor>(nullptr, TEXT("/Game/NormalWaveIndicator/BP_NwiPulse.BP_NwiPulse_C"));
    auto* WidgetClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/NormalWaveIndicator/WBP_NwiMarker.WBP_NwiMarker_C"));
    auto* ResourceClass = LoadClass<AActor>(nullptr, TEXT("/Game/NormalWaveIndicator/BP_NwiResources.BP_NwiResources_C"));
    auto* VisualClass = LoadClass<AActor>(nullptr, TEXT("/Game/NormalWaveIndicator/BP_NwiVisualTest.BP_NwiVisualTest_C"));
    NWI_REQUIRE(PulseClass && WidgetClass && ResourceClass && VisualClass);
    NWI_REQUIRE(ValidatePlacement(WidgetClass));
    UE_LOG(LogTemp, Display, TEXT("NWI_TEST saved Blueprint classes loaded"));
    const int32 Before = GEngine->GetWorldContexts().Num();
    NWI_REQUIRE(ValidateAutomatic(PulseClass, WidgetClass));
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
    UE_LOG(LogTemp, Display, TEXT("NWI_TEST five isolated worlds cleaned up; runtime graph checks passed"));
    return true;
}
