#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/DefaultPawn.h"
#include "Dialogue/QuestNPC.h"
#include "Dialogue/DialogueComponent.h"
#include "Quest/PlayerQuestComponent.h"
#include "Quest/QuestDefinition.h"
#include "Quest/QuestGiverComponent.h"
#include "Quest/QuestObjectiveComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuestDialogueFlowTest, "ProductionGame.Quests.AuthoritativeFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FQuestDialogueFlowTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
		.RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	auto MakePlayer = [World](FVector Location)
	{
		auto* PC = World->SpawnActor<APlayerController>();
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Pawn = World->SpawnActor<ADefaultPawn>(Location, FRotator::ZeroRotator, Params);
		PC->Possess(Pawn);
		auto* Player = NewObject<UPlayerQuestComponent>(PC);
		Player->bCreateDefaultUI = false;
		PC->AddInstanceComponent(Player);
		Player->RegisterComponent();
		return Player;
	};
	auto* First = MakePlayer(FVector(100, 0, 0));
	auto* Second = MakePlayer(FVector(100, 100, 0));
	auto* NPC = World->SpawnActor<AQuestNPC>();
	auto* OtherNPC = World->SpawnActor<AQuestNPC>(FVector(0, 200, 0), FRotator::ZeroRotator);
	auto* Quest = NewObject<UQuestDefinition>(World);
	Quest->QuestID = TEXT("TestQuest");
	Quest->ProgressEvent = TEXT("TestPickup");
	Quest->RequiredAmount = 3;
	NPC->QuestGiver->AvailableQuests.Add(Quest);
	OtherNPC->QuestGiver->AvailableQuests.Add(Quest);
	auto* Pickup = NewObject<UQuestObjectiveComponent>(OtherNPC);
	OtherNPC->AddInstanceComponent(Pickup);
	Pickup->RegisterComponent();
	Pickup->ProgressEvent = Quest->ProgressEvent;
	TestFalse(TEXT("Pickup without active quest rejected"), Pickup->Collect(First));

	TestFalse(TEXT("Cannot accept without conversation"), First->AcceptQuest(Quest, NPC));
	TestTrue(TEXT("First player acquires NPC"), NPC->DialogueComponent->TryStart(First));
	TestFalse(TEXT("Second player cannot acquire busy NPC"), NPC->DialogueComponent->TryStart(Second));
	TestFalse(TEXT("Player cannot talk to two NPCs"), OtherNPC->DialogueComponent->TryStart(First));
	const int32 OfferRevision = First->Conversation.Revision;
	NPC->DialogueComponent->Choose(First, 100000, OfferRevision);
	TestEqual(TEXT("Quest accepted through choice"), First->Quests.Num(), 1);
	TestEqual(TEXT("Other player has independent quests"), Second->Quests.Num(), 0);
	NPC->DialogueComponent->Choose(First, 100000, OfferRevision);
	TestEqual(TEXT("Stale choice does not accept twice"), First->Quests.Num(), 1);
	TestFalse(TEXT("Cannot turn in incomplete quest"), First->TurnInQuest(Quest, NPC));
	TestFalse(TEXT("Reject negative progress"), First->AddQuestProgress(Quest->QuestID, -1));
	First->ReportQuestEvent(TEXT("WrongEvent"));
	TestEqual(TEXT("Unrelated event ignored"), First->Quests[0].Amount, 0);
	Pickup->InteractionDistance = 10.f;
	TestFalse(TEXT("Distant pickup rejected"), Pickup->Collect(First));
	Pickup->InteractionDistance = 350.f;
	TestTrue(TEXT("Active quest pickup accepted"), Pickup->Collect(First));
	TestFalse(TEXT("Duplicate pickup rejected"), Pickup->Collect(First));
	TestEqual(TEXT("Pickup counted once"), First->Quests[0].Amount, 1);
	First->ReportQuestEvent(TEXT("TestPickup"), MAX_int32);
	TestEqual(TEXT("Progress clamps without overflow"), First->Quests[0].Amount, 3);
	TestTrue(TEXT("Objective completion awaits turn in"), First->Quests[0].State == EPlayerQuestState::ReadyToTurnIn);
	NPC->DialogueComponent->Release(First);
	TestFalse(TEXT("Cannot turn in remotely"), First->TurnInQuest(Quest, NPC));
	TestTrue(TEXT("Different NPC is reachable"), OtherNPC->DialogueComponent->TryStart(First));
	TestFalse(TEXT("Cannot turn in at wrong NPC"), First->TurnInQuest(Quest, OtherNPC));
	OtherNPC->DialogueComponent->Release(First);
	TestTrue(TEXT("Return to original NPC"), NPC->DialogueComponent->TryStart(First));
	const int32 TurnInRevision = First->Conversation.Revision;
	NPC->DialogueComponent->Choose(First, 100001, TurnInRevision);
	TestTrue(TEXT("Quest complete after turn in"), First->Quests[0].State == EPlayerQuestState::Completed);
	NPC->DialogueComponent->Choose(First, 100001, TurnInRevision);
	TestFalse(TEXT("Direct duplicate turn in rejected"), First->TurnInQuest(Quest, NPC));
	TestTrue(TEXT("Duplicate requests preserve completed state"), First->Quests[0].State == EPlayerQuestState::Completed);
	TestFalse(TEXT("Completed quest cannot restart"), First->AcceptQuest(Quest, NPC));
	NPC->DialogueComponent->Release(Second);
	TestTrue(TEXT("Nonparticipant cannot release another player's lock"), NPC->DialogueComponent->bIsInConversation);
	NPC->DialogueComponent->Release(First);
	TestTrue(TEXT("Second player can talk after release"), NPC->DialogueComponent->TryStart(Second));
	TestTrue(TEXT("Second player accepts independently"), Second->AcceptQuest(Quest, NPC));
	TestTrue(TEXT("Same pickup remains available to second player"), Pickup->Collect(Second));
	NPC->DialogueComponent->Release(Second);
	auto* SecondPC = CastChecked<APlayerController>(Second->GetOwner());
	SecondPC->GetPawn()->SetActorLocation(FVector(10000, 0, 0));
	TestFalse(TEXT("Distant conversation rejected"), NPC->DialogueComponent->TryStart(Second));
	World->DestroyWorld(false);
	return true;
}
#endif
