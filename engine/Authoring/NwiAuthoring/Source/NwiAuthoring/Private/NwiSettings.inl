// Mod Hub interfaces and local SaveGame settings. No framework assets are packaged with this mod.
UClass* HubInterface(const TCHAR* Name)
{
    auto* Result = LoadClass<UInterface>(nullptr, *FString::Printf(TEXT("/Game/_ModHub/%s.%s_C"), Name, Name));
    check(Result); return Result;
}

// Interface implementation creates the correctly typed function graph and output pins.
UK2Node_FunctionResult* HubResult(UBlueprint* BP, const TCHAR* Name)
{
    TArray<UEdGraph*> Graphs = BP->FunctionGraphs;
    for (const auto& Interface : BP->ImplementedInterfaces) Graphs.Append(Interface.Graphs);
    for (auto* G : Graphs) if (G->GetFName() == FName(Name))
        for (auto* N : G->Nodes) if (auto* Result = Cast<UK2Node_FunctionResult>(N)) return Result;
    checkf(false, TEXT("Missing Mod Hub implementation %s"), Name); return nullptr;
}

// Cross-object fields remain ordinary typed Blueprint references, never raw native property guesses.
UK2Node_VariableGet* Field(UEdGraph* G, UClass* Owner, const TCHAR* Name, UEdGraphNode* Object, const TCHAR* Output)
{
    auto* N = Node<UK2Node_VariableGet>(G); N->VariableReference.SetExternalMember(Name, Owner); N->AllocateDefaultPins();
    Link(Object, Output, N, TEXT("self")); return N;
}
UK2Node_VariableSet* WriteField(UEdGraph* G, UClass* Owner, const TCHAR* Name, UEdGraphNode* Object, const TCHAR* Output)
{
    auto* N = Node<UK2Node_VariableSet>(G); N->VariableReference.SetExternalMember(Name, Owner); N->AllocateDefaultPins();
    Link(Object, Output, N, TEXT("self")); return N;
}

const TCHAR* SettingNames[] = {TEXT("Label"), TEXT("Duration"), TEXT("Radius"), TEXT("Red"), TEXT("Green"), TEXT("Blue"), TEXT("Blink")};
const TCHAR* SettingDefaults[] = {TEXT("[!] NORMAL WAVE"), TEXT("8"), TEXT("3.75"), TEXT("3"), TEXT("0.01"), TEXT("0.005"), TEXT("true")};
void AddSettings(UBlueprint* BP)
{
    for (int32 I = 0; I < 7; ++I) Variable(BP, SettingNames[I], Type(I == 0 ? UEdGraphSchema_K2::PC_String : I == 6 ? UEdGraphSchema_K2::PC_Boolean : UEdGraphSchema_K2::PC_Float), SettingDefaults[I]);
    Variable(BP, TEXT("Revision"), Type(UEdGraphSchema_K2::PC_Int), TEXT("1"));
}

void BuildSettings()
{
    auto* SaveBP = Blueprint(TEXT("SG_NwiSettings"), false, USaveGame::StaticClass()); AddSettings(SaveBP); Compile(SaveBP); Save(SaveBP);
    auto* BP = CastChecked<UWidgetBlueprint>(Blueprint(TEXT("WBP_NwiSettings"), true));
    Variable(BP, TEXT("Settings"), Type(UEdGraphSchema_K2::PC_Object, SaveBP->GeneratedClass));
    Variable(BP, TEXT("SaveSlot"), Type(UEdGraphSchema_K2::PC_String), TEXT("NormalWaveIndicator_v1"));
    check(FBlueprintEditorUtils::ImplementNewInterface(BP, HubInterface(TEXT("IHubPageWidget"))->GetFName()));
    auto* Info = HubResult(BP, TEXT("GetPageInfo")); GetDefault<UEdGraphSchema_K2>()->TrySetDefaultText(*Pin(Info, TEXT("PageName")), FText::FromString(TEXT("Indicator settings")));
    auto* Mode = HubResult(BP, TEXT("GetContainerMode")); Value(Mode, TEXT("bStretchToContainer"), TEXT("true"));
    auto* Root = BP->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SettingsPanel")); BP->WidgetTree->RootWidget = Root;
    auto AddText = [&](const TCHAR* Name, const TCHAR* Label) {
        auto* T = BP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name); T->SetText(FText::FromString(Label));
        auto Font = T->Font; Font.Size = 16; T->SetFont(Font); Root->AddChildToVerticalBox(T); return T;
    };
    AddText(TEXT("Title"), TEXT("Normal Wave Indicator  |  Host-local alpha 0.6.0"));
    const TCHAR* Labels[] = {TEXT("Warning text (up to 64 characters)"), TEXT("Visibility after the last spawn (seconds, 1 - 30)"), TEXT("Sphere size (0.5 - 12; default 3.75)"), TEXT("Sphere red (linear intensity, 0 - 5)"), TEXT("Sphere green (0 - 5)"), TEXT("Sphere blue (0 - 5)"), TEXT("Gently pulse warning text (1.5 Hz)")};
    for (int32 I = 0; I < 7; ++I)
    {
        AddText(*FString::Printf(TEXT("Caption%d"), I), Labels[I]);
        const FName Name(*FString::Printf(TEXT("Input%s"), SettingNames[I])); UWidget* Input = nullptr;
        if (I == 0) Input = BP->WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), Name);
        else if (I == 6) Input = BP->WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), Name);
        else {
            auto* Spin = BP->WidgetTree->ConstructWidget<USpinBox>(USpinBox::StaticClass(), Name);
            Spin->SetMinValue(I == 1 ? 1.f : I == 2 ? 0.5f : 0.f); Spin->SetMaxValue(I == 1 ? 30.f : I == 2 ? 12.f : 5.f);
            Spin->SetMinSliderValue(I == 1 ? 1.f : I == 2 ? .5f : 0.f); Spin->SetMaxSliderValue(I == 1 ? 30.f : I == 2 ? 12.f : 5.f);
            Spin->SetDelta(.1f); Input = Spin;
        }
        Input->bIsVariable = true; Root->AddChildToVerticalBox(Input);
    }
    auto* Button = BP->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ApplyButton")); Button->bIsVariable = true;
    auto* ButtonText = BP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ApplyText")); ButtonText->SetText(FText::FromString(TEXT("Apply and save"))); Button->AddChild(ButtonText); Root->AddChildToVerticalBox(Button);
    auto* Status = AddText(TEXT("SaveStatus"), TEXT("Distance is straight-line meters. Duration changes apply to new spawn events.")); Status->bIsVariable = true;
    Compile(BP); auto* G = Graph(BP);
    auto* Config = Get(G, TEXT("Settings"));
    auto* Construct = Event(G, UUserWidget::StaticClass(), TEXT("Construct")); auto* Gate = Branch(G, Valid(G, Config, TEXT("Settings")), TEXT("ReturnValue")); Link(Construct, TEXT("then"), Gate, TEXT("execute"));
    UEdGraphNode* Exec = Gate;
    for (int32 I = 0; I < 7; ++I) {
        auto* Input = Get(G, *FString::Printf(TEXT("Input%s"), SettingNames[I]));
        auto* Read = Field(G, SaveBP->GeneratedClass, SettingNames[I], Config, TEXT("Settings"));
        auto* SetInput = Call(G, I == 0 ? UEditableTextBox::StaticClass() : I == 6 ? UCheckBox::StaticClass() : USpinBox::StaticClass(), I == 0 ? TEXT("SetText") : I == 6 ? TEXT("SetIsChecked") : TEXT("SetValue"));
        Link(Input, *FString::Printf(TEXT("Input%s"), SettingNames[I]), SetInput, TEXT("self"));
        if (I == 0) { auto* T = Call(G, UKismetTextLibrary::StaticClass(), TEXT("Conv_StringToText")); Link(Read, SettingNames[I], T, TEXT("InString")); Link(T, TEXT("ReturnValue"), SetInput, TEXT("InText")); }
        else Link(Read, SettingNames[I], SetInput, I == 6 ? TEXT("InIsChecked") : TEXT("NewValue"));
        Link(Exec, TEXT("then"), SetInput, TEXT("execute")); Exec = SetInput;
    }
    auto* Click = Node<UK2Node_ComponentBoundEvent>(G);
    Click->InitializeComponentBoundEventParams(FindFProperty<FObjectProperty>(BP->GeneratedClass, TEXT("ApplyButton")), FindFProperty<FMulticastDelegateProperty>(UButton::StaticClass(), TEXT("OnClicked"))); Click->AllocateDefaultPins();
    auto* ApplyGate = Branch(G, Valid(G, Config, TEXT("Settings")), TEXT("ReturnValue")); Link(Click, TEXT("then"), ApplyGate, TEXT("execute")); Exec = ApplyGate;
    for (int32 I = 0; I < 7; ++I) {
        auto* Input = Get(G, *FString::Printf(TEXT("Input%s"), SettingNames[I]));
        auto* Read = Call(G, I == 0 ? UEditableTextBox::StaticClass() : I == 6 ? UCheckBox::StaticClass() : USpinBox::StaticClass(), I == 0 ? TEXT("GetText") : I == 6 ? TEXT("IsChecked") : TEXT("GetValue"));
        Link(Input, *FString::Printf(TEXT("Input%s"), SettingNames[I]), Read, TEXT("self"));
        UEdGraphNode* Source = Read;
        if (I == 0) {
            auto* T = Call(G, UKismetTextLibrary::StaticClass(), TEXT("Conv_TextToString")); Link(Read, TEXT("ReturnValue"), T, TEXT("InText"));
            auto* Limit = Call(G, UKismetStringLibrary::StaticClass(), TEXT("Left")); Link(T, TEXT("ReturnValue"), Limit, TEXT("SourceString")); Value(Limit, TEXT("Count"), TEXT("64")); Source = Limit;
        }
        auto* Write = WriteField(G, SaveBP->GeneratedClass, SettingNames[I], Config, TEXT("Settings")); Link(Source, TEXT("ReturnValue"), Write, SettingNames[I]); Link(Exec, TEXT("then"), Write, TEXT("execute")); Exec = Write;
    }
    auto* Revision = Field(G, SaveBP->GeneratedClass, TEXT("Revision"), Config, TEXT("Settings")); auto* Increment = Call(G, UKismetMathLibrary::StaticClass(), TEXT("Add_IntInt")); Link(Revision, TEXT("Revision"), Increment, TEXT("A")); Value(Increment, TEXT("B"), TEXT("1"));
    auto* Commit = WriteField(G, SaveBP->GeneratedClass, TEXT("Revision"), Config, TEXT("Settings")); Link(Increment, TEXT("ReturnValue"), Commit, TEXT("Revision")); Link(Exec, TEXT("then"), Commit, TEXT("execute"));
    auto* Store = Call(G, UGameplayStatics::StaticClass(), TEXT("SaveGameToSlot")); Link(Config, TEXT("Settings"), Store, TEXT("SaveGameObject")); Link(Get(G, TEXT("SaveSlot")), TEXT("SaveSlot"), Store, TEXT("SlotName")); Link(Commit, TEXT("then"), Store, TEXT("execute"));
    auto* Message = Call(G, UKismetMathLibrary::StaticClass(), TEXT("SelectString")); Value(Message, TEXT("A"), TEXT("Applied and saved.")); Value(Message, TEXT("B"), TEXT("Applied for this session; disk save failed.")); Link(Store, TEXT("ReturnValue"), Message, TEXT("bPickA"));
    auto* AsText = Call(G, UKismetTextLibrary::StaticClass(), TEXT("Conv_StringToText")); Link(Message, TEXT("ReturnValue"), AsText, TEXT("InString"));
    auto* StatusSet = Call(G, UTextBlock::StaticClass(), TEXT("SetText")); Link(Get(G, TEXT("SaveStatus")), TEXT("SaveStatus"), StatusSet, TEXT("self")); Link(AsText, TEXT("ReturnValue"), StatusSet, TEXT("InText")); Link(Store, TEXT("then"), StatusSet, TEXT("execute"));
    Compile(BP); Save(BP);
}

void AddControllerSettings(UBlueprint* BP)
{
    auto* SaveClass = LoadClass<USaveGame>(nullptr, TEXT("/Game/NormalWaveIndicator/SG_NwiSettings.SG_NwiSettings_C"));
    auto* PageClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/NormalWaveIndicator/WBP_NwiSettings.WBP_NwiSettings_C"));
    Variable(BP, TEXT("Settings"), Type(UEdGraphSchema_K2::PC_Object, SaveClass));
    Variable(BP, TEXT("SettingsPage"), Type(UEdGraphSchema_K2::PC_Object, PageClass));
    Variable(BP, TEXT("AppliedSettingsRevision"), Type(UEdGraphSchema_K2::PC_Int));
    Variable(BP, TEXT("DurationSec"), Type(UEdGraphSchema_K2::PC_Float), TEXT("8"));
    check(FBlueprintEditorUtils::ImplementNewInterface(BP, HubInterface(TEXT("IHubMod"))->GetFName()));
    auto* Info = HubResult(BP, TEXT("GetModInfo"));
    const TCHAR* Names[] = {TEXT("ModName"), TEXT("ModAuthor"), TEXT("ModVersion")};
    const TCHAR* Values[] = {TEXT("Normal Wave Indicator"), TEXT("LostPatrol"), TEXT("0.6.0 alpha")};
    for (int32 I = 0; I < 3; ++I) GetDefault<UEdGraphSchema_K2>()->TrySetDefaultText(*Pin(Info, Names[I]), FText::FromString(Values[I]));
}

void BuildControllerSettings(UBlueprint* BP, UEdGraph* G, UClass* PulseClass, UClass* HudClass)
{
    auto* SaveClass = LoadClass<USaveGame>(nullptr, TEXT("/Game/NormalWaveIndicator/SG_NwiSettings.SG_NwiSettings_C"));
    auto* PageClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/NormalWaveIndicator/WBP_NwiSettings.WBP_NwiSettings_C"));
    auto* Config = Get(G, TEXT("Settings"));
    // One tiny settings file read during world startup, never in spawn or frame callbacks.
    auto* Begin = Event(G, AActor::StaticClass(), TEXT("ReceiveBeginPlay"));
    auto* Parent = Node<UK2Node_CallParentFunction>(G); Parent->SetFromFunction(AActor::StaticClass()->FindFunctionByName(TEXT("ReceiveBeginPlay"))); Parent->AllocateDefaultPins(); Link(Begin, TEXT("then"), Parent, TEXT("execute"));
    auto* Exists = Call(G, UGameplayStatics::StaticClass(), TEXT("DoesSaveGameExist")); Value(Exists, TEXT("SlotName"), TEXT("NormalWaveIndicator_v1")); Link(Parent, TEXT("then"), Exists, TEXT("execute"));
    auto* HasFile = Branch(G, Exists, TEXT("ReturnValue")); Link(Exists, TEXT("then"), HasFile, TEXT("execute"));
    auto* Load = Call(G, UGameplayStatics::StaticClass(), TEXT("LoadGameFromSlot")); Value(Load, TEXT("SlotName"), TEXT("NormalWaveIndicator_v1")); Link(HasFile, TEXT("then"), Load, TEXT("execute"));
    auto* LoadedCast = Node<UK2Node_DynamicCast>(G); LoadedCast->TargetType = SaveClass; LoadedCast->AllocateDefaultPins();
    check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(Pin(Load, TEXT("ReturnValue")), LoadedCast->GetCastSourcePin())); Link(Load, TEXT("then"), LoadedCast, TEXT("execute"));
    auto* Store = Set(G, TEXT("Settings")); check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(LoadedCast->GetCastResultPin(), Pin(Store, TEXT("Settings")))); Link(LoadedCast, TEXT("then"), Store, TEXT("execute"));
    auto* Create = Call(G, UGameplayStatics::StaticClass(), TEXT("CreateSaveGameObject")); Pin(Create, TEXT("SaveGameClass"))->DefaultObject = SaveClass;
    Link(HasFile, TEXT("else"), Create, TEXT("execute")); Link(LoadedCast, TEXT("CastFailed"), Create, TEXT("execute"));
    auto* CastNew = Node<UK2Node_DynamicCast>(G); CastNew->TargetType = SaveClass; CastNew->AllocateDefaultPins(); CastNew->SetPurity(true);
    check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(Pin(Create, TEXT("ReturnValue")), CastNew->GetCastSourcePin()));
    auto* StoreNew = Set(G, TEXT("Settings")); check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(CastNew->GetCastResultPin(), Pin(StoreNew, TEXT("Settings")))); Link(Create, TEXT("then"), StoreNew, TEXT("execute"));

    auto* Result = HubResult(BP, TEXT("GetModPages")); auto* PagesGraph = Result->GetGraph();
    UK2Node_FunctionEntry* Entry = nullptr; for (auto* N : PagesGraph->Nodes) if (auto* E = Cast<UK2Node_FunctionEntry>(N)) Entry = E;
    check(Entry); Pin(Entry, TEXT("then"))->BreakAllPinLinks();
    auto* Page = Get(PagesGraph, TEXT("SettingsPage")); auto* HavePage = Branch(PagesGraph, Valid(PagesGraph, Page, TEXT("SettingsPage")), TEXT("ReturnValue")); Link(Entry, TEXT("then"), HavePage, TEXT("execute")); Link(HavePage, TEXT("then"), Result, TEXT("execute"));
    auto* NewPage = Call(PagesGraph, UWidgetBlueprintLibrary::StaticClass(), TEXT("Create")); Pin(NewPage, TEXT("WidgetType"))->DefaultObject = PageClass;
    auto* Player = Call(PagesGraph, UGameplayStatics::StaticClass(), TEXT("GetPlayerController")); Link(Player, TEXT("ReturnValue"), NewPage, TEXT("OwningPlayer")); Link(HavePage, TEXT("else"), NewPage, TEXT("execute"));
    auto* PageCast = Node<UK2Node_DynamicCast>(PagesGraph); PageCast->TargetType = PageClass; PageCast->AllocateDefaultPins(); PageCast->SetPurity(true);
    check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(Pin(NewPage, TEXT("ReturnValue")), PageCast->GetCastSourcePin()));
    auto* StorePage = Set(PagesGraph, TEXT("SettingsPage")); check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(PageCast->GetCastResultPin(), Pin(StorePage, TEXT("SettingsPage")))); Link(NewPage, TEXT("then"), StorePage, TEXT("execute"));
    auto* PageConfig = WriteField(PagesGraph, PageClass, TEXT("Settings"), Page, TEXT("SettingsPage")); Link(Get(PagesGraph, TEXT("Settings")), TEXT("Settings"), PageConfig, TEXT("Settings")); Link(StorePage, TEXT("then"), PageConfig, TEXT("execute")); Link(PageConfig, TEXT("then"), Result, TEXT("execute"));
    auto* Array = Node<UK2Node_MakeArray>(PagesGraph); Array->AllocateDefaultPins(); Link(Array, TEXT("Array"), Result, TEXT("HubPages")); Link(Page, TEXT("SettingsPage"), Array, TEXT("[0]"));

    // A revision compare is the only per-frame configuration work; edits update the fixed pool once.
    auto* Refresh = Custom(G, TEXT("RefreshSettings"));
    auto* ConfigGate = Branch(G, Valid(G, Config, TEXT("Settings")), TEXT("ReturnValue")); Link(Refresh, TEXT("then"), ConfigGate, TEXT("execute"));
    auto* Revision = Field(G, SaveClass, TEXT("Revision"), Config, TEXT("Settings"));
    auto* Changed = Call(G, UKismetMathLibrary::StaticClass(), TEXT("NotEqual_IntInt")); Link(Revision, TEXT("Revision"), Changed, TEXT("A")); Link(Get(G, TEXT("AppliedSettingsRevision")), TEXT("AppliedSettingsRevision"), Changed, TEXT("B"));
    auto* Gate = Branch(G, Changed, TEXT("ReturnValue")); Link(ConfigGate, TEXT("then"), Gate, TEXT("execute"));
    auto* Commit = Set(G, TEXT("AppliedSettingsRevision")); Link(Revision, TEXT("Revision"), Commit, TEXT("AppliedSettingsRevision")); Link(Gate, TEXT("then"), Commit, TEXT("execute"));
    auto* Duration = Field(G, SaveClass, TEXT("Duration"), Config, TEXT("Settings"));
    auto* ClampTime = Call(G, UKismetMathLibrary::StaticClass(), TEXT("FClamp")); Link(Duration, TEXT("Duration"), ClampTime, TEXT("Value")); Value(ClampTime, TEXT("Min"), TEXT("1")); Value(ClampTime, TEXT("Max"), TEXT("30"));
    auto* CacheTime = Set(G, TEXT("DurationSec")); Link(ClampTime, TEXT("ReturnValue"), CacheTime, TEXT("DurationSec")); Link(Commit, TEXT("then"), CacheTime, TEXT("execute")); UEdGraphNode* Exec = CacheTime;
    auto* Tint = Call(G, UKismetMathLibrary::StaticClass(), TEXT("MakeColor"));
    for (int32 I = 3; I < 6; ++I) {
        auto* Channel = Field(G, SaveClass, SettingNames[I], Config, TEXT("Settings")); auto* Clamp = Call(G, UKismetMathLibrary::StaticClass(), TEXT("FClamp")); Link(Channel, SettingNames[I], Clamp, TEXT("Value")); Value(Clamp, TEXT("Max"), TEXT("5")); Link(Clamp, TEXT("ReturnValue"), Tint, I == 3 ? TEXT("R") : I == 4 ? TEXT("G") : TEXT("B"));
    }
    Value(Tint, TEXT("A"), TEXT("1"));
    for (int32 I = 0; I < 8; ++I) {
        const FString H = FString::Printf(TEXT("AutoHud%d"), I), P = FString::Printf(TEXT("AutoPulse%d"), I);
        auto* Hud = Get(G, *H); auto* Pulse = Get(G, *P);
        auto* Label = Field(G, SaveClass, TEXT("Label"), Config, TEXT("Settings")); auto* AsText = Call(G, UKismetTextLibrary::StaticClass(), TEXT("Conv_StringToText")); Link(Label, TEXT("Label"), AsText, TEXT("InString"));
        auto* SetLabel = Call(G, HudClass, TEXT("SetMarkerLabel")); Link(Hud, *H, SetLabel, TEXT("self")); Link(AsText, TEXT("ReturnValue"), SetLabel, TEXT("Label")); Link(Exec, TEXT("then"), SetLabel, TEXT("execute"));
        auto* Blink = WriteField(G, HudClass, TEXT("BlinkEnabled"), Hud, *H); Link(Field(G, SaveClass, TEXT("Blink"), Config, TEXT("Settings")), TEXT("Blink"), Blink, TEXT("BlinkEnabled")); Link(SetLabel, TEXT("then"), Blink, TEXT("execute"));
        auto* Radius = WriteField(G, PulseClass, TEXT("RadiusScale"), Pulse, *P);
        auto* Limit = Call(G, UKismetMathLibrary::StaticClass(), TEXT("FClamp")); Link(Field(G, SaveClass, TEXT("Radius"), Config, TEXT("Settings")), TEXT("Radius"), Limit, TEXT("Value")); Value(Limit, TEXT("Min"), TEXT("0.5")); Value(Limit, TEXT("Max"), TEXT("12")); Link(Limit, TEXT("ReturnValue"), Radius, TEXT("RadiusScale")); Link(Blink, TEXT("then"), Radius, TEXT("execute"));
        auto* Material = Field(G, PulseClass, TEXT("PulseMaterial"), Pulse, *P); auto* Color = Call(G, UMaterialInstanceDynamic::StaticClass(), TEXT("SetVectorParameterValue")); Link(Material, TEXT("PulseMaterial"), Color, TEXT("self")); Value(Color, TEXT("ParameterName"), TEXT("Tint")); Link(Tint, TEXT("ReturnValue"), Color, TEXT("Value")); Link(Radius, TEXT("then"), Color, TEXT("execute")); Exec = Color;
    }
}
