#include "Heist/RobberDevMenu.h"

#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Heist/RobberExhibitTalk.h"
#include "ProductionGameCharacter.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ContentWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "GameFramework/PlayerState.h"
#include "Heist/RobberBreakInProps.h"
#include "Heist/RobberDevText.h"
#include "Heist/RobberEntryPoint.h"
#include "Heist/RobberInteractables.h"
#include "Heist/RobberJournal.h"
#include "Heist/RobberLaunch.h"
#include "Heist/RobberStage.h"
#include "Heist/RobberTriggerZones.h"
#include "Heist/RobberVault.h"
#include "Components/VerticalBoxSlot.h"
#include "Quest/PlayerQuestComponent.h"
#include "Robber/RobberController.h"
#include "Robber/RobberGameState.h"

void URobberDevMenuClick::OnClicked()
{
	if (URobberDevMenu* Menu = Cast<URobberDevMenu>(GetOuter()))
	{
		Menu->Send(Action, Value);
	}
}

void URobberDevMenu::Bind(const FString& ButtonName, ERobberDevAction Action, int32 Value)
{
	if (UButton* Button = Find<UButton>(ButtonName))
	{
		URobberDevMenuClick* Click = NewObject<URobberDevMenuClick>(this);
		Click->Action = Action;
		Click->Value = Value;
		Button->OnClicked.AddUniqueDynamic(Click, &URobberDevMenuClick::OnClicked);
		Clicks.Add(Click);
	}
}

void URobberDevMenu::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	Bind(TEXT("Night_Tutorial"), ERobberDevAction::SwitchNight, INDEX_NONE);
	for (int32 Night = 1; Night <= 7; ++Night)
	{
		Bind(FString::Printf(TEXT("Night_%d"), Night), ERobberDevAction::SwitchNight, Night - 1);
	}
	Bind(TEXT("Stage_BreakIn"), ERobberDevAction::JumpToStage, RobberStage::BreakIn);
	Bind(TEXT("Stage_Investigate"), ERobberDevAction::JumpToStage, RobberStage::Investigate);
	Bind(TEXT("Stage_Heist"), ERobberDevAction::JumpToStage, RobberStage::Heist);
	Bind(TEXT("Stage_Escape"), ERobberDevAction::JumpToStage, RobberStage::Escape);
	Bind(TEXT("End_Win"), ERobberDevAction::FinishNight, 1);
	Bind(TEXT("End_Lose"), ERobberDevAction::FinishNight, 0);
	Bind(TEXT("GiveCode"), ERobberDevAction::GiveVaultCode);
	Bind(TEXT("GiveKeys"), ERobberDevAction::GiveKeys);
	Bind(TEXT("HeatUp"), ERobberDevAction::AddHeat, 25);
	Bind(TEXT("HeatCool"), ERobberDevAction::CoolHeat);
	Bind(TEXT("GuardsMore"), ERobberDevAction::AddGuardsTime, 60);
	Bind(TEXT("Immunity"), ERobberDevAction::ToggleImmunity);
	Bind(TEXT("ShowQuests"), ERobberDevAction::ShowQuests);
	TriggersLabel = AddToggleButton(TEXT("ShowTriggers"), TEXT("Immunity"), ERobberDevAction::ToggleTriggers);
	DevTextLabel = AddToggleButton(TEXT("ShowDevText"), TEXT("ShowTriggers"), ERobberDevAction::ToggleDevText);
	if (UTextBlock* CreditsLabel = AddToggleButton(TEXT("PlayCredits"), TEXT("ShowDevText"), ERobberDevAction::PlayCredits))
	{
		CreditsLabel->SetText(INVTEXT("Play credits"));
	}
	if (UTextBlock* SecretsLabel = AddToggleButton(TEXT("ShowSecrets"), TEXT("ShowQuests"), ERobberDevAction::ShowSecrets))
	{
		SecretsLabel->SetText(INVTEXT("Map secrets"));
	}
	if (UTextBlock* AllGoLabel = AddToggleButton(TEXT("CompleteSecrets"), TEXT("ShowSecrets"), ERobberDevAction::CompleteSecrets))
	{
		AllGoLabel->SetText(INVTEXT("Secrets: all GO"));
	}
}

UTextBlock* URobberDevMenu::AddToggleButton(const TCHAR* Name, const TCHAR* After, ERobberDevAction Action)
{
	UButton* Model = Find<UButton>(After);
	UPanelWidget* Panel = Model ? Model->GetParent() : nullptr;
	if (!Panel)
	{
		return nullptr;
	}
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
	Button->SetStyle(Model->GetStyle());
	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
	if (const UTextBlock* ModelLabel = Cast<UTextBlock>(Model->GetContent()))
	{
		Label->SetFont(ModelLabel->GetFont());
		Label->SetColorAndOpacity(ModelLabel->GetColorAndOpacity());
	}
	Button->SetContent(Label);
	UPanelSlot* NewSlot = Panel->InsertChildAt(Panel->GetChildIndex(Model) + 1, Button);
	// Same spacing as its neighbour, whichever kind of box the WBP put them in.
	if (const UVerticalBoxSlot* ModelV = Cast<UVerticalBoxSlot>(Model->Slot))
	{
		UVerticalBoxSlot* V = CastChecked<UVerticalBoxSlot>(NewSlot);
		V->SetPadding(ModelV->GetPadding());
		V->SetSize(ModelV->GetSize());
		V->SetHorizontalAlignment(ModelV->GetHorizontalAlignment());
		V->SetVerticalAlignment(ModelV->GetVerticalAlignment());
	}
	else if (const UHorizontalBoxSlot* ModelH = Cast<UHorizontalBoxSlot>(Model->Slot))
	{
		UHorizontalBoxSlot* H = CastChecked<UHorizontalBoxSlot>(NewSlot);
		H->SetPadding(ModelH->GetPadding());
		H->SetSize(ModelH->GetSize());
		H->SetHorizontalAlignment(ModelH->GetHorizontalAlignment());
		H->SetVerticalAlignment(ModelH->GetVerticalAlignment());
	}
	URobberDevMenuClick* Click = NewObject<URobberDevMenuClick>(this);
	Click->Action = Action;
	Button->OnClicked.AddUniqueDynamic(Click, &URobberDevMenuClick::OnClicked);
	Clicks.Add(Click);
	return Label;
}

void URobberDevMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	SetText(TriggersLabel, ARobberTriggerZone::AreShapesShownInGame() ? INVTEXT("Triggers: ON") : INVTEXT("Triggers: OFF"));
	SetText(DevTextLabel, URobberDevText::IsShown() ? INVTEXT("Labels: ON") : INVTEXT("Labels: OFF"));
	if (const ARobberGameState* State = GetRobberState())
	{
		SetText(Find<UTextBlock>(TEXT("StatusText")), FText::Format(INVTEXT("{0} - {1} - heat {2}"), NightName(State),
			RobberStage::GetDisplayName(State->GetGameStage()), FMath::RoundToInt(State->GetHeat())));
	}
	if (const ARobberController* Robber = GetRobber())
	{
		// One window, two lists: opening either closes the other (see Send), so at most one has lines.
		Show(Find(TEXT("QuestPanel")), !Robber->DevQuestLines.IsEmpty() || !Robber->DevSecretLines.IsEmpty());
		if (ShownQuestRevision != Robber->DevQuestRevision || ShownSecretRevision != Robber->DevSecretRevision)
		{
			ShownQuestRevision = Robber->DevQuestRevision;
			ShownSecretRevision = Robber->DevSecretRevision;
			if (!Robber->DevSecretLines.IsEmpty())
			{
				RebuildRows(Robber->DevSecretLines, Robber->DevSecretCanFind, ERobberDevAction::FindSecret, INVTEXT("TP"));
			}
			else
			{
				RebuildRows(Robber->DevQuestLines, Robber->DevQuestCanFind, ERobberDevAction::FindExhibit, INVTEXT("Find"));
			}
		}
		SetText(Find<UTextBlock>(TEXT("ResultText")), Robber->IsHeistMessageVisible() ? Robber->GetHeistMessage() : FText::GetEmpty());
	}
}

void URobberDevMenu::Send(ERobberDevAction Action, int32 Value)
{
	if (Action == ERobberDevAction::ToggleTriggers)
	{
		ARobberTriggerZone::SetShapesShownInGame(!ARobberTriggerZone::AreShapesShownInGame());
		return;
	}
	if (Action == ERobberDevAction::ToggleDevText)
	{
		URobberDevText::SetShown(!URobberDevText::IsShown());
		return;
	}
	if (Action == ERobberDevAction::PlayCredits)
	{
		if (ARobberController* Robber = GetRobber())
		{
			Robber->OpenCredits();
		}
		return;
	}
	if (ARobberController* Robber = GetRobber())
	{
		// Pressing it again closes the list, which needs nothing from the server.
		if (Action == ERobberDevAction::ShowQuests)
		{
			Robber->ClearDevSecretList();
			if (!Robber->GetDevQuestList().IsEmpty())
			{
				Robber->ClearDevQuestList();
				return;
			}
		}
		if (Action == ERobberDevAction::CompleteSecrets)
		{
			Robber->ClearDevQuestList();
		}
		if (Action == ERobberDevAction::ShowSecrets)
		{
			Robber->ClearDevQuestList();
			if (!Robber->GetDevSecretList().IsEmpty())
			{
				Robber->ClearDevSecretList();
				return;
			}
		}
		Robber->ServerDevAction(Action, Value);
	}
}

void URobberDevMenu::RebuildRows(const TArray<FString>& Lines, const TArray<uint8>& CanFind, ERobberDevAction FindAction, const FText& ButtonText)
{
	if (!QuestRows)
	{
		// The widget has one text block; it's swapped for rows here so the WBP doesn't need rebuilding.
		QuestTextStyle = Find<UTextBlock>(TEXT("QuestListText"));
		UContentWidget* Holder = QuestTextStyle ? Cast<UContentWidget>(QuestTextStyle->GetParent()) : nullptr;
		if (!Holder)
		{
			return;
		}
		QuestRows = WidgetTree->ConstructWidget<UVerticalBox>();
		Holder->SetContent(QuestRows);
	}
	QuestRows->ClearChildren();
	const UButton* ButtonStyle = Find<UButton>(TEXT("GiveCode"));
	for (int32 Index = 0; Index < Lines.Num(); ++Index)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		if (CanFind.IsValidIndex(Index) && CanFind[Index])
		{
			UButton* Button = WidgetTree->ConstructWidget<UButton>();
			if (ButtonStyle)
			{
				Button->SetStyle(ButtonStyle->GetStyle());
			}
			UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
			Label->SetFont(QuestTextStyle->GetFont());
			Label->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
			Label->SetText(ButtonText);
			Button->SetContent(Label);
			URobberDevMenuClick* Click = NewObject<URobberDevMenuClick>(this);
			Click->Action = FindAction;
			Click->Value = Index;
			Button->OnClicked.AddUniqueDynamic(Click, &URobberDevMenuClick::OnClicked);
			Clicks.Add(Click);
			UHorizontalBoxSlot* ButtonSlot = Row->AddChildToHorizontalBox(Button);
			ButtonSlot->SetVerticalAlignment(VAlign_Top);
			ButtonSlot->SetPadding(FMargin(0.f, 1.f, 8.f, 1.f));
		}
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
		Text->SetFont(QuestTextStyle->GetFont());
		Text->SetColorAndOpacity(QuestTextStyle->GetColorAndOpacity());
		Text->SetAutoWrapText(true);
		Text->SetText(FText::FromString(Lines[Index].IsEmpty() ? TEXT(" ") : Lines[Index]));
		UHorizontalBoxSlot* TextSlot = Row->AddChildToHorizontalBox(Text);
		TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		TextSlot->SetVerticalAlignment(VAlign_Center);
		QuestRows->AddChildToVerticalBox(Row);
	}
}

void ARobberController::ClientDevQuestList_Implementation(const TArray<FString>& Lines, const TArray<uint8>& CanFind)
{
	DevQuestLines = Lines;
	DevQuestCanFind = CanFind;
	++DevQuestRevision;
}

void ARobberController::ClientDevSecretList_Implementation(const TArray<FString>& Lines, const TArray<uint8>& CanFind)
{
	DevSecretLines = Lines;
	DevSecretCanFind = CanFind;
	++DevSecretRevision;
}

#if !UE_BUILD_SHIPPING
namespace RobberDevTools
{
	int32 StageRank(uint8 Stage)
	{
		switch (Stage)
		{
		case RobberStage::BreakIn: return 1;
		case RobberStage::Investigate: return 2;
		case RobberStage::Heist: return 3;
		case RobberStage::Escape: return 4;
		default: return 0;
		}
	}

	/** Walks the night's real transitions up to Target, so the crew is moved and the night set up as in play. Returns what went wrong, or empty. */
	FText JumpToStage(ARobberGameState& State, APlayerState* Asker, uint8 Target)
	{
		if (StageRank(State.GetGameStage()) >= StageRank(Target))
		{
			return INVTEXT("Already at or past that stage. Pick a night to start again.");
		}
		if (RobberStage::IsInLobby(State.GetGameStage()))
		{
			if (!State.GetWinningEntry())
			{
				TActorIterator<ARobberEntryPoint> Entry(State.GetWorld());
				if (!Entry)
				{
					return INVTEXT("This night has no way in, so it can't start.");
				}
				State.CastVote(Asker, *Entry);
			}
			if (!State.StartBreakIn())
			{
				return INVTEXT("The break-in wouldn't start.");
			}
		}
		if (StageRank(Target) >= StageRank(RobberStage::Investigate))
		{
			State.FinishBreakIn();
		}
		if (StageRank(Target) >= StageRank(RobberStage::Heist))
		{
			State.StartHeist();
		}
		if (Target == RobberStage::Escape)
		{
			State.SetGameStage(RobberStage::Escape);
		}
		return FText::GetEmpty();
	}

	FText GiveVaultCode(ARobberController& Controller, const UWorld* World)
	{
		TActorIterator<ARobberVaultDoor> Door(World);
		URobberJournalComponent* Journal = Controller.GetJournal();
		if (!Door || Door->GetCode().IsEmpty() || !Journal)
		{
			return INVTEXT("No vault door on this night.");
		}
		const TArray<int32>& Code = Door->GetCode();
		// Learning a digit adds to the journal's sentence, so a second press mustn't repeat them.
		if (Journal->GetVaultDigits() != Code)
		{
			for (int32 Index = 0; Index < Code.Num(); ++Index)
			{
				Journal->LearnVaultDigit(*Door, INVTEXT("Dev menu"), Index, Code[Index], ARobberVaultDoor::CodeLength);
			}
		}
		// Dialled in through the dials, so the job, crew message and achievement happen as in play.
		Door->DialInCode();
		return FText::Format(INVTEXT("Vault code: {0}. Vault open."), FText::FromString(FString::JoinBy(Code, TEXT(" "), [](int32 Digit) { return FString::FromInt(Digit); })));
	}

	/** Every key the loaded night asks for or hands out, including ones with no pickup. */
	FText GiveKeys(ARobberController& Controller, const UWorld* World)
	{
		TSet<FName> Keys;
		for (TActorIterator<ARobberLockedDoor> It(World); It; ++It) Keys.Add(It->KeyId);
		for (TActorIterator<ARobberKeyPickup> It(World); It; ++It) Keys.Add(It->KeyId);
		for (TActorIterator<ARobberCrowbar> It(World); It; ++It) Keys.Add(It->KeyId);
		for (TActorIterator<ARobberSewerGrate> It(World); It; ++It) Keys.Add(It->KeyId);
		Keys.Remove(NAME_None);
		UPlayerQuestComponent* Quests = Controller.FindComponentByClass<UPlayerQuestComponent>();
		if (Keys.IsEmpty() || !Quests)
		{
			return INVTEXT("No keys on this night.");
		}
		for (const FName& Key : Keys)
		{
			Quests->GiveKey(Key);
		}
		return FText::Format(INVTEXT("Gave you {0} key(s)."), Keys.Num());
	}

	/**
	 * Where to put the pawn so it can talk to Target: on a floor near it, with room for the capsule and nothing between
	 * the eyes and the target (CanReach's rule). Exhibits hang on walls, so a fixed spot "in front" lands behind the
	 * wall or in a prop half the time. Tries the facing first, then round the clock, near before far.
	 */
	bool FindTalkSpot(const APawn* Pawn, const AActor* Target, FVector& OutLocation, FRotator& OutFacing,
		const FVector* AimAt = nullptr, std::initializer_list<float> Distances = { 170.f, 240.f, 320.f })
	{
		UWorld* World = Target->GetWorld();
		const ACharacter* Character = Cast<ACharacter>(Pawn);
		const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
		const float Radius = Capsule ? Capsule->GetScaledCapsuleRadius() : 40.f;
		const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.f;
		const float EyeHeight = Pawn->BaseEyeHeight + HalfHeight;
		const FVector Aim = AimAt ? *AimAt : Target->GetActorLocation();
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DevFindSpot), false);
		Params.AddIgnoredActor(Pawn);
		Params.AddIgnoredActor(Target);

		const FVector Forward = Target->GetActorForwardVector().GetSafeNormal2D();
		for (const float Distance : Distances)
		{
			for (int32 Step = 0; Step < 12; ++Step)
			{
				// 0, +30, -30, +60, -60 ... so the exhibit's own facing wins when it's free.
				const float Angle = (Step + 1) / 2 * 30.f * (Step % 2 ? 1.f : -1.f);
				const FVector Out = Forward.RotateAngleAxis(Angle, FVector::UpVector) * Distance;
				// The floor under that spot, no more than a storey below the target, never above it.
				FHitResult Floor;
				const FVector Top = FVector(Aim.X + Out.X, Aim.Y + Out.Y, Aim.Z + 50.f);
				if (!World->LineTraceSingleByChannel(Floor, Top, Top - FVector(0, 0, 450.f), ECC_Visibility, Params))
				{
					continue;
				}
				const FVector Feet = Floor.ImpactPoint + FVector(0, 0, 2.f);
				const FVector Centre = Feet + FVector(0, 0, HalfHeight);
				if (World->OverlapBlockingTestByChannel(Centre, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Radius, HalfHeight), Params))
				{
					continue;
				}
				FHitResult Blocked;
				if (World->LineTraceSingleByChannel(Blocked, Feet + FVector(0, 0, EyeHeight), Aim, ECC_Visibility, Params))
				{
					continue;
				}
				OutLocation = Centre;
				OutFacing = (Aim - (Feet + FVector(0, 0, EyeHeight))).Rotation();
				return true;
			}
		}
		return false;
	}

	/**
	 * The loaded night's secrets for the dev window: a heading, the screen that shows progress, then each hidden step in
	 * order. Targets lines up with Lines (null = no TP button). Only Night 5's Escape Velocity exists so far.
	 */
	void DescribeSecrets(const UWorld* World, TArray<FString>& Lines, TArray<AActor*>& Targets)
	{
		const TArray<ARobberLaunchTask*> Tasks = ARobberLaunchTask::All(World);
		TActorIterator<ARobberLaunchBoard> Board(const_cast<UWorld*>(World));
		TActorIterator<ARobberLaunchKeys> Keys(const_cast<UWorld*>(World));
		if (Tasks.IsEmpty() && !Board && !Keys)
		{
			return;
		}
		Lines.Add(TEXT("ESCAPE VELOCITY (secret ending)"));
		Targets.Add(nullptr);
		if (Board)
		{
			Lines.Add(TEXT("Launch checklist screen (Mission Control)"));
			Targets.Add(*Board);
		}
		int32 Number = 0;
		for (ARobberLaunchTask* Task : Tasks)
		{
			Lines.Add(FString::Printf(TEXT("   Secret %d: %s - %s"), ++Number, *Task->TaskName.ToString(), Task->IsDone() ? TEXT("GO") : TEXT("NO-GO")));
			Targets.Add(Task);
		}
		if (Keys)
		{
			const TCHAR* KeysState = Keys->IsLaunched() ? TEXT("launched") : Keys->IsReady() ? TEXT("ready") : TEXT("locked until every step is GO");
			Lines.Add(FString::Printf(TEXT("   Secret %d: Launch keys - %s"), ++Number, KeysState));
			Targets.Add(*Keys);
		}
	}
}
#endif

void ARobberController::ServerDevAction_Implementation(ERobberDevAction Action, int32 Value)
{
#if !UE_BUILD_SHIPPING
	using namespace RobberDevTools;
	ARobberGameState* State = GetWorld()->GetGameState<ARobberGameState>();
	if (!State)
	{
		return;
	}
	const bool bInLobby = RobberStage::IsInLobby(State->GetGameStage());
	FText Result;
	switch (Action)
	{
	case ERobberDevAction::SwitchNight:
		State->SwitchNight(Value);
		break;
	case ERobberDevAction::JumpToStage:
		Result = JumpToStage(*State, PlayerState, static_cast<uint8>(Value));
		break;
	case ERobberDevAction::FinishNight:
		if (bInLobby)
		{
			Result = INVTEXT("Only during a night.");
		}
		else
		{
			State->FinishNight(Value != 0);
		}
		break;
	case ERobberDevAction::GiveVaultCode:
		Result = GiveVaultCode(*this, GetWorld());
		break;
	case ERobberDevAction::GiveKeys:
		Result = GiveKeys(*this, GetWorld());
		break;
	case ERobberDevAction::AddHeat:
		if (bInLobby)
		{
			Result = INVTEXT("Only during a night.");
		}
		else
		{
			State->AddHeat(Value, TEXT("Dev menu"));
		}
		break;
	case ERobberDevAction::CoolHeat:
		// CoolHeat does nothing once the alarm's up, so say so rather than look broken.
		if (State->IsAlarmRaised())
		{
			Result = INVTEXT("Too late: the alarm's up.");
		}
		else
		{
			State->CoolHeat(ARobberGameState::MaxHeat);
		}
		break;
	case ERobberDevAction::AddGuardsTime:
		State->AddGuardsTime(Value);
		Result = FText::Format(INVTEXT("Guards pushed back {0}s."), Value);
		break;
	case ERobberDevAction::ShowQuests:
	{
		TArray<FString> Lines;
		TArray<AActor*> Targets;
		for (TActorIterator<ARobberNightDirector> It(GetWorld()); It; ++It)
		{
			if (It->IsDealt())
			{
				It->DescribeDealForDev(Lines, Targets);
			}
		}
		if (Lines.IsEmpty())
		{
			Lines.Add(TEXT("No quests have been dealt yet. Go to a night first."));
			Targets.Add(nullptr);
		}
		DevFindTargets.Reset();
		TArray<uint8> CanFind;
		for (AActor* Target : Targets)
		{
			DevFindTargets.Add(Target);
			CanFind.Add(Target ? 1 : 0);
		}
		ClientDevQuestList(Lines, CanFind);
		break;
	}
	case ERobberDevAction::FindExhibit:
	{
		AActor* Target = DevFindTargets.IsValidIndex(Value) ? DevFindTargets[Value].Get() : nullptr;
		FVector Stand;
		FRotator Facing;
		if (Target && GetPawn() && RobberDevTools::FindTalkSpot(GetPawn(), Target, Stand, Facing))
		{
			GetPawn()->TeleportTo(Stand, FRotator(0, Facing.Yaw, 0), false, true);
			SetControlRotation(Facing);
			ClientSetRotation(Facing);
			Result = INVTEXT("Teleported.");
		}
		else if (Target)
		{
			Result = INVTEXT("No free spot to stand next to them.");
		}
		break;
	}
	case ERobberDevAction::CompleteSecrets:
	{
		const TArray<ARobberLaunchTask*> Tasks = ARobberLaunchTask::All(GetWorld());
		if (Tasks.IsEmpty())
		{
			Result = INVTEXT("No secrets on this night. Only Night 5 has one so far.");
			break;
		}
		for (ARobberLaunchTask* Task : Tasks)
		{
			Task->MarkDone();
		}
		// The keys refuse in the lobby stage, so say so instead of leaving them looking broken.
		Result = bInLobby
			? FText::Format(INVTEXT("{0} launch steps GO. The keys only turn during a night."), Tasks.Num())
			: FText::Format(INVTEXT("{0} launch steps GO. The keys are ready."), Tasks.Num());
		// Then show the list, so the GO states are there to check.
		[[fallthrough]];
	}
	case ERobberDevAction::ShowSecrets:
	{
		TArray<FString> Lines;
		TArray<AActor*> Targets;
		DescribeSecrets(GetWorld(), Lines, Targets);
		if (Lines.IsEmpty())
		{
			Lines.Add(TEXT("No secrets on this night. Only Night 5 has one so far."));
			Targets.Add(nullptr);
		}
		DevSecretTargets.Reset();
		TArray<uint8> CanFind;
		for (AActor* Target : Targets)
		{
			DevSecretTargets.Add(Target);
			CanFind.Add(Target ? 1 : 0);
		}
		ClientDevSecretList(Lines, CanFind);
		break;
	}
	case ERobberDevAction::FindSecret:
	{
		AActor* Target = DevSecretTargets.IsValidIndex(Value) ? DevSecretTargets[Value].Get() : nullptr;
		if (!Target || !GetPawn())
		{
			break;
		}
		// Aim at the middle of what's there: task actors sit on their base, so their location is the floor.
		FVector Centre, Extent;
		// All components: the screen has no collision, so its colliding-only bounds are empty (centre = world origin).
		Target->GetActorBounds(false, Centre, Extent);
		// The screen is up on the wall, so step back to read all of it; the steps need E's 300-unit reach.
		const bool bScreen = Target->IsA<ARobberLaunchBoard>();
		FVector Stand;
		FRotator Facing;
		const bool bFound = bScreen
			? FindTalkSpot(GetPawn(), Target, Stand, Facing, &Centre, { 1000.f, 1200.f, 800.f, 600.f })
			: FindTalkSpot(GetPawn(), Target, Stand, Facing, &Centre, { 140.f, 190.f, 240.f });
		if (bFound)
		{
			GetPawn()->TeleportTo(Stand, FRotator(0, Facing.Yaw, 0), false, true);
			SetControlRotation(Facing);
			ClientSetRotation(Facing);
			Result = INVTEXT("Teleported.");
		}
		else
		{
			Result = INVTEXT("No free spot to stand that sees it.");
		}
		break;
	}
	case ERobberDevAction::ToggleImmunity:
		if (AProductionGameCharacter* Robber = Cast<AProductionGameCharacter>(GetPawn()))
		{
			Robber->SetDevImmune(!Robber->IsDevImmune());
			Result = Robber->IsDevImmune() ? INVTEXT("Immunity ON: guards and cameras can't notice you.") : INVTEXT("Immunity OFF.");
		}
		break;
	}
	if (!Result.IsEmpty())
	{
		ClientHeistMessage(Result);
	}
#endif
}
