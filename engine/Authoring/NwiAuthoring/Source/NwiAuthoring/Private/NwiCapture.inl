// Emit stock Blueprint graphs that observe existing FSD events without changing enemy spawning.
// Eight regions bound per-spawn work and presentation cost; 800 cm is the merge radius.
void DeclareCapture(UBlueprint* BP)
{
    Variable(BP, TEXT("CaptureManager"), Type(UEdGraphSchema_K2::PC_Object, UEnemySpawnManager::StaticClass()));
    Variable(BP, TEXT("WaveManager"), Type(UEdGraphSchema_K2::PC_Object, UEnemyWaveManager::StaticClass()));
    Variable(BP, TEXT("SamplePoint"), Type(UEdGraphSchema_K2::PC_Struct, TBaseStructure<FVector>::Get()));
    Variable(BP, TEXT("SampleLabel"), Type(UEdGraphSchema_K2::PC_String));
    for (const TCHAR* Name : {TEXT("SampleTime"), TEXT("NextBindAttempt"), TEXT("OldestTime")}) Variable(BP, Name, Type(UEdGraphSchema_K2::PC_Float));
    for (const TCHAR* Name : {TEXT("OldestSlot"), TEXT("CapturedSpawns")}) Variable(BP, Name, Type(UEdGraphSchema_K2::PC_Int));
    for (int32 I = 0; I < 8; ++I) {
        Variable(BP, *FString::Printf(TEXT("RegionExpires%d"), I), Type(UEdGraphSchema_K2::PC_Float));
        Variable(BP, *FString::Printf(TEXT("RegionLabel%d"), I), Type(UEdGraphSchema_K2::PC_String), TEXT("Unknown / possible natural wave"));
    }
}

// Preserve user text as a prefix; event context is never represented as certain provenance.
UK2Node_CallFunction* UpdateRegionLabel(UEdGraph* G, UClass* HudClass, int32 I)
{
    auto* Config = Get(G, TEXT("Settings"));
    auto* SaveClass = LoadClass<USaveGame>(nullptr, TEXT("/Game/NormalWaveIndicator/SG_NwiSettings.SG_NwiSettings_C"));
    auto* Prefix = Call(G, UKismetStringLibrary::StaticClass(), TEXT("Concat_StrStr"));
    Link(Field(G, SaveClass, TEXT("Label"), Config, TEXT("Settings")), TEXT("Label"), Prefix, TEXT("A")); Value(Prefix, TEXT("B"), TEXT(" | "));
    auto* Full = Call(G, UKismetStringLibrary::StaticClass(), TEXT("Concat_StrStr")); Link(Prefix, TEXT("ReturnValue"), Full, TEXT("A"));
    const auto Name = FString::Printf(TEXT("RegionLabel%d"), I); Link(Get(G, *Name), *Name, Full, TEXT("B"));
    auto* Text = Call(G, UKismetTextLibrary::StaticClass(), TEXT("Conv_StringToText")); Link(Full, TEXT("ReturnValue"), Text, TEXT("InString"));
    auto* SetLabel = Call(G, HudClass, TEXT("SetMarkerLabel"));
    const auto Hud = FString::Printf(TEXT("AutoHud%d"), I); Link(Get(G, *Hud), *Hud, SetLabel, TEXT("self")); Link(Text, TEXT("ReturnValue"), SetLabel, TEXT("Label"));
    return SetLabel;
}

// Bind/unbind only our own delegate, including manager replacement and EndPlay.
UK2Node_BaseMCDelegate* CaptureDelegate(UEdGraph* G, bool Add)
{
    UK2Node_BaseMCDelegate* N = Add ? static_cast<UK2Node_BaseMCDelegate*>(Node<UK2Node_AddDelegate>(G)) : Node<UK2Node_RemoveDelegate>(G);
    N->DelegateReference.SetExternalMember(TEXT("OnEnemySpawned"), UEnemySpawnManager::StaticClass()); N->AllocateDefaultPins();
    Link(Get(G, TEXT("CaptureManager")), TEXT("CaptureManager"), N, TEXT("self"));
    auto* D = Node<UK2Node_CreateDelegate>(G); D->AllocateDefaultPins();
    check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(D->GetDelegateOutPin(), N->GetDelegatePin()));
    D->SetFunction(TEXT("ObserveEnemy")); return N;
}

void BuildCapture(UBlueprint* BP, UEdGraph* G)
{
    auto* Attach = Custom(G, TEXT("AttachCapture")); Attach->CreateUserDefinedPin(TEXT("Manager"), Type(UEdGraphSchema_K2::PC_Object, UEnemyWaveManager::StaticClass()), EGPD_Output);
    auto* Connect = Custom(G, TEXT("ConnectCapture")); auto* Disconnect = Custom(G, TEXT("DisconnectCapture"));
    auto* Observe = Custom(G, TEXT("ObserveEnemy"));
    Observe->CreateUserDefinedPin(TEXT("enemy"), Type(UEdGraphSchema_K2::PC_Object, APawn::StaticClass()), EGPD_Output);
    Observe->CreateUserDefinedPin(TEXT("descriptor"), Type(UEdGraphSchema_K2::PC_Object, UEnemyDescriptor::StaticClass()), EGPD_Output);
    auto* Record = Custom(G, TEXT("RecordSpawn")); auto* Choose = Custom(G, TEXT("ChooseOldest"));
    UK2Node_CustomEvent* Find[8]{}; UK2Node_CustomEvent* Seed[8]{}; UK2Node_CustomEvent* Touch[8]{};
    for (int32 I = 0; I < 8; ++I) {
        Find[I] = Custom(G, *FString::Printf(TEXT("FindRegion%d"), I));
        Seed[I] = Custom(G, *FString::Printf(TEXT("SeedRegion%d"), I));
        Touch[I] = Custom(G, *FString::Printf(TEXT("TouchRegion%d"), I));
    }
    Compile(BP);
    auto* Bound = Get(G, TEXT("CaptureManager")); auto* Wave = Get(G, TEXT("WaveManager"));
    auto* HaveBound = Valid(G, Bound, TEXT("CaptureManager"));
    auto* UnbindGate = Branch(G, HaveBound, TEXT("ReturnValue")); Link(Disconnect, TEXT("then"), UnbindGate, TEXT("execute"));
    auto* Remove = CaptureDelegate(G, false); Link(UnbindGate, TEXT("then"), Remove, TEXT("execute"));
    auto* Clear = Set(G, TEXT("CaptureManager")); Link(Remove, TEXT("then"), Clear, TEXT("execute")); Link(UnbindGate, TEXT("else"), Clear, TEXT("execute"));
    auto* ClearWave = Set(G, TEXT("WaveManager")); Link(Clear, TEXT("then"), ClearWave, TEXT("execute"));
    auto* BeforeAttach = Call(G, BP->GeneratedClass, TEXT("DisconnectCapture")); Link(Attach, TEXT("then"), BeforeAttach, TEXT("execute"));
    auto* SaveWave = Set(G, TEXT("WaveManager")); Link(Attach, TEXT("Manager"), SaveWave, TEXT("WaveManager")); Link(BeforeAttach, TEXT("then"), SaveWave, TEXT("execute"));
    auto* WaveValid = Branch(G, Valid(G, Wave, TEXT("WaveManager")), TEXT("ReturnValue")); Link(SaveWave, TEXT("then"), WaveValid, TEXT("execute"));
    auto* SaveBound = Set(G, TEXT("CaptureManager")); Link(Field(G, UEnemyWaveManager::StaticClass(), TEXT("SpawnManager"), Wave, TEXT("WaveManager")), TEXT("SpawnManager"), SaveBound, TEXT("CaptureManager")); Link(WaveValid, TEXT("then"), SaveBound, TEXT("execute"));
    auto* BindGate = Branch(G, HaveBound, TEXT("ReturnValue")); Link(SaveBound, TEXT("then"), BindGate, TEXT("execute"));
    auto* Add = CaptureDelegate(G, true); Link(BindGate, TEXT("then"), Add, TEXT("execute"));

    // Retry initialization once per second until a host's manager exists, independently of visual loading.
    auto* Server = Branch(G, Call(G, UKismetSystemLibrary::StaticClass(), TEXT("IsServer")), TEXT("ReturnValue")); Link(Connect, TEXT("then"), Server, TEXT("execute"));
    auto* Connected = Branch(G, HaveBound, TEXT("ReturnValue")); Link(Server, TEXT("then"), Connected, TEXT("execute"));
    auto* Now = Call(G, UGameplayStatics::StaticClass(), TEXT("GetTimeSeconds"));
    auto* Due = Call(G, UKismetMathLibrary::StaticClass(), TEXT("GreaterEqual_FloatFloat")); Link(Now, TEXT("ReturnValue"), Due, TEXT("A")); Link(Get(G, TEXT("NextBindAttempt")), TEXT("NextBindAttempt"), Due, TEXT("B"));
    auto* Retry = Branch(G, Due, TEXT("ReturnValue")); Link(Connected, TEXT("else"), Retry, TEXT("execute"));
    auto* Next = Call(G, UKismetMathLibrary::StaticClass(), TEXT("Add_FloatFloat")); Link(Now, TEXT("ReturnValue"), Next, TEXT("A")); Value(Next, TEXT("B"), TEXT("1"));
    auto* SaveNext = Set(G, TEXT("NextBindAttempt")); Link(Next, TEXT("ReturnValue"), SaveNext, TEXT("NextBindAttempt")); Link(Retry, TEXT("then"), SaveNext, TEXT("execute"));
    auto* Mode = Call(G, UGameplayStatics::StaticClass(), TEXT("GetGameMode"));
    auto* Cast = Node<UK2Node_DynamicCast>(G); Cast->TargetType = AFSDGameMode::StaticClass(); Cast->AllocateDefaultPins();
    check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(Pin(Mode, TEXT("ReturnValue")), Cast->GetCastSourcePin())); Link(SaveNext, TEXT("then"), Cast, TEXT("execute"));
    auto* Manager = Call(G, AFSDGameMode::StaticClass(), TEXT("GetWaveManager")); check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(Cast->GetCastResultPin(), Pin(Manager, TEXT("self"))));
    auto* DoAttach = Call(G, BP->GeneratedClass, TEXT("AttachCapture")); Link(Manager, TEXT("ReturnValue"), DoAttach, TEXT("Manager")); Link(Cast, TEXT("then"), DoAttach, TEXT("execute"));

    // Snapshot the Pawn before looking at optional event context; never spawn, move or modify an enemy.
    auto* EnemyValid = Branch(G, Valid(G, Observe, TEXT("enemy")), TEXT("ReturnValue")); Link(Observe, TEXT("then"), EnemyValid, TEXT("execute"));
    auto* Authority = Branch(G, Call(G, UKismetSystemLibrary::StaticClass(), TEXT("IsServer")), TEXT("ReturnValue")); Link(EnemyValid, TEXT("then"), Authority, TEXT("execute"));
    auto* Position = Call(G, AActor::StaticClass(), TEXT("K2_GetActorLocation")); Link(Observe, TEXT("enemy"), Position, TEXT("self"));
    auto* Point = Set(G, TEXT("SamplePoint")); Link(Position, TEXT("ReturnValue"), Point, TEXT("SamplePoint")); Link(Authority, TEXT("then"), Point, TEXT("execute"));
    auto* Stamp = Set(G, TEXT("SampleTime")); Link(Now, TEXT("ReturnValue"), Stamp, TEXT("SampleTime")); Link(Point, TEXT("then"), Stamp, TEXT("execute"));
    auto* Fallback = Set(G, TEXT("SampleLabel")); Value(Fallback, TEXT("SampleLabel"), TEXT("Unknown / possible natural wave")); Link(Stamp, TEXT("then"), Fallback, TEXT("execute"));
    auto* ContextGate = Branch(G, Valid(G, Wave, TEXT("WaveManager")), TEXT("ReturnValue")); Link(Fallback, TEXT("then"), ContextGate, TEXT("execute"));
    auto* DoRecord = Call(G, BP->GeneratedClass, TEXT("RecordSpawn")); Link(ContextGate, TEXT("else"), DoRecord, TEXT("execute"));
    auto* Waves = Field(G, UEnemyWaveManager::StaticClass(), TEXT("ActiveScriptedWaves"), Wave, TEXT("WaveManager"));
    auto* Length = Call(G, UKismetArrayLibrary::StaticClass(), TEXT("Array_Length")); Link(Waves, TEXT("ActiveScriptedWaves"), Length, TEXT("TargetArray"));
    auto* Any = Call(G, UKismetMathLibrary::StaticClass(), TEXT("Greater_IntInt")); Link(Length, TEXT("ReturnValue"), Any, TEXT("A"));
    auto* AnyGate = Branch(G, Any, TEXT("ReturnValue")); Link(ContextGate, TEXT("then"), AnyGate, TEXT("execute")); Link(AnyGate, TEXT("else"), DoRecord, TEXT("execute"));
    auto* First = Call(G, UKismetArrayLibrary::StaticClass(), TEXT("Array_Get")); Link(Waves, TEXT("ActiveScriptedWaves"), First, TEXT("TargetArray"));
    auto* FirstValid = Branch(G, Valid(G, First, TEXT("Item")), TEXT("ReturnValue")); Link(AnyGate, TEXT("then"), FirstValid, TEXT("execute")); Link(FirstValid, TEXT("else"), DoRecord, TEXT("execute"));
    auto* Class = Call(G, UGameplayStatics::StaticClass(), TEXT("GetObjectClass")); Link(First, TEXT("Item"), Class, TEXT("Object"));
    auto* Name = Call(G, UKismetSystemLibrary::StaticClass(), TEXT("GetClassDisplayName")); Link(Class, TEXT("ReturnValue"), Name, TEXT("Class"));
    auto* Suffix = Call(G, UKismetStringLibrary::StaticClass(), TEXT("EndsWith")); Link(Name, TEXT("ReturnValue"), Suffix, TEXT("SourceString")); Value(Suffix, TEXT("InSuffix"), TEXT("_C"));
    auto* Trim = Call(G, UKismetStringLibrary::StaticClass(), TEXT("LeftChop")); Link(Name, TEXT("ReturnValue"), Trim, TEXT("SourceString")); Value(Trim, TEXT("Count"), TEXT("2"));
    auto* CleanName = Call(G, UKismetMathLibrary::StaticClass(), TEXT("SelectString")); Link(Suffix, TEXT("ReturnValue"), CleanName, TEXT("bPickA")); Link(Trim, TEXT("ReturnValue"), CleanName, TEXT("A")); Link(Name, TEXT("ReturnValue"), CleanName, TEXT("B"));
    UEdGraphNode* Readable = CleanName;
    for (const TCHAR* Part : {TEXT("EWC_"), TEXT("_")}) {
        auto* Replace = Call(G, UKismetStringLibrary::StaticClass(), TEXT("Replace")); Link(Readable, TEXT("ReturnValue"), Replace, TEXT("SourceString"));
        Value(Replace, TEXT("From"), Part); Value(Replace, TEXT("To"), FString(Part) == TEXT("_") ? TEXT(" ") : TEXT("")); Readable = Replace;
    }
    auto* Multiple = Call(G, UKismetMathLibrary::StaticClass(), TEXT("Greater_IntInt")); Link(Length, TEXT("ReturnValue"), Multiple, TEXT("A")); Value(Multiple, TEXT("B"), TEXT("1"));
    auto* Prefix = Call(G, UKismetMathLibrary::StaticClass(), TEXT("SelectString")); Link(Multiple, TEXT("ReturnValue"), Prefix, TEXT("bPickA")); Value(Prefix, TEXT("A"), TEXT("Mixed events (first): ")); Value(Prefix, TEXT("B"), TEXT("Event context: "));
    auto* Label = Call(G, UKismetStringLibrary::StaticClass(), TEXT("Concat_StrStr")); Link(Prefix, TEXT("ReturnValue"), Label, TEXT("A")); Link(Readable, TEXT("ReturnValue"), Label, TEXT("B"));
    auto* SaveLabel = Set(G, TEXT("SampleLabel")); Link(Label, TEXT("ReturnValue"), SaveLabel, TEXT("SampleLabel")); Link(FirstValid, TEXT("then"), SaveLabel, TEXT("execute")); Link(SaveLabel, TEXT("then"), DoRecord, TEXT("execute"));

    // Fixed-size spatial aggregation: retain the first spawn origin, extend its lifetime on nearby events.
    auto* Count = Call(G, UKismetMathLibrary::StaticClass(), TEXT("Add_IntInt")); Link(Get(G, TEXT("CapturedSpawns")), TEXT("CapturedSpawns"), Count, TEXT("A")); Value(Count, TEXT("B"), TEXT("1"));
    auto* SaveCount = Set(G, TEXT("CapturedSpawns")); Link(Count, TEXT("ReturnValue"), SaveCount, TEXT("CapturedSpawns")); Link(Record, TEXT("then"), SaveCount, TEXT("execute"));
    auto* Start = Call(G, BP->GeneratedClass, TEXT("FindRegion0")); Link(SaveCount, TEXT("then"), Start, TEXT("execute"));
    auto* Zero = Set(G, TEXT("OldestSlot")); Value(Zero, TEXT("OldestSlot"), TEXT("0")); Link(Choose, TEXT("then"), Zero, TEXT("execute"));
    auto* Best = Set(G, TEXT("OldestTime")); Link(Get(G, TEXT("RegionExpires0")), TEXT("RegionExpires0"), Best, TEXT("OldestTime")); Link(Zero, TEXT("then"), Best, TEXT("execute"));
    UEdGraphNode* OldestExec = Best;
    for (int32 I = 0; I < 8; ++I) {
        const auto P = FString::Printf(TEXT("RegionPoint%d"), I), E = FString::Printf(TEXT("RegionExpires%d"), I), L = FString::Printf(TEXT("RegionLabel%d"), I), S = FString::Printf(TEXT("RegionSerial%d"), I), V = FString::Printf(TEXT("RegionVisible%d"), I);
        auto* Alive = Call(G, UKismetMathLibrary::StaticClass(), TEXT("Greater_FloatFloat")); Link(Get(G, *E), *E, Alive, TEXT("A")); Link(Get(G, TEXT("SampleTime")), TEXT("SampleTime"), Alive, TEXT("B"));
        auto* Distance = Call(G, UKismetMathLibrary::StaticClass(), TEXT("Vector_DistanceSquared")); Link(Get(G, *P), *P, Distance, TEXT("V1")); Link(Get(G, TEXT("SamplePoint")), TEXT("SamplePoint"), Distance, TEXT("V2"));
        auto* Nearby = Call(G, UKismetMathLibrary::StaticClass(), TEXT("LessEqual_FloatFloat")); Link(Distance, TEXT("ReturnValue"), Nearby, TEXT("A")); Value(Nearby, TEXT("B"), TEXT("640000"));
        auto* Same = Call(G, UKismetStringLibrary::StaticClass(), TEXT("EqualEqual_StrStr")); Link(Get(G, *L), *L, Same, TEXT("A")); Link(Get(G, TEXT("SampleLabel")), TEXT("SampleLabel"), Same, TEXT("B"));
        auto* Spatial = Call(G, UKismetMathLibrary::StaticClass(), TEXT("BooleanAND")); Link(Alive, TEXT("ReturnValue"), Spatial, TEXT("A")); Link(Nearby, TEXT("ReturnValue"), Spatial, TEXT("B"));
        auto* Match = Call(G, UKismetMathLibrary::StaticClass(), TEXT("BooleanAND")); Link(Spatial, TEXT("ReturnValue"), Match, TEXT("A")); Link(Same, TEXT("ReturnValue"), Match, TEXT("B"));
        auto* MatchGate = Branch(G, Match, TEXT("ReturnValue")); Link(Find[I], TEXT("then"), MatchGate, TEXT("execute"));
        auto* TouchCall = Call(G, BP->GeneratedClass, *FString::Printf(TEXT("TouchRegion%d"), I)); Link(MatchGate, TEXT("then"), TouchCall, TEXT("execute"));
        auto* Continue = Call(G, BP->GeneratedClass, I == 7 ? TEXT("ChooseOldest") : *FString::Printf(TEXT("FindRegion%d"), I+1)); Link(MatchGate, TEXT("else"), Continue, TEXT("execute"));
        auto* Anchor = Set(G, *P); Link(Get(G, TEXT("SamplePoint")), TEXT("SamplePoint"), Anchor, *P); Link(Seed[I], TEXT("then"), Anchor, TEXT("execute")); Link(Anchor, TEXT("then"), TouchCall, TEXT("execute"));
        auto* Expires = Call(G, UKismetMathLibrary::StaticClass(), TEXT("Add_FloatFloat")); Link(Get(G, TEXT("SampleTime")), TEXT("SampleTime"), Expires, TEXT("A")); Link(Get(G, TEXT("DurationSec")), TEXT("DurationSec"), Expires, TEXT("B"));
        auto* StoreExpiry = Set(G, *E); Link(Expires, TEXT("ReturnValue"), StoreExpiry, *E); Link(Touch[I], TEXT("then"), StoreExpiry, TEXT("execute"));
        auto* StoreLabel = Set(G, *L); Link(Get(G, TEXT("SampleLabel")), TEXT("SampleLabel"), StoreLabel, *L); Link(StoreExpiry, TEXT("then"), StoreLabel, TEXT("execute"));
        auto* Serial = Set(G, *S); Link(Get(G, TEXT("CapturedSpawns")), TEXT("CapturedSpawns"), Serial, *S); Link(StoreLabel, TEXT("then"), Serial, TEXT("execute"));
        auto* Visible = Set(G, *V); Value(Visible, *V, TEXT("1")); Link(Serial, TEXT("then"), Visible, TEXT("execute"));
        // Evict the least recently extended region only when all eight spatial matches failed.
        auto* Older = Call(G, UKismetMathLibrary::StaticClass(), TEXT("Less_FloatFloat")); Link(Get(G, *E), *E, Older, TEXT("A")); Link(Get(G, TEXT("OldestTime")), TEXT("OldestTime"), Older, TEXT("B"));
        auto* OlderGate = Branch(G, Older, TEXT("ReturnValue")); Link(OldestExec, TEXT("then"), OlderGate, TEXT("execute"));
        auto* SaveBest = Set(G, TEXT("OldestTime")); Link(Get(G, *E), *E, SaveBest, TEXT("OldestTime")); Link(OlderGate, TEXT("then"), SaveBest, TEXT("execute"));
        auto* Slot = Set(G, TEXT("OldestSlot")); Value(Slot, TEXT("OldestSlot"), *FString::FromInt(I)); Link(SaveBest, TEXT("then"), Slot, TEXT("execute"));
        // Use a no-op self setter as the merge point, with a standard 'then' output.
        auto* Merge = Set(G, TEXT("OldestSlot")); Link(Get(G, TEXT("OldestSlot")), TEXT("OldestSlot"), Merge, TEXT("OldestSlot")); Link(Slot, TEXT("then"), Merge, TEXT("execute")); Link(OlderGate, TEXT("else"), Merge, TEXT("execute")); OldestExec = Merge;
    }
    for (int32 I = 0; I < 8; ++I) {
        auto* IsSlot = Call(G, UKismetMathLibrary::StaticClass(), TEXT("EqualEqual_IntInt")); Link(Get(G, TEXT("OldestSlot")), TEXT("OldestSlot"), IsSlot, TEXT("A")); Value(IsSlot, TEXT("B"), *FString::FromInt(I));
        auto* Gate = Branch(G, IsSlot, TEXT("ReturnValue")); Link(OldestExec, TEXT("then"), Gate, TEXT("execute"));
        auto* SeedCall = Call(G, BP->GeneratedClass, *FString::Printf(TEXT("SeedRegion%d"), I)); Link(Gate, TEXT("then"), SeedCall, TEXT("execute"));
        auto* NextSlot = Set(G, TEXT("OldestSlot")); Link(Get(G, TEXT("OldestSlot")), TEXT("OldestSlot"), NextSlot, TEXT("OldestSlot")); Link(Gate, TEXT("else"), NextSlot, TEXT("execute")); OldestExec = NextSlot;
    }
}

// DRG discovers these two entry actors; each world owns at most one controller, without a loader DLL.
void BuildNativeInitializers()
{
    auto* Controller = LoadClass<AActor>(nullptr, TEXT("/Game/NormalWaveIndicator/BP_NwiAuto.BP_NwiAuto_C")); check(Controller);
    for (const TCHAR* Name : {TEXT("InitCave"), TEXT("InitSpacerig")}) {
        auto* BP = Blueprint(Name, false, AActor::StaticClass());
        Variable(BP, TEXT("OwnedController"), Type(UEdGraphSchema_K2::PC_Object, Controller)); Compile(BP);
        auto* G = Graph(BP);
        auto* Begin = Event(G, AActor::StaticClass(), TEXT("ReceiveBeginPlay"));
        auto* Existing = Call(G, UGameplayStatics::StaticClass(), TEXT("GetActorOfClass")); Pin(Existing, TEXT("ActorClass"))->DefaultObject = Controller;
        auto* Present = Branch(G, Valid(G, Existing, TEXT("ReturnValue")), TEXT("ReturnValue")); Link(Begin, TEXT("then"), Existing, TEXT("execute")); Link(Existing, TEXT("then"), Present, TEXT("execute"));
        auto* Spawn = Node<UK2Node_SpawnActorFromClass>(G); Spawn->AllocateDefaultPins(); Spawn->GetClassPin()->DefaultObject = Controller; Spawn->PinDefaultValueChanged(Spawn->GetClassPin());
        Link(Call(G, UKismetMathLibrary::StaticClass(), TEXT("MakeTransform")), TEXT("ReturnValue"), Spawn, TEXT("SpawnTransform")); Value(Spawn, TEXT("CollisionHandlingOverride"), TEXT("AlwaysSpawn")); Link(Present, TEXT("else"), Spawn, TEXT("execute"));
        auto* Store = Set(G, TEXT("OwnedController")); Link(Spawn, TEXT("ReturnValue"), Store, TEXT("OwnedController")); Link(Spawn, TEXT("then"), Store, TEXT("execute"));
        // Only the entry that created the controller owns cleanup; duplicate entry actors leave it alone.
        auto* End = Event(G, AActor::StaticClass(), TEXT("ReceiveEndPlay")); auto* Owned = Get(G, TEXT("OwnedController"));
        auto* HaveOwned = Branch(G, Valid(G, Owned, TEXT("OwnedController")), TEXT("ReturnValue")); Link(End, TEXT("then"), HaveOwned, TEXT("execute"));
        auto* Destroy = Call(G, AActor::StaticClass(), TEXT("K2_DestroyActor")); Link(Owned, TEXT("OwnedController"), Destroy, TEXT("self")); Link(HaveOwned, TEXT("then"), Destroy, TEXT("execute"));
        Compile(BP); Save(BP);
    }
}
