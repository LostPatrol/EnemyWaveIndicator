// Generate one host-local automatic controller with eight prewarmed reusable pulse/widget pairs.
void BuildAutomatic()
{
    auto* Parent = LoadClass<AActor>(nullptr, TEXT("/Game/NormalWaveIndicator/BP_NwiResources.BP_NwiResources_C"));
    auto* PulseClass = LoadClass<AActor>(nullptr, TEXT("/Game/NormalWaveIndicator/BP_NwiPulse.BP_NwiPulse_C"));
    auto* HudClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/NormalWaveIndicator/WBP_NwiMarker.WBP_NwiMarker_C"));
    check(Parent && PulseClass && HudClass);
    auto* BP = Blueprint(TEXT("BP_NwiAuto"), false, Parent);
    AddControllerSettings(BP);
    Variable(BP, TEXT("PoolAttempted"), Type(UEdGraphSchema_K2::PC_Boolean), TEXT("false"));
    Variable(BP, TEXT("PoolReady"), Type(UEdGraphSchema_K2::PC_Boolean), TEXT("false"));
    for (int32 I = 0; I < 8; ++I)
    {
        Variable(BP, *FString::Printf(TEXT("RegionPoint%d"), I), Type(UEdGraphSchema_K2::PC_Struct, TBaseStructure<FVector>::Get()));
        for (auto* Prefix : { TEXT("RegionSerial"), TEXT("RegionVisible"), TEXT("AppliedSerial"), TEXT("AppliedVisible") })
            Variable(BP, *FString::Printf(TEXT("%s%d"), Prefix, I), Type(UEdGraphSchema_K2::PC_Int), TEXT("0"));
        Variable(BP, *FString::Printf(TEXT("AutoPulse%d"), I), Type(UEdGraphSchema_K2::PC_Object, PulseClass));
        Variable(BP, *FString::Printf(TEXT("AutoHud%d"), I), Type(UEdGraphSchema_K2::PC_Object, HudClass));
    }
    DeclareCapture(BP);
    auto* G = Graph(BP);
    auto* Service = Custom(G, TEXT("ServiceRegions"));
    auto* Setup = Custom(G, TEXT("InitializePool"));
    auto* Cleanup = Custom(G, TEXT("CleanupPool"));
    BuildControllerSettings(BP, G, PulseClass, HudClass);
    UK2Node_CustomEvent* Apply[8]{}; UK2Node_CustomEvent* Hide[8]{}; UK2Node_CustomEvent* Next[8]{};
    for (int32 I = 0; I < 8; ++I) {
        Apply[I] = Custom(G, *FString::Printf(TEXT("ApplyRegion%d"), I));
        Hide[I] = Custom(G, *FString::Printf(TEXT("HideRegion%d"), I));
        Next[I] = Custom(G, *FString::Printf(TEXT("CleanupNext%d"), I));
    }
    Compile(BP);
    BuildCapture(BP, G);
    auto* Self = Node<UK2Node_Self>(G); Self->AllocateDefaultPins();
    auto* Player = Call(G, UGameplayStatics::StaticClass(), TEXT("GetPlayerController"));
    auto* Ready = Get(G, TEXT("Ready")); auto* PlayerValid = Valid(G, Player, TEXT("ReturnValue"));
    auto* Host = Call(G, UKismetSystemLibrary::StaticClass(), TEXT("IsServer"));
    auto* Local = Call(G, UKismetMathLibrary::StaticClass(), TEXT("BooleanAND"));
    Link(PlayerValid, TEXT("ReturnValue"), Local, TEXT("A")); Link(Host, TEXT("ReturnValue"), Local, TEXT("B"));
    auto* CanRun = Call(G, UKismetMathLibrary::StaticClass(), TEXT("BooleanAND"));
    Link(Local, TEXT("ReturnValue"), CanRun, TEXT("A")); Link(Ready, TEXT("Ready"), CanRun, TEXT("B"));
    auto* Tick = Event(G, AActor::StaticClass(), TEXT("ReceiveTick"));
    auto* TickGate = Branch(G, CanRun, TEXT("ReturnValue")); auto* Connect = Call(G, BP->GeneratedClass, TEXT("ConnectCapture")); Link(Tick, TEXT("then"), Connect, TEXT("execute")); Link(Connect, TEXT("then"), TickGate, TEXT("execute"));
    auto* CallSetup = Call(G, BP->GeneratedClass, TEXT("InitializePool")); Link(TickGate, TEXT("then"), CallSetup, TEXT("execute"));
    auto* Pool = Get(G, TEXT("PoolReady")); auto* PoolGate = Branch(G, Pool, TEXT("PoolReady")); Link(CallSetup, TEXT("then"), PoolGate, TEXT("execute"));
    auto* Refresh = Call(G, BP->GeneratedClass, TEXT("RefreshSettings")); Link(PoolGate, TEXT("then"), Refresh, TEXT("execute"));
    auto* CallService = Call(G, BP->GeneratedClass, TEXT("ServiceRegions")); Link(Refresh, TEXT("then"), CallService, TEXT("execute"));
    auto* Attempted = Get(G, TEXT("PoolAttempted")); auto* First = Branch(G, Attempted, TEXT("PoolAttempted")); Link(Setup, TEXT("then"), First, TEXT("execute"));
    auto* Commit = Set(G, TEXT("PoolAttempted")); Value(Commit, TEXT("PoolAttempted"), TEXT("true")); Link(First, TEXT("else"), Commit, TEXT("execute"));
    UEdGraphNode* BuildExec = Commit;
    UEdGraphNode* ServiceExec = Service;
    UEdGraphNode* CleanupExec = Cleanup;
    for (int32 I = 0; I < 8; ++I)
    {
        const FString PulseName = FString::Printf(TEXT("AutoPulse%d"), I), HudName = FString::Printf(TEXT("AutoHud%d"), I);
        const FString PointName = FString::Printf(TEXT("RegionPoint%d"), I), SerialName = FString::Printf(TEXT("RegionSerial%d"), I);
        const FString VisibleName = FString::Printf(TEXT("RegionVisible%d"), I), AppliedName = FString::Printf(TEXT("AppliedSerial%d"), I);
        const FString ShownName = FString::Printf(TEXT("AppliedVisible%d"), I), HideName = FString::Printf(TEXT("HideRegion%d"), I);
        auto* Spawn = Node<UK2Node_SpawnActorFromClass>(G); Spawn->AllocateDefaultPins();
        Spawn->GetClassPin()->DefaultObject = PulseClass; Spawn->PinDefaultValueChanged(Spawn->GetClassPin());
        auto* Transform = Call(G, UKismetMathLibrary::StaticClass(), TEXT("MakeTransform"));
        Link(Transform, TEXT("ReturnValue"), Spawn, TEXT("SpawnTransform")); Link(Self, TEXT("self"), Spawn, TEXT("Owner"));
        Value(Spawn, TEXT("CollisionHandlingOverride"), TEXT("AlwaysSpawn")); Link(BuildExec, TEXT("then"), Spawn, TEXT("execute"));
        auto* SavePulse = Set(G, *PulseName); Link(Spawn, TEXT("ReturnValue"), SavePulse, *PulseName); Link(Spawn, TEXT("then"), SavePulse, TEXT("execute"));
        auto* Create = Call(G, UWidgetBlueprintLibrary::StaticClass(), TEXT("Create")); Pin(Create, TEXT("WidgetType"))->DefaultObject = HudClass;
        Link(Player, TEXT("ReturnValue"), Create, TEXT("OwningPlayer")); Link(SavePulse, TEXT("then"), Create, TEXT("execute"));
        auto* CastHud = Node<UK2Node_DynamicCast>(G); CastHud->TargetType = HudClass; CastHud->AllocateDefaultPins(); CastHud->SetPurity(true);
        check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(Pin(Create, TEXT("ReturnValue")), CastHud->GetCastSourcePin()));
        auto* SaveHud = Set(G, *HudName);
        check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(CastHud->GetCastResultPin(), Pin(SaveHud, *HudName)));
        Link(Create, TEXT("then"), SaveHud, TEXT("execute"));
        auto* Pulse = Get(G, *PulseName); auto* Hud = Get(G, *HudName);
        auto* PulseValid = Valid(G, Pulse, *PulseName); auto* HudValid = Valid(G, Hud, *HudName);
        auto* Pair = Call(G, UKismetMathLibrary::StaticClass(), TEXT("BooleanAND"));
        Link(PulseValid, TEXT("ReturnValue"), Pair, TEXT("A")); Link(HudValid, TEXT("ReturnValue"), Pair, TEXT("B"));
        auto* ValidPair = Branch(G, Pair, TEXT("ReturnValue")); Link(SaveHud, TEXT("then"), ValidPair, TEXT("execute"));
        auto* Init = Call(G, PulseClass, TEXT("InitializeVisual")); Link(Pulse, *PulseName, Init, TEXT("self"));
        for (auto* ResourceName : { TEXT("Material"), TEXT("Scale"), TEXT("Alpha") }) { auto* Resource = Get(G, ResourceName); Link(Resource, ResourceName, Init, ResourceName); }
        Link(ValidPair, TEXT("then"), Init, TEXT("execute"));
        auto* Deactivate = Call(G, PulseClass, TEXT("DeactivateVisual")); Link(Pulse, *PulseName, Deactivate, TEXT("self")); Link(Init, TEXT("then"), Deactivate, TEXT("execute"));
        auto* Add = Call(G, UUserWidget::StaticClass(), TEXT("AddToViewport")); Link(Hud, *HudName, Add, TEXT("self")); Link(Deactivate, TEXT("then"), Add, TEXT("execute"));
        auto* Label = Call(G, HudClass, TEXT("SetMarkerLabel")); Link(Hud, *HudName, Label, TEXT("self"));
        GetDefault<UEdGraphSchema_K2>()->TrySetDefaultText(*Pin(Label, TEXT("Label")), FText::FromString(TEXT("[!] SPAWN AREA")));
        Link(Add, TEXT("then"), Label, TEXT("execute"));
        auto* Collapse = Call(G, UWidget::StaticClass(), TEXT("SetVisibility")); Link(Hud, *HudName, Collapse, TEXT("self")); Value(Collapse, TEXT("InVisibility"), TEXT("Collapsed")); Link(Label, TEXT("then"), Collapse, TEXT("execute"));
        BuildExec = Collapse;

        auto* ApplyCall = Call(G, BP->GeneratedClass, *FString::Printf(TEXT("ApplyRegion%d"), I)); Link(ServiceExec, TEXT("then"), ApplyCall, TEXT("execute")); ServiceExec = ApplyCall;
        auto* Serial = Get(G, *SerialName); auto* Applied = Get(G, *AppliedName);
        auto* Different = Call(G, UKismetMathLibrary::StaticClass(), TEXT("NotEqual_IntInt")); Link(Serial, *SerialName, Different, TEXT("A")); Link(Applied, *AppliedName, Different, TEXT("B"));
        auto* Changed = Branch(G, Different, TEXT("ReturnValue")); Link(Apply[I], TEXT("then"), Changed, TEXT("execute"));
        auto* SaveSerial = Set(G, *AppliedName); Link(Serial, *SerialName, SaveSerial, *AppliedName); Link(Changed, TEXT("then"), SaveSerial, TEXT("execute"));
        auto* RegionVisible = Get(G, *VisibleName); auto* IsVisible = Call(G, UKismetMathLibrary::StaticClass(), TEXT("Greater_IntInt")); Link(RegionVisible, *VisibleName, IsVisible, TEXT("A"));
        auto* Remaining = Call(G, UKismetMathLibrary::StaticClass(), TEXT("Subtract_FloatFloat")); Link(Get(G, *FString::Printf(TEXT("RegionExpires%d"), I)), *FString::Printf(TEXT("RegionExpires%d"), I), Remaining, TEXT("A")); Link(Call(G, UGameplayStatics::StaticClass(), TEXT("GetTimeSeconds")), TEXT("ReturnValue"), Remaining, TEXT("B"));
        auto* Fresh = Call(G, UKismetMathLibrary::StaticClass(), TEXT("Greater_FloatFloat")); Link(Remaining, TEXT("ReturnValue"), Fresh, TEXT("A"));
        auto* VisibleFresh = Call(G, UKismetMathLibrary::StaticClass(), TEXT("BooleanAND")); Link(IsVisible, TEXT("ReturnValue"), VisibleFresh, TEXT("A")); Link(Fresh, TEXT("ReturnValue"), VisibleFresh, TEXT("B"));
        auto* ShowGate = Branch(G, VisibleFresh, TEXT("ReturnValue")); Link(SaveSerial, TEXT("then"), ShowGate, TEXT("execute"));
        auto* HideCall = Call(G, BP->GeneratedClass, *HideName); Link(ShowGate, TEXT("else"), HideCall, TEXT("execute"));
        auto* ShowPair = Branch(G, Pair, TEXT("ReturnValue")); Link(ShowGate, TEXT("then"), ShowPair, TEXT("execute"));
        auto* Point = Get(G, *PointName); auto* Move = Call(G, AActor::StaticClass(), TEXT("K2_SetActorLocation"));
        Link(Pulse, *PulseName, Move, TEXT("self")); Link(Point, *PointName, Move, TEXT("NewLocation")); Value(Move, TEXT("bTeleport"), TEXT("true")); Link(ShowPair, TEXT("then"), Move, TEXT("execute"));
        auto* Location = Node<UK2Node_VariableSet>(G); Location->VariableReference.SetExternalMember(TEXT("WorldLocation"), HudClass); Location->AllocateDefaultPins();
        Link(Hud, *HudName, Location, TEXT("self")); Link(Point, *PointName, Location, TEXT("WorldLocation")); auto* LabelUpdate = UpdateRegionLabel(G, HudClass, I); Link(Move, TEXT("then"), LabelUpdate, TEXT("execute")); Link(LabelUpdate, TEXT("then"), Location, TEXT("execute"));
        auto* Shown = Get(G, *ShownName); auto* WasVisible = Call(G, UKismetMathLibrary::StaticClass(), TEXT("Greater_IntInt")); Link(Shown, *ShownName, WasVisible, TEXT("A"));
        auto* Reuse = Branch(G, WasVisible, TEXT("ReturnValue")); Link(Location, TEXT("then"), Reuse, TEXT("execute"));
        auto* Reactivate = Call(G, PulseClass, TEXT("ReactivateVisual")); Link(Pulse, *PulseName, Reactivate, TEXT("self")); Link(Reuse, TEXT("else"), Reactivate, TEXT("execute"));
        auto* MarkShown = Set(G, *ShownName); Value(MarkShown, *ShownName, TEXT("1")); Link(Reactivate, TEXT("then"), MarkShown, TEXT("execute"));
        auto* Display = Call(G, UWidget::StaticClass(), TEXT("SetVisibility")); Link(Hud, *HudName, Display, TEXT("self")); Value(Display, TEXT("InVisibility"), TEXT("HitTestInvisible")); Link(MarkShown, TEXT("then"), Display, TEXT("execute"));
        auto* Timer = Call(G, UKismetSystemLibrary::StaticClass(), TEXT("K2_SetTimer")); Link(Self, TEXT("self"), Timer, TEXT("Object")); Value(Timer, TEXT("FunctionName"), *HideName); Link(Remaining, TEXT("ReturnValue"), Timer, TEXT("Time"));
        Link(Display, TEXT("then"), Timer, TEXT("execute")); Link(Reuse, TEXT("then"), Timer, TEXT("execute"));
        auto* HidePair = Branch(G, Pair, TEXT("ReturnValue")); auto* ClearVisible = Set(G, *VisibleName); Value(ClearVisible, *VisibleName, TEXT("0")); Link(Hide[I], TEXT("then"), ClearVisible, TEXT("execute")); Link(ClearVisible, TEXT("then"), HidePair, TEXT("execute"));
        auto* MarkHidden = Set(G, *ShownName); Value(MarkHidden, *ShownName, TEXT("0")); Link(HidePair, TEXT("then"), MarkHidden, TEXT("execute"));
        auto* Stop = Call(G, PulseClass, TEXT("DeactivateVisual")); Link(Pulse, *PulseName, Stop, TEXT("self")); Link(MarkHidden, TEXT("then"), Stop, TEXT("execute"));
        auto* Conceal = Call(G, UWidget::StaticClass(), TEXT("SetVisibility")); Link(Hud, *HudName, Conceal, TEXT("self")); Value(Conceal, TEXT("InVisibility"), TEXT("Collapsed")); Link(Stop, TEXT("then"), Conceal, TEXT("execute"));

        auto* Clear = Call(G, UKismetSystemLibrary::StaticClass(), TEXT("K2_ClearTimer")); Link(Self, TEXT("self"), Clear, TEXT("Object")); Value(Clear, TEXT("FunctionName"), *HideName); Link(CleanupExec, TEXT("then"), Clear, TEXT("execute"));
        auto* CleanHud = Branch(G, HudValid, TEXT("ReturnValue")); Link(Clear, TEXT("then"), CleanHud, TEXT("execute"));
        auto* Remove = Call(G, UWidget::StaticClass(), TEXT("RemoveFromParent")); Link(Hud, *HudName, Remove, TEXT("self")); Link(CleanHud, TEXT("then"), Remove, TEXT("execute"));
        auto* CleanPulse = Branch(G, PulseValid, TEXT("ReturnValue")); Link(Remove, TEXT("then"), CleanPulse, TEXT("execute")); Link(CleanHud, TEXT("else"), CleanPulse, TEXT("execute"));
        auto* Destroy = Call(G, AActor::StaticClass(), TEXT("K2_DestroyActor")); Link(Pulse, *PulseName, Destroy, TEXT("self")); Link(CleanPulse, TEXT("then"), Destroy, TEXT("execute"));
        // A sequence keeps cleanup advancing even when a pair was only partially initialized.
        auto* NextCall = Call(G, BP->GeneratedClass, *FString::Printf(TEXT("CleanupNext%d"), I));
        Link(Destroy, TEXT("then"), NextCall, TEXT("execute")); Link(CleanPulse, TEXT("else"), NextCall, TEXT("execute")); CleanupExec = Next[I];
    }
    auto* ReadyPool = Set(G, TEXT("PoolReady")); Value(ReadyPool, TEXT("PoolReady"), TEXT("true")); Link(BuildExec, TEXT("then"), ReadyPool, TEXT("execute"));
    auto* End = Event(G, AActor::StaticClass(), TEXT("ReceiveEndPlay"));
    auto* Clean = Call(G, BP->GeneratedClass, TEXT("CleanupPool")); auto* Unbind = Call(G, BP->GeneratedClass, TEXT("DisconnectCapture")); Link(End, TEXT("then"), Unbind, TEXT("execute")); Link(Unbind, TEXT("then"), Clean, TEXT("execute"));
    auto* ParentEnd = Node<UK2Node_CallParentFunction>(G); ParentEnd->SetFromFunction(AActor::StaticClass()->FindFunctionByName(TEXT("ReceiveEndPlay"))); ParentEnd->AllocateDefaultPins();
    Link(End, TEXT("EndPlayReason"), ParentEnd, TEXT("EndPlayReason")); Link(Clean, TEXT("then"), ParentEnd, TEXT("execute"));
    Compile(BP);
    auto* CDO = CastChecked<AActor>(BP->GeneratedClass->GetDefaultObject());
    CDO->PrimaryActorTick.bCanEverTick = true; CDO->PrimaryActorTick.bStartWithTickEnabled = true;
    CDO->PrimaryActorTick.TickGroup = TG_PostUpdateWork;
    check(!CDO->GetIsReplicated()); Save(BP);
}
