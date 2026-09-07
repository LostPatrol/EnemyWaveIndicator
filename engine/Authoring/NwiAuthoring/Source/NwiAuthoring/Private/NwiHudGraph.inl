// Shared square-wave color preview and HUD rendering; Hz means complete A/B cycles per second.
UK2Node_CallFunction* PaintWarning(UEdGraph* G, UEdGraphNode* Target, const TCHAR* TargetPin, UEdGraphNode* A, const TCHAR* APin, UEdGraphNode* B, const TCHAR* BPin, UEdGraphNode* Hz, const TCHAR* HzPin, UEdGraphNode* Enabled, const TCHAR* EnabledPin)
{
    auto* Frequency=Call(G,UKismetMathLibrary::StaticClass(),TEXT("FClamp")); Link(Hz,HzPin,Frequency,TEXT("Value"));Value(Frequency,TEXT("Min"),TEXT("0.1"));Value(Frequency,TEXT("Max"),TEXT("10"));
    auto* Speed=Call(G,UKismetMathLibrary::StaticClass(),TEXT("Multiply_FloatFloat"));Link(Frequency,TEXT("ReturnValue"),Speed,TEXT("A"));Value(Speed,TEXT("B"),TEXT("6.2831853"));
    auto* Phase=Call(G,UKismetMathLibrary::StaticClass(),TEXT("Multiply_FloatFloat"));Link(Call(G,UGameplayStatics::StaticClass(),TEXT("GetRealTimeSeconds")),TEXT("ReturnValue"),Phase,TEXT("A"));Link(Speed,TEXT("ReturnValue"),Phase,TEXT("B"));
    auto* Cos=Call(G,UKismetMathLibrary::StaticClass(),TEXT("Cos"));Link(Phase,TEXT("ReturnValue"),Cos,TEXT("A"));
    auto* Half=Call(G,UKismetMathLibrary::StaticClass(),TEXT("GreaterEqual_FloatFloat"));Link(Cos,TEXT("ReturnValue"),Half,TEXT("A"));
    auto* Off=Call(G,UKismetMathLibrary::StaticClass(),TEXT("Not_PreBool"));Link(Enabled,EnabledPin,Off,TEXT("A"));
    auto* Pick=Call(G,UKismetMathLibrary::StaticClass(),TEXT("BooleanOR"));Link(Half,TEXT("ReturnValue"),Pick,TEXT("A"));Link(Off,TEXT("ReturnValue"),Pick,TEXT("B"));
    auto* Color=Call(G,UKismetMathLibrary::StaticClass(),TEXT("SelectColor"));Link(A,APin,Color,TEXT("A"));Link(B,BPin,Color,TEXT("B"));Link(Pick,TEXT("ReturnValue"),Color,TEXT("bPickA"));
    auto* Slate=Node<UK2Node_MakeStruct>(G);Slate->StructType=FSlateColor::StaticStruct();Slate->AllocateDefaultPins();Link(Color,TEXT("ReturnValue"),Slate,TEXT("SpecifiedColor"));
    auto* Paint=Call(G,UTextBlock::StaticClass(),TEXT("SetColorAndOpacity"));Link(Target,TargetPin,Paint,TEXT("self"));Link(Slate,TEXT("SlateColor"),Paint,TEXT("InColorAndOpacity"));return Paint;
}
// Build stock Blueprint arithmetic for a DPI-local edge indicator; no runtime plugin dependency.
struct FHudValue { UEdGraphNode* Node = nullptr; FString Pin; FString Literal; };

void BuildPlacement(UBlueprint* BP, UEdGraph* G)
{
    auto* EventNode = Node<UK2Node_CustomEvent>(G);
    EventNode->CustomFunctionName = TEXT("UpdatePlacement"); EventNode->AllocateDefaultPins();
    const auto V2 = Type(UEdGraphSchema_K2::PC_Struct, TBaseStructure<FVector2D>::Get());
    EventNode->CreateUserDefinedPin(TEXT("Projected"), V2, EGPD_Output);
    EventNode->CreateUserDefinedPin(TEXT("Viewport"), V2, EGPD_Output);
    EventNode->CreateUserDefinedPin(TEXT("LabelSize"), V2, EGPD_Output);
    EventNode->CreateUserDefinedPin(TEXT("CameraDelta"), Type(UEdGraphSchema_K2::PC_Struct, TBaseStructure<FVector>::Get()), EGPD_Output);
    EventNode->CreateUserDefinedPin(TEXT("Front"), Type(UEdGraphSchema_K2::PC_Boolean), EGPD_Output);
    UEdGraphNode* Exec = EventNode;
    auto Lit = [](const ANSICHAR* S) { return FHudValue{nullptr, TEXT(""), UTF8_TO_TCHAR(S)}; };
    auto Bind = [](FHudValue V, UEdGraphNode* N, const TCHAR* P) {
        if (V.Node) Link(V.Node, *V.Pin, N, P); else Value(N, P, *V.Literal);
    };
    auto Unary = [&](const TCHAR* Name, FHudValue A) {
        auto* N = Call(G, UKismetMathLibrary::StaticClass(), Name); Bind(A, N, TEXT("A"));
        return FHudValue{N, TEXT("ReturnValue"), TEXT("")};
    };
    auto Binary = [&](const TCHAR* Name, FHudValue A, FHudValue B) {
        auto* N = Call(G, UKismetMathLibrary::StaticClass(), Name); Bind(A, N, TEXT("A")); Bind(B, N, TEXT("B"));
        return FHudValue{N, TEXT("ReturnValue"), TEXT("")};
    };
    auto Pick = [&](FHudValue A, FHudValue B, FHudValue Test) {
        auto* N = Call(G, UKismetMathLibrary::StaticClass(), TEXT("SelectFloat"));
        Bind(A, N, TEXT("A")); Bind(B, N, TEXT("B")); Bind(Test, N, TEXT("bPickA"));
        return FHudValue{N, TEXT("ReturnValue"), TEXT("")};
    };
    // Cache intermediates once per event; shared pure subgraphs must not be reevaluated exponentially.
    auto Cache = [&](const TCHAR* Name, FHudValue V, FName Kind = UEdGraphSchema_K2::PC_Float) {
        Variable(BP, Name, Type(Kind)); auto* N = Set(G, Name); Bind(V, N, Name);
        Link(Exec, TEXT("then"), N, TEXT("execute")); Exec = N;
        return FHudValue{Get(G, Name), Name, TEXT("")};
    };
    auto Split = [&](const TCHAR* Input, bool Three = false) {
        auto* N = Call(G, UKismetMathLibrary::StaticClass(), Three ? TEXT("BreakVector") : TEXT("BreakVector2D"));
        Link(EventNode, Input, N, TEXT("InVec")); return N;
    };
    auto Out = [](UEdGraphNode* N, const TCHAR* P) { return FHudValue{N, P, TEXT("")}; };
    auto* Size = Split(TEXT("Viewport")); auto* Label = Split(TEXT("LabelSize"));
    auto* Projected = Split(TEXT("Projected")); auto* Camera = Split(TEXT("CameraDelta"), true);
    auto CX = Cache(TEXT("CenterX"), Binary(TEXT("Multiply_FloatFloat"), Out(Size, TEXT("X")), Lit("0.5")));
    auto CY = Cache(TEXT("CenterY"), Binary(TEXT("Multiply_FloatFloat"), Out(Size, TEXT("Y")), Lit("0.5")));
    // Inset by measured text size plus arrow room; retain usable bounds even on a small viewport.
    auto Extent = [&](FHudValue C, FHudValue TextSize, const ANSICHAR* Gap) {
        auto Margin = Binary(TEXT("FMin"), Binary(TEXT("Multiply_FloatFloat"), C, Lit("0.8")),
            Binary(TEXT("Add_FloatFloat"), Binary(TEXT("Multiply_FloatFloat"), TextSize, Lit("0.5")), Lit(Gap)));
        return Binary(TEXT("FMax"), Binary(TEXT("Subtract_FloatFloat"), C, Margin), Lit("1"));
    };
    auto HX = Cache(TEXT("HalfSafeX"), Extent(CX, Out(Label, TEXT("X")), "24"));
    auto HY = Cache(TEXT("HalfSafeY"), Extent(CY, Out(Label, TEXT("Y")), "44"));
    auto Front = Out(EventNode, TEXT("Front"));
    auto DX = Cache(TEXT("DirectionX"), Pick(Binary(TEXT("Subtract_FloatFloat"), Out(Projected, TEXT("X")), CX), Out(Camera, TEXT("Y")), Front));
    auto DY0 = Cache(TEXT("RawDirectionY"), Pick(Binary(TEXT("Subtract_FloatFloat"), Out(Projected, TEXT("Y")), CY), Binary(TEXT("Multiply_FloatFloat"), Out(Camera, TEXT("Z")), Lit("-1")), Front));
    auto Degenerate = Binary(TEXT("BooleanAND"), Unary(TEXT("Not_PreBool"), Front), Binary(TEXT("Less_FloatFloat"),
        Binary(TEXT("Add_FloatFloat"), Unary(TEXT("Abs"), DX), Unary(TEXT("Abs"), DY0)), Lit("0.0001")));
    // Directly behind has no unique left/right bearing: choose the bottom edge deterministically.
    auto DY = Cache(TEXT("DirectionY"), Pick(Lit("1"), DY0, Degenerate));
    auto Ratio = Cache(TEXT("EdgeRatio"), Binary(TEXT("FMax"), Lit("0.0001"), Binary(TEXT("FMax"),
        Binary(TEXT("Divide_FloatFloat"), Unary(TEXT("Abs"), DX), HX), Binary(TEXT("Divide_FloatFloat"), Unary(TEXT("Abs"), DY), HY))));
    auto Edge = Cache(TEXT("AtEdge"), Binary(TEXT("BooleanOR"), Unary(TEXT("Not_PreBool"), Front), Binary(TEXT("Greater_FloatFloat"), Ratio, Lit("1"))), UEdGraphSchema_K2::PC_Boolean);
    auto Scale = Cache(TEXT("EdgeScale"), Pick(Binary(TEXT("Divide_FloatFloat"), Lit("1"), Ratio), Lit("1"), Edge));
    auto* Position = Call(G, UKismetMathLibrary::StaticClass(), TEXT("MakeVector2D"));
    Bind(Binary(TEXT("Add_FloatFloat"), CX, Binary(TEXT("Multiply_FloatFloat"), DX, Scale)), Position, TEXT("X"));
    Bind(Binary(TEXT("Add_FloatFloat"), CY, Binary(TEXT("Multiply_FloatFloat"), DY, Scale)), Position, TEXT("Y"));
    Variable(BP, TEXT("MarkerPosition"), V2);
    auto* SavePosition = Set(G, TEXT("MarkerPosition")); Link(Position, TEXT("ReturnValue"), SavePosition, TEXT("MarkerPosition"));
    Link(Exec, TEXT("then"), SavePosition, TEXT("execute")); Exec = SavePosition;
    auto* Angle = Call(G, UKismetMathLibrary::StaticClass(), TEXT("DegAtan2")); Bind(DX, Angle, TEXT("X")); Bind(DY, Angle, TEXT("Y"));
    Cache(TEXT("ArrowAngle"), Out(Angle, TEXT("ReturnValue")));
    Cache(TEXT("ArrowOpacity"), Pick(Lit("1"), Lit("0"), Edge));
}

// Connect the placement event to current camera/viewport data and then update existing widgets.
// Per-frame arithmetic is bounded; FString/FText formatting happens only on a rounded-meter change.
void BuildWarningTick(UBlueprint* BP, UEdGraph* G)
{
    auto* Start = Node<UK2Node_CustomEvent>(G); Start->CustomFunctionName = TEXT("UpdateWarning"); Start->AllocateDefaultPins();
    auto* Marker = Get(G, TEXT("MarkerText"));
    auto* Fade=PaintWarning(G,Marker,TEXT("MarkerText"),Get(G,TEXT("BlinkA")),TEXT("BlinkA"),Get(G,TEXT("BlinkB")),TEXT("BlinkB"),Get(G,TEXT("BlinkHz")),TEXT("BlinkHz"),Get(G,TEXT("BlinkEnabled")),TEXT("BlinkEnabled"));Link(Start,TEXT("then"),Fade,TEXT("execute"));
    auto* Pawn = Call(G, UUserWidget::StaticClass(), TEXT("GetOwningPlayerPawn"));
    auto* ValidPawn = Call(G, UKismetSystemLibrary::StaticClass(), TEXT("IsValid")); Link(Pawn, TEXT("ReturnValue"), ValidPawn, TEXT("Object"));
    auto* Gate = Node<UK2Node_IfThenElse>(G); Gate->AllocateDefaultPins(); Link(ValidPawn, TEXT("ReturnValue"), Gate, TEXT("Condition")); Link(Fade, TEXT("then"), Gate, TEXT("execute"));
    auto* Position = Call(G, AActor::StaticClass(), TEXT("K2_GetActorLocation")); Link(Pawn, TEXT("ReturnValue"), Position, TEXT("self"));
    auto* Distance = Call(G, UKismetMathLibrary::StaticClass(), TEXT("Vector_Distance")); Link(Position, TEXT("ReturnValue"), Distance, TEXT("V1")); Link(Get(G, TEXT("WorldLocation")), TEXT("WorldLocation"), Distance, TEXT("V2"));
    auto* Meters = Call(G, UKismetMathLibrary::StaticClass(), TEXT("Divide_FloatFloat")); Link(Distance, TEXT("ReturnValue"), Meters, TEXT("A")); Value(Meters, TEXT("B"), TEXT("100"));
    auto* Rounded = Call(G, UKismetMathLibrary::StaticClass(), TEXT("Round")); Link(Meters, TEXT("ReturnValue"), Rounded, TEXT("A"));
    auto* Changed = Call(G, UKismetMathLibrary::StaticClass(), TEXT("NotEqual_IntInt")); Link(Rounded, TEXT("ReturnValue"), Changed, TEXT("A")); Link(Get(G, TEXT("LastMeters")), TEXT("LastMeters"), Changed, TEXT("B"));
    auto* Update = Node<UK2Node_IfThenElse>(G); Update->AllocateDefaultPins(); Link(Changed, TEXT("ReturnValue"), Update, TEXT("Condition")); Link(Gate, TEXT("then"), Update, TEXT("execute"));
    auto* Cache = Set(G, TEXT("LastMeters")); Link(Rounded, TEXT("ReturnValue"), Cache, TEXT("LastMeters")); Link(Update, TEXT("then"), Cache, TEXT("execute"));
    auto* Prefix = Call(G, UKismetTextLibrary::StaticClass(), TEXT("Conv_TextToString")); Link(Get(G, TEXT("BaseLabel")), TEXT("BaseLabel"), Prefix, TEXT("InText"));
    auto* Number = Call(G, UKismetStringLibrary::StaticClass(), TEXT("Conv_IntToString")); Link(Get(G, TEXT("LastMeters")), TEXT("LastMeters"), Number, TEXT("InInt"));
    auto* First = Call(G, UKismetStringLibrary::StaticClass(), TEXT("Concat_StrStr")); Link(Prefix, TEXT("ReturnValue"), First, TEXT("A")); Value(First, TEXT("B"), TEXT("  |  "));
    auto* Second = Call(G, UKismetStringLibrary::StaticClass(), TEXT("Concat_StrStr")); Link(First, TEXT("ReturnValue"), Second, TEXT("A")); Link(Number, TEXT("ReturnValue"), Second, TEXT("B"));
    auto* Third = Call(G, UKismetStringLibrary::StaticClass(), TEXT("Concat_StrStr")); Link(Second, TEXT("ReturnValue"), Third, TEXT("A")); Value(Third, TEXT("B"), TEXT(" m"));
    auto* Text = Call(G, UKismetTextLibrary::StaticClass(), TEXT("Conv_StringToText")); Link(Third, TEXT("ReturnValue"), Text, TEXT("InString"));
    auto* Write = Call(G, UTextBlock::StaticClass(), TEXT("SetText")); Link(Marker, TEXT("MarkerText"), Write, TEXT("self")); Link(Text, TEXT("ReturnValue"), Write, TEXT("InText")); Link(Cache, TEXT("then"), Write, TEXT("execute"));
}

void ConnectHudTick(UBlueprint* BP, UEdGraph* G)
{
    auto* Tick = Event(G, UUserWidget::StaticClass(), TEXT("Tick"));
    auto* Player = Call(G, UWidget::StaticClass(), TEXT("GetOwningPlayer"));
    auto* Location = Get(G, TEXT("WorldLocation"));
    auto* Project = Call(G, UWidgetLayoutLibrary::StaticClass(), TEXT("ProjectWorldLocationToWidgetPosition"));
    Link(Player, TEXT("ReturnValue"), Project, TEXT("PlayerController")); Link(Location, TEXT("WorldLocation"), Project, TEXT("WorldLocation"));
    Value(Project, TEXT("bPlayerViewportRelative"), TEXT("true"));
    auto* View = Call(G, UWidgetLayoutLibrary::StaticClass(), TEXT("GetViewportSize"));
    auto* DPI = Call(G, UWidgetLayoutLibrary::StaticClass(), TEXT("GetViewportScale"));
    auto* LocalSize = Call(G, UKismetMathLibrary::StaticClass(), TEXT("Divide_Vector2DFloat"));
    Link(View, TEXT("ReturnValue"), LocalSize, TEXT("A")); Link(DPI, TEXT("ReturnValue"), LocalSize, TEXT("B"));
    auto* Camera = Call(G, UGameplayStatics::StaticClass(), TEXT("GetPlayerCameraManager"));
    auto* CameraPos = Call(G, APlayerCameraManager::StaticClass(), TEXT("GetCameraLocation"));
    auto* Rotation = Call(G, APlayerCameraManager::StaticClass(), TEXT("GetCameraRotation"));
    Link(Camera, TEXT("ReturnValue"), CameraPos, TEXT("self")); Link(Camera, TEXT("ReturnValue"), Rotation, TEXT("self"));
    auto* Delta = Call(G, UKismetMathLibrary::StaticClass(), TEXT("Subtract_VectorVector"));
    Link(Location, TEXT("WorldLocation"), Delta, TEXT("A")); Link(CameraPos, TEXT("ReturnValue"), Delta, TEXT("B"));
    auto* Local = Call(G, UKismetMathLibrary::StaticClass(), TEXT("LessLess_VectorRotator"));
    Link(Delta, TEXT("ReturnValue"), Local, TEXT("A")); Link(Rotation, TEXT("ReturnValue"), Local, TEXT("B"));
    auto* Marker = Get(G, TEXT("MarkerText")); auto* Arrow = Get(G, TEXT("EdgeArrow"));
    auto* Measure = Call(G, UWidget::StaticClass(), TEXT("GetDesiredSize")); Link(Marker, TEXT("MarkerText"), Measure, TEXT("self"));
    auto* Place = Call(G, BP->GeneratedClass, TEXT("UpdatePlacement"));
    Link(Project, TEXT("ScreenPosition"), Place, TEXT("Projected")); Link(Project, TEXT("ReturnValue"), Place, TEXT("Front"));
    Link(LocalSize, TEXT("ReturnValue"), Place, TEXT("Viewport")); Link(Local, TEXT("ReturnValue"), Place, TEXT("CameraDelta"));
    Link(Measure, TEXT("ReturnValue"), Place, TEXT("LabelSize"));
    // Slate calls Tick only while the viewport widget is active; validity guards cover teardown.
    auto* Valid = Call(G, UKismetSystemLibrary::StaticClass(), TEXT("IsValid")); Link(Camera, TEXT("ReturnValue"), Valid, TEXT("Object"));
    auto* Gate = Node<UK2Node_IfThenElse>(G); Gate->AllocateDefaultPins();
    auto* Warning = Call(G, BP->GeneratedClass, TEXT("UpdateWarning")); Link(Tick, TEXT("then"), Warning, TEXT("execute"));
    Link(Valid, TEXT("ReturnValue"), Gate, TEXT("Condition")); Link(Warning, TEXT("then"), Gate, TEXT("execute")); Link(Gate, TEXT("then"), Place, TEXT("execute"));
    auto* Position = Get(G, TEXT("MarkerPosition"));
    auto* Move = Call(G, UWidget::StaticClass(), TEXT("SetRenderTranslation"));
    Link(Marker, TEXT("MarkerText"), Move, TEXT("self")); Link(Position, TEXT("MarkerPosition"), Move, TEXT("Translation")); Link(Place, TEXT("then"), Move, TEXT("execute"));
    auto* Offset = Call(G, UKismetMathLibrary::StaticClass(), TEXT("Add_Vector2DVector2D"));
    Link(Position, TEXT("MarkerPosition"), Offset, TEXT("A")); Value(Offset, TEXT("B"), TEXT("(X=0,Y=-28)"));
    auto* ArrowMove = Call(G, UWidget::StaticClass(), TEXT("SetRenderTranslation"));
    Link(Arrow, TEXT("EdgeArrow"), ArrowMove, TEXT("self")); Link(Offset, TEXT("ReturnValue"), ArrowMove, TEXT("Translation")); Link(Move, TEXT("then"), ArrowMove, TEXT("execute"));
    auto* Turn = Call(G, UWidget::StaticClass(), TEXT("SetRenderTransformAngle"));
    auto* Angle = Get(G, TEXT("ArrowAngle")); Link(Arrow, TEXT("EdgeArrow"), Turn, TEXT("self")); Link(Angle, TEXT("ArrowAngle"), Turn, TEXT("Angle")); Link(ArrowMove, TEXT("then"), Turn, TEXT("execute"));
    auto* Opacity = Call(G, UWidget::StaticClass(), TEXT("SetRenderOpacity")); auto* Alpha = Get(G, TEXT("ArrowOpacity"));
    Link(Arrow, TEXT("EdgeArrow"), Opacity, TEXT("self")); Link(Alpha, TEXT("ArrowOpacity"), Opacity, TEXT("InOpacity")); Link(Turn, TEXT("then"), Opacity, TEXT("execute"));
}


