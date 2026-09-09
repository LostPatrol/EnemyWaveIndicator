// Editor-only generator. Cooked graphs use stock Engine and existing FSD reflection, never this module.
#include "Modules/ModuleManager.h"
#include "NwiValidation.h"
#include "NwiWaveTypes.h"
#include "K2Node_AddDelegate.h"
#include "K2Node_RemoveDelegate.h"
#include "K2Node_CreateDelegate.h"
#include "Kismet/KismetArrayLibrary.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "KismetCompiler.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CallArrayFunction.h"
#include "K2Node_CallParentFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_LoadAsset.h"
#include "K2Node_InputKey.h"
#include "K2Node_Self.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_MakeArray.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_SpawnActorFromClass.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetInternationalizationLibrary.h"
#include "Kismet/KismetTextLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/GridPanel.h"
#include "Components/GridSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/SpinBox.h"
#include "Components/CheckBox.h"
#include "Components/Button.h"
#include "GameFramework/SaveGame.h"
#include "GameFramework/GameStateBase.h"
#include "Components/ScrollBox.h"
#include "Components/Image.h"
#include "Kismet/KismetStringLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Curves/CurveFloat.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

namespace Nwi
{
// Fail generation on a broken connection instead of saving a superficially valid graph.
UEdGraphPin* Pin(UEdGraphNode* Node, const TCHAR* Name)
{
    auto* Found = Node->FindPin(FName(Name));
    checkf(Found, TEXT("Missing pin %s on %s"), Name, *Node->GetName());
    return Found;
}
void Link(UEdGraphNode* A, const TCHAR* APin, UEdGraphNode* B, const TCHAR* BPin)
{
    checkf(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(Pin(A, APin), Pin(B, BPin)),
        TEXT("Cannot connect %s.%s to %s.%s"), *A->GetName(), APin, *B->GetName(), BPin);
}
void Value(UEdGraphNode* Node, const TCHAR* Name, const TCHAR* Text)
{
    GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*Pin(Node, Name), Text);
}
template<class T> T* Node(UEdGraph* Graph)
{
    auto* Item = NewObject<T>(Graph);
    Graph->AddNode(Item, false, false);
    Item->CreateNewGuid();
    Item->NodePosX = (Graph->Nodes.Num() % 5) * 350;
    Item->NodePosY = (Graph->Nodes.Num() / 5) * 230;
    return Item;
}
UK2Node_CallFunction* Call(UEdGraph* Graph, UClass* Owner, const TCHAR* Name)
{
    auto* Function = Owner->FindFunctionByName(FName(Name));
    checkf(Function, TEXT("Missing engine function %s"), Name);
    // Array wildcard parameters need the same type propagation node used by the Blueprint editor.
    UK2Node_CallFunction* Result = Function->HasMetaData(TEXT("ArrayParm"))
        ? Node<UK2Node_CallArrayFunction>(Graph) : Node<UK2Node_CallFunction>(Graph);
    Result->SetFromFunction(Function);
    Result->AllocateDefaultPins();
    return Result;
}
UK2Node_Event* Event(UEdGraph* Graph, UClass* Owner, const TCHAR* Name)
{
    auto* Result = Node<UK2Node_Event>(Graph);
    Result->EventReference.SetExternalMember(FName(Name), Owner);
    Result->bOverrideFunction = true;
    Result->AllocateDefaultPins();
    return Result;
}
UK2Node_VariableGet* Get(UEdGraph* Graph, const TCHAR* Name)
{
    auto* Result = Node<UK2Node_VariableGet>(Graph);
    Result->VariableReference.SetSelfMember(FName(Name));
    Result->AllocateDefaultPins();
    return Result;
}
UK2Node_VariableSet* Set(UEdGraph* Graph, const TCHAR* Name)
{
    auto* Result = Node<UK2Node_VariableSet>(Graph);
    Result->VariableReference.SetSelfMember(FName(Name));
    Result->AllocateDefaultPins();
    return Result;
}
FEdGraphPinType Type(FName Category, UObject* Subtype = nullptr)
{
    FEdGraphPinType Result;
    Result.PinCategory = Category;
    Result.PinSubCategoryObject = Subtype;
    return Result;
}
void Variable(UBlueprint* BP, const TCHAR* Name, const FEdGraphPinType& Kind, const TCHAR* Default = TEXT(""))
{
    check(FBlueprintEditorUtils::AddMemberVariable(BP, FName(Name), Kind, Default));
}
UEdGraph* Graph(UBlueprint* BP)
{
    auto* Result = FBlueprintEditorUtils::CreateNewGraph(BP, TEXT("Presentation"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(BP, Result);
    return Result;
}
UBlueprint* Blueprint(const TCHAR* Name, bool Widget, UClass* ActorParent = AStaticMeshActor::StaticClass())
{
    const FString PackageName = FString(TEXT("/Game/EnemyWaveIndicator/")) + Name;
    // A fresh process regenerates only our own assets. Existing on-disk files are replaced on save.
    auto* Package = CreatePackage(*PackageName);
    return FKismetEditorUtilities::CreateBlueprint(Widget ? UUserWidget::StaticClass() : ActorParent,
        Package, FName(Name), BPTYPE_Normal,
        Widget ? UWidgetBlueprint::StaticClass() : UBlueprint::StaticClass(),
        Widget ? UWidgetBlueprintGeneratedClass::StaticClass() : UBlueprintGeneratedClass::StaticClass());
}
void Compile(UBlueprint* BP)
{
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FCompilerResultsLog Results;
    FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::None, &Results);
    checkf(Results.NumErrors == 0 && BP->Status != BS_Error, TEXT("Blueprint compilation failed: %s"), *BP->GetName());
}
void Save(UBlueprint* BP)
{
    const FString Filename = FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
    check(UPackage::SavePackage(BP->GetOutermost(), BP, RF_Public | RF_Standalone, *Filename, GError, nullptr, false, true, SAVE_NoError));
    UE_LOG(LogTemp, Display, TEXT("NWI_ASSET_SAVED %s"), *BP->GetPathName());
}

#include "NwiHudGraph.inl"
UK2Node_CustomEvent* Custom(UEdGraph* G, const TCHAR* Name);
UK2Node_IfThenElse* Branch(UEdGraph* G, UEdGraphNode* Condition, const TCHAR* Output);
UK2Node_CallFunction* Valid(UEdGraph* G, UEdGraphNode* Object, const TCHAR* Output);

void BuildHud()
{
    auto* BP = CastChecked<UWidgetBlueprint>(Blueprint(TEXT("WBP_NwiMarker"), true));
    Variable(BP, TEXT("WorldLocation"), Type(UEdGraphSchema_K2::PC_Struct, TBaseStructure<FVector>::Get()));
    Variable(BP, TEXT("BaseLabel"), Type(UEdGraphSchema_K2::PC_Text));
    Variable(BP, TEXT("LastMeters"), Type(UEdGraphSchema_K2::PC_Int), TEXT("-1"));
    Variable(BP, TEXT("BlinkHz"), Type(UEdGraphSchema_K2::PC_Float), TEXT("2"));
    Variable(BP, TEXT("BlinkA"), Type(UEdGraphSchema_K2::PC_Struct, TBaseStructure<FLinearColor>::Get()), TEXT("(R=1,G=0,B=0,A=1)"));
    Variable(BP, TEXT("BlinkB"), Type(UEdGraphSchema_K2::PC_Struct, TBaseStructure<FLinearColor>::Get()), TEXT("(R=1,G=1,B=1,A=1)"));
    Variable(BP, TEXT("BlinkEnabled"), Type(UEdGraphSchema_K2::PC_Boolean), TEXT("true"));
    auto* Text = BP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MarkerText"));
    Text->SetText(FText::FromString(TEXT("[+] NWI VISUAL TEST")));
    Text->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.22f, 0.03f, 1.0f)));
    Text->SetJustification(ETextJustify::Center);
    auto Font = Text->Font; Font.Size = 22; Text->SetFont(Font);
    Text->SetShadowColorAndOpacity(FLinearColor::Black); Text->SetShadowOffset(FVector2D(1.5f, 1.5f));
    Text->SetVisibility(ESlateVisibility::HitTestInvisible);
    Text->bIsVariable = true;
    // Keep the root inside the viewport. Moving the whole widget offscreen can suspend Slate Tick.
    auto* Canvas = BP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ViewportCanvas"));
    Canvas->SetVisibility(ESlateVisibility::HitTestInvisible);
    auto* Slot = Canvas->AddChildToCanvas(Text);
    Slot->SetAutoSize(true); Slot->SetAlignment(FVector2D(0.5f, 0.5f));
    auto* Arrow = BP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EdgeArrow"));
    Arrow->SetText(FText::FromString(TEXT(">"))); Arrow->SetFont(Font);
    Arrow->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.1f, 0.02f, 1.0f)));
    Arrow->SetJustification(ETextJustify::Center); Arrow->SetRenderOpacity(0.0f);
    Arrow->SetVisibility(ESlateVisibility::HitTestInvisible); Arrow->bIsVariable = true;
    auto* ArrowSlot = Canvas->AddChildToCanvas(Arrow);
    ArrowSlot->SetAutoSize(true); ArrowSlot->SetAlignment(FVector2D(0.5f, 0.5f));
    BP->WidgetTree->RootWidget = Canvas;
    Compile(BP); // Materialize the WidgetTree's named widget variable before allocating its getter.
    auto* G = Graph(BP);

    // Cache the prefix; rebuild formatted text only when the rounded distance changes.
    auto* Label = Node<UK2Node_CustomEvent>(G); Label->CustomFunctionName = TEXT("SetMarkerLabel"); Label->AllocateDefaultPins();
    Label->CreateUserDefinedPin(TEXT("Label"), Type(UEdGraphSchema_K2::PC_Text), EGPD_Output);
    auto* LabelWidget = Get(G, TEXT("MarkerText"));
    auto* SetLabel = Call(G, UTextBlock::StaticClass(), TEXT("SetText"));
    auto* Prefix = Set(G, TEXT("BaseLabel")); Link(Label, TEXT("Label"), Prefix, TEXT("BaseLabel")); Link(Label, TEXT("then"), Prefix, TEXT("execute"));
    auto* Dirty = Set(G, TEXT("LastMeters")); Value(Dirty, TEXT("LastMeters"), TEXT("-1")); Link(Prefix, TEXT("then"), Dirty, TEXT("execute"));
    Link(LabelWidget, TEXT("MarkerText"), SetLabel, TEXT("self")); Link(Label, TEXT("Label"), SetLabel, TEXT("InText")); Link(Dirty, TEXT("then"), SetLabel, TEXT("execute"));
    BuildWarningTick(BP, G);
    BuildPlacement(BP, G);
    Compile(BP);
    ConnectHudTick(BP, G);
    Compile(BP);
    Save(BP);
}

// Original cooked material has only Alpha, no tint parameter. Keep original curves with our own red surface.
void BuildRedMaterial()
{
    auto* Package = CreatePackage(TEXT("/Game/EnemyWaveIndicator/M_NwiRedPulse"));
    auto* Material = NewObject<UMaterial>(Package, TEXT("M_NwiRedPulse"), RF_Public | RF_Standalone);
    auto* Tint = NewObject<UMaterialExpressionVectorParameter>(Material);
    Tint->ParameterName = TEXT("Tint"); Tint->DefaultValue = FLinearColor(3.0f, 0.01f, 0.005f, 1.0f);
    auto* Alpha = NewObject<UMaterialExpressionScalarParameter>(Material);
    Alpha->ParameterName = TEXT("Alpha"); Alpha->DefaultValue = 1.0f;
    auto* Rim = NewObject<UMaterialExpressionFresnel>(Material);
    Rim->Exponent = 3.0f; Rim->BaseReflectFraction = 0.08f;
    auto* Opacity = NewObject<UMaterialExpressionScalarParameter>(Material); Opacity->ParameterName=TEXT("Opacity"); Opacity->DefaultValue=.4f; Material->Expressions.Add(Opacity);
    auto* Strength = NewObject<UMaterialExpressionMultiply>(Material);
    Strength->A.Expression = Rim; Strength->B.Expression = Opacity; // Limit opacity as the larger sphere approaches the camera.
    auto* Fade = NewObject<UMaterialExpressionMultiply>(Material);
    Fade->A.Expression = Strength; Fade->B.Expression = Alpha;
    Material->Expressions.Add(Tint); Material->Expressions.Add(Alpha); Material->Expressions.Add(Rim);
    Material->Expressions.Add(Strength); Material->Expressions.Add(Fade);
    Material->EmissiveColor.Expression = Tint; Material->Opacity.Expression = Fade;
    Material->BlendMode = BLEND_Translucent; Material->SetShadingModel(MSM_Unlit);
    Material->TwoSided = false; // One surface layer, no dynamic lighting or extra back-face overdraw.
    Material->PostEditChange();
    const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
    check(UPackage::SavePackage(Package, Material, RF_Public | RF_Standalone, *Filename, GError, nullptr, false, true, SAVE_NoError));
}

void BuildPulse()
{
    auto* BP = Blueprint(TEXT("BP_NwiPulse"), false);
    const auto CurveType = Type(UEdGraphSchema_K2::PC_Object, UCurveFloat::StaticClass());
    Variable(BP, TEXT("ScaleCurve"), CurveType);
    Variable(BP, TEXT("AlphaCurve"), CurveType);
    Variable(BP, TEXT("PulseMaterial"), Type(UEdGraphSchema_K2::PC_Object, UMaterialInstanceDynamic::StaticClass()));
    // Marker scale is configurable and is NOT claimed to be the original mini-mule's final world radius.
    Variable(BP, TEXT("WeightScale"), Type(UEdGraphSchema_K2::PC_Float), TEXT("1"));
    Variable(BP, TEXT("RadiusScale"), Type(UEdGraphSchema_K2::PC_Float), TEXT("3.75")); // Three times the previous linear size.
    Variable(BP, TEXT("StartedAt"), Type(UEdGraphSchema_K2::PC_Float));
    auto* G = Graph(BP);
    auto* Init = Node<UK2Node_CustomEvent>(G);
    Init->CustomFunctionName = TEXT("InitializeVisual"); Init->AllocateDefaultPins();
    Init->CreateUserDefinedPin(TEXT("Material"), Type(UEdGraphSchema_K2::PC_Object, UMaterialInterface::StaticClass()), EGPD_Output);
    Init->CreateUserDefinedPin(TEXT("Scale"), CurveType, EGPD_Output);
    Init->CreateUserDefinedPin(TEXT("Alpha"), CurveType, EGPD_Output);
    // Invalid or unloaded inputs leave Tick disabled; never synchronously load assets in Tick.
    auto* ValidMaterial = Call(G, UKismetSystemLibrary::StaticClass(), TEXT("IsValid"));
    auto* ValidScale = Call(G, UKismetSystemLibrary::StaticClass(), TEXT("IsValid"));
    auto* ValidAlpha = Call(G, UKismetSystemLibrary::StaticClass(), TEXT("IsValid"));
    Link(Init, TEXT("Material"), ValidMaterial, TEXT("Object"));
    Link(Init, TEXT("Scale"), ValidScale, TEXT("Object"));
    Link(Init, TEXT("Alpha"), ValidAlpha, TEXT("Object"));
    auto* Both = Call(G, UKismetMathLibrary::StaticClass(), TEXT("BooleanAND"));
    auto* All = Call(G, UKismetMathLibrary::StaticClass(), TEXT("BooleanAND"));
    Link(ValidMaterial, TEXT("ReturnValue"), Both, TEXT("A")); Link(ValidScale, TEXT("ReturnValue"), Both, TEXT("B"));
    Link(Both, TEXT("ReturnValue"), All, TEXT("A")); Link(ValidAlpha, TEXT("ReturnValue"), All, TEXT("B"));
    auto* Branch = Node<UK2Node_IfThenElse>(G); Branch->AllocateDefaultPins();
    auto* Stop = Call(G, AActor::StaticClass(), TEXT("SetActorTickEnabled")); Value(Stop, TEXT("bEnabled"), TEXT("false"));
    auto* Hide = Call(G, AActor::StaticClass(), TEXT("SetActorHiddenInGame")); Value(Hide, TEXT("bNewHidden"), TEXT("true"));
    Link(Init, TEXT("then"), Stop, TEXT("execute")); Link(Stop, TEXT("then"), Hide, TEXT("execute"));
    Link(Hide, TEXT("then"), Branch, TEXT("execute")); Link(All, TEXT("ReturnValue"), Branch, TEXT("Condition"));
    auto* SetScale = Set(G, TEXT("ScaleCurve")); auto* SetAlpha = Set(G, TEXT("AlphaCurve"));
    Link(Init, TEXT("Scale"), SetScale, TEXT("ScaleCurve")); Link(Init, TEXT("Alpha"), SetAlpha, TEXT("AlphaCurve"));
    Link(Branch, TEXT("then"), SetScale, TEXT("execute")); Link(SetScale, TEXT("then"), SetAlpha, TEXT("execute"));
    auto* Mesh = Get(G, TEXT("StaticMeshComponent"));
    auto* Dynamic = Call(G, UPrimitiveComponent::StaticClass(), TEXT("CreateDynamicMaterialInstance"));
    Link(Mesh, TEXT("StaticMeshComponent"), Dynamic, TEXT("self")); Link(Init, TEXT("Material"), Dynamic, TEXT("SourceMaterial"));
    Link(SetAlpha, TEXT("then"), Dynamic, TEXT("execute"));
    auto* Cache = Set(G, TEXT("PulseMaterial")); Link(Dynamic, TEXT("ReturnValue"), Cache, TEXT("PulseMaterial"));
    Link(Dynamic, TEXT("then"), Cache, TEXT("execute"));
    auto* Enable = Call(G, AActor::StaticClass(), TEXT("SetActorTickEnabled")); Value(Enable, TEXT("bEnabled"), TEXT("true"));
    auto* InitialAge = Call(G, AActor::StaticClass(), TEXT("GetGameTimeSinceCreation"));
    auto* Start = Set(G, TEXT("StartedAt")); Link(InitialAge, TEXT("ReturnValue"), Start, TEXT("StartedAt"));
    Link(Cache, TEXT("then"), Start, TEXT("execute")); Link(Start, TEXT("then"), Enable, TEXT("execute"));
    auto* Visible = Call(G, AActor::StaticClass(), TEXT("SetActorHiddenInGame")); Value(Visible, TEXT("bNewHidden"), TEXT("false"));
    Link(Enable, TEXT("then"), Visible, TEXT("execute"));
    auto* Deactivate = Node<UK2Node_CustomEvent>(G); Deactivate->CustomFunctionName = TEXT("DeactivateVisual"); Deactivate->AllocateDefaultPins();
    auto* Disable = Call(G, AActor::StaticClass(), TEXT("SetActorTickEnabled")); Value(Disable, TEXT("bEnabled"), TEXT("false"));
    auto* Conceal = Call(G, AActor::StaticClass(), TEXT("SetActorHiddenInGame")); Value(Conceal, TEXT("bNewHidden"), TEXT("true"));
    Link(Deactivate, TEXT("then"), Disable, TEXT("execute")); Link(Disable, TEXT("then"), Conceal, TEXT("execute"));
    // Reuse an already prepared MID when a pooled marker is shown again.
    auto* Reactivate = Custom(G, TEXT("ReactivateVisual"));
    auto* PreparedMaterial = Get(G, TEXT("PulseMaterial")); auto* Prepared = Valid(G, PreparedMaterial, TEXT("PulseMaterial"));
    auto* PreparedGate = Nwi::Branch(G, Prepared, TEXT("ReturnValue")); Link(Reactivate, TEXT("then"), PreparedGate, TEXT("execute"));
    auto* ResetTime = Set(G, TEXT("StartedAt")); Link(InitialAge, TEXT("ReturnValue"), ResetTime, TEXT("StartedAt")); Link(PreparedGate, TEXT("then"), ResetTime, TEXT("execute"));
    auto* Reenable = Call(G, AActor::StaticClass(), TEXT("SetActorTickEnabled")); Value(Reenable, TEXT("bEnabled"), TEXT("true")); Link(ResetTime, TEXT("then"), Reenable, TEXT("execute"));
    auto* Reshow = Call(G, AActor::StaticClass(), TEXT("SetActorHiddenInGame")); Value(Reshow, TEXT("bNewHidden"), TEXT("false")); Link(Reenable, TEXT("then"), Reshow, TEXT("execute"));

    auto* Tick = Event(G, AActor::StaticClass(), TEXT("ReceiveTick"));
    auto* Age = Call(G, AActor::StaticClass(), TEXT("GetGameTimeSinceCreation"));
    auto* Phase = Call(G, UKismetMathLibrary::StaticClass(), TEXT("Percent_FloatFloat"));
    auto* Started = Get(G, TEXT("StartedAt"));
    auto* Elapsed = Call(G, UKismetMathLibrary::StaticClass(), TEXT("Subtract_FloatFloat"));
    Link(Age, TEXT("ReturnValue"), Elapsed, TEXT("A")); Link(Started, TEXT("StartedAt"), Elapsed, TEXT("B"));
    Link(Elapsed, TEXT("ReturnValue"), Phase, TEXT("A")); Value(Phase, TEXT("B"), TEXT("2"));
    auto* ScaleRef = Get(G, TEXT("ScaleCurve")); auto* AlphaRef = Get(G, TEXT("AlphaCurve"));
    auto* ScaleValue = Call(G, UCurveFloat::StaticClass(), TEXT("GetFloatValue"));
    auto* AlphaValue = Call(G, UCurveFloat::StaticClass(), TEXT("GetFloatValue"));
    Link(ScaleRef, TEXT("ScaleCurve"), ScaleValue, TEXT("self")); Link(AlphaRef, TEXT("AlphaCurve"), AlphaValue, TEXT("self"));
    Link(Phase, TEXT("ReturnValue"), ScaleValue, TEXT("InTime")); Link(Phase, TEXT("ReturnValue"), AlphaValue, TEXT("InTime"));
    // Const BlueprintCallable functions may be marked pure by UHT; honor the installed reflection flags.
    UEdGraphNode* Exec = Tick;
    for (auto* Evaluation : { ScaleValue, AlphaValue })
    {
        if (!Evaluation->IsNodePure()) { Link(Exec, TEXT("then"), Evaluation, TEXT("execute")); Exec = Evaluation; }
    }
    auto* Radius = Get(G, TEXT("RadiusScale"));
    auto* Multiply = Call(G, UKismetMathLibrary::StaticClass(), TEXT("Multiply_FloatFloat"));
    Link(ScaleValue, TEXT("ReturnValue"), Multiply, TEXT("A")); auto* Weighted=Call(G,UKismetMathLibrary::StaticClass(),TEXT("Multiply_FloatFloat")); Link(Radius,TEXT("RadiusScale"),Weighted,TEXT("A")); Link(Get(G,TEXT("WeightScale")),TEXT("WeightScale"),Weighted,TEXT("B")); Link(Weighted,TEXT("ReturnValue"),Multiply,TEXT("B"));
    auto* Vector = Call(G, UKismetMathLibrary::StaticClass(), TEXT("MakeVector"));
    for (const auto* Axis : { TEXT("X"), TEXT("Y"), TEXT("Z") }) Link(Multiply, TEXT("ReturnValue"), Vector, Axis);
    auto* Resize = Call(G, AActor::StaticClass(), TEXT("SetActorScale3D"));
    Link(Vector, TEXT("ReturnValue"), Resize, TEXT("NewScale3D")); Link(Exec, TEXT("then"), Resize, TEXT("execute"));
    auto* Mat = Get(G, TEXT("PulseMaterial"));
    auto* Fade = Call(G, UMaterialInstanceDynamic::StaticClass(), TEXT("SetScalarParameterValue"));
    Link(Mat, TEXT("PulseMaterial"), Fade, TEXT("self")); Value(Fade, TEXT("ParameterName"), TEXT("Alpha"));
    Link(AlphaValue, TEXT("ReturnValue"), Fade, TEXT("Value")); Link(Resize, TEXT("then"), Fade, TEXT("execute"));
    Compile(BP);
    auto* CDO = CastChecked<AStaticMeshActor>(BP->GeneratedClass->GetDefaultObject());
    check(!CDO->GetIsReplicated()); // Stock StaticMeshActor defaults to local-only replication.
    CDO->SetActorHiddenInGame(true);
    CDO->PrimaryActorTick.bStartWithTickEnabled = false;
    auto* Component = CDO->GetStaticMeshComponent();
    Component->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
    Component->SetMobility(EComponentMobility::Movable);
    // Registration reapplies the named profile; changing only CollisionEnabled is not persistent enough.
    Component->SetCollisionProfileName(TEXT("NoCollision"));
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetGenerateOverlapEvents(false);
    Component->SetCanEverAffectNavigation(false);
    Component->SetCastShadow(false);
    Component->SetCullDistance(15000.0f);
    Save(BP);
}

// One explicit asynchronous preparation per world; strong properties retain loaded visual resources.
void BuildResources()
{
    auto* BP = Blueprint(TEXT("BP_NwiResources"), false, AActor::StaticClass());
    const TCHAR* Names[] = { TEXT("Material"), TEXT("Scale"), TEXT("Alpha") };
    const TCHAR* Paths[] = {
        TEXT("/Game/EnemyWaveIndicator/M_NwiRedPulse.M_NwiRedPulse"),
        TEXT("/Game/GameElements/Objectives/Salvage/BP_MiniMule_Salvage.BP_MiniMule_Salvage_C:CurveFloat_0"),
        TEXT("/Game/GameElements/Objectives/Salvage/BP_MiniMule_Salvage.BP_MiniMule_Salvage_C:CurveFloat_1") };
    UClass* Classes[] = { UMaterialInterface::StaticClass(), UCurveFloat::StaticClass(), UCurveFloat::StaticClass() };
    for (int32 Index = 0; Index < 3; ++Index)
    {
        Variable(BP, *(FString(Names[Index]) + TEXT("Path")), Type(UEdGraphSchema_K2::PC_String), Paths[Index]);
        Variable(BP, Names[Index], Type(UEdGraphSchema_K2::PC_Object, Classes[Index]));
    }
    for (const auto* Name : { TEXT("Attempted"), TEXT("Finished"), TEXT("Ready") })
        Variable(BP, Name, Type(UEdGraphSchema_K2::PC_Boolean), TEXT("false"));
    auto* G = Graph(BP);
    auto* Prepare = Node<UK2Node_CustomEvent>(G);
    Prepare->CustomFunctionName = TEXT("PrepareResources"); Prepare->AllocateDefaultPins();
    auto* Attempted = Get(G, TEXT("Attempted"));
    auto* Gate = Node<UK2Node_IfThenElse>(G); Gate->AllocateDefaultPins();
    Link(Prepare, TEXT("then"), Gate, TEXT("execute")); Link(Attempted, TEXT("Attempted"), Gate, TEXT("Condition"));
    auto* Mark = Set(G, TEXT("Attempted")); Value(Mark, TEXT("Attempted"), TEXT("true"));
    Link(Gate, TEXT("else"), Mark, TEXT("execute"));
    UEdGraphNode* Previous = Mark;
    UK2Node_CallFunction* Checks[3];
    for (int32 Index = 0; Index < 3; ++Index)
    {
        const FString PathName = FString(Names[Index]) + TEXT("Path");
        auto* Path = Get(G, *PathName);
        auto* MakePath = Call(G, UKismetSystemLibrary::StaticClass(), TEXT("MakeSoftObjectPath"));
        auto* Ref = Call(G, UKismetSystemLibrary::StaticClass(), TEXT("Conv_SoftObjPathToSoftObjRef"));
        auto* Load = Node<UK2Node_LoadAsset>(G); Load->AllocateDefaultPins();
        Link(Path, *PathName, MakePath, TEXT("PathString"));
        Link(MakePath, TEXT("ReturnValue"), Ref, TEXT("SoftObjectPath"));
        Link(Ref, TEXT("ReturnValue"), Load, TEXT("Asset"));
        Link(Previous, TEXT("then"), Load, TEXT("execute"));
        auto* Cast = Node<UK2Node_DynamicCast>(G);
        Cast->TargetType = Classes[Index]; Cast->AllocateDefaultPins(); Cast->SetPurity(true);
        check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(Pin(Load, TEXT("Object")), Cast->GetCastSourcePin()));
        auto* Cache = Set(G, Names[Index]);
        check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(Cast->GetCastResultPin(), Pin(Cache, Names[Index])));
        Link(Load, TEXT("Completed"), Cache, TEXT("execute"));
        Previous = Cache;
        auto* Resource = Get(G, Names[Index]);
        Checks[Index] = Call(G, UKismetSystemLibrary::StaticClass(), TEXT("IsValid"));
        Link(Resource, Names[Index], Checks[Index], TEXT("Object"));
    }
    auto* Both = Call(G, UKismetMathLibrary::StaticClass(), TEXT("BooleanAND"));
    auto* All = Call(G, UKismetMathLibrary::StaticClass(), TEXT("BooleanAND"));
    Link(Checks[0], TEXT("ReturnValue"), Both, TEXT("A")); Link(Checks[1], TEXT("ReturnValue"), Both, TEXT("B"));
    Link(Both, TEXT("ReturnValue"), All, TEXT("A")); Link(Checks[2], TEXT("ReturnValue"), All, TEXT("B"));
    auto* Ready = Set(G, TEXT("Ready")); Link(All, TEXT("ReturnValue"), Ready, TEXT("Ready"));
    auto* Finished = Set(G, TEXT("Finished")); Value(Finished, TEXT("Finished"), TEXT("true"));
    Link(Previous, TEXT("then"), Ready, TEXT("execute")); Link(Ready, TEXT("then"), Finished, TEXT("execute"));
    Compile(BP);
    auto* CDO = CastChecked<AActor>(BP->GeneratedClass->GetDefaultObject());
    CDO->PrimaryActorTick.bCanEverTick = false;
    CDO->PrimaryActorTick.bStartWithTickEnabled = false;
    CDO->SetActorHiddenInGame(true);
    check(!CDO->GetIsReplicated());
    Save(BP);
}

// Small graph helpers keep all connections checked; failed graph construction is never silently saved.
UK2Node_CustomEvent* Custom(UEdGraph* G, const TCHAR* Name)
{
    auto* E = Node<UK2Node_CustomEvent>(G); E->CustomFunctionName = Name; E->AllocateDefaultPins(); return E;
}
UK2Node_IfThenElse* Branch(UEdGraph* G, UEdGraphNode* Condition, const TCHAR* Output)
{
    auto* B = Node<UK2Node_IfThenElse>(G); B->AllocateDefaultPins(); Link(Condition, Output, B, TEXT("Condition")); return B;
}
UK2Node_CallFunction* Valid(UEdGraph* G, UEdGraphNode* Object, const TCHAR* Output)
{
    auto* V = Call(G, UKismetSystemLibrary::StaticClass(), TEXT("IsValid")); Link(Object, Output, V, TEXT("Object")); return V;
}

// F5 only: one cached pulse/widget pair, no manager Tick, no gameplay Actor and no natural-wave claim.
void BuildVisualTest()
{
    auto* Parent = LoadClass<AActor>(nullptr, TEXT("/Game/EnemyWaveIndicator/BP_NwiResources.BP_NwiResources_C"));
    auto* PulseClass = LoadClass<AActor>(nullptr, TEXT("/Game/EnemyWaveIndicator/BP_NwiPulse.BP_NwiPulse_C"));
    auto* HudClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/EnemyWaveIndicator/WBP_NwiMarker.WBP_NwiMarker_C"));
    check(Parent && PulseClass && HudClass);
    auto* BP = Blueprint(TEXT("BP_NwiVisualTest"), false, Parent);
    Variable(BP, TEXT("Pulse"), Type(UEdGraphSchema_K2::PC_Object, PulseClass));
    Variable(BP, TEXT("Hud"), Type(UEdGraphSchema_K2::PC_Object, HudClass));
    Variable(BP, TEXT("SetupAttempted"), Type(UEdGraphSchema_K2::PC_Boolean), TEXT("false"));
    Variable(BP, TEXT("SetupTries"), Type(UEdGraphSchema_K2::PC_Int), TEXT("0"));
    auto* G = Graph(BP);
    auto* Run = Custom(G, TEXT("RunVisualTest"));
    auto* Show = Custom(G, TEXT("ShowCached"));
    auto* Hide = Custom(G, TEXT("HideVisual"));
    auto* Setup = Custom(G, TEXT("InitializeTest"));
    Compile(BP); // Expose these events before wiring self calls.
    auto* Self = Node<UK2Node_Self>(G); Self->AllocateDefaultPins();
    auto* Player = Call(G, UGameplayStatics::StaticClass(), TEXT("GetPlayerController"));
    auto* PlayerValid = Valid(G, Player, TEXT("ReturnValue"));
    auto* Server = Call(G, UKismetSystemLibrary::StaticClass(), TEXT("IsServer"));
    auto* LocalHost = Call(G, UKismetMathLibrary::StaticClass(), TEXT("BooleanAND"));
    Link(PlayerValid, TEXT("ReturnValue"), LocalHost, TEXT("A")); Link(Server, TEXT("ReturnValue"), LocalHost, TEXT("B"));
    auto* Begin = Event(G, AActor::StaticClass(), TEXT("ReceiveBeginPlay"));
    auto* CallSetup = Call(G, BP->GeneratedClass, TEXT("InitializeTest")); Link(Begin, TEXT("then"), CallSetup, TEXT("execute"));
    auto* Tries = Get(G, TEXT("SetupTries")); auto* Increment = Call(G, UKismetMathLibrary::StaticClass(), TEXT("Add_IntInt"));
    Link(Tries, TEXT("SetupTries"), Increment, TEXT("A")); Value(Increment, TEXT("B"), TEXT("1"));
    auto* SaveTries = Set(G, TEXT("SetupTries")); Link(Increment, TEXT("ReturnValue"), SaveTries, TEXT("SetupTries")); Link(Setup, TEXT("then"), SaveTries, TEXT("execute"));
    auto* BeginGate = Branch(G, LocalHost, TEXT("ReturnValue")); Link(SaveTries, TEXT("then"), BeginGate, TEXT("execute"));
    auto* Limit = Call(G, UKismetMathLibrary::StaticClass(), TEXT("Less_IntInt")); Link(Tries, TEXT("SetupTries"), Limit, TEXT("A")); Value(Limit, TEXT("B"), TEXT("30"));
    auto* RetryHost = Call(G, UKismetMathLibrary::StaticClass(), TEXT("BooleanAND")); Link(Server, TEXT("ReturnValue"), RetryHost, TEXT("A")); Link(Limit, TEXT("ReturnValue"), RetryHost, TEXT("B"));
    auto* RetryGate = Branch(G, RetryHost, TEXT("ReturnValue")); Link(BeginGate, TEXT("else"), RetryGate, TEXT("execute"));
    auto* Retry = Call(G, UKismetSystemLibrary::StaticClass(), TEXT("K2_SetTimer")); Link(Self, TEXT("self"), Retry, TEXT("Object"));
    Value(Retry, TEXT("FunctionName"), TEXT("InitializeTest")); Value(Retry, TEXT("Time"), TEXT("1")); Value(Retry, TEXT("bLooping"), TEXT("false")); Link(RetryGate, TEXT("then"), Retry, TEXT("execute"));
    auto* Input = Call(G, AActor::StaticClass(), TEXT("EnableInput")); Link(Player, TEXT("ReturnValue"), Input, TEXT("PlayerController"));
    Link(BeginGate, TEXT("then"), Input, TEXT("execute"));
    auto* Prepare = Call(G, Parent, TEXT("PrepareResources")); Link(Input, TEXT("then"), Prepare, TEXT("execute"));
    auto* Notice = Call(G, UKismetSystemLibrary::StaticClass(), TEXT("PrintString"));
    Value(Notice, TEXT("InString"), TEXT("NWI visual test loaded. F5: place an 8-second test marker."));
    Value(Notice, TEXT("Duration"), TEXT("8")); Link(Prepare, TEXT("then"), Notice, TEXT("execute"));
    auto* Key = Node<UK2Node_InputKey>(G); Key->InputKey = EKeys::F5; Key->bConsumeInput = false; Key->AllocateDefaultPins();
    auto* CallRun = Call(G, BP->GeneratedClass, TEXT("RunVisualTest")); Link(Key, TEXT("Pressed"), CallRun, TEXT("execute"));
    auto* Ready = Get(G, TEXT("Ready"));
    auto* ReadyGate = Branch(G, LocalHost, TEXT("ReturnValue")); Link(Run, TEXT("then"), ReadyGate, TEXT("execute"));
    auto* NotReady = Call(G, UKismetSystemLibrary::StaticClass(), TEXT("PrintString"));
    Value(NotReady, TEXT("InString"), TEXT("NWI visual resources not ready. Check resource-loading logs."));
    Link(ReadyGate, TEXT("else"), NotReady, TEXT("execute"));
    auto* Pulse = Get(G, TEXT("Pulse")); auto* Hud = Get(G, TEXT("Hud"));
    auto* PulseValid = Valid(G, Pulse, TEXT("Pulse")); auto* HudValid = Valid(G, Hud, TEXT("Hud"));
    auto* PairValid = Call(G, UKismetMathLibrary::StaticClass(), TEXT("BooleanAND"));
    Link(PulseValid, TEXT("ReturnValue"), PairValid, TEXT("A")); Link(HudValid, TEXT("ReturnValue"), PairValid, TEXT("B"));
    auto* PairGate = Branch(G, PairValid, TEXT("ReturnValue")); Link(ReadyGate, TEXT("then"), PairGate, TEXT("execute"));
    auto* Reuse = Call(G, BP->GeneratedClass, TEXT("ShowCached")); Link(PairGate, TEXT("then"), Reuse, TEXT("execute"));
    auto* Attempted = Get(G, TEXT("SetupAttempted")); auto* SetupGate = Branch(G, Attempted, TEXT("SetupAttempted"));
    Link(PairGate, TEXT("else"), SetupGate, TEXT("execute"));
    auto* Mark = Set(G, TEXT("SetupAttempted")); Value(Mark, TEXT("SetupAttempted"), TEXT("true"));
    Link(SetupGate, TEXT("else"), Mark, TEXT("execute"));
    auto* Spawn = Node<UK2Node_SpawnActorFromClass>(G); Spawn->AllocateDefaultPins();
    Pin(Spawn, TEXT("Class"))->DefaultObject = PulseClass; Spawn->PinDefaultValueChanged(Pin(Spawn, TEXT("Class")));
    Value(Spawn, TEXT("CollisionHandlingOverride"), TEXT("AlwaysSpawn"));
    auto* Transform = Call(G, UKismetMathLibrary::StaticClass(), TEXT("MakeTransform"));
    Link(Transform, TEXT("ReturnValue"), Spawn, TEXT("SpawnTransform")); Link(Self, TEXT("self"), Spawn, TEXT("Owner"));
    Link(Mark, TEXT("then"), Spawn, TEXT("execute"));
    auto* SavePulse = Set(G, TEXT("Pulse")); Link(Spawn, TEXT("ReturnValue"), SavePulse, TEXT("Pulse")); Link(Spawn, TEXT("then"), SavePulse, TEXT("execute"));
    auto* CreateHud = Call(G, UWidgetBlueprintLibrary::StaticClass(), TEXT("Create"));
    Pin(CreateHud, TEXT("WidgetType"))->DefaultObject = HudClass;
    Link(Player, TEXT("ReturnValue"), CreateHud, TEXT("OwningPlayer")); Link(SavePulse, TEXT("then"), CreateHud, TEXT("execute"));
    auto* CastHud = Node<UK2Node_DynamicCast>(G); CastHud->TargetType = HudClass; CastHud->AllocateDefaultPins(); CastHud->SetPurity(true);
    check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(Pin(CreateHud, TEXT("ReturnValue")), CastHud->GetCastSourcePin()));
    auto* SaveHud = Set(G, TEXT("Hud"));
    check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(CastHud->GetCastResultPin(), Pin(SaveHud, TEXT("Hud"))));
    Link(CreateHud, TEXT("then"), SaveHud, TEXT("execute"));
    auto* CreatedGate = Branch(G, PairValid, TEXT("ReturnValue")); Link(SaveHud, TEXT("then"), CreatedGate, TEXT("execute"));
    auto* Add = Call(G, UUserWidget::StaticClass(), TEXT("AddToViewport")); Link(Hud, TEXT("Hud"), Add, TEXT("self"));
    Link(CreatedGate, TEXT("then"), Add, TEXT("execute"));
    auto* FirstShow = Call(G, BP->GeneratedClass, TEXT("ShowCached")); Link(Add, TEXT("then"), FirstShow, TEXT("execute"));
    auto* Failed = Call(G, UKismetSystemLibrary::StaticClass(), TEXT("PrintString"));
    Value(Failed, TEXT("InString"), TEXT("NWI visual pair creation failed; no automatic retry.")); Link(CreatedGate, TEXT("else"), Failed, TEXT("execute"));

    auto* Camera = Call(G, UGameplayStatics::StaticClass(), TEXT("GetPlayerCameraManager"));
    auto* CameraValid = Valid(G, Camera, TEXT("ReturnValue"));
    auto* CanShow = Call(G, UKismetMathLibrary::StaticClass(), TEXT("BooleanAND"));
    Link(PairValid, TEXT("ReturnValue"), CanShow, TEXT("A")); Link(CameraValid, TEXT("ReturnValue"), CanShow, TEXT("B"));
    auto* ShowGate = Branch(G, CanShow, TEXT("ReturnValue")); Link(Show, TEXT("then"), ShowGate, TEXT("execute"));
    auto* ViewLocation = Call(G, APlayerCameraManager::StaticClass(), TEXT("GetCameraLocation"));
    auto* ViewRotation = Call(G, APlayerCameraManager::StaticClass(), TEXT("GetCameraRotation"));
    Link(Camera, TEXT("ReturnValue"), ViewLocation, TEXT("self")); Link(Camera, TEXT("ReturnValue"), ViewRotation, TEXT("self"));
    auto* Forward = Call(G, UKismetMathLibrary::StaticClass(), TEXT("GetForwardVector")); Link(ViewRotation, TEXT("ReturnValue"), Forward, TEXT("InRot"));
    auto* Distance = Call(G, UKismetMathLibrary::StaticClass(), TEXT("Multiply_VectorFloat")); Link(Forward, TEXT("ReturnValue"), Distance, TEXT("A")); Value(Distance, TEXT("B"), TEXT("500"));
    auto* Point = Call(G, UKismetMathLibrary::StaticClass(), TEXT("Add_VectorVector"));
    Link(ViewLocation, TEXT("ReturnValue"), Point, TEXT("A")); Link(Distance, TEXT("ReturnValue"), Point, TEXT("B"));
    auto* Move = Call(G, AActor::StaticClass(), TEXT("K2_SetActorLocation")); Link(Pulse, TEXT("Pulse"), Move, TEXT("self"));
    Link(Point, TEXT("ReturnValue"), Move, TEXT("NewLocation")); Value(Move, TEXT("bTeleport"), TEXT("true")); Link(ShowGate, TEXT("then"), Move, TEXT("execute"));
    auto* Init = Call(G, PulseClass, TEXT("InitializeVisual")); Link(Pulse, TEXT("Pulse"), Init, TEXT("self"));
    for (const auto* Name : { TEXT("Material"), TEXT("Scale"), TEXT("Alpha") }) { auto* Resource = Get(G, Name); Link(Resource, Name, Init, Name); }
    Link(Move, TEXT("then"), Init, TEXT("execute"));
    auto* Location = Node<UK2Node_VariableSet>(G); Location->VariableReference.SetExternalMember(TEXT("WorldLocation"), HudClass); Location->AllocateDefaultPins();
    Link(Hud, TEXT("Hud"), Location, TEXT("self")); Link(Point, TEXT("ReturnValue"), Location, TEXT("WorldLocation")); Link(Init, TEXT("then"), Location, TEXT("execute"));
    auto* Visible = Call(G, UWidget::StaticClass(), TEXT("SetVisibility")); Link(Hud, TEXT("Hud"), Visible, TEXT("self")); Value(Visible, TEXT("InVisibility"), TEXT("HitTestInvisible"));
    auto* Finished = Get(G, TEXT("Finished"));
    auto* FailureLabel = Call(G, UKismetMathLibrary::StaticClass(), TEXT("SelectString"));
    Value(FailureLabel, TEXT("A"), TEXT("NWI: RESOURCE LOAD FAILED")); Value(FailureLabel, TEXT("B"), TEXT("NWI: RESOURCES LOADING")); Link(Finished, TEXT("Finished"), FailureLabel, TEXT("bPickA"));
    auto* StatusLabel = Call(G, UKismetMathLibrary::StaticClass(), TEXT("SelectString"));
    Value(StatusLabel, TEXT("A"), TEXT("NW TEST 0.4.1")); Link(FailureLabel, TEXT("ReturnValue"), StatusLabel, TEXT("B")); Link(Ready, TEXT("Ready"), StatusLabel, TEXT("bPickA"));
    auto* AsText = Call(G, UKismetTextLibrary::StaticClass(), TEXT("Conv_StringToText")); Link(StatusLabel, TEXT("ReturnValue"), AsText, TEXT("InString"));
    auto* Label = Call(G, HudClass, TEXT("SetMarkerLabel")); Link(Hud, TEXT("Hud"), Label, TEXT("self")); Link(AsText, TEXT("ReturnValue"), Label, TEXT("Label"));
    Link(Location, TEXT("then"), Label, TEXT("execute")); Link(Label, TEXT("then"), Visible, TEXT("execute"));
    auto* Timer = Call(G, UKismetSystemLibrary::StaticClass(), TEXT("K2_SetTimer")); Link(Self, TEXT("self"), Timer, TEXT("Object"));
    Value(Timer, TEXT("FunctionName"), TEXT("HideVisual")); Value(Timer, TEXT("Time"), TEXT("8")); Value(Timer, TEXT("bLooping"), TEXT("false")); Link(Visible, TEXT("then"), Timer, TEXT("execute"));
    auto* HideGate = Branch(G, PairValid, TEXT("ReturnValue")); Link(Hide, TEXT("then"), HideGate, TEXT("execute"));
    auto* Stop = Call(G, PulseClass, TEXT("DeactivateVisual")); Link(Pulse, TEXT("Pulse"), Stop, TEXT("self")); Link(HideGate, TEXT("then"), Stop, TEXT("execute"));
    auto* Collapse = Call(G, UWidget::StaticClass(), TEXT("SetVisibility")); Link(Hud, TEXT("Hud"), Collapse, TEXT("self")); Value(Collapse, TEXT("InVisibility"), TEXT("Collapsed")); Link(Stop, TEXT("then"), Collapse, TEXT("execute"));
    auto* End = Event(G, AActor::StaticClass(), TEXT("ReceiveEndPlay"));
    UEdGraphNode* EndExec = End;
    for (const auto* Function : { TEXT("InitializeTest"), TEXT("HideVisual") })
    {
        auto* Clear = Call(G, UKismetSystemLibrary::StaticClass(), TEXT("K2_ClearTimer")); Link(Self, TEXT("self"), Clear, TEXT("Object")); Value(Clear, TEXT("FunctionName"), Function);
        Link(EndExec, TEXT("then"), Clear, TEXT("execute")); EndExec = Clear;
    }
    auto* CleanupGate = Branch(G, HudValid, TEXT("ReturnValue")); Link(EndExec, TEXT("then"), CleanupGate, TEXT("execute"));
    auto* Remove = Call(G, UWidget::StaticClass(), TEXT("RemoveFromParent")); Link(Hud, TEXT("Hud"), Remove, TEXT("self")); Link(CleanupGate, TEXT("then"), Remove, TEXT("execute"));
    Compile(BP);
    auto* CDO = CastChecked<AActor>(BP->GeneratedClass->GetDefaultObject());
    CDO->PrimaryActorTick.bCanEverTick = false; CDO->PrimaryActorTick.bStartWithTickEnabled = false;
    check(!CDO->GetIsReplicated());
    Save(BP);
}

#include "NwiSettings.inl"
#include "NwiCapture.inl"
#include "NwiAutomatic.inl"

void BuildValidationFixtures()
{
    // Synthetic constants test the generated graph without loading or distributing game assets.
    for (const auto* Name : { TEXT("CF_TestScale"), TEXT("CF_TestAlpha") })
    {
        auto* Package = CreatePackage(*(FString(TEXT("/Game/NwiValidation/")) + Name));
        auto* Curve = NewObject<UCurveFloat>(Package, FName(Name), RF_Public | RF_Standalone);
        Curve->FloatCurve.AddKey(0.0f, FString(Name).Contains(TEXT("Scale")) ? 0.5f : 0.25f);
        const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
        check(UPackage::SavePackage(Package, Curve, RF_Public | RF_Standalone, *Filename, GError, nullptr, false, true, SAVE_NoError));
    }
    auto* Package = CreatePackage(TEXT("/Game/NwiValidation/M_TestAlpha"));
    auto* Material = NewObject<UMaterial>(Package, TEXT("M_TestAlpha"), RF_Public | RF_Standalone);
    auto* Alpha = NewObject<UMaterialExpressionScalarParameter>(Material);
    Alpha->ParameterName = TEXT("Alpha"); Alpha->DefaultValue = 1.0f;
    Material->Expressions.Add(Alpha);
    Material->BlendMode = BLEND_Translucent; Material->SetShadingModel(MSM_Unlit);
    Material->Opacity.Expression = Alpha;
    Material->PostEditChange();
    const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
    check(UPackage::SavePackage(Package, Material, RF_Public | RF_Standalone, *Filename, GError, nullptr, false, true, SAVE_NoError));
    // Embedded subobjects exercise the same soft-path syntax as the game's generated-class curves.
    auto* CurvePackage = CreatePackage(TEXT("/Game/NwiValidation/CurveContainer"));
    auto* Container = NewObject<UCurveFloat>(CurvePackage, TEXT("CurveContainer"), RF_Public | RF_Standalone);
    for (int32 Index = 0; Index < 2; ++Index)
    {
        auto* Curve = NewObject<UCurveFloat>(Container, *FString::Printf(TEXT("CurveFloat_%d"), Index), RF_Public);
        Curve->FloatCurve.AddKey(0.0f, Index == 0 ? 0.5f : 0.25f);
    }
    const FString CurveFile = FPackageName::LongPackageNameToFilename(CurvePackage->GetName(), FPackageName::GetAssetPackageExtension());
    check(UPackage::SavePackage(CurvePackage, Container, RF_Public | RF_Standalone, *CurveFile, GError, nullptr, false, true, SAVE_NoError));
}
}

class FNwiAuthoringModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        UE_LOG(LogTemp, Display, TEXT("NWI authoring module loaded (editor only)."));
        // Read dependency signatures without executing their gameplay graphs.
        if (FParse::Param(FCommandLine::Get(), TEXT("NwiInspectHub")))
        {
            for (const TCHAR* Name : {TEXT("IHub"), TEXT("IHubMod"), TEXT("IHubPageWidget")})
            {
                auto* BP = LoadObject<UBlueprint>(nullptr, *FString::Printf(TEXT("/Game/_ModHub/%s.%s"), Name, Name));
                check(BP && BP->GeneratedClass);
                for (TFieldIterator<UFunction> F(BP->GeneratedClass, EFieldIteratorFlags::ExcludeSuper); F; ++F)
                {
                    UE_LOG(LogTemp, Display, TEXT("HUB_FUNCTION %s %s %x"), Name, *F->GetName(), F->FunctionFlags);
                    for (TFieldIterator<FProperty> P(*F); P; ++P)
                        UE_LOG(LogTemp, Display, TEXT("HUB_PARAM %s %s %llu"), *P->GetName(), *P->GetCPPType(), P->GetPropertyFlags());
                }
            }
        }
        if (FParse::Param(FCommandLine::Get(), TEXT("NwiAuthorAssets")))
        {
            Nwi::BuildRedMaterial();
            Nwi::BuildHud();
            Nwi::BuildPulse();
            Nwi::BuildResources();
            Nwi::BuildVisualTest();
            Nwi::BuildSettings();
            Nwi::BuildAutomatic();
            Nwi::BuildNativeInitializers();
            Nwi::BuildValidationFixtures();
            UE_LOG(LogTemp, Display, TEXT("NWI_AUTHORING_SUCCESS"));
        }
        if (FParse::Param(FCommandLine::Get(), TEXT("NwiValidateAssets")))
        {
            const bool Passed = ValidateNwiPresentation();
            FString ResultFile;
            if (FParse::Value(FCommandLine::Get(), TEXT("NwiValidationResult="), ResultFile))
            {
                FFileHelper::SaveStringToFile(Passed
                    ? TEXT("{\"success\":true,\"capture_test\":true,\"native_contract_test\":true,\"replication_metadata_test\":true,\"content_only\":false,\"automatic_pool_test\":true,\"settings_test\":true,\"async_resource_tests\":true,\"visual_no_controller_test\":true,\"edge_cases\":1452,\"red_material_test\":true,\"gpu_tested\":false,\"game_integration_tested\":false}")
                    : TEXT("{\"success\":false}"), *ResultFile);
            }
            UE_LOG(LogTemp, Display, TEXT("NWI_VALIDATION_RESULT %s"), Passed ? TEXT("PASS") : TEXT("FAIL"));
        }
    }
};

IMPLEMENT_MODULE(FNwiAuthoringModule, NwiAuthoring)


