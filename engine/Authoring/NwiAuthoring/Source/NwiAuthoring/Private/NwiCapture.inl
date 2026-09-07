// Versioned native handoff; the DLL alone supplies proven natural-wave records.
void DeclareCapture(UBlueprint* BP)
{
    Variable(BP, TEXT("NativeAbi"), Type(UEdGraphSchema_K2::PC_Int), TEXT("524288"));
    Variable(BP, TEXT("NativeCookie"), Type(UEdGraphSchema_K2::PC_Int));
    Variable(BP, TEXT("NativeTime"), Type(UEdGraphSchema_K2::PC_Float));
    Variable(BP, TEXT("NativeEnabled"), Type(UEdGraphSchema_K2::PC_Int), TEXT("1"));
    for (int32 I=0; I<8; ++I) {
        Variable(BP, *FString::Printf(TEXT("RegionExpires%d"), I), Type(UEdGraphSchema_K2::PC_Float));
        Variable(BP, *FString::Printf(TEXT("RegionScale%d"), I), Type(UEdGraphSchema_K2::PC_Float), TEXT("1"));
    }
    // Only our zero-argument no-op is replaced; no game-wide Blueprint interception.
    auto* F = FBlueprintEditorUtils::CreateNewGraph(BP, TEXT("NwiPoll"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddFunctionGraph(BP, F, true, static_cast<UClass*>(nullptr));
}
UK2Node_CallFunction* UpdateRegionLabel(UEdGraph* G, UClass* HudClass, int32 I)
{
    auto* SaveClass=LoadClass<USaveGame>(nullptr,TEXT("/Game/NormalWaveIndicator/SG_NwiSettings.SG_NwiSettings_C"));
    auto* Text=Call(G,UKismetTextLibrary::StaticClass(),TEXT("Conv_StringToText"));
    Link(Field(G,SaveClass,TEXT("Label"),Get(G,TEXT("Settings")),TEXT("Settings")),TEXT("Label"),Text,TEXT("InString"));
    auto* Label=Call(G,HudClass,TEXT("SetMarkerLabel"));
    const auto H=FString::Printf(TEXT("AutoHud%d"),I);Link(Get(G,*H),*H,Label,TEXT("self"));Link(Text,TEXT("ReturnValue"),Label,TEXT("Label"));return Label;
}
// The host owns the replicated controller. Clients render the host-created instance.
void BuildNativeInitializers()
{
    auto* Controller = LoadClass<AActor>(nullptr, TEXT("/Game/NormalWaveIndicator/BP_NwiAuto.BP_NwiAuto_C")); check(Controller);
    for (const TCHAR* Name : {TEXT("InitCave"), TEXT("InitSpacerig")}) {
        auto* BP = Blueprint(Name, false, AActor::StaticClass());
        Variable(BP, TEXT("OwnedController"), Type(UEdGraphSchema_K2::PC_Object, Controller)); Compile(BP);
        auto* G = Graph(BP);
        auto* Begin = Event(G, AActor::StaticClass(), TEXT("ReceiveBeginPlay"));
        auto* Existing = Call(G, UGameplayStatics::StaticClass(), TEXT("GetActorOfClass")); Pin(Existing, TEXT("ActorClass"))->DefaultObject = Controller;
        auto* Present = Branch(G, Valid(G, Existing, TEXT("ReturnValue")), TEXT("ReturnValue")); auto* Server = Branch(G, Call(G, UKismetSystemLibrary::StaticClass(), TEXT("IsServer")), TEXT("ReturnValue")); Link(Begin, TEXT("then"), Server, TEXT("execute")); Link(Server, TEXT("then"), Existing, TEXT("execute")); Link(Existing, TEXT("then"), Present, TEXT("execute"));
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
