#include "DreamBlueprintBuilder.h"

#if WITH_EDITOR

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/CollisionProfile.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_Event.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_InputKey.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Materials/MaterialParameterCollection.h"

namespace
{
	/** Small helper that spawns K2 nodes left to right and records every failed connection. */
	struct FGraphBuilder
	{
		UBlueprint* Blueprint = nullptr;
		UEdGraph* Graph = nullptr;
		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		TArray<FString> Errors;
		int32 X = 0;
		int32 Y = 0;

		void NewRow() { X = 0; Y += 420; }

		template <typename T>
		T* Spawn(TFunctionRef<void(T*)> Init)
		{
			FGraphNodeCreator<T> Creator(*Graph);
			T* Node = Creator.CreateNode(false);
			Init(Node);
			Node->NodePosX = X;
			Node->NodePosY = Y;
			X += 300;
			Creator.Finalize();
			return Node;
		}

		UEdGraphPin* Pin(UEdGraphNode* Node, const TCHAR* Name)
		{
			UEdGraphPin* Found = Node ? Node->FindPin(FName(Name)) : nullptr;
			if (!Found)
			{
				FString Available;
				if (Node)
				{
					for (UEdGraphPin* P : Node->Pins) { Available += P->PinName.ToString() + TEXT(" "); }
				}
				Errors.Add(FString::Printf(TEXT("pin '%s' missing on %s (has: %s)"), Name,
					Node ? *Node->GetNodeTitle(ENodeTitleType::ListView).ToString() : TEXT("null"), *Available));
			}
			return Found;
		}

		void Link(UEdGraphPin* From, UEdGraphPin* To)
		{
			if (From && To && !Schema->TryCreateConnection(From, To))
			{
				Errors.Add(FString::Printf(TEXT("cannot connect %s.%s -> %s.%s"),
					*From->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView).ToString(), *From->PinName.ToString(),
					*To->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView).ToString(), *To->PinName.ToString()));
			}
		}

		void Link(UEdGraphNode* A, const TCHAR* PinA, UEdGraphNode* B, const TCHAR* PinB) { Link(Pin(A, PinA), Pin(B, PinB)); }

		/** Exec chain: the "then" of each node into the "execute" of the next. */
		void Chain(std::initializer_list<UEdGraphNode*> Nodes)
		{
			UEdGraphNode* Previous = nullptr;
			for (UEdGraphNode* Node : Nodes)
			{
				if (Previous) { Link(Previous, TEXT("then"), Node, TEXT("execute")); }
				Previous = Node;
			}
		}

		void Default(UEdGraphNode* Node, const TCHAR* PinName, const FString& Value)
		{
			if (UEdGraphPin* P = Pin(Node, PinName)) { Schema->TrySetDefaultValue(*P, Value); }
		}

		UK2Node_CallFunction* Call(UClass* Class, const TCHAR* Function)
		{
			UFunction* Fn = Class->FindFunctionByName(FName(Function));
			if (!Fn)
			{
				Errors.Add(FString::Printf(TEXT("function %s::%s not found"), *Class->GetName(), Function));
				return nullptr;
			}
			return Spawn<UK2Node_CallFunction>([Fn](UK2Node_CallFunction* N) { N->SetFromFunction(Fn); });
		}

		UK2Node_CallFunction* CallSelf(const TCHAR* EventName)
		{
			return Spawn<UK2Node_CallFunction>([EventName](UK2Node_CallFunction* N) { N->FunctionReference.SetSelfMember(FName(EventName)); });
		}

		UK2Node_CustomEvent* Event(const TCHAR* Name, bool bCallInEditor = false)
		{
			NewRow();
			return Spawn<UK2Node_CustomEvent>([Name, bCallInEditor](UK2Node_CustomEvent* N)
			{
				N->CustomFunctionName = FName(Name);
				N->bCallInEditor = bCallInEditor;
			});
		}

		UK2Node_InputKey* Key(const FKey& InKey)
		{
			NewRow();
			return Spawn<UK2Node_InputKey>([InKey](UK2Node_InputKey* N) { N->InputKey = InKey; N->bConsumeInput = false; });
		}

		UK2Node_VariableGet* Get(const TCHAR* Variable)
		{
			return Spawn<UK2Node_VariableGet>([Variable](UK2Node_VariableGet* N) { N->VariableReference.SetSelfMember(FName(Variable)); });
		}

		UK2Node_VariableSet* Set(const TCHAR* Variable)
		{
			return Spawn<UK2Node_VariableSet>([Variable](UK2Node_VariableSet* N) { N->VariableReference.SetSelfMember(FName(Variable)); });
		}

		/** SetScalarParameterValue on the collection variable. Value comes from a pin or a literal. */
		UK2Node_CallFunction* SetScalar(const TCHAR* Parameter, UEdGraphPin* ValuePin, const TCHAR* Literal = nullptr)
		{
			UK2Node_CallFunction* Node = Call(UKismetMaterialLibrary::StaticClass(), TEXT("SetScalarParameterValue"));
			Link(Pin(Get(TEXT("DreamCollection")), TEXT("DreamCollection")), Pin(Node, TEXT("Collection")));
			Default(Node, TEXT("ParameterName"), Parameter);
			if (ValuePin) { Link(ValuePin, Pin(Node, TEXT("ParameterValue"))); }
			else if (Literal) { Default(Node, TEXT("ParameterValue"), Literal); }
			return Node;
		}

		/** SelectFloat(A, B, bPickA) with literal A and B; returns the result pin. */
		UEdGraphPin* Select(const TCHAR* A, const TCHAR* B, UEdGraphPin* PickA)
		{
			UK2Node_CallFunction* Node = Call(UKismetMathLibrary::StaticClass(), TEXT("SelectFloat"));
			Default(Node, TEXT("A"), A);
			Default(Node, TEXT("B"), B);
			Link(PickA, Pin(Node, TEXT("bPickA")));
			return Pin(Node, TEXT("ReturnValue"));
		}

		/** (IntPin == Literal) as a bool pin. */
		UEdGraphPin* IntEquals(UEdGraphPin* IntPin, int32 Literal)
		{
			UK2Node_CallFunction* Node = Call(UKismetMathLibrary::StaticClass(), TEXT("EqualEqual_IntInt"));
			Link(IntPin, Pin(Node, TEXT("A")));
			Default(Node, TEXT("B"), FString::FromInt(Literal));
			return Pin(Node, TEXT("ReturnValue"));
		}

		UEdGraphPin* Bool2(const TCHAR* Function, UEdGraphPin* A, UEdGraphPin* B)
		{
			UK2Node_CallFunction* Node = Call(UKismetMathLibrary::StaticClass(), Function);
			Link(A, Pin(Node, TEXT("A")));
			Link(B, Pin(Node, TEXT("B")));
			return Pin(Node, TEXT("ReturnValue"));
		}

		UK2Node_MacroInstance* ForEach()
		{
			UBlueprint* Macros = LoadObject<UBlueprint>(nullptr, TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros"));
			UEdGraph* MacroGraph = nullptr;
			if (Macros)
			{
				for (UEdGraph* Candidate : Macros->MacroGraphs)
				{
					if (Candidate && Candidate->GetFName() == FName(TEXT("ForEachLoop"))) { MacroGraph = Candidate; }
				}
			}
			if (!MacroGraph)
			{
				Errors.Add(TEXT("ForEachLoop macro not found"));
				return nullptr;
			}
			return Spawn<UK2Node_MacroInstance>([MacroGraph](UK2Node_MacroInstance* N) { N->SetMacroGraph(MacroGraph); });
		}

		/** Sets APostProcessVolume::bEnabled on the volume coming from TargetPin. */
		UK2Node_VariableSet* SetVolumeEnabled(UEdGraphPin* TargetPin, UEdGraphPin* ValuePin, const TCHAR* Literal = nullptr)
		{
			UK2Node_VariableSet* Node = Spawn<UK2Node_VariableSet>([](UK2Node_VariableSet* N)
			{
				N->VariableReference.SetExternalMember(FName(TEXT("bEnabled")), APostProcessVolume::StaticClass());
			});
			Link(TargetPin, Pin(Node, TEXT("self")));
			if (ValuePin) { Link(ValuePin, Pin(Node, TEXT("bEnabled"))); }
			else if (Literal) { Default(Node, TEXT("bEnabled"), Literal); }
			return Node;
		}
	};

	FEdGraphPinType MakeType(FName Category, UObject* SubObject = nullptr, FName SubCategory = NAME_None)
	{
		FEdGraphPinType Type;
		Type.PinCategory = Category;
		Type.PinSubCategory = SubCategory;
		Type.PinSubCategoryObject = SubObject;
		return Type;
	}

	UBlueprint* CreateActorBlueprint(const FString& PackagePath, const FString& Name, FString& OutError)
	{
		const FString PackageName = PackagePath / Name;
		if (FindObject<UBlueprint>(nullptr, *(PackageName + TEXT(".") + Name)) || FPackageName::DoesPackageExist(PackageName))
		{
			OutError = FString::Printf(TEXT("%s already exists: delete it first"), *PackageName);
			return nullptr;
		}
		UPackage* Package = CreatePackage(*PackageName);
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, FName(*Name),
			BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), NAME_None);
		if (Blueprint)
		{
			FAssetRegistryModule::AssetCreated(Blueprint);
			Package->MarkPackageDirty();
		}
		else
		{
			OutError = TEXT("CreateBlueprint failed for ") + PackageName;
		}
		return Blueprint;
	}

	void AddVariable(UBlueprint* Blueprint, const TCHAR* Name, const FEdGraphPinType& Type, const FString& DefaultValue, const TCHAR* Tooltip)
	{
		FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(Name), Type, DefaultValue);
		FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(Blueprint, FName(Name), false);     // editable on instances
		FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(Name), nullptr, FText::FromString(TEXT("DreamDemo")));
		FBlueprintEditorUtils::SetBlueprintVariableMetaData(Blueprint, FName(Name), nullptr, TEXT("tooltip"), Tooltip);
	}

	FString BuildInstancedField(const FString& PackagePath)
	{
		FString Error;
		UBlueprint* Blueprint = CreateActorBlueprint(PackagePath, TEXT("BP_DreamInstancedField"), Error);
		if (!Blueprint) { return Error; }

		USCS_Node* Node = Blueprint->SimpleConstructionScript->CreateNode(UInstancedStaticMeshComponent::StaticClass(), TEXT("Instances"));
		Blueprint->SimpleConstructionScript->AddNode(Node);
		UInstancedStaticMeshComponent* Template = CastChecked<UInstancedStaticMeshComponent>(Node->ComponentTemplate);
		Template->SetMobility(EComponentMobility::Movable);
		Template->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Template->SetGenerateOverlapEvents(false);
		Template->SetCanEverAffectNavigation(false);
		Template->CastShadow = false;
		Template->bAffectDistanceFieldLighting = false;
		Template->bAffectDynamicIndirectLighting = false;
		Template->bVisibleInRayTracing = false;
		Template->bReceivesDecals = false;

		Blueprint->BlueprintDescription = TEXT("DreamDemo: instances of one mesh (star fish school, memory bubbles, cosmic dust). "
			"No graph: the instances are placed by Tools/DreamDemo/ue/step5_dream_vfx.py and all motion is world position offset in the material.");
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		return FString();
	}

	FString BuildWorldState(const FString& PackagePath)
	{
		FString Error;
		UBlueprint* Blueprint = CreateActorBlueprint(PackagePath, TEXT("BP_DreamWorldState"), Error);
		if (!Blueprint) { return Error; }
		Blueprint->BlueprintDescription = TEXT("DreamDemo: switches the level between DAY (0), NIGHT (1) and DREAM (2). "
			"Actors opt in with the tag DreamState plus State_Day / State_Night / State_Dream. "
			"Keys in Play: 1 2 3 states, G dream gate, H wake, F1-F3 or 7-9 cameras, F4 or 0 player camera, M reduced motion. "
			"In the editor use the Apply Day / Apply Night / Apply Dream buttons.");

		const FName Int = UEdGraphSchema_K2::PC_Int;
		const FName Real = UEdGraphSchema_K2::PC_Real;
		const FName Object = UEdGraphSchema_K2::PC_Object;
		const FName Boolean = UEdGraphSchema_K2::PC_Boolean;
		const FName Double = UEdGraphSchema_K2::PC_Double;
		AddVariable(Blueprint, TEXT("InitialState"), MakeType(Int), TEXT("0"), TEXT("State applied at BeginPlay: 0 Day, 1 Night, 2 Dream"));
		AddVariable(Blueprint, TEXT("DreamCollection"), MakeType(Object, UMaterialParameterCollection::StaticClass()), FString(), TEXT("MPC_Dream"));
		AddVariable(Blueprint, TEXT("CamWide"), MakeType(Object, AActor::StaticClass()), FString(), TEXT("F1 / 7"));
		AddVariable(Blueprint, TEXT("CamWall"), MakeType(Object, AActor::StaticClass()), FString(), TEXT("F2 / 8"));
		AddVariable(Blueprint, TEXT("CamSky"), MakeType(Object, AActor::StaticClass()), FString(), TEXT("F3 / 9"));
		AddVariable(Blueprint, TEXT("GateVolume"), MakeType(Object, APostProcessVolume::StaticClass()), FString(), TEXT("Unbound volume holding M_PP_DreamGate; enabled only during a gate transition"));
		AddVariable(Blueprint, TEXT("GateHalfSeconds"), MakeType(Real, nullptr, Double), TEXT("1.2"), TEXT("Seconds for cover and for reveal when entering the dream"));
		AddVariable(Blueprint, TEXT("WakeHalfSeconds"), MakeType(Real, nullptr, Double), TEXT("0.6"), TEXT("Seconds for cover and for reveal when waking up"));
		AddVariable(Blueprint, TEXT("GateHoldSeconds"), MakeType(Real, nullptr, Double), TEXT("0.1"), TEXT("Time fully covered after the lighting swap, so Lumen can settle"));
		AddVariable(Blueprint, TEXT("bStartAtCamWide"), MakeType(Boolean), TEXT("true"), TEXT("Switch to CamWide when Play starts"));
		AddVariable(Blueprint, TEXT("CurrentState"), MakeType(Int), TEXT("0"), TEXT("Last state applied"));
		AddVariable(Blueprint, TEXT("bGateBusy"), MakeType(Boolean), TEXT("false"), TEXT("True while a gate transition runs"));
		AddVariable(Blueprint, TEXT("bReducedMotion"), MakeType(Boolean), TEXT("false"), TEXT("Reduced motion: caustics stand still, props rest, fish hidden, dust slower"));

		FGraphBuilder B;
		B.Blueprint = Blueprint;
		B.Graph = FBlueprintEditorUtils::FindEventGraph(Blueprint);
		if (!B.Graph) { return TEXT("event graph missing"); }
		for (UEdGraphNode* Old : TArray<UEdGraphNode*>(B.Graph->Nodes)) { B.Graph->RemoveNode(Old); }   // drop the ghost nodes

		// Events first, then a compile, so that calls to them and variable nodes resolve.
		UK2Node_CustomEvent* ApplyState = B.Event(TEXT("ApplyState"));
		ApplyState->CreateUserDefinedPin(TEXT("NewState"), MakeType(Int), EGPD_Output);
		UK2Node_CustomEvent* Gate = B.Event(TEXT("Gate"));
		Gate->CreateUserDefinedPin(TEXT("TargetState"), MakeType(Int), EGPD_Output);
		UK2Node_CustomEvent* ApplyDay = B.Event(TEXT("ApplyDay"), true);
		UK2Node_CustomEvent* ApplyNight = B.Event(TEXT("ApplyNight"), true);
		UK2Node_CustomEvent* ApplyDream = B.Event(TEXT("ApplyDream"), true);
		UK2Node_CustomEvent* ViewWide = B.Event(TEXT("ViewWide"));
		UK2Node_CustomEvent* ViewWall = B.Event(TEXT("ViewWall"));
		UK2Node_CustomEvent* ViewSky = B.Event(TEXT("ViewSky"));
		UK2Node_CustomEvent* ViewPlayer = B.Event(TEXT("ViewPlayer"));
		UK2Node_CustomEvent* ToggleMotion = B.Event(TEXT("ToggleReducedMotion"));
		FKismetEditorUtilities::CompileBlueprint(Blueprint);

		UClass* Math = UKismetMathLibrary::StaticClass();
		UClass* System = UKismetSystemLibrary::StaticClass();
		UClass* Statics = UGameplayStatics::StaticClass();

		// ---- ApplyState(NewState) ------------------------------------------------------------
		{
			B.X = 300; B.Y = ApplyState->NodePosY;
			UEdGraphPin* State = B.Pin(ApplyState, TEXT("NewState"));
			UK2Node_VariableSet* Remember = B.Set(TEXT("CurrentState"));
			B.Link(State, B.Pin(Remember, TEXT("CurrentState")));
			UK2Node_CallFunction* World = B.SetScalar(TEXT("WorldBlend"), B.Select(TEXT("1.0"), TEXT("0.0"), B.IntEquals(State, 2)));
			UK2Node_CallFunction* Night = B.SetScalar(TEXT("NightBlend"), B.Select(TEXT("1.0"), TEXT("0.0"), B.IntEquals(State, 1)));
			UK2Node_CallFunction* Find = B.Call(Statics, TEXT("GetAllActorsWithTag"));
			B.Default(Find, TEXT("Tag"), TEXT("DreamState"));
			UK2Node_MacroInstance* Actors = B.ForEach();
			B.Link(Find, TEXT("OutActors"), Actors, TEXT("Array"));
			B.Chain({ ApplyState, Remember, World, Night, Find });
			B.Link(Find, TEXT("then"), Actors, TEXT("Exec"));

			// bActive = the actor carries the tag of the new state
			UEdGraphPin* Actor = B.Pin(Actors, TEXT("Array Element"));
			UEdGraphPin* Active = nullptr;
			const TCHAR* Tags[3] = { TEXT("State_Day"), TEXT("State_Night"), TEXT("State_Dream") };
			for (int32 Index = 0; Index < 3; ++Index)
			{
				UK2Node_CallFunction* HasTag = B.Call(AActor::StaticClass(), TEXT("ActorHasTag"));
				B.Link(Actor, B.Pin(HasTag, TEXT("self")));
				B.Default(HasTag, TEXT("Tag"), Tags[Index]);
				UEdGraphPin* Both = B.Bool2(TEXT("BooleanAND"), B.IntEquals(State, Index), B.Pin(HasTag, TEXT("ReturnValue")));
				Active = Active ? B.Bool2(TEXT("BooleanOR"), Active, Both) : Both;
			}

			// every scene component of the actor follows bActive (lights, sky, fog, meshes, decals)
			B.X = 2400; B.Y += 220;
			UK2Node_CallFunction* Components = B.Call(AActor::StaticClass(), TEXT("K2_GetComponentsByClass"));
			B.Link(Actor, B.Pin(Components, TEXT("self")));
			if (UEdGraphPin* ClassPin = B.Pin(Components, TEXT("ComponentClass")))
			{
				B.Schema->TrySetDefaultObject(*ClassPin, USceneComponent::StaticClass());
			}
			UK2Node_MacroInstance* Each = B.ForEach();
			B.Link(Components, TEXT("ReturnValue"), Each, TEXT("Array"));
			B.Link(Actors, TEXT("LoopBody"), Each, TEXT("Exec"));
			UK2Node_CallFunction* Visible = B.Call(USceneComponent::StaticClass(), TEXT("SetVisibility"));
			B.Link(B.Pin(Each, TEXT("Array Element")), B.Pin(Visible, TEXT("self")));
			B.Link(Active, B.Pin(Visible, TEXT("bNewVisibility")));
			B.Default(Visible, TEXT("bPropagateToChildren"), TEXT("false"));
			B.Link(Each, TEXT("LoopBody"), Visible, TEXT("execute"));

			// post-process volumes ignore visibility: switch bEnabled
			UK2Node_DynamicCast* Cast = B.Spawn<UK2Node_DynamicCast>([](UK2Node_DynamicCast* N)
			{
				N->TargetType = APostProcessVolume::StaticClass();
				N->SetPurity(false);
			});
			B.Link(Actor, Cast->GetCastSourcePin());
			B.Link(Each, TEXT("Completed"), Cast, TEXT("execute"));
			UK2Node_VariableSet* Enable = B.SetVolumeEnabled(Cast->GetCastResultPin(), Active);
			B.Link(Cast, TEXT("then"), Enable, TEXT("execute"));
		}

		// ---- editor buttons and BeginPlay -----------------------------------------------------
		auto CallApply = [&B](UEdGraphNode* From, const TCHAR* FromPin, int32 State, UEdGraphPin* StatePin)
		{
			UK2Node_CallFunction* Node = B.CallSelf(TEXT("ApplyState"));
			if (StatePin) { B.Link(StatePin, B.Pin(Node, TEXT("NewState"))); }
			else { B.Default(Node, TEXT("NewState"), FString::FromInt(State)); }
			B.Link(From, FromPin, Node, TEXT("execute"));
			return Node;
		};
		UK2Node_CustomEvent* Buttons[3] = { ApplyDay, ApplyNight, ApplyDream };
		for (int32 Index = 0; Index < 3; ++Index)
		{
			B.X = 300; B.Y = Buttons[Index]->NodePosY;
			CallApply(Buttons[Index], TEXT("then"), Index, nullptr);
		}
		{
			B.NewRow();
			UK2Node_Event* BeginPlay = B.Spawn<UK2Node_Event>([](UK2Node_Event* N)
			{
				N->EventReference.SetExternalMember(FName(TEXT("ReceiveBeginPlay")), AActor::StaticClass());
				N->bOverrideFunction = true;
			});
			UK2Node_CallFunction* Reset = B.SetScalar(TEXT("GateMode"), nullptr, TEXT("0.0"));
			B.Link(BeginPlay, TEXT("then"), Reset, TEXT("execute"));
			UK2Node_CallFunction* Apply = CallApply(Reset, TEXT("then"), 0, B.Pin(B.Get(TEXT("InitialState")), TEXT("InitialState")));
			UK2Node_IfThenElse* StartWide = B.Spawn<UK2Node_IfThenElse>([](UK2Node_IfThenElse*) {});
			B.Link(B.Pin(B.Get(TEXT("bStartAtCamWide")), TEXT("bStartAtCamWide")), B.Pin(StartWide, TEXT("Condition")));
			B.Link(Apply, TEXT("then"), StartWide, TEXT("execute"));
			B.Link(StartWide, TEXT("then"), B.CallSelf(TEXT("ViewWide")), TEXT("execute"));
		}

		// ---- Gate(TargetState): cover, swap while covered, reveal ----------------------------
		{
			B.X = 300; B.Y = Gate->NodePosY;
			UEdGraphPin* Target = B.Pin(Gate, TEXT("TargetState"));
			UK2Node_IfThenElse* Busy = B.Spawn<UK2Node_IfThenElse>([](UK2Node_IfThenElse*) {});
			B.Link(B.Pin(B.Get(TEXT("bGateBusy")), TEXT("bGateBusy")), B.Pin(Busy, TEXT("Condition")));
			B.Link(Gate, TEXT("then"), Busy, TEXT("execute"));
			UK2Node_VariableSet* SetBusy = B.Set(TEXT("bGateBusy"));
			B.Default(SetBusy, TEXT("bGateBusy"), TEXT("true"));
			B.Link(Busy, TEXT("else"), SetBusy, TEXT("execute"));

			// half duration: GateHalfSeconds into the dream, WakeHalfSeconds out of it
			auto Half = [&B, Target]()
			{
				UK2Node_CallFunction* Select = B.Call(UKismetMathLibrary::StaticClass(), TEXT("SelectFloat"));
				B.Link(B.Pin(B.Get(TEXT("GateHalfSeconds")), TEXT("GateHalfSeconds")), B.Pin(Select, TEXT("A")));
				B.Link(B.Pin(B.Get(TEXT("WakeHalfSeconds")), TEXT("WakeHalfSeconds")), B.Pin(Select, TEXT("B")));
				B.Link(B.IntEquals(Target, 2), B.Pin(Select, TEXT("bPickA")));
				return B.Pin(Select, TEXT("ReturnValue"));
			};
			auto Now = [&B, System]() { return B.Pin(B.Call(System, TEXT("GetGameTimeInSeconds")), TEXT("ReturnValue")); };
			auto Wait = [&B, System](UEdGraphPin* Duration)
			{
				UK2Node_CallFunction* Delay = B.Call(System, TEXT("Delay"));
				B.Link(Duration, B.Pin(Delay, TEXT("Duration")));
				return Delay;
			};
			auto Volume = [&B]() { return B.Pin(B.Get(TEXT("GateVolume")), TEXT("GateVolume")); };

			UK2Node_CallFunction* Duration = B.SetScalar(TEXT("GateDuration"), Half());
			UK2Node_CallFunction* Start = B.SetScalar(TEXT("GateStartTime"), Now());
			UK2Node_CallFunction* Cover = B.SetScalar(TEXT("GateMode"), nullptr, TEXT("1.0"));
			UK2Node_VariableSet* On = B.SetVolumeEnabled(Volume(), nullptr, TEXT("true"));
			UK2Node_CallFunction* WaitCover = Wait(Half());
			B.NewRow(); B.X = 600;
			UK2Node_CallFunction* Swap = B.CallSelf(TEXT("ApplyState"));
			B.Link(Target, B.Pin(Swap, TEXT("NewState")));
			UK2Node_CallFunction* Hold = Wait(B.Pin(B.Get(TEXT("GateHoldSeconds")), TEXT("GateHoldSeconds")));
			UK2Node_CallFunction* Start2 = B.SetScalar(TEXT("GateStartTime"), Now());
			UK2Node_CallFunction* Reveal = B.SetScalar(TEXT("GateMode"), nullptr, TEXT("2.0"));
			UK2Node_CallFunction* WaitReveal = Wait(Half());
			UK2Node_CallFunction* Done = B.SetScalar(TEXT("GateMode"), nullptr, TEXT("0.0"));
			UK2Node_VariableSet* Off = B.SetVolumeEnabled(Volume(), nullptr, TEXT("false"));
			UK2Node_VariableSet* Free = B.Set(TEXT("bGateBusy"));
			B.Default(Free, TEXT("bGateBusy"), TEXT("false"));
			B.Chain({ SetBusy, Duration, Start, Cover, On, WaitCover, Swap, Hold, Start2, Reveal, WaitReveal, Done, Off, Free });
		}

		// ---- cameras ---------------------------------------------------------------------------
		auto View = [&B, Statics](UK2Node_CustomEvent* Event, const TCHAR* Variable)
		{
			B.X = 300; B.Y = Event->NodePosY;
			UK2Node_CallFunction* Controller = B.Call(Statics, TEXT("GetPlayerController"));
			UK2Node_CallFunction* SetView = B.Call(APlayerController::StaticClass(), TEXT("SetViewTargetWithBlend"));
			B.Link(B.Pin(Controller, TEXT("ReturnValue")), B.Pin(SetView, TEXT("self")));
			if (Variable)
			{
				B.Link(B.Pin(B.Get(Variable), Variable), B.Pin(SetView, TEXT("NewViewTarget")));
			}
			else
			{
				UK2Node_CallFunction* Pawn = B.Call(Statics, TEXT("GetPlayerPawn"));
				B.Link(B.Pin(Pawn, TEXT("ReturnValue")), B.Pin(SetView, TEXT("NewViewTarget")));
			}
			B.Default(SetView, TEXT("BlendTime"), TEXT("0.35"));
			B.Link(Event, TEXT("then"), SetView, TEXT("execute"));
		};
		View(ViewWide, TEXT("CamWide"));
		View(ViewWall, TEXT("CamWall"));
		View(ViewSky, TEXT("CamSky"));
		View(ViewPlayer, nullptr);

		// ---- reduced motion --------------------------------------------------------------------
		{
			B.X = 300; B.Y = ToggleMotion->NodePosY;
			UK2Node_CallFunction* Not = B.Call(Math, TEXT("Not_PreBool"));
			B.Link(B.Pin(B.Get(TEXT("bReducedMotion")), TEXT("bReducedMotion")), B.Pin(Not, TEXT("A")));
			UK2Node_VariableSet* Store = B.Set(TEXT("bReducedMotion"));
			B.Link(B.Pin(Not, TEXT("ReturnValue")), B.Pin(Store, TEXT("bReducedMotion")));
			auto Reduced = [&B]() { return B.Pin(B.Get(TEXT("bReducedMotion")), TEXT("bReducedMotion")); };
			UK2Node_CallFunction* Speed = B.SetScalar(TEXT("CausticsSpeed"), B.Select(TEXT("0.0"), TEXT("0.15"), Reduced()));
			UK2Node_CallFunction* Float = B.SetScalar(TEXT("FloatAmount"), B.Select(TEXT("0.0"), TEXT("1.0"), Reduced()));
			UK2Node_CallFunction* Dust = B.SetScalar(TEXT("DustSpeed"), B.Select(TEXT("0.3"), TEXT("1.0"), Reduced()));
			UK2Node_CallFunction* Fish = B.SetScalar(TEXT("MotionReduced"), B.Select(TEXT("1.0"), TEXT("0.0"), Reduced()));
			B.Chain({ ToggleMotion, Store, Speed, Float, Dust, Fish });
		}

		// ---- keys ------------------------------------------------------------------------------
		auto Bind = [&B](const FKey& InKey, const TCHAR* EventName, int32 IntArgument, const TCHAR* ArgumentPin)
		{
			UK2Node_InputKey* KeyNode = B.Key(InKey);
			UK2Node_CallFunction* Node = B.CallSelf(EventName);
			if (ArgumentPin) { B.Default(Node, ArgumentPin, FString::FromInt(IntArgument)); }
			B.Link(KeyNode, TEXT("Pressed"), Node, TEXT("execute"));
		};
		Bind(EKeys::One, TEXT("ApplyState"), 0, TEXT("NewState"));
		Bind(EKeys::Two, TEXT("ApplyState"), 1, TEXT("NewState"));
		Bind(EKeys::Three, TEXT("ApplyState"), 2, TEXT("NewState"));
		Bind(EKeys::G, TEXT("Gate"), 2, TEXT("TargetState"));
		Bind(EKeys::H, TEXT("Gate"), 0, TEXT("TargetState"));
		Bind(EKeys::F1, TEXT("ViewWide"), 0, nullptr);
		Bind(EKeys::Seven, TEXT("ViewWide"), 0, nullptr);
		Bind(EKeys::F2, TEXT("ViewWall"), 0, nullptr);
		Bind(EKeys::Eight, TEXT("ViewWall"), 0, nullptr);
		Bind(EKeys::F3, TEXT("ViewSky"), 0, nullptr);
		Bind(EKeys::Nine, TEXT("ViewSky"), 0, nullptr);
		Bind(EKeys::F4, TEXT("ViewPlayer"), 0, nullptr);
		Bind(EKeys::Zero, TEXT("ViewPlayer"), 0, nullptr);
		Bind(EKeys::M, TEXT("ToggleReducedMotion"), 0, nullptr);

		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		if (Blueprint->Status == BS_Error)
		{
			B.Errors.Add(TEXT("blueprint compiled with errors"));
		}
		return FString::Join(B.Errors, TEXT("\n"));
	}
}

FString UDreamBlueprintBuilder::BuildDreamBlueprints(const FString& PackagePath)
{
	const FString FieldError = BuildInstancedField(PackagePath);
	const FString StateError = BuildWorldState(PackagePath);
	return FieldError.IsEmpty() ? StateError : (FieldError + TEXT("\n") + StateError);
}

#else

FString UDreamBlueprintBuilder::BuildDreamBlueprints(const FString&)
{
	return TEXT("editor only");
}

#endif
