// Shared formatting keeps custom prefix and per-region event context together.
UK2Node_CallFunction* UpdateRegionLabel(UEdGraph* G, UClass* HudClass, int32 I);
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

const TCHAR* SettingNames[] = {TEXT("Label"), TEXT("Duration"), TEXT("Radius"), TEXT("Red"), TEXT("Green"), TEXT("Blue"), TEXT("Blink"), TEXT("Opacity"), TEXT("BlinkHz"), TEXT("TextAR"), TEXT("TextAG"), TEXT("TextAB"), TEXT("TextBR"), TEXT("TextBG"), TEXT("TextBB"), TEXT("NaturalEnabled")};
// Marker defaults are deliberately English in every culture; only their settings captions are localized.
const TCHAR* SettingDefaults[] = {TEXT("[!] NATURAL WAVE"), TEXT("8"), TEXT("3.75"), TEXT("1"), TEXT("0.01"), TEXT("0.005"), TEXT("true"), TEXT("0.4"), TEXT("2"), TEXT("1"), TEXT("1"), TEXT("0"), TEXT("1"), TEXT("0"), TEXT("0"), TEXT("true")};
const TCHAR* SettingLabelsEn[] = {
    TEXT("Natural marker text"), TEXT("Duration (s)"), TEXT("Size"), TEXT("Sphere R"), TEXT("Sphere G"), TEXT("Sphere B"), TEXT("Flash A / B"),
    TEXT("Opacity"), TEXT("Flash rate (Hz)"), TEXT("Text A · R"), TEXT("Text A · G"), TEXT("Text A · B"), TEXT("Text B · R"), TEXT("Text B · G"), TEXT("Text B · B"), TEXT("Natural waves")
};
const TCHAR* SettingLabelsZhCn[] = {
    TEXT("自然潮标记文字"), TEXT("显示时长（秒）"), TEXT("尺寸"), TEXT("球体 R"), TEXT("球体 G"), TEXT("球体 B"), TEXT("闪烁 A / B"),
    TEXT("不透明度"), TEXT("闪烁频率（Hz）"), TEXT("文字 A · R"), TEXT("文字 A · G"), TEXT("文字 A · B"), TEXT("文字 B · R"), TEXT("文字 B · G"), TEXT("文字 B · B"), TEXT("自然潮")
};
// These names mirror docs/WAVE-TYPES.md and are UI captions, never marker defaults.
const TCHAR* WaveTitlesZhCn[] = {
    TEXT("自然潮"), TEXT("深掘钻梯"), TEXT("虫蛋伏击"), TEXT("常规撤离"), TEXT("定点提取撤离"), TEXT("教程撤离"),
    TEXT("执勤护送：钻进"), TEXT("执勤护送：心石防守"), TEXT("执勤护送：撤离"), TEXT("执勤护送：补充燃料"),
    TEXT("重型采掘：挖掘"), TEXT("重型采掘：升空"), TEXT("播报潮／通用潮"), TEXT("啤酒节伏击"), TEXT("设施破坏：发电站"),
    TEXT("噬岩体陨石防守"), TEXT("就地精炼：恒压潮"), TEXT("就地精炼：炼油虫潮"), TEXT("就地精炼：管道故障"), TEXT("就地精炼：撤离"),
    TEXT("特殊虫潮：战士"), TEXT("特殊虫潮：异虫蝇"), TEXT("特殊虫潮：岩痘"), TEXT("特殊虫潮：禁卫"), TEXT("特殊虫潮：蜂拥"),
    TEXT("搜救行动：矿骡伏击"), TEXT("搜救行动：据点防守"), TEXT("搜救行动：撤离"), TEXT("设施破坏：无人机"), TEXT("无畏异虫潮"),
    TEXT("定点提取压力潮"), TEXT("教程战士潮"), TEXT("核心岩事件"), TEXT("强敌科技通讯事件"), TEXT("核心侵扰警告"), TEXT("骇入防守"),
    TEXT("三提石矿藏"), TEXT("矿化爆发（机械事件）"), TEXT("氪石感染"), TEXT("蜂拥浩劫"), TEXT("自爆群袭"),
    TEXT("掠痕集居"), TEXT("凝血化糖"), TEXT("强敌环伺"), TEXT("矿化爆发（任务警告）"), TEXT("噬岩体爆发"), TEXT("诡异洞穴")
};
static_assert(sizeof(SettingLabelsEn) / sizeof(SettingLabelsEn[0]) == 16, "English settings caption count changed");
static_assert(sizeof(SettingLabelsZhCn) / sizeof(SettingLabelsZhCn[0]) == 16, "Chinese settings caption count changed");
static_assert(sizeof(WaveTitlesZhCn) / sizeof(WaveTitlesZhCn[0]) == nwi::WaveTypeCount, "Chinese wave title count changed");
constexpr int32 SettingCount = 16 + 2 * (nwi::WaveTypeCount - 1);
// Explicit typography avoids UMG's large default font reversing the visual hierarchy in game.
constexpr int32 PageTitleFontSize = 21;
constexpr int32 SectionTitleFontSize = 19;
constexpr int32 BodyFontSize = 13;
constexpr int32 PreviewFontSize = 18;
constexpr int32 ButtonFontSize = 16;
FString SettingName(int32 I) { return I < 16 ? FString(SettingNames[I]) : (I % 2 == 0 ? FString::Printf(TEXT("EnabledType%d"), (I-16)/2+1) : FString::Printf(TEXT("LabelType%d"), (I-16)/2+1)); }
// 0.9.4 reports every catalogued source except the two drilling phases, both Core sources, and Haunted Cave.
bool DefaultWaveEnabled(int32 Type) { return Type != 1 && Type != 6 && Type != 32 && Type != 34 && Type != 46; }
FString SettingDefault(int32 I) { return I < 16 ? FString(SettingDefaults[I]) : I % 2 == 0 ? FString(DefaultWaveEnabled((I-16)/2+1) ? TEXT("true") : TEXT("false")) : FString(TEXT("[!] ")) + nwi::WaveTypes[(I-16)/2+1].title; }
bool IsTextSetting(int32 I) { return I == 0 || (I >= 16 && I % 2 == 1); }
bool IsBoolSetting(int32 I) { return I == 6 || I == 15 || (I >= 16 && I % 2 == 0); }
float NumericMinimum(int32 I) { return I == 1 ? 1.f : I == 2 ? .5f : I == 8 ? .1f : 0.f; }
float NumericMaximum(int32 I) { return I == 1 ? 30.f : I == 2 ? 12.f : I == 8 ? 10.f : 1.f; }
UK2Node_CallFunction* ClampNumericSetting(UEdGraph* G, UEdGraphNode* Source, const TCHAR* SourceOutput, int32 I)
{
    auto* Clamp = Call(G, UKismetMathLibrary::StaticClass(), TEXT("FClamp"));
    Link(Source, SourceOutput, Clamp, TEXT("Value"));
    Value(Clamp, TEXT("Min"), *FString::SanitizeFloat(NumericMinimum(I)));
    Value(Clamp, TEXT("Max"), *FString::SanitizeFloat(NumericMaximum(I)));
    return Clamp;
}
FString SettingCaption(int32 I, bool Chinese)
{
    if (I < 16) return Chinese ? FString(SettingLabelsZhCn[I]) : FString(SettingLabelsEn[I]);
    const int32 Type = (I - 16) / 2 + 1;
    if (Chinese) return I % 2 == 0 ? FString(TEXT("显示")) + WaveTitlesZhCn[Type] : FString(WaveTitlesZhCn[Type]) + TEXT("提示文字（最多 64 个字符）");
    return I % 2 == 0 ? FString(TEXT("Show ")) + nwi::WaveTypes[Type].title : FString(nwi::WaveTypes[Type].title) + TEXT(" text (up to 64 characters)");
}

// DRG's Simplified Chinese localization currently reports zh-CN; zh-Hans covers canonical aliases.
UK2Node_CallFunction* IsSimplifiedChinese(UEdGraph* G)
{
    auto* Language = Call(G, UKismetInternationalizationLibrary::StaticClass(), TEXT("GetCurrentLanguage"));
    auto Starts = [&](const TCHAR* Prefix) {
        auto* Result = Call(G, UKismetStringLibrary::StaticClass(), TEXT("StartsWith"));
        Link(Language, TEXT("ReturnValue"), Result, TEXT("SourceString")); Value(Result, TEXT("InPrefix"), Prefix); return Result;
    };
    auto* ZhCn = Starts(TEXT("zh-CN")); auto* ZhHans = Starts(TEXT("zh-Hans"));
    auto* Either = Call(G, UKismetMathLibrary::StaticClass(), TEXT("BooleanOR"));
    Link(ZhCn, TEXT("ReturnValue"), Either, TEXT("A")); Link(ZhHans, TEXT("ReturnValue"), Either, TEXT("B")); return Either;
}

UK2Node_CallFunction* LocalizedText(UEdGraph* G, UEdGraphNode* Chinese, const FString& English, const FString& ZhCn)
{
    auto* Select = Call(G, UKismetMathLibrary::StaticClass(), TEXT("SelectString"));
    Value(Select, TEXT("A"), *ZhCn); Value(Select, TEXT("B"), *English); Link(Chinese, TEXT("ReturnValue"), Select, TEXT("bPickA"));
    auto* Text = Call(G, UKismetTextLibrary::StaticClass(), TEXT("Conv_StringToText")); Link(Select, TEXT("ReturnValue"), Text, TEXT("InString")); return Text;
}

UK2Node_CallFunction* SetLocalizedWidgetText(UEdGraph* G, UEdGraphNode* Chinese, const TCHAR* Widget, const FString& English, const FString& ZhCn)
{
    auto* SetText = Call(G, UTextBlock::StaticClass(), TEXT("SetText"));
    Link(Get(G, Widget), Widget, SetText, TEXT("self")); Link(LocalizedText(G, Chinese, English, ZhCn), TEXT("ReturnValue"), SetText, TEXT("InText")); return SetText;
}
void AddSettings(UBlueprint* BP)
{
    for (int32 I = 0; I < SettingCount; ++I) Variable(BP, *SettingName(I), Type(IsTextSetting(I) ? UEdGraphSchema_K2::PC_String : IsBoolSetting(I) ? UEdGraphSchema_K2::PC_Boolean : UEdGraphSchema_K2::PC_Float), *SettingDefault(I));
    Variable(BP, TEXT("Revision"), Type(UEdGraphSchema_K2::PC_Int), TEXT("1"));
}

void BuildSettings()
{
    auto* SaveBP = Blueprint(TEXT("SG_NwiSettings"), false, USaveGame::StaticClass()); AddSettings(SaveBP); Compile(SaveBP); Save(SaveBP);
    auto* BP = CastChecked<UWidgetBlueprint>(Blueprint(TEXT("WBP_NwiSettings"), true));
    Variable(BP, TEXT("Settings"), Type(UEdGraphSchema_K2::PC_Object, SaveBP->GeneratedClass));
    Variable(BP, TEXT("SaveSlot"), Type(UEdGraphSchema_K2::PC_String), TEXT("EnemyWaveIndicator_v1"));
    check(FBlueprintEditorUtils::ImplementNewInterface(BP, HubInterface(TEXT("IHubPageWidget"))->GetFName()));
    auto* Info = HubResult(BP, TEXT("GetPageInfo"));
    // Mod Hub consumes page metadata during discovery; keep this pure interface output constant.
    GetDefault<UEdGraphSchema_K2>()->TrySetDefaultText(*Pin(Info, TEXT("PageName")), FText::FromString(TEXT("Indicator settings")));
    auto* Mode = HubResult(BP, TEXT("GetContainerMode")); Value(Mode, TEXT("bStretchToContainer"), TEXT("true"));
    auto* Root = BP->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SettingsPanel")); auto* Scroll=BP->WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(),TEXT("SettingsScroll"));Scroll->AddChild(Root);auto* Frame=BP->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("SettingsFrame"));BP->WidgetTree->RootWidget=Frame;
    auto SetFontSize = [](UTextBlock* Text, int32 Size) {
        auto Font = Text->Font; Font.Size = Size; Text->SetFont(Font);
    };
    auto AddText = [&](const TCHAR* Name, const TCHAR* Label, UVerticalBox* Parent = nullptr, int32 Size = BodyFontSize) {
        auto* T = BP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name); T->SetText(FText::FromString(Label));
        SetFontSize(T, Size); T->bIsVariable = true; (Parent ? Parent : Root)->AddChildToVerticalBox(T); return T;
    };
    auto AddInput = [&](int32 I) {
        const FName Name(*FString::Printf(TEXT("Input%s"), *SettingName(I))); UWidget* Input = nullptr;
        if (IsTextSetting(I)) Input = BP->WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), Name);
        else if (IsBoolSetting(I)) Input = BP->WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), Name);
        else {
            auto* Spin = BP->WidgetTree->ConstructWidget<USpinBox>(USpinBox::StaticClass(), Name);
            Spin->SetMinValue(NumericMinimum(I)); Spin->SetMaxValue(NumericMaximum(I));
            Spin->SetMinSliderValue(NumericMinimum(I)); Spin->SetMaxSliderValue(NumericMaximum(I));
            Spin->SetDelta(.1f); Input = Spin;
        }
        Input->bIsVariable = true; return Input;
    };
    auto GridAdd = [](UGridPanel* Grid, UWidget* Widget, int32 Row, int32 Column, bool Fill = false) {
        auto* Slot = Grid->AddChildToGrid(Widget, Row, Column); Slot->SetPadding(FMargin(4.f, 2.f));
        Slot->SetVerticalAlignment(VAlign_Center); Slot->SetHorizontalAlignment(Fill ? HAlign_Fill : HAlign_Left);
    };
    AddText(TEXT("Title"), TEXT("Enemy Wave Indicator  |  0.9.4"), nullptr, PageTitleFontSize);
    AddText(TEXT("WaveSection"), TEXT("Wave broadcasts"), nullptr, SectionTitleFontSize);
    auto* WaveGrid = BP->WidgetTree->ConstructWidget<UGridPanel>(UGridPanel::StaticClass(), TEXT("WaveGrid")); WaveGrid->bIsVariable = true; Root->AddChildToVerticalBox(WaveGrid);
    WaveGrid->SetColumnFill(2, 1.f); WaveGrid->SetColumnFill(5, 1.f);
    // Two columns keep every independent switch and marker text visible without wasting a full row per control.
    constexpr int32 RowsPerColumn = (nwi::WaveTypeCount + 1) / 2;
    for (int32 Wave = 0; Wave < nwi::WaveTypeCount; ++Wave) {
        const int32 Enable = Wave == 0 ? 15 : 16 + 2 * (Wave - 1); const int32 Label = Wave == 0 ? 0 : Enable + 1;
        const int32 Row = Wave % RowsPerColumn; const int32 Column = (Wave / RowsPerColumn) * 3;
        GridAdd(WaveGrid, AddInput(Enable), Row, Column);
        auto* Name = BP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("WaveName%d"), Wave)); Name->bIsVariable = true; Name->SetText(FText::FromString(nwi::WaveTypes[Wave].title)); SetFontSize(Name, BodyFontSize); GridAdd(WaveGrid, Name, Row, Column + 1);
        GridAdd(WaveGrid, AddInput(Label), Row, Column + 2, true);
    }
    auto AddControl = [&](UGridPanel* Grid, int32 I, int32 Row, int32 Pair) {
        auto* Caption = BP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("Caption%d"), I)); Caption->bIsVariable = true; Caption->SetText(FText::FromString(SettingCaption(I, false))); SetFontSize(Caption, BodyFontSize);
        GridAdd(Grid, Caption, Row, Pair * 2); GridAdd(Grid, AddInput(I), Row, Pair * 2 + 1, true); Grid->SetColumnFill(Pair * 2 + 1, 1.f);
    };
    AddText(TEXT("TextSection"), TEXT("Warning text"), nullptr, SectionTitleFontSize);
    auto* TextGrid = BP->WidgetTree->ConstructWidget<UGridPanel>(UGridPanel::StaticClass(), TEXT("TextGrid")); TextGrid->bIsVariable = true; Root->AddChildToVerticalBox(TextGrid);
    AddControl(TextGrid,6,0,0); AddControl(TextGrid,8,0,1);
    AddControl(TextGrid,9,1,0); AddControl(TextGrid,10,1,1); AddControl(TextGrid,11,1,2);
    AddControl(TextGrid,12,2,0); AddControl(TextGrid,13,2,1); AddControl(TextGrid,14,2,2);
    AddText(TEXT("PreviewCaption"),TEXT("Live text preview"));
    auto* Preview=BP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("TextPreview"));Preview->SetText(FText::FromString(TEXT("[!] NATURAL WAVE")));Preview->bIsVariable=true;SetFontSize(Preview,PreviewFontSize);Root->AddChildToVerticalBox(Preview);
    AddText(TEXT("SphereSection"), TEXT("Warning sphere"), nullptr, SectionTitleFontSize);
    auto* SphereGrid = BP->WidgetTree->ConstructWidget<UGridPanel>(UGridPanel::StaticClass(), TEXT("SphereGrid")); SphereGrid->bIsVariable = true; Root->AddChildToVerticalBox(SphereGrid);
    AddControl(SphereGrid,1,0,0); AddControl(SphereGrid,2,0,1);
    AddControl(SphereGrid,3,1,0); AddControl(SphereGrid,4,1,1); AddControl(SphereGrid,5,1,2); AddControl(SphereGrid,7,1,3);
    auto* Swatch=BP->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),TEXT("SpherePreview"));Swatch->bIsVariable=true;Swatch->Brush.ImageSize=FVector2D(160,36);Root->AddChildToVerticalBox(Swatch);
    auto* ScrollSlot=Frame->AddChildToVerticalBox(Scroll);ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto* Button = BP->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ApplyButton")); Button->bIsVariable = true;
    auto* ButtonText = BP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ApplyText")); ButtonText->SetText(FText::FromString(TEXT("Apply and save"))); ButtonText->bIsVariable = true; SetFontSize(ButtonText, ButtonFontSize); Button->AddChild(ButtonText); Frame->AddChildToVerticalBox(Button);
    auto* Status = AddText(TEXT("SaveStatus"), TEXT("")); Status->bIsVariable = true;
    Compile(BP); auto* G = Graph(BP);
    auto* Config = Get(G, TEXT("Settings"));
    auto* Construct = Event(G, UUserWidget::StaticClass(), TEXT("Construct")); auto* Gate = Branch(G, Valid(G, Config, TEXT("Settings")), TEXT("ReturnValue")); Link(Construct, TEXT("then"), Gate, TEXT("execute"));
    UEdGraphNode* Exec = Gate;
    auto* Chinese = IsSimplifiedChinese(G);
    auto Localize = [&](const TCHAR* Widget, const FString& English, const FString& ZhCn) {
        auto* SetText = SetLocalizedWidgetText(G, Chinese, Widget, English, ZhCn); Link(Exec, TEXT("then"), SetText, TEXT("execute")); Exec = SetText;
    };
    Localize(TEXT("Title"), TEXT("Enemy Wave Indicator  |  0.9.4"), TEXT("敌潮指示器  |  0.9.4"));
    Localize(TEXT("WaveSection"), TEXT("Wave broadcasts"), TEXT("虫潮播报"));
    for (int32 Wave = 0; Wave < nwi::WaveTypeCount; ++Wave) Localize(*FString::Printf(TEXT("WaveName%d"), Wave), nwi::WaveTypes[Wave].title, WaveTitlesZhCn[Wave]);
    Localize(TEXT("TextSection"), TEXT("Warning text"), TEXT("播报警示文本"));
    for (int32 I : {6,8,9,10,11,12,13,14}) Localize(*FString::Printf(TEXT("Caption%d"), I), SettingCaption(I, false), SettingCaption(I, true));
    Localize(TEXT("PreviewCaption"), TEXT("Live text preview"), TEXT("实时文字预览"));
    Localize(TEXT("SphereSection"), TEXT("Warning sphere"), TEXT("警示球体"));
    for (int32 I : {1,2,3,4,5,7}) Localize(*FString::Printf(TEXT("Caption%d"), I), SettingCaption(I, false), SettingCaption(I, true));
    Localize(TEXT("ApplyText"), TEXT("Apply and save"), TEXT("应用并保存"));
    for (int32 I = 0; I < SettingCount; ++I) {
        auto* Input = Get(G, *FString::Printf(TEXT("Input%s"), *SettingName(I)));
        auto* Read = Field(G, SaveBP->GeneratedClass, *SettingName(I), Config, TEXT("Settings"));
        auto* SetInput = Call(G, IsTextSetting(I) ? UEditableTextBox::StaticClass() : IsBoolSetting(I) ? UCheckBox::StaticClass() : USpinBox::StaticClass(), IsTextSetting(I) ? TEXT("SetText") : IsBoolSetting(I) ? TEXT("SetIsChecked") : TEXT("SetValue"));
        Link(Input, *FString::Printf(TEXT("Input%s"), *SettingName(I)), SetInput, TEXT("self"));
        if (IsTextSetting(I)) { auto* T = Call(G, UKismetTextLibrary::StaticClass(), TEXT("Conv_StringToText")); Link(Read, *SettingName(I), T, TEXT("InString")); Link(T, TEXT("ReturnValue"), SetInput, TEXT("InText")); }
        else if (IsBoolSetting(I)) Link(Read, *SettingName(I), SetInput, TEXT("InIsChecked"));
        else Link(ClampNumericSetting(G, Read, *SettingName(I), I), TEXT("ReturnValue"), SetInput, TEXT("NewValue"));
        Link(Exec, TEXT("then"), SetInput, TEXT("execute")); Exec = SetInput;
    }
    // Preview reads draft controls every widget tick, without saving or changing gameplay.
    auto Draft=[&](int32 I) { auto* R=Call(G,USpinBox::StaticClass(),TEXT("GetValue"));const auto N=FString::Printf(TEXT("Input%s"),*SettingName(I));Link(Get(G,*N),*N,R,TEXT("self"));return R; };
    auto DraftColor=[&](int32 First) { auto* C=Call(G,UKismetMathLibrary::StaticClass(),TEXT("MakeColor"));for(int32 J=0;J<3;++J)Link(Draft(First+J),TEXT("ReturnValue"),C,J==0?TEXT("R"):J==1?TEXT("G"):TEXT("B"));Value(C,TEXT("A"),TEXT("1"));return C; };
    auto* Tick=Event(G,UUserWidget::StaticClass(),TEXT("Tick"));
    auto* DraftTint=DraftColor(3);Link(Draft(7),TEXT("ReturnValue"),DraftTint,TEXT("A"));
    auto* SwatchColor=Call(G,UImage::StaticClass(),TEXT("SetColorAndOpacity"));Link(Get(G,TEXT("SpherePreview")),TEXT("SpherePreview"),SwatchColor,TEXT("self"));Link(DraftTint,TEXT("ReturnValue"),SwatchColor,TEXT("InColorAndOpacity"));Link(Tick,TEXT("then"),SwatchColor,TEXT("execute"));
    auto* DraftBlink=Call(G,UCheckBox::StaticClass(),TEXT("IsChecked"));Link(Get(G,TEXT("InputBlink")),TEXT("InputBlink"),DraftBlink,TEXT("self"));
    auto* Paint=PaintWarning(G,Get(G,TEXT("TextPreview")),TEXT("TextPreview"),DraftColor(9),TEXT("ReturnValue"),DraftColor(12),TEXT("ReturnValue"),Draft(8),TEXT("ReturnValue"),DraftBlink,TEXT("ReturnValue"));Link(SwatchColor,TEXT("then"),Paint,TEXT("execute"));
    auto* Click = Node<UK2Node_ComponentBoundEvent>(G);
    Click->InitializeComponentBoundEventParams(FindFProperty<FObjectProperty>(BP->GeneratedClass, TEXT("ApplyButton")), FindFProperty<FMulticastDelegateProperty>(UButton::StaticClass(), TEXT("OnClicked"))); Click->AllocateDefaultPins();
    auto* ApplyGate = Branch(G, Valid(G, Config, TEXT("Settings")), TEXT("ReturnValue")); Link(Click, TEXT("then"), ApplyGate, TEXT("execute")); Exec = ApplyGate;
    for (int32 I = 0; I < SettingCount; ++I) {
        auto* Input = Get(G, *FString::Printf(TEXT("Input%s"), *SettingName(I)));
        auto* Read = Call(G, IsTextSetting(I) ? UEditableTextBox::StaticClass() : IsBoolSetting(I) ? UCheckBox::StaticClass() : USpinBox::StaticClass(), IsTextSetting(I) ? TEXT("GetText") : IsBoolSetting(I) ? TEXT("IsChecked") : TEXT("GetValue"));
        Link(Input, *FString::Printf(TEXT("Input%s"), *SettingName(I)), Read, TEXT("self"));
        UEdGraphNode* Source = Read;
        if (IsTextSetting(I)) {
            auto* T = Call(G, UKismetTextLibrary::StaticClass(), TEXT("Conv_TextToString")); Link(Read, TEXT("ReturnValue"), T, TEXT("InText"));
            auto* Limit = Call(G, UKismetStringLibrary::StaticClass(), TEXT("Left")); Link(T, TEXT("ReturnValue"), Limit, TEXT("SourceString")); Value(Limit, TEXT("Count"), TEXT("64")); Source = Limit;
        }
        else if (!IsBoolSetting(I)) Source = ClampNumericSetting(G, Read, TEXT("ReturnValue"), I);
        auto* Write = WriteField(G, SaveBP->GeneratedClass, *SettingName(I), Config, TEXT("Settings")); Link(Source, TEXT("ReturnValue"), Write, *SettingName(I)); Link(Exec, TEXT("then"), Write, TEXT("execute")); Exec = Write;
    }
    auto* Revision = Field(G, SaveBP->GeneratedClass, TEXT("Revision"), Config, TEXT("Settings")); auto* Increment = Call(G, UKismetMathLibrary::StaticClass(), TEXT("Add_IntInt")); Link(Revision, TEXT("Revision"), Increment, TEXT("A")); Value(Increment, TEXT("B"), TEXT("1"));
    auto* Commit = WriteField(G, SaveBP->GeneratedClass, TEXT("Revision"), Config, TEXT("Settings")); Link(Increment, TEXT("ReturnValue"), Commit, TEXT("Revision")); Link(Exec, TEXT("then"), Commit, TEXT("execute"));
    auto* Store = Call(G, UGameplayStatics::StaticClass(), TEXT("SaveGameToSlot")); Link(Config, TEXT("Settings"), Store, TEXT("SaveGameObject")); Link(Get(G, TEXT("SaveSlot")), TEXT("SaveSlot"), Store, TEXT("SlotName")); Link(Commit, TEXT("then"), Store, TEXT("execute"));
    auto* MessageEn = Call(G, UKismetMathLibrary::StaticClass(), TEXT("SelectString")); Value(MessageEn, TEXT("A"), TEXT("Applied and saved.")); Value(MessageEn, TEXT("B"), TEXT("Applied for this session; disk save failed.")); Link(Store, TEXT("ReturnValue"), MessageEn, TEXT("bPickA"));
    auto* MessageZh = Call(G, UKismetMathLibrary::StaticClass(), TEXT("SelectString")); Value(MessageZh, TEXT("A"), TEXT("已应用并保存。")); Value(MessageZh, TEXT("B"), TEXT("已在当前会话中应用，但保存到磁盘失败。")); Link(Store, TEXT("ReturnValue"), MessageZh, TEXT("bPickA"));
    auto* Message = Call(G, UKismetMathLibrary::StaticClass(), TEXT("SelectString")); Link(MessageZh, TEXT("ReturnValue"), Message, TEXT("A")); Link(MessageEn, TEXT("ReturnValue"), Message, TEXT("B")); Link(Chinese, TEXT("ReturnValue"), Message, TEXT("bPickA"));
    auto* AsText = Call(G, UKismetTextLibrary::StaticClass(), TEXT("Conv_StringToText")); Link(Message, TEXT("ReturnValue"), AsText, TEXT("InString"));
    auto* StatusSet = Call(G, UTextBlock::StaticClass(), TEXT("SetText")); Link(Get(G, TEXT("SaveStatus")), TEXT("SaveStatus"), StatusSet, TEXT("self")); Link(AsText, TEXT("ReturnValue"), StatusSet, TEXT("InText")); Link(Store, TEXT("then"), StatusSet, TEXT("execute"));
    Compile(BP); Save(BP);
}

void AddControllerSettings(UBlueprint* BP)
{
    auto* SaveClass = LoadClass<USaveGame>(nullptr, TEXT("/Game/EnemyWaveIndicator/SG_NwiSettings.SG_NwiSettings_C"));
    auto* PageClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/EnemyWaveIndicator/WBP_NwiSettings.WBP_NwiSettings_C"));
    Variable(BP, TEXT("Settings"), Type(UEdGraphSchema_K2::PC_Object, SaveClass));
    Variable(BP, TEXT("SettingsPage"), Type(UEdGraphSchema_K2::PC_Object, PageClass));
    Variable(BP, TEXT("AppliedSettingsRevision"), Type(UEdGraphSchema_K2::PC_Int));
    Variable(BP, TEXT("DurationSec"), Type(UEdGraphSchema_K2::PC_Float), TEXT("8"));
    check(FBlueprintEditorUtils::ImplementNewInterface(BP, HubInterface(TEXT("IHubMod"))->GetFName()));
    auto* Info = HubResult(BP, TEXT("GetModInfo"));
    // Match the last working registration contract: no runtime calls inside GetModInfo.
    const TCHAR* Names[] = {TEXT("ModName"), TEXT("ModAuthor"), TEXT("ModVersion")};
    const TCHAR* Values[] = {TEXT("Enemy Wave Indicator"), TEXT("LostPatrol"), TEXT("0.9.4 test")};
    for (int32 I = 0; I < 3; ++I)
        GetDefault<UEdGraphSchema_K2>()->TrySetDefaultText(*Pin(Info, Names[I]), FText::FromString(Values[I]));
}

void BuildControllerSettings(UBlueprint* BP, UEdGraph* G, UClass* PulseClass, UClass* HudClass)
{
    auto* SaveClass = LoadClass<USaveGame>(nullptr, TEXT("/Game/EnemyWaveIndicator/SG_NwiSettings.SG_NwiSettings_C"));
    auto* PageClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/EnemyWaveIndicator/WBP_NwiSettings.WBP_NwiSettings_C"));
    auto* Config = Get(G, TEXT("Settings"));
    // One tiny settings file read during world startup, never in spawn or frame callbacks.
    auto* Begin = Event(G, AActor::StaticClass(), TEXT("ReceiveBeginPlay"));
    auto* Parent = Node<UK2Node_CallParentFunction>(G); Parent->SetFromFunction(AActor::StaticClass()->FindFunctionByName(TEXT("ReceiveBeginPlay"))); Parent->AllocateDefaultPins(); Link(Begin, TEXT("then"), Parent, TEXT("execute"));
    auto* Prepare = Call(G, BP->ParentClass, TEXT("PrepareResources")); Link(Parent, TEXT("then"), Prepare, TEXT("execute"));
    auto* Exists = Call(G, UGameplayStatics::StaticClass(), TEXT("DoesSaveGameExist")); Value(Exists, TEXT("SlotName"), TEXT("EnemyWaveIndicator_v1")); Link(Prepare, TEXT("then"), Exists, TEXT("execute"));
    auto* HasFile = Branch(G, Exists, TEXT("ReturnValue")); Link(Exists, TEXT("then"), HasFile, TEXT("execute"));
    auto* Load = Call(G, UGameplayStatics::StaticClass(), TEXT("LoadGameFromSlot")); Value(Load, TEXT("SlotName"), TEXT("EnemyWaveIndicator_v1")); Link(HasFile, TEXT("then"), Load, TEXT("execute"));
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
        auto* Channel = Field(G, SaveClass, *SettingName(I), Config, TEXT("Settings")); auto* Clamp = Call(G, UKismetMathLibrary::StaticClass(), TEXT("FClamp")); Link(Channel, *SettingName(I), Clamp, TEXT("Value")); Value(Clamp, TEXT("Max"), TEXT("1")); Link(Clamp, TEXT("ReturnValue"), Tint, I == 3 ? TEXT("R") : I == 4 ? TEXT("G") : TEXT("B"));
    }
    Value(Tint, TEXT("A"), TEXT("1"));
    auto* Natural=Call(G,UKismetMathLibrary::StaticClass(),TEXT("SelectInt"));Value(Natural,TEXT("A"),TEXT("1"));Value(Natural,TEXT("B"),TEXT("0"));Link(Field(G,SaveClass,TEXT("NaturalEnabled"),Config,TEXT("Settings")),TEXT("NaturalEnabled"),Natural,TEXT("bPickA"));
    auto* Enabled=Set(G,TEXT("NativeEnabled"));Link(Natural,TEXT("ReturnValue"),Enabled,TEXT("NativeEnabled"));Link(Exec,TEXT("then"),Enabled,TEXT("execute"));Exec=Enabled;
    for (uint32 I=1; I<nwi::WaveTypeCount; ++I) {
        const auto FieldName=FString::Printf(TEXT("EnabledType%u"),I), NativeName=FString::Printf(TEXT("NativeEnabled%u"),I);
        auto* Number=Call(G,UKismetMathLibrary::StaticClass(),TEXT("SelectInt"));Value(Number,TEXT("A"),TEXT("1"));Value(Number,TEXT("B"),TEXT("0"));Link(Field(G,SaveClass,*FieldName,Config,TEXT("Settings")),*FieldName,Number,TEXT("bPickA"));
        auto* StoreType=Set(G,*NativeName);Link(Number,TEXT("ReturnValue"),StoreType,*NativeName);Link(Exec,TEXT("then"),StoreType,TEXT("execute"));Exec=StoreType;
    }
    auto TextColor=[&](int32 First) { auto* C=Call(G,UKismetMathLibrary::StaticClass(),TEXT("MakeColor"));for(int32 J=0;J<3;++J)Link(Field(G,SaveClass,*SettingName(First+J),Config,TEXT("Settings")),*SettingName(First+J),C,J==0?TEXT("R"):J==1?TEXT("G"):TEXT("B"));Value(C,TEXT("A"),TEXT("1"));return C; };
    for (int32 I = 0; I < 8; ++I) {
        const FString H = FString::Printf(TEXT("AutoHud%d"), I), P = FString::Printf(TEXT("AutoPulse%d"), I);
        auto* Hud = Get(G, *H); auto* Pulse = Get(G, *P);
        auto* Reapply=Set(G,*FString::Printf(TEXT("AppliedSerial%d"),I));Value(Reapply,*FString::Printf(TEXT("AppliedSerial%d"),I),TEXT("-1"));Link(Exec,TEXT("then"),Reapply,TEXT("execute"));
        auto* SetLabel = UpdateRegionLabel(G, HudClass, I); Link(Reapply, TEXT("then"), SetLabel, TEXT("execute"));
        auto* Blink = WriteField(G, HudClass, TEXT("BlinkEnabled"), Hud, *H); Link(Field(G, SaveClass, TEXT("Blink"), Config, TEXT("Settings")), TEXT("Blink"), Blink, TEXT("BlinkEnabled")); Link(SetLabel, TEXT("then"), Blink, TEXT("execute"));
        auto* Radius = WriteField(G, PulseClass, TEXT("RadiusScale"), Pulse, *P);
        auto* Limit = Call(G, UKismetMathLibrary::StaticClass(), TEXT("FClamp")); Link(Field(G, SaveClass, TEXT("Radius"), Config, TEXT("Settings")), TEXT("Radius"), Limit, TEXT("Value")); Value(Limit, TEXT("Min"), TEXT("0.5")); Value(Limit, TEXT("Max"), TEXT("12")); Link(Limit, TEXT("ReturnValue"), Radius, TEXT("RadiusScale")); auto* Hz=WriteField(G,HudClass,TEXT("BlinkHz"),Hud,*H);Link(Field(G,SaveClass,TEXT("BlinkHz"),Config,TEXT("Settings")),TEXT("BlinkHz"),Hz,TEXT("BlinkHz"));Link(Blink,TEXT("then"),Hz,TEXT("execute"));
        auto* A=WriteField(G,HudClass,TEXT("BlinkA"),Hud,*H);Link(TextColor(9),TEXT("ReturnValue"),A,TEXT("BlinkA"));Link(Hz,TEXT("then"),A,TEXT("execute"));
        auto* B=WriteField(G,HudClass,TEXT("BlinkB"),Hud,*H);Link(TextColor(12),TEXT("ReturnValue"),B,TEXT("BlinkB"));Link(A,TEXT("then"),B,TEXT("execute"));Link(B,TEXT("then"),Radius,TEXT("execute"));
        auto* Material = Field(G, PulseClass, TEXT("PulseMaterial"), Pulse, *P); auto* Color = Call(G, UMaterialInstanceDynamic::StaticClass(), TEXT("SetVectorParameterValue")); Link(Material, TEXT("PulseMaterial"), Color, TEXT("self")); Value(Color, TEXT("ParameterName"), TEXT("Tint")); Link(Tint, TEXT("ReturnValue"), Color, TEXT("Value")); Link(Radius, TEXT("then"), Color, TEXT("execute")); auto* Opacity=Call(G,UMaterialInstanceDynamic::StaticClass(),TEXT("SetScalarParameterValue"));Link(Material,TEXT("PulseMaterial"),Opacity,TEXT("self"));Value(Opacity,TEXT("ParameterName"),TEXT("Opacity"));Link(Field(G,SaveClass,TEXT("Opacity"),Config,TEXT("Settings")),TEXT("Opacity"),Opacity,TEXT("Value"));Link(Color,TEXT("then"),Opacity,TEXT("execute"));Exec=Opacity;
    }
}
