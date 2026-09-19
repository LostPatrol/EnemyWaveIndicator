// Headless integration tests follow UE's EngineAutomationTests transient UWorld lifecycle.
#include "EwiValidation.h"
#include "EwiWaveTypes.h"
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
#include "Components/GridSlot.h"
#include "Components/HorizontalBox.h"
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
#include "EwiValidationPlayerController.h"

// Test failures log and unwind normally; they must not deliberately crash the editor.
#define EWI_REQUIRE(Condition) do { if (!(Condition)) { UE_LOG(LogTemp, Error, TEXT("EWI validation failed: %s"), TEXT(#Condition)); return false; } } while (false)

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
    EWI_REQUIRE(Function);
    FStructOnScope Parameters(Function);
    const TCHAR* Names[] = { TEXT("Material"), TEXT("Scale"), TEXT("Alpha") };
    UObject* Inputs[] = { Material, Scale, Alpha };
    for (int32 Index = 0; Index < 3; ++Index)
    {
        auto* Property = FindFProperty<FObjectPropertyBase>(Function, Names[Index]);
        EWI_REQUIRE(Property && Property->HasAnyPropertyFlags(CPF_Parm));
        Property->SetObjectPropertyValue_InContainer(Parameters.GetStructMemory(), Inputs[Index]);
    }
    Actor->ProcessEvent(Function, Parameters.GetStructMemory());
    return true;
}

// Tick the serialized Blueprint graph while async asset loads complete.
bool TickUntil(UWorld* World, TFunctionRef<bool()> Complete, int32 MaximumTicks = 240)
{
    for (int32 Index = 0; Index < MaximumTicks; ++Index)
    {
        FlushAsyncLoading();
        ++GFrameCounter;
        World->Tick(LEVELTICK_All, 0.05f);
        if (Complete()) return true;
    }
    return false;
}

// Exercise Auto's real BeginPlay/Tick pool path under both local-first and remote-first ordering.
bool ValidateListenHostControllerSelection(UClass* AutomaticClass)
{
    if (!FSlateApplication::IsInitialized())
    {
        auto& NullRenderer = FModuleManager::LoadModuleChecked<ISlateNullRendererModule>(TEXT("SlateNullRenderer"));
        FSlateApplication::InitializeAsStandaloneApplication(NullRenderer.CreateSlateNullRenderer());
    }
    auto ConfigureWorld = [](FTestWorld& Test, AEwiValidationPlayerController*& Host,
        AEwiValidationPlayerController*& Remote, TSharedPtr<SOverlay>& Overlay, bool RemoteFirst)
    {
        auto* Instance = NewObject<UGameInstance>(GEngine);
        Test.World->SetGameInstance(Instance);
        auto* Viewport = NewObject<UGameViewportClient>(GEngine);
        const TSharedRef<SOverlay> ViewportOverlay = SNew(SOverlay);
        Overlay = ViewportOverlay;
        Viewport->SetViewportOverlayWidget(TSharedPtr<SWindow>(), ViewportOverlay);
        GEngine->GetWorldContextFromWorldChecked(Test.World).GameViewport = Viewport;

        if (RemoteFirst)
        {
            Remote = Test.World->SpawnActor<AEwiValidationPlayerController>();
            if (Remote) Remote->bValidationLocal = false;
        }
        Host = Test.World->SpawnActor<AEwiValidationPlayerController>();
        if (Host)
        {
            Host->bValidationLocal = true;
            auto* Local = NewObject<ULocalPlayer>(GEngine);
            Local->SetControllerId(0);
            Host->Player = Local;
            Local->PlayerController = Host;
        }
        if (!RemoteFirst) Remote = nullptr;
        return Instance && Viewport && Host;
    };

    // Normal listen-host order: adding a remote controller must not disturb the existing local pool.
    {
        FTestWorld Test;
        AEwiValidationPlayerController* Host = nullptr;
        AEwiValidationPlayerController* Remote = nullptr;
        TSharedPtr<SOverlay> Overlay;
        EWI_REQUIRE(Test.World && ConfigureWorld(Test, Host, Remote, Overlay, false));
        EWI_REQUIRE(UGameplayStatics::GetPlayerController(Test.World, 0) == Host && Host->IsLocalController());
        auto* Controller = Test.World->SpawnActor<AActor>(AutomaticClass);
        auto* PoolReady = FindFProperty<FBoolProperty>(AutomaticClass, TEXT("PoolReady"));
        auto* Hud = FindFProperty<FObjectPropertyBase>(AutomaticClass, TEXT("AutoHud0"));
        EWI_REQUIRE(Controller && PoolReady && Hud);
        EWI_REQUIRE(TickUntil(Test.World, [&] { return PoolReady->GetPropertyValue_InContainer(Controller); }));
        auto* OriginalHud = Cast<UUserWidget>(Hud->GetObjectPropertyValue_InContainer(Controller));
        EWI_REQUIRE(OriginalHud && OriginalHud->GetOwningPlayer() == Host);

        TArray<AEwiValidationPlayerController*> Remotes;
        for (int32 PlayerIndex = 1; PlayerIndex < 4; ++PlayerIndex)
        {
            Remote = Test.World->SpawnActor<AEwiValidationPlayerController>();
            EWI_REQUIRE(Remote); Remote->bValidationLocal = false; Remotes.Add(Remote);
            EWI_REQUIRE(UGameplayStatics::GetPlayerController(Test.World, 0) == Host);
            for (int32 TickIndex = 0; TickIndex < 20; ++TickIndex) { ++GFrameCounter; Test.World->Tick(LEVELTICK_All, 0.05f); }
            EWI_REQUIRE(PoolReady->GetPropertyValue_InContainer(Controller));
            EWI_REQUIRE(Hud->GetObjectPropertyValue_InContainer(Controller) == OriginalHud && OriginalHud->GetOwningPlayer() == Host);
        }
        auto* Service = AutomaticClass->FindFunctionByName(TEXT("ServiceRegions"));
        auto* Point = FindFProperty<FStructProperty>(AutomaticClass, TEXT("RegionPoint0"));
        auto* Serial = FindFProperty<FIntProperty>(AutomaticClass, TEXT("RegionSerial0"));
        auto* Visible = FindFProperty<FIntProperty>(AutomaticClass, TEXT("RegionVisible0"));
        auto* Expires = FindFProperty<FFloatProperty>(AutomaticClass, TEXT("RegionExpires0"));
        EWI_REQUIRE(Service && Point && Serial && Visible && Expires);
        *Point->ContainerPtrToValuePtr<FVector>(Controller) = FVector(1000.f, 2000.f, 300.f);
        Serial->SetPropertyValue_InContainer(Controller, 1);
        Visible->SetPropertyValue_InContainer(Controller, 1);
        Expires->SetPropertyValue_InContainer(Controller, Test.World->GetTimeSeconds() + 8.f);
        Controller->ProcessEvent(Service, nullptr);
        EWI_REQUIRE(OriginalHud->GetVisibility() == ESlateVisibility::HitTestInvisible);
        for (auto* JoinedRemote : Remotes) EWI_REQUIRE(JoinedRemote->Destroy());
        EWI_REQUIRE(Controller->Destroy() && Host->Destroy());
        UE_LOG(LogTemp, Display, TEXT("EWI_TEST listen host marker remained visible after three sequential remote joins"));
    }

    // Adverse ordering: index zero is remote, but controller ID zero still resolves the local host.
    {
        FTestWorld Test;
        AEwiValidationPlayerController* Host = nullptr;
        AEwiValidationPlayerController* Remote = nullptr;
        TSharedPtr<SOverlay> Overlay;
        EWI_REQUIRE(Test.World && ConfigureWorld(Test, Host, Remote, Overlay, true) && Remote);
        EWI_REQUIRE(Host->Destroy()); Host = nullptr;
        EWI_REQUIRE(UGameplayStatics::GetPlayerController(Test.World, 0) == Remote && !Remote->IsLocalController());
        EWI_REQUIRE(UGameplayStatics::GetPlayerControllerFromID(Test.World, 0) == nullptr);
        auto* Controller = Test.World->SpawnActor<AActor>(AutomaticClass);
        auto* ResourcesReady = FindFProperty<FBoolProperty>(AutomaticClass, TEXT("Ready"));
        auto* PoolAttempted = FindFProperty<FBoolProperty>(AutomaticClass, TEXT("PoolAttempted"));
        auto* PoolReady = FindFProperty<FBoolProperty>(AutomaticClass, TEXT("PoolReady"));
        auto* Hud = FindFProperty<FObjectPropertyBase>(AutomaticClass, TEXT("AutoHud0"));
        EWI_REQUIRE(Controller && ResourcesReady && PoolAttempted && PoolReady && Hud);
        // Isolate controller selection from game-only soft assets absent in the authoring project.
        ResourcesReady->SetPropertyValue_InContainer(Controller, true);
        for (int32 Index = 0; Index < 20; ++Index) { ++GFrameCounter; Test.World->Tick(LEVELTICK_All, 0.05f); }
        EWI_REQUIRE(!PoolAttempted->GetPropertyValue_InContainer(Controller) && !PoolReady->GetPropertyValue_InContainer(Controller));
        Host = Test.World->SpawnActor<AEwiValidationPlayerController>();
        EWI_REQUIRE(Host); Host->bValidationLocal = true;
        auto* Local = NewObject<ULocalPlayer>(GEngine); Local->SetControllerId(0);
        Host->Player = Local; Local->PlayerController = Host;
        EWI_REQUIRE(UGameplayStatics::GetPlayerController(Test.World, 0) == Remote);
        EWI_REQUIRE(UGameplayStatics::GetPlayerControllerFromID(Test.World, 0) == Host);
        EWI_REQUIRE(TickUntil(Test.World, [&] { return PoolReady->GetPropertyValue_InContainer(Controller); }));
        auto* LocalHud = Cast<UUserWidget>(Hud->GetObjectPropertyValue_InContainer(Controller));
        EWI_REQUIRE(PoolAttempted->GetPropertyValue_InContainer(Controller) && LocalHud && LocalHud->GetOwningPlayer() == Host);
        auto* Service = AutomaticClass->FindFunctionByName(TEXT("ServiceRegions"));
        auto* Cleanup = AutomaticClass->FindFunctionByName(TEXT("CleanupPool"));
        auto* Serial = FindFProperty<FIntProperty>(AutomaticClass, TEXT("RegionSerial0"));
        auto* Visible = FindFProperty<FIntProperty>(AutomaticClass, TEXT("RegionVisible0"));
        auto* Expires = FindFProperty<FFloatProperty>(AutomaticClass, TEXT("RegionExpires0"));
        EWI_REQUIRE(Service && Cleanup && Serial && Visible && Expires);
        Serial->SetPropertyValue_InContainer(Controller, 1);
        Visible->SetPropertyValue_InContainer(Controller, 1);
        Expires->SetPropertyValue_InContainer(Controller, Test.World->GetTimeSeconds() + 8.f);
        Controller->ProcessEvent(Service, nullptr);
        EWI_REQUIRE(LocalHud->GetVisibility() == ESlateVisibility::HitTestInvisible);
        Controller->ProcessEvent(Cleanup, nullptr);
        EWI_REQUIRE(!PoolAttempted->GetPropertyValue_InContainer(Controller) && !PoolReady->GetPropertyValue_InContainer(Controller));
        EWI_REQUIRE(Controller->Destroy() && Host->Destroy() && Remote->Destroy());
        UE_LOG(LogTemp, Display, TEXT("EWI_TEST remote-first ordering selected the local host and cleanup enabled retry"));
    }
    return true;
}

bool ValidateInWorld(UClass* PulseClass)
{
    FTestWorld Test;
    EWI_REQUIRE(Test.World);
    UE_LOG(LogTemp, Display, TEXT("EWI_TEST world initialized; spawning directly through UWorld"));
    auto* Actor = Test.World->SpawnActor<AStaticMeshActor>(PulseClass, FVector::ZeroVector, FRotator::ZeroRotator);
    EWI_REQUIRE(Actor);
    auto* Mesh = Actor->GetStaticMeshComponent();
    EWI_REQUIRE(Mesh && Mesh->GetStaticMesh());
    UE_LOG(LogTemp, Display, TEXT("EWI_TEST registered mesh profile=%s collision=%d tick=%d hidden=%d"),
        *Mesh->GetCollisionProfileName().ToString(), static_cast<int32>(Mesh->GetCollisionEnabled()), Actor->IsActorTickEnabled(), Actor->IsHidden());
    EWI_REQUIRE(Mesh->GetStaticMesh()->GetPathName() == TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    EWI_REQUIRE(Mesh->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
    EWI_REQUIRE(!Mesh->GetGenerateOverlapEvents() && !Mesh->CastShadow && !Mesh->CanEverAffectNavigation());
    EWI_REQUIRE(!Actor->GetIsReplicated() && Actor->IsHidden() && !Actor->IsActorTickEnabled());
    EWI_REQUIRE(Initialize(Actor, nullptr, nullptr, nullptr));
    EWI_REQUIRE(Actor->IsHidden() && !Actor->IsActorTickEnabled());

    auto* Material = LoadObject<UMaterial>(nullptr, TEXT("/Game/EnemyWaveIndicator/M_EwiRedPulse.M_EwiRedPulse"));
    auto* Scale = LoadObject<UCurveFloat>(nullptr, TEXT("/Game/EwiValidation/CF_TestScale.CF_TestScale"));
    auto* Alpha = LoadObject<UCurveFloat>(nullptr, TEXT("/Game/EwiValidation/CF_TestAlpha.CF_TestAlpha"));
    EWI_REQUIRE(Material && Scale && Alpha);
    FLinearColor Tint;
    EWI_REQUIRE(Material->GetVectorParameterValue(FMaterialParameterInfo(TEXT("Tint")), Tint));
    EWI_REQUIRE(Tint.Equals(FLinearColor(1.0f, 0.01f, 0.005f, 1.0f)) && !Material->TwoSided);
    EWI_REQUIRE(Material->GetShadingModels().HasShadingModel(MSM_Unlit));
    EWI_REQUIRE(Initialize(Actor, Material, Scale, Alpha));
    EWI_REQUIRE(!Actor->IsHidden() && Actor->IsActorTickEnabled() && Actor->PrimaryActorTick.IsTickFunctionRegistered());
    constexpr float Delta = 1.0f / 60.0f; // Synthetic engine timestep, not a measured display refresh rate.
    ++GFrameCounter;
    Test.World->Tick(LEVELTICK_All, Delta);
    EWI_REQUIRE(Actor->GetActorScale3D().Equals(FVector(1.875f), 0.00001f));
    auto* Property = FindFProperty<FObjectPropertyBase>(PulseClass, TEXT("PulseMaterial"));
    EWI_REQUIRE(Property);
    auto* Mid = Cast<UMaterialInstanceDynamic>(Property->GetObjectPropertyValue_InContainer(Actor));
    float Opacity = 0.0f;
    EWI_REQUIRE(Mid && Mid->GetScalarParameterValue(FMaterialParameterInfo(TEXT("Alpha")), Opacity));
    EWI_REQUIRE(FMath::IsNearlyEqual(Opacity, 0.25f, 0.00001f));
    UE_LOG(LogTemp, Display, TEXT("EWI_TEST valid input, registered World Tick, scale and Alpha passed"));

    auto* Deactivate = Actor->FindFunction(TEXT("DeactivateVisual"));
    EWI_REQUIRE(Deactivate);
    Actor->ProcessEvent(Deactivate, nullptr);
    EWI_REQUIRE(Actor->IsHidden() && !Actor->IsActorTickEnabled());
    // A varying curve proves repeated World ticks update the saved Blueprint, not just initialization.
    auto* Ramp = NewObject<UCurveFloat>();
    Ramp->FloatCurve.AddKey(0.0f, 0.0f);
    Ramp->FloatCurve.AddKey(1.0f, 1.0f);
    EWI_REQUIRE(Initialize(Actor, Material, Ramp, Alpha));
    const float Start = Actor->GetGameTimeSinceCreation();
    int32 ChangedFrames = 0;
    float Previous = -1.0f;
    for (int32 Index = 0; Index < 150; ++Index)
    {
        ++GFrameCounter;
        Test.World->Tick(LEVELTICK_All, Delta);
        const float Expected = 3.75f * Ramp->GetFloatValue(FMath::Fmod(Actor->GetGameTimeSinceCreation() - Start, 2.0f));
        const float Actual = Actor->GetActorScale3D().X;
        EWI_REQUIRE(FMath::IsNearlyEqual(Actual, Expected, 0.0001f));
        ChangedFrames += !FMath::IsNearlyEqual(Previous, Actual, 0.0001f);
        Previous = Actual;
    }
    EWI_REQUIRE(ChangedFrames > 80);
    EWI_REQUIRE(Initialize(Actor, Material, nullptr, Alpha));
    EWI_REQUIRE(Actor->IsHidden() && !Actor->IsActorTickEnabled());
    EWI_REQUIRE(Actor->Destroy());
    UE_LOG(LogTemp, Display, TEXT("EWI_TEST 150 World ticks, phase wrap, reactivation, invalid reinit and destruction passed"));
    return true;
}

// Drive stock latent actions in a headless world; blocking flush is test-only, never shipped graph code.
bool ValidateResources(UClass* ResourceClass, UClass* PulseClass)
{
    FTestWorld Test;
    EWI_REQUIRE(Test.World);
    const TCHAR* Names[] = { TEXT("Material"), TEXT("Scale"), TEXT("Alpha") };
    const TCHAR* Paths[] = { TEXT("/Game/EwiValidation/M_TestAlpha.M_TestAlpha"),
        TEXT("/Game/EwiValidation/CurveContainer.CurveContainer:CurveFloat_0"),
        TEXT("/Game/EwiValidation/CurveContainer.CurveContainer:CurveFloat_1") };
    auto* Prepare = ResourceClass->FindFunctionByName(TEXT("PrepareResources"));
    auto* Attempted = FindFProperty<FBoolProperty>(ResourceClass, TEXT("Attempted"));
    auto* Finished = FindFProperty<FBoolProperty>(ResourceClass, TEXT("Finished"));
    auto* Ready = FindFProperty<FBoolProperty>(ResourceClass, TEXT("Ready"));
    EWI_REQUIRE(Prepare && Attempted && Finished && Ready);
    FStrProperty* PathProperties[3];
    FObjectPropertyBase* Resources[3];
    for (int32 Index = 0; Index < 3; ++Index)
    {
        PathProperties[Index] = FindFProperty<FStrProperty>(ResourceClass, *(FString(Names[Index]) + TEXT("Path")));
        Resources[Index] = FindFProperty<FObjectPropertyBase>(ResourceClass, Names[Index]);
        EWI_REQUIRE(PathProperties[Index] && Resources[Index]);
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
        EWI_REQUIRE(Provider && !Provider->IsActorTickEnabled() && !Provider->GetIsReplicated());
        EWI_REQUIRE(!Attempted->GetPropertyValue_InContainer(Provider) && !Ready->GetPropertyValue_InContainer(Provider));
        for (int32 Index = 0; Index < 3; ++Index)
            PathProperties[Index]->SetPropertyValue_InContainer(Provider, Paths[Mode == 1 ? 0 : Index]);
        Provider->ProcessEvent(Prepare, nullptr);
        EWI_REQUIRE(Attempted->GetPropertyValue_InContainer(Provider));
        if (Mode == 2)
        {
            // Destroy while async work is outstanding: latent actions must not revive the provider.
            EWI_REQUIRE(Provider->Destroy());
            TickLoading(4);
            EWI_REQUIRE(Provider->IsActorBeingDestroyed());
            continue;
        }
        TickLoading(8);
        EWI_REQUIRE(Finished->GetPropertyValue_InContainer(Provider));
        EWI_REQUIRE(Ready->GetPropertyValue_InContainer(Provider) == (Mode == 0));
        if (Mode == 0)
        {
            UObject* Loaded[3];
            for (int32 Index = 0; Index < 3; ++Index)
            {
                Loaded[Index] = Resources[Index]->GetObjectPropertyValue_InContainer(Provider);
                EWI_REQUIRE(Loaded[Index] && Loaded[Index]->GetPathName() == Paths[Index]);
                PathProperties[Index]->SetPropertyValue_InContainer(Provider, TEXT(""));
            }
            // Repeated preparation must use the cache, including while a caller changes path variables.
            Provider->ProcessEvent(Prepare, nullptr);
            EWI_REQUIRE(Ready->GetPropertyValue_InContainer(Provider));
            for (int32 Index = 0; Index < 3; ++Index)
                EWI_REQUIRE(Resources[Index]->GetObjectPropertyValue_InContainer(Provider) == Loaded[Index]);
            auto* Pulse = Test.World->SpawnActor<AStaticMeshActor>(PulseClass);
            EWI_REQUIRE(Pulse && Initialize(Pulse, Loaded[0], Loaded[1], Loaded[2]));
            EWI_REQUIRE(Pulse->IsActorTickEnabled() && !Pulse->IsHidden());
            EWI_REQUIRE(Pulse->Destroy());
        }
        else EWI_REQUIRE(Resources[1]->GetObjectPropertyValue_InContainer(Provider) == nullptr);
        EWI_REQUIRE(Provider->Destroy());
    }
    UE_LOG(LogTemp, Display, TEXT("EWI_TEST async embedded resources, cached reuse, pulse handoff, wrong type and pending destruction passed; 20 resource World ticks"));
    return true;
}
}

// Execute the serialized Blueprint's actual placement event with independent geometric expectations.
bool ValidatePlacement(UClass* WidgetClass)
{
    auto* Widget = NewObject<UUserWidget>(GetTransientPackage(), WidgetClass);
    EWI_REQUIRE(Widget && Widget->Initialize());
    auto* Row = Cast<UHorizontalBox>(Widget->WidgetTree->FindWidget(TEXT("MarkerRow")));
    auto* Icon = Cast<UImage>(Widget->WidgetTree->FindWidget(TEXT("WarningIcon")));
    auto* Text = Cast<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("MarkerText")));
    EWI_REQUIRE(Row && Icon && Text && Row->GetChildAt(0) == Icon && Row->GetChildAt(1) == Text);
    EWI_REQUIRE(Icon->GetVisibility() == ESlateVisibility::Collapsed);
    EWI_REQUIRE(Icon->ColorAndOpacity.Equals(FLinearColor(1.f, 0.1f, 0.02f)));
    EWI_REQUIRE(Icon->Brush.ImageSize.Equals(FVector2D(28.f, 28.f)));
    auto* Update = WidgetClass->FindFunctionByName(TEXT("UpdatePlacement"));
    auto* Position = FindFProperty<FStructProperty>(WidgetClass, TEXT("MarkerPosition"));
    auto* Edge = FindFProperty<FBoolProperty>(WidgetClass, TEXT("AtEdge"));
    auto* Angle = FindFProperty<FFloatProperty>(WidgetClass, TEXT("ArrowAngle"));
    EWI_REQUIRE(Widget && Update && Position && Edge && Angle);
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
        EWI_REQUIRE(Evaluate(Point, true, FVector(100, 0, 0), View, Label));
        EWI_REQUIRE(!Edge->GetPropertyValue_InContainer(Widget));
        EWI_REQUIRE(Position->ContainerPtrToValuePtr<FVector2D>(Widget)->Equals(Point, 0.001f));
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
        EWI_REQUIRE(Evaluate(C.Projected, C.Front, C.Camera, View, Label));
        EWI_REQUIRE(Edge->GetPropertyValue_InContainer(Widget));
        EWI_REQUIRE(Position->ContainerPtrToValuePtr<FVector2D>(Widget)->Equals(C.Expected, 0.01f));
        EWI_REQUIRE(FMath::IsNearlyEqual(Angle->GetPropertyValue_InContainer(Widget), C.Degrees, 0.01f));
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
            EWI_REQUIRE(Evaluate(Projected, Front, Camera, Size, Label));
        }
    }
    UE_LOG(LogTemp, Display, TEXT("EWI_TEST %d serialized HUD placement cases: in-view preserved, four edges, rear, aspect ratios and viewport resize"), Cases);
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
    auto* Class = LoadClass<AActor>(nullptr, TEXT("/Game/EnemyWaveIndicator/BP_EwiAuto.BP_EwiAuto_C"));
    EWI_REQUIRE(Test.World && Class);
    auto* Controller = Test.World->SpawnActor<AActor>(Class);
    auto* HubModInterface = LoadClass<UInterface>(nullptr, TEXT("/Game/_ModHub/IHubMod.IHubMod_C"));
    EWI_REQUIRE(HubModInterface && Class->ImplementsInterface(HubModInterface));
    TArray<AActor*> DiscoveredMods;
    UGameplayStatics::GetAllActorsWithInterface(Test.World, HubModInterface, DiscoveredMods);
    EWI_REQUIRE(DiscoveredMods.Contains(Controller));
    EWI_REQUIRE(Class->FindFunctionByName(TEXT("EwiPoll")));
    auto* Service = Class->FindFunctionByName(TEXT("ServiceRegions"));
    auto* Attempted = FindFProperty<FBoolProperty>(Class, TEXT("PoolAttempted"));
    EWI_REQUIRE(Controller && Service && Attempted);
    EWI_REQUIRE(Controller->IsActorTickEnabled() && Controller->GetIsReplicated());
    EWI_REQUIRE(Controller->bAlwaysRelevant && Controller->NetUpdateFrequency == 20.f && Controller->MinNetUpdateFrequency == 20.f);
    ++GFrameCounter; Test.World->Tick(LEVELTICK_All, 0.1f);
    EWI_REQUIRE(!Attempted->GetPropertyValue_InContainer(Controller));
    auto* Material = LoadObject<UMaterial>(nullptr, TEXT("/Game/EnemyWaveIndicator/M_EwiRedPulse.M_EwiRedPulse"));
    auto* Scale = LoadObject<UCurveFloat>(nullptr, TEXT("/Game/EwiValidation/CF_TestScale.CF_TestScale"));
    auto* Alpha = LoadObject<UCurveFloat>(nullptr, TEXT("/Game/EwiValidation/CF_TestAlpha.CF_TestAlpha"));
    EWI_REQUIRE(Material && Scale && Alpha);
    AStaticMeshActor* Pulses[8]{}; UUserWidget* Huds[8]{}; UObject* Mids[8]{};
    auto* Mid = FindFProperty<FObjectPropertyBase>(PulseClass, TEXT("PulseMaterial"));
    auto* Started = FindFProperty<FFloatProperty>(PulseClass, TEXT("StartedAt"));
    EWI_REQUIRE(Mid && Started);
    for (int32 I = 0; I < 8; ++I) {
        auto* Point = FindFProperty<FStructProperty>(Class, *FString::Printf(TEXT("RegionPoint%d"), I));
        auto* Serial = FindFProperty<FIntProperty>(Class, *FString::Printf(TEXT("RegionSerial%d"), I));
        auto* Visible = FindFProperty<FIntProperty>(Class, *FString::Printf(TEXT("RegionVisible%d"), I));
        auto* Type = FindFProperty<FIntProperty>(Class, *FString::Printf(TEXT("RegionType%d"), I));
        auto* Expires = FindFProperty<FFloatProperty>(Class, *FString::Printf(TEXT("RegionExpires%d"), I));
        auto* RegionScale = FindFProperty<FFloatProperty>(Class, *FString::Printf(TEXT("RegionScale%d"), I));
        auto* Pulse = FindFProperty<FObjectPropertyBase>(Class, *FString::Printf(TEXT("AutoPulse%d"), I));
        auto* Hud = FindFProperty<FObjectPropertyBase>(Class, *FString::Printf(TEXT("AutoHud%d"), I));
        EWI_REQUIRE(Point && Point->Struct == TBaseStructure<FVector>::Get() && Serial && Visible && Type && Expires && RegionScale && Pulse && Hud);
        EWI_REQUIRE(Point->HasAnyPropertyFlags(CPF_Net) && Serial->HasAnyPropertyFlags(CPF_Net)
            && Visible->HasAnyPropertyFlags(CPF_Net) && Type->HasAnyPropertyFlags(CPF_Net)
            && Expires->HasAnyPropertyFlags(CPF_Net) && RegionScale->HasAnyPropertyFlags(CPF_Net));
        EWI_REQUIRE(!Pulse->HasAnyPropertyFlags(CPF_Net) && !Hud->HasAnyPropertyFlags(CPF_Net));
        Pulses[I] = Test.World->SpawnActor<AStaticMeshActor>(PulseClass);
        Huds[I] = NewObject<UUserWidget>(GetTransientPackage(), WidgetClass);
        EWI_REQUIRE(Huds[I]->Initialize());
        EWI_REQUIRE(Pulses[I] && Huds[I] && Initialize(Pulses[I], Material, Scale, Alpha));
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
        EWI_REQUIRE(Pulses[I]->GetActorLocation().Equals(FVector(I * 1000, I * 100, 200)));
        EWI_REQUIRE(!Pulses[I]->IsHidden() && Pulses[I]->IsActorTickEnabled() && Huds[I]->GetVisibility() == ESlateVisibility::HitTestInvisible);
        StartTimes[I] = Started->GetPropertyValue_InContainer(Pulses[I]);
    }
    for (int32 J = 0; J < 100; ++J) {
        for (int32 I = 0; I < 8; ++I) FindFProperty<FIntProperty>(Class, *FString::Printf(TEXT("RegionSerial%d"), I))->SetPropertyValue_InContainer(Controller, J+2);
        Controller->ProcessEvent(Service, nullptr);
    }
    for (int32 I = 0; I < 8; ++I) {
        EWI_REQUIRE(Mid->GetObjectPropertyValue_InContainer(Pulses[I]) == Mids[I]);
        EWI_REQUIRE(Started->GetPropertyValue_InContainer(Pulses[I]) == StartTimes[I]);
    }
    // Independent timer expiry must hide the pool even if no further spawn events arrive.
    for (int32 J = 0; J < 90; ++J) { ++GFrameCounter; Test.World->Tick(LEVELTICK_All, 0.1f); }
    for (int32 I = 0; I < 8; ++I) EWI_REQUIRE(Pulses[I]->IsHidden() && !Pulses[I]->IsActorTickEnabled() && Huds[I]->GetVisibility() == ESlateVisibility::Collapsed);
    for (int32 I = 0; I < 8; ++I) {
        FindFProperty<FIntProperty>(Class, *FString::Printf(TEXT("RegionSerial%d"), I))->SetPropertyValue_InContainer(Controller, 200);
        FindFProperty<FIntProperty>(Class, *FString::Printf(TEXT("RegionVisible%d"), I))->SetPropertyValue_InContainer(Controller, 1);
        FindFProperty<FFloatProperty>(Class, *FString::Printf(TEXT("RegionExpires%d"), I))->SetPropertyValue_InContainer(Controller, Test.World->GetTimeSeconds()+8.f);
    }
    Controller->ProcessEvent(Service, nullptr);
    EWI_REQUIRE(!Pulses[0]->IsHidden());
    for (int32 I = 0; I < 8; ++I) FindFProperty<FIntProperty>(Class, *FString::Printf(TEXT("RegionVisible%d"), I))->SetPropertyValue_InContainer(Controller, 0);
    for (int32 I = 0; I < 8; ++I) FindFProperty<FIntProperty>(Class, *FString::Printf(TEXT("RegionSerial%d"), I))->SetPropertyValue_InContainer(Controller, 201);
    Controller->ProcessEvent(Service, nullptr);
    for (int32 I = 0; I < 8; ++I) EWI_REQUIRE(Pulses[I]->IsHidden() && Huds[I]->GetVisibility() == ESlateVisibility::Collapsed);
    // Exercise the serialized page's actual button delegate, disk round trip and pool update.
    auto* Settings = FindFProperty<FObjectPropertyBase>(Class, TEXT("Settings"))->GetObjectPropertyValue_InContainer(Controller);
    EWI_REQUIRE(Settings);
    // Class defaults backfill fields absent from older EnemyWaveIndicator_v1 saves; native mirrors control replication and HUD display.
    Controller->ProcessEvent(Class->FindFunctionByName(TEXT("RefreshSettings")), nullptr);
    for (uint32 I=1; I<ewi::WaveTypeCount; ++I) {
        const bool Expected = I!=1 && I!=6 && I!=32 && I!=34 && I!=46;
        EWI_REQUIRE(FindFProperty<FBoolProperty>(Settings->GetClass(),*FString::Printf(TEXT("EnabledType%u"),I))->GetPropertyValue_InContainer(Settings)==Expected);
        EWI_REQUIRE(FindFProperty<FIntProperty>(Class,*FString::Printf(TEXT("NativeEnabled%u"),I))->GetPropertyValue_InContainer(Controller)==int32(Expected));
    }
    for (const int32 Type : {1, 6, 32, 34, 46}) {
        FindFProperty<FIntProperty>(Class,TEXT("RegionType0"))->SetPropertyValue_InContainer(Controller,Type);
        FindFProperty<FIntProperty>(Class,TEXT("RegionSerial0"))->SetPropertyValue_InContainer(Controller,300+Type);
        FindFProperty<FIntProperty>(Class,TEXT("RegionVisible0"))->SetPropertyValue_InContainer(Controller,1);
        FindFProperty<FFloatProperty>(Class,TEXT("RegionExpires0"))->SetPropertyValue_InContainer(Controller,Test.World->GetTimeSeconds()+8.f);
        Controller->ProcessEvent(Service,nullptr);
        EWI_REQUIRE(Huds[0]->GetVisibility()==ESlateVisibility::Collapsed && Pulses[0]->IsHidden());
    }
    for (const int32 Type : {39, 42}) {
        FindFProperty<FIntProperty>(Class,TEXT("RegionType0"))->SetPropertyValue_InContainer(Controller,Type);
        FindFProperty<FIntProperty>(Class,TEXT("RegionSerial0"))->SetPropertyValue_InContainer(Controller,400+Type);
        Controller->ProcessEvent(Service,nullptr);
        EWI_REQUIRE(Huds[0]->GetVisibility()==ESlateVisibility::HitTestInvisible && !Pulses[0]->IsHidden());
    }
    FindFProperty<FIntProperty>(Class,TEXT("RegionType0"))->SetPropertyValue_InContainer(Controller,0);
    FindFProperty<FIntProperty>(Class,TEXT("RegionVisible0"))->SetPropertyValue_InContainer(Controller,0);
    FindFProperty<FIntProperty>(Class,TEXT("RegionSerial0"))->SetPropertyValue_InContainer(Controller,500);
    Controller->ProcessEvent(Service,nullptr);
    FFloatProperty* SphereChannels[] = {
        Settings ? FindFProperty<FFloatProperty>(Settings->GetClass(), TEXT("Red")) : nullptr,
        Settings ? FindFProperty<FFloatProperty>(Settings->GetClass(), TEXT("Green")) : nullptr,
        Settings ? FindFProperty<FFloatProperty>(Settings->GetClass(), TEXT("Blue")) : nullptr
    };
    auto* SettingsRevision = Settings ? FindFProperty<FIntProperty>(Settings->GetClass(), TEXT("Revision")) : nullptr;
    EWI_REQUIRE(SphereChannels[0] && SphereChannels[1] && SphereChannels[2] && SettingsRevision);
    EWI_REQUIRE(SphereChannels[0]->GetPropertyValue_InContainer(Settings) == 1.f);
    // Persisted settings may contain HDR-era values; runtime must normalize them before opening the page.
    for (auto* Channel : SphereChannels) Channel->SetPropertyValue_InContainer(Settings, 1.5f);
    SettingsRevision->SetPropertyValue_InContainer(Settings, SettingsRevision->GetPropertyValue_InContainer(Settings) + 1);
    Controller->ProcessEvent(Class->FindFunctionByName(TEXT("RefreshSettings")), nullptr);
    FLinearColor ClampedSphereTint;
    EWI_REQUIRE(Cast<UMaterialInstanceDynamic>(Mids[0])->GetVectorParameterValue(FMaterialParameterInfo(TEXT("Tint")), ClampedSphereTint));
    EWI_REQUIRE(ClampedSphereTint.Equals(FLinearColor::White));
    auto* PageClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/EnemyWaveIndicator/WBP_EwiSettings.WBP_EwiSettings_C"));
    FScopedLanguage Language;
    EWI_REQUIRE(Language.Set(TEXT("en")));
    auto* Page = NewObject<UUserWidget>(GetTransientPackage(), PageClass);
    EWI_REQUIRE(Settings && Page && Page->Initialize());
    FindFProperty<FObjectPropertyBase>(PageClass, TEXT("Settings"))->SetObjectPropertyValue_InContainer(Page, Settings);
    const FString Slot = TEXT("EwiValidationSettings_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    FindFProperty<FStrProperty>(PageClass, TEXT("SaveSlot"))->SetPropertyValue_InContainer(Page, Slot);
    Page->ProcessEvent(PageClass->FindFunctionByName(TEXT("Construct")), nullptr);
    auto* LabelInput = Cast<UEditableTextBox>(Page->WidgetTree->FindWidget(TEXT("InputLabel")));
    auto* RadiusInput = Cast<USpinBox>(Page->WidgetTree->FindWidget(TEXT("InputRadius")));
    auto* TimeInput = Cast<USpinBox>(Page->WidgetTree->FindWidget(TEXT("InputDuration")));
    auto* Button = Cast<UButton>(Page->WidgetTree->FindWidget(TEXT("ApplyButton")));
    EWI_REQUIRE(LabelInput && RadiusInput && TimeInput && Button);
    EWI_REQUIRE(Cast<UTextBlock>(Page->WidgetTree->FindWidget(TEXT("Title")))->GetText().ToString() == TEXT("Enemy Wave Indicator  |  1.0.0"));
    EWI_REQUIRE(Cast<UTextBlock>(Page->WidgetTree->FindWidget(TEXT("WaveName2")))->GetText().ToString() == TEXT("Egg hunt ambush"));
    auto* SettingsPanel = Cast<UVerticalBox>(Page->WidgetTree->FindWidget(TEXT("SettingsPanel")));
    auto* WaveGrid = Cast<UGridPanel>(Page->WidgetTree->FindWidget(TEXT("WaveGrid")));
    auto* TextGrid = Cast<UGridPanel>(Page->WidgetTree->FindWidget(TEXT("TextGrid")));
    auto* SphereGrid = Cast<UGridPanel>(Page->WidgetTree->FindWidget(TEXT("SphereGrid")));
    EWI_REQUIRE(SettingsPanel && WaveGrid && TextGrid && SphereGrid);
    auto* LastWaveName = Page->WidgetTree->FindWidget(TEXT("WaveName46"));
    auto* LastWaveSlot = LastWaveName ? Cast<UGridSlot>(LastWaveName->Slot) : nullptr;
    EWI_REQUIRE(LastWaveSlot && LastWaveSlot->Column == 4);
    auto* PageTitle = Cast<UTextBlock>(Page->WidgetTree->FindWidget(TEXT("Title")));
    auto* SectionTitle = Cast<UTextBlock>(Page->WidgetTree->FindWidget(TEXT("WaveSection")));
    auto* WaveBody = Cast<UTextBlock>(Page->WidgetTree->FindWidget(TEXT("WaveName2")));
    auto* ControlBody = Cast<UTextBlock>(Page->WidgetTree->FindWidget(TEXT("Caption9")));
    auto* PreviewText = Cast<UTextBlock>(Page->WidgetTree->FindWidget(TEXT("TextPreview")));
    auto* ApplyText = Cast<UTextBlock>(Page->WidgetTree->FindWidget(TEXT("ApplyText")));
    EWI_REQUIRE(PageTitle && SectionTitle && WaveBody && ControlBody && PreviewText && ApplyText);
    EWI_REQUIRE(PageTitle->Font.Size == 21 && SectionTitle->Font.Size == 19);
    EWI_REQUIRE(WaveBody->Font.Size == 13 && ControlBody->Font.Size == 13);
    EWI_REQUIRE(PreviewText->Font.Size == 18 && ApplyText->Font.Size == 16);
    EWI_REQUIRE(PageTitle->Font.Size > SectionTitle->Font.Size && SectionTitle->Font.Size > WaveBody->Font.Size);
    EWI_REQUIRE(SettingsPanel->GetChildAt(2) == WaveGrid && SettingsPanel->GetChildAt(4) == TextGrid && SettingsPanel->GetChildAt(8) == SphereGrid);
    EWI_REQUIRE(WaveGrid->GetChildrenCount() == ewi::WaveTypeCount * 3 && TextGrid->GetChildrenCount() == 16 && SphereGrid->GetChildrenCount() == 12);
    EWI_REQUIRE(TextOutput(Page, TEXT("GetPageInfo"), TEXT("PageName")) == TEXT("Indicator settings"));
    EWI_REQUIRE(TextOutput(Controller, TEXT("GetModInfo"), TEXT("ModName")) == TEXT("Enemy Wave Indicator"));
    EWI_REQUIRE(Language.Set(TEXT("zh-CN")));
    auto* ChinesePage = NewObject<UUserWidget>(GetTransientPackage(), PageClass);
    EWI_REQUIRE(ChinesePage && ChinesePage->Initialize());
    FindFProperty<FObjectPropertyBase>(PageClass, TEXT("Settings"))->SetObjectPropertyValue_InContainer(ChinesePage, Settings);
    ChinesePage->ProcessEvent(PageClass->FindFunctionByName(TEXT("Construct")), nullptr);
    EWI_REQUIRE(Cast<UTextBlock>(ChinesePage->WidgetTree->FindWidget(TEXT("Title")))->GetText().ToString() == TEXT("敌潮指示器  |  1.0.0"));
    EWI_REQUIRE(Cast<UTextBlock>(ChinesePage->WidgetTree->FindWidget(TEXT("WaveSection")))->GetText().ToString() == TEXT("虫潮播报"));
    EWI_REQUIRE(Cast<UTextBlock>(ChinesePage->WidgetTree->FindWidget(TEXT("WaveName0")))->GetText().ToString() == TEXT("自然潮"));
    EWI_REQUIRE(Cast<UTextBlock>(ChinesePage->WidgetTree->FindWidget(TEXT("WaveName2")))->GetText().ToString() == TEXT("虫蛋伏击"));
    EWI_REQUIRE(Cast<UTextBlock>(ChinesePage->WidgetTree->FindWidget(TEXT("WaveName25")))->GetText().ToString() == TEXT("搜救行动：矿骡伏击"));
    EWI_REQUIRE(Cast<UTextBlock>(ChinesePage->WidgetTree->FindWidget(TEXT("WaveName26")))->GetText().ToString() == TEXT("搜救行动：据点防守"));
    EWI_REQUIRE(Cast<UTextBlock>(ChinesePage->WidgetTree->FindWidget(TEXT("WaveName27")))->GetText().ToString() == TEXT("搜救行动：撤离"));
    EWI_REQUIRE(Cast<UTextBlock>(ChinesePage->WidgetTree->FindWidget(TEXT("WaveName29")))->GetText().ToString() == TEXT("无畏异虫潮"));
    EWI_REQUIRE(Cast<UTextBlock>(ChinesePage->WidgetTree->FindWidget(TEXT("WaveName36")))->GetText().ToString() == TEXT("三提石矿藏"));
    EWI_REQUIRE(Cast<UTextBlock>(ChinesePage->WidgetTree->FindWidget(TEXT("WaveName37")))->GetText().ToString() == TEXT("矿化爆发（事件）"));
    EWI_REQUIRE(Cast<UTextBlock>(ChinesePage->WidgetTree->FindWidget(TEXT("WaveName38")))->GetText().ToString() == TEXT("氪石感染"));
    EWI_REQUIRE(Cast<UTextBlock>(ChinesePage->WidgetTree->FindWidget(TEXT("WaveName39")))->GetText().ToString() == TEXT("蜂拥浩劫"));
    EWI_REQUIRE(Cast<UTextBlock>(ChinesePage->WidgetTree->FindWidget(TEXT("WaveName42")))->GetText().ToString() == TEXT("凝血化糖"));
    EWI_REQUIRE(Cast<UTextBlock>(ChinesePage->WidgetTree->FindWidget(TEXT("WaveName44")))->GetText().ToString() == TEXT("矿化爆发（警告）"));
    EWI_REQUIRE(Cast<UTextBlock>(ChinesePage->WidgetTree->FindWidget(TEXT("WaveName46")))->GetText().ToString() == TEXT("幽魂不散"));
    EWI_REQUIRE(Cast<UTextBlock>(ChinesePage->WidgetTree->FindWidget(TEXT("TextSection")))->GetText().ToString() == TEXT("播报警示文本"));
    EWI_REQUIRE(Cast<UTextBlock>(ChinesePage->WidgetTree->FindWidget(TEXT("SphereSection")))->GetText().ToString() == TEXT("警示球体"));
    EWI_REQUIRE(Cast<UTextBlock>(ChinesePage->WidgetTree->FindWidget(TEXT("PreviewCaption")))->GetText().ToString() == TEXT("实时文字预览"));
    EWI_REQUIRE(Cast<UTextBlock>(ChinesePage->WidgetTree->FindWidget(TEXT("SaveStatus")))->GetText().IsEmpty());
    // Discovery-facing metadata stays stable; only the actual page contents are localized.
    EWI_REQUIRE(TextOutput(ChinesePage, TEXT("GetPageInfo"), TEXT("PageName")) == TEXT("Indicator settings"));
    EWI_REQUIRE(TextOutput(Controller, TEXT("GetModInfo"), TEXT("ModName")) == TEXT("Enemy Wave Indicator"));
    EWI_REQUIRE(Cast<UEditableTextBox>(ChinesePage->WidgetTree->FindWidget(TEXT("InputLabel")))->GetText().ToString() == TEXT("NORMAL WAVE"));
    EWI_REQUIRE(Cast<UEditableTextBox>(ChinesePage->WidgetTree->FindWidget(TEXT("InputLabelType2")))->GetText().ToString() == TEXT("Egg hunt ambush"));
    EWI_REQUIRE(FindFProperty<FStrProperty>(Settings->GetClass(), TEXT("Label"))->GetPropertyValue_InContainer(Settings) == TEXT("NORMAL WAVE"));
    auto* PreviewIcon = Cast<UImage>(Page->WidgetTree->FindWidget(TEXT("PreviewIcon")));
    auto* PreviewRow = Cast<UHorizontalBox>(Page->WidgetTree->FindWidget(TEXT("PreviewRow")));
    EWI_REQUIRE(PreviewIcon && PreviewRow && PreviewRow->GetChildAt(0) == PreviewIcon);
    EWI_REQUIRE(PreviewIcon->GetVisibility() == ESlateVisibility::Collapsed);
    EWI_REQUIRE(PreviewIcon->ColorAndOpacity.Equals(FLinearColor(1.f, 0.1f, 0.02f)));
    EWI_REQUIRE(PreviewIcon->Brush.ImageSize.Equals(FVector2D(22.f, 22.f)));
    EWI_REQUIRE(PreviewText->GetText().ToString() == TEXT("NORMAL WAVE"));
    const FString ChineseSlot = Slot + TEXT("_zh-CN");
    FindFProperty<FStrProperty>(PageClass, TEXT("SaveSlot"))->SetPropertyValue_InContainer(ChinesePage, ChineseSlot);
    Cast<UButton>(ChinesePage->WidgetTree->FindWidget(TEXT("ApplyButton")))->OnClicked.Broadcast();
    EWI_REQUIRE(Cast<UTextBlock>(ChinesePage->WidgetTree->FindWidget(TEXT("SaveStatus")))->GetText().ToString() == TEXT("已应用并保存。"));
    EWI_REQUIRE(UGameplayStatics::DeleteGameInSlot(ChineseSlot, 0));
    EWI_REQUIRE(Language.Set(TEXT("en")));
    EWI_REQUIRE(Cast<UCheckBox>(Page->WidgetTree->FindWidget(TEXT("InputNormalEnabled")))->IsChecked());
    for (uint32 I=1; I<ewi::WaveTypeCount; ++I) {
        auto* Toggle=Cast<UCheckBox>(Page->WidgetTree->FindWidget(*FString::Printf(TEXT("InputEnabledType%u"),I)));
        auto* Text=Cast<UEditableTextBox>(Page->WidgetTree->FindWidget(*FString::Printf(TEXT("InputLabelType%u"),I)));
        EWI_REQUIRE(Toggle && Text && Toggle->IsChecked()==(I!=1 && I!=6 && I!=32 && I!=34 && I!=46));
        Toggle->SetIsChecked((I%2)==1);Text->SetText(FText::FromString(FString::Printf(TEXT("TYPE %u"),I)));
    }
    EWI_REQUIRE(Cast<USpinBox>(Page->WidgetTree->FindWidget(TEXT("InputTextAR")))->GetValue()==1.f);
    EWI_REQUIRE(Cast<USpinBox>(Page->WidgetTree->FindWidget(TEXT("InputTextAG")))->GetValue()==1.f);
    EWI_REQUIRE(Cast<USpinBox>(Page->WidgetTree->FindWidget(TEXT("InputTextAB")))->GetValue()==0.f);
    EWI_REQUIRE(Cast<USpinBox>(Page->WidgetTree->FindWidget(TEXT("InputTextBR")))->GetValue()==1.f);
    EWI_REQUIRE(Cast<USpinBox>(Page->WidgetTree->FindWidget(TEXT("InputTextBG")))->GetValue()==0.f);
    EWI_REQUIRE(Cast<USpinBox>(Page->WidgetTree->FindWidget(TEXT("InputTextBB")))->GetValue()==0.f);
    for (const TCHAR* Name : {TEXT("InputRed"), TEXT("InputGreen"), TEXT("InputBlue")}) {
        auto* Channel = Cast<USpinBox>(Page->WidgetTree->FindWidget(Name));
        EWI_REQUIRE(Channel && Channel->GetMaxValue() == 1.f && Channel->GetMaxSliderValue() == 1.f);
        EWI_REQUIRE(Channel->GetValue() == 1.f); // Construct clamps legacy saved values before displaying them.
        Channel->SetValue(1.1f);
    }
    LabelInput->SetText(FText::FromString(TEXT("WATCH OUT"))); RadiusInput->SetValue(6.f); TimeInput->SetValue(12.f);
    auto* OpacityInput=Cast<USpinBox>(Page->WidgetTree->FindWidget(TEXT("InputOpacity")));auto* HzInput=Cast<USpinBox>(Page->WidgetTree->FindWidget(TEXT("InputBlinkHz")));EWI_REQUIRE(OpacityInput && HzInput);
    OpacityInput->SetValue(.65f);HzInput->SetValue(2.f);
    auto* PageTick=PageClass->FindFunctionByName(TEXT("Tick"));EWI_REQUIRE(PageTick);
    FStructOnScope TickParams(PageTick);Page->ProcessEvent(PageTick,TickParams.GetStructMemory());
    auto* Swatch=Cast<UImage>(Page->WidgetTree->FindWidget(TEXT("SpherePreview")));EWI_REQUIRE(Swatch && Swatch->ColorAndOpacity.A==.65f);
    EWI_REQUIRE(FindFProperty<FFloatProperty>(Settings->GetClass(),TEXT("Opacity"))->GetPropertyValue_InContainer(Settings)==.4f); // Preview has no save side effects.
    Button->OnClicked.Broadcast();
    EWI_REQUIRE(FindFProperty<FStrProperty>(Settings->GetClass(), TEXT("Label"))->GetPropertyValue_InContainer(Settings) == TEXT("WATCH OUT"));
    for (auto* Channel : SphereChannels) EWI_REQUIRE(Channel->GetPropertyValue_InContainer(Settings) == 1.f);
    EWI_REQUIRE(Cast<UTextBlock>(Page->WidgetTree->FindWidget(TEXT("SaveStatus")))->GetText().ToString() == TEXT("Applied and saved."));
    auto* Reloaded = UGameplayStatics::LoadGameFromSlot(Slot, 0);
    EWI_REQUIRE(Reloaded && UGameplayStatics::DeleteGameInSlot(Slot, 0));
    EWI_REQUIRE(FindFProperty<FFloatProperty>(Reloaded->GetClass(), TEXT("Duration"))->GetPropertyValue_InContainer(Reloaded) == 12.f);
    EWI_REQUIRE(FindFProperty<FFloatProperty>(Reloaded->GetClass(),TEXT("Opacity"))->GetPropertyValue_InContainer(Reloaded)==.65f);
    for (uint32 I=1; I<ewi::WaveTypeCount; ++I) {
        EWI_REQUIRE(FindFProperty<FBoolProperty>(Reloaded->GetClass(),*FString::Printf(TEXT("EnabledType%u"),I))->GetPropertyValue_InContainer(Reloaded)==((I%2)==1));
        EWI_REQUIRE(FindFProperty<FStrProperty>(Reloaded->GetClass(),*FString::Printf(TEXT("LabelType%u"),I))->GetPropertyValue_InContainer(Reloaded)==FString::Printf(TEXT("TYPE %u"),I));
    }
    Controller->ProcessEvent(Class->FindFunctionByName(TEXT("RefreshSettings")), nullptr);
    float SavedOpacity=0; EWI_REQUIRE(Cast<UMaterialInstanceDynamic>(Mids[0])->GetScalarParameterValue(FMaterialParameterInfo(TEXT("Opacity")),SavedOpacity) && SavedOpacity==.65f);
    EWI_REQUIRE(FindFProperty<FFloatProperty>(Class, TEXT("DurationSec"))->GetPropertyValue_InContainer(Controller) == 12.f);
    for (int32 I = 0; I < 8; ++I) EWI_REQUIRE(FindFProperty<FFloatProperty>(PulseClass, TEXT("RadiusScale"))->GetPropertyValue_InContainer(Pulses[I]) == 6.f && Mid->GetObjectPropertyValue_InContainer(Pulses[I]) == Mids[I]);
    auto* Pc = Test.World->SpawnActor<APlayerController>(); auto* Pawn = Test.World->SpawnActor<ADefaultPawn>();
    EWI_REQUIRE(Pc && Pawn); auto* Instance = NewObject<UGameInstance>(GEngine); Test.World->SetGameInstance(Instance);
    auto* Local = NewObject<ULocalPlayer>(GEngine); Local->SetControllerId(0); Pc->Player = Local; Local->PlayerController = Pc;
    Pc->Possess(Pawn); Huds[0]->SetOwningPlayer(Pc);
    EWI_REQUIRE(Pawn->SetActorLocation(FVector(0, 0, 0), false, nullptr, ETeleportType::TeleportPhysics));
    *FindFProperty<FStructProperty>(WidgetClass, TEXT("WorldLocation"))->ContainerPtrToValuePtr<FVector>(Huds[0]) = FVector(300, 400, 0);
    Huds[0]->ProcessEvent(WidgetClass->FindFunctionByName(TEXT("UpdateWarning")), nullptr);
    auto* MarkerText = Cast<UTextBlock>(Huds[0]->WidgetTree->FindWidget(TEXT("MarkerText")));
    auto* WarningIcon = Cast<UImage>(Huds[0]->WidgetTree->FindWidget(TEXT("WarningIcon")));
    EWI_REQUIRE(MarkerText && MarkerText->GetText().ToString() == TEXT("WATCH OUT  |  5 m"));
    EWI_REQUIRE(MarkerText->GetRenderOpacity()==1.f);
    EWI_REQUIRE(MarkerText->ColorAndOpacity.GetSpecifiedColor()==FLinearColor::Red || MarkerText->ColorAndOpacity.GetSpecifiedColor()==FLinearColor::Yellow);
    EWI_REQUIRE(WarningIcon && WarningIcon->GetVisibility()==ESlateVisibility::Collapsed);
    EWI_REQUIRE(WarningIcon->ColorAndOpacity.Equals(FLinearColor(1.f, 0.1f, 0.02f)));
    EWI_REQUIRE(FindFProperty<FFloatProperty>(WidgetClass,TEXT("BlinkHz"))->GetPropertyValue_InContainer(Huds[0])==2.f);
    FindFProperty<FBoolProperty>(WidgetClass, TEXT("BlinkEnabled"))->SetPropertyValue_InContainer(Huds[0], false);
    Huds[0]->ProcessEvent(WidgetClass->FindFunctionByName(TEXT("UpdateWarning")), nullptr);
    EWI_REQUIRE(MarkerText->GetRenderOpacity() == 1.f && MarkerText->ColorAndOpacity.GetSpecifiedColor()==FLinearColor::Yellow);
    EWI_REQUIRE(WarningIcon->ColorAndOpacity.Equals(FLinearColor(1.f, 0.1f, 0.02f)));
    auto* PagesFunction = Class->FindFunctionByName(TEXT("GetModPages")); EWI_REQUIRE(PagesFunction);
    FStructOnScope PageParams(PagesFunction); Controller->ProcessEvent(PagesFunction, PageParams.GetStructMemory());
    auto* PagesProperty = FindFProperty<FArrayProperty>(PagesFunction, TEXT("HubPages")); EWI_REQUIRE(PagesProperty);
    FScriptArrayHelper Pages(PagesProperty, PagesProperty->ContainerPtrToValuePtr<void>(PageParams.GetStructMemory()));
    EWI_REQUIRE(Pages.Num() == 1 && CastField<FInterfaceProperty>(PagesProperty->Inner));
    auto* HubPage = Cast<UUserWidget>(reinterpret_cast<FScriptInterface*>(Pages.GetRawPtr(0))->GetObject());
    EWI_REQUIRE(HubPage && HubPage->GetClass() == PageClass);
    EWI_REQUIRE(FindFProperty<FObjectPropertyBase>(PageClass, TEXT("Settings"))->GetObjectPropertyValue_InContainer(HubPage) == Settings);
    Controller->ProcessEvent(PagesFunction, PageParams.GetStructMemory());
    EWI_REQUIRE(reinterpret_cast<FScriptInterface*>(Pages.GetRawPtr(0))->GetObject() == HubPage);
    // Exercise every replicated source ID through actual saved Blueprint display branches.
    for (uint32 I=1; I<ewi::WaveTypeCount; ++I) {
        FindFProperty<FIntProperty>(Class,TEXT("RegionType0"))->SetPropertyValue_InContainer(Controller,I);
        FindFProperty<FIntProperty>(Class,TEXT("RegionSerial0"))->SetPropertyValue_InContainer(Controller,1000+I);
        FindFProperty<FIntProperty>(Class,TEXT("RegionVisible0"))->SetPropertyValue_InContainer(Controller,1);
        FindFProperty<FFloatProperty>(Class,TEXT("RegionExpires0"))->SetPropertyValue_InContainer(Controller,Test.World->GetTimeSeconds()+8.f);
        Controller->ProcessEvent(Service,nullptr);
        EWI_REQUIRE((Huds[0]->GetVisibility()==ESlateVisibility::HitTestInvisible)==((I%2)==1));
        if(I%2) EWI_REQUIRE(FindFProperty<FTextProperty>(WidgetClass,TEXT("BaseLabel"))->GetPropertyValue_InContainer(Huds[0]).ToString()==FString::Printf(TEXT("TYPE %u"),I));
        EWI_REQUIRE(FindFProperty<FIntProperty>(Class,*FString::Printf(TEXT("NativeEnabled%u"),I))->GetPropertyValue_InContainer(Controller)==int32(I%2));
    }
    UE_LOG(LogTemp,Display,TEXT("EWI_TEST Mod Hub interface discovery, stable metadata, explicit 21/19/13 heading-body typography, compact English/zh-CN page contents and unchanged English marker defaults passed"));
    UE_LOG(LogTemp,Display,TEXT("EWI_TEST 47 wave types in 24 rows: all default on except IDs 1/6/32/34/46, bilingual text, persistence and replicated source selection passed"));
    // The catalog loop ends on disabled ID 46; restore an enabled source before testing viewport recovery.
    FindFProperty<FIntProperty>(Class,TEXT("RegionType0"))->SetPropertyValue_InContainer(Controller,37);
    FindFProperty<FIntProperty>(Class,TEXT("RegionSerial0"))->SetPropertyValue_InContainer(Controller,2000);
    FindFProperty<FIntProperty>(Class,TEXT("RegionVisible0"))->SetPropertyValue_InContainer(Controller,1);
    FindFProperty<FFloatProperty>(Class,TEXT("RegionExpires0"))->SetPropertyValue_InContainer(Controller,Test.World->GetTimeSeconds()+8.f);
    Controller->ProcessEvent(Service,nullptr);
    EWI_REQUIRE(Huds[0]->GetVisibility()==ESlateVisibility::HitTestInvisible && !Pulses[0]->IsHidden());
    // Real Slate attachment matters: visibility alone cannot recover a viewport cleared after Init.
    auto* Viewport = NewObject<UGameViewportClient>(GEngine);
    auto Overlay = SNew(SOverlay);
    Viewport->SetViewportOverlayWidget(TSharedPtr<SWindow>(), Overlay);
    GEngine->GetWorldContextFromWorldChecked(Test.World).GameViewport = Viewport;
    Huds[0]->AddToViewport();
    UE_LOG(LogTemp,Display,TEXT("EWI_TEST real viewport attached=%d slots=%d before simulated HUD cleanup"),Huds[0]->IsInViewport(),Overlay->GetChildren()->Num());
    EWI_REQUIRE(Huds[0]->IsInViewport() && Overlay->GetChildren()->Num() == 1);
    Huds[0]->RemoveFromParent();
    EWI_REQUIRE(!Huds[0]->IsInViewport() && !Pulses[0]->IsHidden());
    const auto SerialBefore = FindFProperty<FIntProperty>(Class,TEXT("AppliedSerial0"))->GetPropertyValue_InContainer(Controller);
    const auto PulseStartBefore = Started->GetPropertyValue_InContainer(Pulses[0]);
    Controller->ProcessEvent(Service, nullptr); // No new spawn/serial: recovery must still run.
    EWI_REQUIRE(Huds[0]->IsInViewport() && Overlay->GetChildren()->Num() == 1);
    for (int32 I=0; I<10; ++I) Controller->ProcessEvent(Service, nullptr);
    EWI_REQUIRE(Overlay->GetChildren()->Num() == 1);
    EWI_REQUIRE(FindFProperty<FIntProperty>(Class,TEXT("AppliedSerial0"))->GetPropertyValue_InContainer(Controller) == SerialBefore);
    EWI_REQUIRE(Started->GetPropertyValue_InContainer(Pulses[0]) == PulseStartBefore);
    Huds[0]->RemoveFromParent();
    UE_LOG(LogTemp,Display,TEXT("EWI_TEST removed viewport HUD reattached without a new serial, duplicate Slate slot or pulse restart"));
    EWI_REQUIRE(Pawn->Destroy() && Pc->Destroy());
    UE_LOG(LogTemp, Display, TEXT("EWI_TEST settings button/save/reload, pooled radius update, actual pawn 3-4-5 distance text and blink disable passed; Mod Hub H discovery still requires game testing"));
    EWI_REQUIRE(Controller->Destroy());
    for (auto* Pulse : Pulses) EWI_REQUIRE(Pulse->IsActorBeingDestroyed());
    UE_LOG(LogTemp, Display, TEXT("EWI_TEST automatic pool: Blueprint region events, 8 placements, 100 reuses without MID allocation/phase reset, timer expiry, explicit hide and EndPlay cleanup; 91 World ticks"));
    return true;
}
}

#include "EwiCaptureValidation.inl"

// Blank the runtime game-texture path so editor tests do not async-load missing FSD art.
struct FClearWarningIconPath
{
    UObject* Defaults = nullptr;
    FStrProperty* Property = nullptr;
    FString Path;
    explicit FClearWarningIconPath(UClass* Class)
    {
        Property = Class ? FindFProperty<FStrProperty>(Class, TEXT("WarningIconPath")) : nullptr;
        if (!Property) return;
        Defaults = Class->GetDefaultObject();
        Path = Property->GetPropertyValue_InContainer(Defaults);
        Property->SetPropertyValue_InContainer(Defaults, TEXT(""));
    }
    ~FClearWarningIconPath()
    {
        if (Property) Property->SetPropertyValue_InContainer(Defaults, Path);
    }
};

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
        S->SetPropertyValue_InContainer(Defaults, TEXT("/Game/EwiValidation/CF_TestScale.CF_TestScale"));
        A->SetPropertyValue_InContainer(Defaults, TEXT("/Game/EwiValidation/CF_TestAlpha.CF_TestAlpha"));
    }
    ~FTestAutomaticCurves()
    {
        FindFProperty<FStrProperty>(Defaults->GetClass(), TEXT("ScalePath"))->SetPropertyValue_InContainer(Defaults, Scale);
        FindFProperty<FStrProperty>(Defaults->GetClass(), TEXT("AlphaPath"))->SetPropertyValue_InContainer(Defaults, Alpha);
    }
};

bool ValidateEwiPresentation()
{
    auto* PulseClass = LoadClass<AStaticMeshActor>(nullptr, TEXT("/Game/EnemyWaveIndicator/BP_EwiPulse.BP_EwiPulse_C"));
    auto* WidgetClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/EnemyWaveIndicator/WBP_EwiMarker.WBP_EwiMarker_C"));
    auto* ResourceClass = LoadClass<AActor>(nullptr, TEXT("/Game/EnemyWaveIndicator/BP_EwiResources.BP_EwiResources_C"));
    auto* VisualClass = LoadClass<AActor>(nullptr, TEXT("/Game/EnemyWaveIndicator/BP_EwiVisualTest.BP_EwiVisualTest_C"));
    EWI_REQUIRE(PulseClass && WidgetClass && ResourceClass && VisualClass);
    auto* AutomaticClass = LoadClass<AActor>(nullptr, TEXT("/Game/EnemyWaveIndicator/BP_EwiAuto.BP_EwiAuto_C"));
    EWI_REQUIRE(AutomaticClass);
    auto* PageClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/EnemyWaveIndicator/WBP_EwiSettings.WBP_EwiSettings_C"));
    EWI_REQUIRE(PageClass);
    const TCHAR* WarningIconPath = TEXT("/Game/UI/Art/MainOnScreenHUD/Drilldozer/Icon_Warning_Drilldozer_V1.Icon_Warning_Drilldozer_V1");
    auto* HudIconPath = FindFProperty<FStrProperty>(WidgetClass, TEXT("WarningIconPath"));
    auto* PageIconPath = FindFProperty<FStrProperty>(PageClass, TEXT("WarningIconPath"));
    EWI_REQUIRE(HudIconPath && HudIconPath->GetPropertyValue_InContainer(WidgetClass->GetDefaultObject()) == WarningIconPath);
    EWI_REQUIRE(PageIconPath && PageIconPath->GetPropertyValue_InContainer(PageClass->GetDefaultObject()) == WarningIconPath);
    FClearWarningIconPath ClearHudIcon(WidgetClass);
    FClearWarningIconPath ClearPageIcon(PageClass);
    FTestAutomaticCurves TestCurves(AutomaticClass);
    EWI_REQUIRE(ValidatePlacement(WidgetClass));
    UE_LOG(LogTemp, Display, TEXT("EWI_TEST saved Blueprint classes loaded"));
    const int32 Before = GEngine->GetWorldContexts().Num();
    EWI_REQUIRE(ValidateListenHostControllerSelection(AutomaticClass));
    EWI_REQUIRE(GEngine->GetWorldContexts().Num() == Before);
    EWI_REQUIRE(ValidateAutomatic(PulseClass, WidgetClass));
    EWI_REQUIRE(ValidateCapture());
    EWI_REQUIRE(GEngine->GetWorldContexts().Num() == Before);
    {
        // No local controller: finite initialization retries and F5 must allocate no visual pair.
        FTestWorld Test;
        EWI_REQUIRE(Test.World);
        auto* Visual = Test.World->SpawnActor<AActor>(VisualClass);
        auto* Run = VisualClass->FindFunctionByName(TEXT("RunVisualTest"));
        auto* Tries = FindFProperty<FIntProperty>(VisualClass, TEXT("SetupTries"));
        auto* Attempted = FindFProperty<FBoolProperty>(VisualClass, TEXT("SetupAttempted"));
        auto* Pulse = FindFProperty<FObjectPropertyBase>(VisualClass, TEXT("Pulse"));
        auto* Hud = FindFProperty<FObjectPropertyBase>(VisualClass, TEXT("Hud"));
        EWI_REQUIRE(Visual && Run && Tries && Attempted && Pulse && Hud);
        EWI_REQUIRE(!Visual->IsActorTickEnabled() && !Visual->GetIsReplicated());
        EWI_REQUIRE(Tries->GetPropertyValue_InContainer(Visual) == 1);
        // Stay below WorldSettings' maximum delta clamp, so timers receive forty game seconds.
        for (int32 I = 0; I < 400; ++I)
        {
            if (I < 80) Visual->ProcessEvent(Run, nullptr);
            ++GFrameCounter;
            Test.World->Tick(LEVELTICK_All, 0.1f);
        }
        UE_LOG(LogTemp, Display, TEXT("EWI_TEST initialization attempts=%d time=%.2f"), Tries->GetPropertyValue_InContainer(Visual), Test.World->GetTimeSeconds());
        EWI_REQUIRE(Tries->GetPropertyValue_InContainer(Visual) == 30);
        EWI_REQUIRE(!Attempted->GetPropertyValue_InContainer(Visual));
        EWI_REQUIRE(!Pulse->GetObjectPropertyValue_InContainer(Visual) && !Hud->GetObjectPropertyValue_InContainer(Visual));
        EWI_REQUIRE(Visual->Destroy());
        UE_LOG(LogTemp, Display, TEXT("EWI_TEST visual test: 30 finite setup attempts, 80 guarded requests without local controller, no visual allocation"));
    }
    EWI_REQUIRE(GEngine->GetWorldContexts().Num() == Before);
    EWI_REQUIRE(ValidateResources(ResourceClass, PulseClass));
    EWI_REQUIRE(GEngine->GetWorldContexts().Num() == Before);
    EWI_REQUIRE(ValidateInWorld(PulseClass));
    EWI_REQUIRE(GEngine->GetWorldContexts().Num() == Before);
    EWI_REQUIRE(ValidateInWorld(PulseClass));
    EWI_REQUIRE(GEngine->GetWorldContexts().Num() == Before);
    UE_LOG(LogTemp, Display, TEXT("EWI_TEST six isolated worlds cleaned up; runtime graph checks passed"));
    return true;
}
