#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Dialogue/QuestDialogueCharacter.h"
#include "Dialogue/DialogueComponent.h"
#include "Quest/PlayerQuestComponent.h"
#include "Quest/QuestDefinition.h"
#include "Quest/QuestObjectiveComponent.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "Quest/QuestConversationWidget.h"
#include "Components/Button.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Layout/WidgetPath.h"
#include "UObject/UObjectIterator.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace
{
	bool IsDialogueAudioPlaying(UWorld* World, USoundBase* Sound)
	{
		if (!Sound) return false;
		for (TObjectIterator<UAudioComponent> It; It; ++It)
			if (It->GetWorld() == World && It->Sound == Sound && It->IsPlaying()) return true;
		return false;
	}

	bool HasEmptyTracker(APlayerController* PC)
	{
		for (TObjectIterator<UQuestConversationWidget> It; It; ++It)
			if (It->GetOwningPlayer() == PC && It->IsInViewport()) return It->GetDisplayedQuestText().IsEmpty();
		return false;
	}

	// Hit-test the real widget geometry and route pointer events through Slate, not a direct RPC.
	bool ClickWidget(UWidget* Widget)
	{
		if (!Widget || Widget->GetCachedGeometry().GetLocalSize().IsNearlyZero()) return false;
		TSharedPtr<SWidget> SlateWidget = Widget->GetCachedWidget();
		if (!SlateWidget) return false;
		auto& App = FSlateApplication::Get();
		TSharedPtr<SWindow> Window = App.FindWidgetWindow(SlateWidget.ToSharedRef());
		if (!Window) return false;
		const FVector2D Position = Widget->GetCachedGeometry().GetAbsolutePosition() + Widget->GetCachedGeometry().GetAbsoluteSize() * 0.5;
		FWidgetPath Path = App.LocateWindowUnderMouse(Position, {Window.ToSharedRef()});
		if (!Path.ContainsWidget(SlateWidget.Get())) return false;
		const TSet<FKey> Pressed{EKeys::LeftMouseButton};
		const TSet<FKey> Released;
		FPointerEvent Down(0, Position, Position, Pressed, EKeys::LeftMouseButton, 0.f, FModifierKeysState());
		FPointerEvent Up(0, Position, Position, Released, EKeys::LeftMouseButton, 0.f, FModifierKeysState());
		// Enter/hover must precede down/up, as with a physical cursor. SButton requires it.
		App.RoutePointerMoveEvent(Path, Up, false);
		App.RoutePointerDownEvent(Path, Down);
		App.RoutePointerUpEvent(Path, Up);
		return true;
	}

	bool ClickChoice(APlayerController* PC, int32 Index)
	{
		for (TObjectIterator<UDialoguePresentationWidget> It; It; ++It)
		{
			if (It->GetOwningPlayer() != PC || !It->IsInViewport()) continue;
			static bool CapturedSpeaker = false;
			static bool CapturedChoices = false;
			bool& Captured = It->IsShowingChoices() ? CapturedChoices : CapturedSpeaker;
			if (!Captured && !It->GetCachedGeometry().GetLocalSize().IsNearlyZero())
			{
				TArray<FColor> Pixels;
				FIntVector Size;
				if (FSlateApplication::Get().TakeScreenshot(It->TakeWidget(), Pixels, Size))
				{
					TArray64<uint8> PNG;
					FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, PNG);
					const FString Folder = FPaths::ProjectSavedDir() / TEXT("Automation");
					IFileManager::Get().MakeDirectory(*Folder, true);
					FFileHelper::SaveArrayToFile(PNG, *(Folder / (It->IsShowingChoices() ? TEXT("DialogueChoices.png") : TEXT("DialogueSpeaker.png"))));
					Captured = true;
				}
			}
			if (!It->IsShowingChoices()) { ClickWidget(*It); return false; }
			const auto& Choices = It->GetChoiceWidgets();
			return Choices.IsValidIndex(Index) && ClickWidget(Choices[Index]->GetChoiceButton());
		}
		return false;
	}

	class FQuestNetworkScenario : public IAutomationLatentCommand
	{
	public:
		explicit FQuestNetworkScenario(FAutomationTestBase* InTest) : Test(InTest), Deadline(FPlatformTime::Seconds() + 60.) {}
		virtual bool Update() override
		{
			if (FPlatformTime::Seconds() > Deadline) { Test->AddError(FString::Printf(TEXT("Network quest scenario timed out at step %d"), Step)); return true; }
			UWorld* Server = nullptr;
			UWorld* Client = nullptr;
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.WorldType != EWorldType::PIE || !Context.World()) continue;
				if (Context.World()->GetNetMode() == NM_ListenServer) Server = Context.World();
				if (Context.World()->GetNetMode() == NM_Client) Client = Context.World();
			}
			if (!Server || !Client) return false;
			APlayerController* ClientPC = Client->GetFirstPlayerController();
			if (!ClientPC || !ClientPC->GetPawn() || !ClientPC->PlayerState) return false;
			UPlayerQuestComponent* Remote = ClientPC->FindComponentByClass<UPlayerQuestComponent>();
			UPlayerQuestComponent* ServerRemote = nullptr;
			UPlayerQuestComponent* Host = nullptr;
			for (auto It = Server->GetPlayerControllerIterator(); It; ++It)
			{
				APlayerController* PC = It->Get();
				if (!PC || !PC->GetPawn() || !PC->PlayerState) continue;
				if (PC->IsLocalController()) Host = PC->FindComponentByClass<UPlayerQuestComponent>();
				else if (PC->PlayerState->GetPlayerId() == ClientPC->PlayerState->GetPlayerId()) ServerRemote = PC->FindComponentByClass<UPlayerQuestComponent>();
			}
			AQuestDialogueCharacter* NPC = nullptr;
			AQuestDialogueCharacter* ClientNPC = nullptr;
			for (TActorIterator<AQuestDialogueCharacter> It(Server); It; ++It) { NPC = *It; break; }
			for (TActorIterator<AQuestDialogueCharacter> It(Client); It; ++It) { ClientNPC = *It; break; }
			if (!Remote || !Host || !ServerRemote || !NPC || !ClientNPC) return false;
			auto* RemotePC = CastChecked<APlayerController>(ServerRemote->GetOwner());
			auto* HostPC = CastChecked<APlayerController>(Host->GetOwner());
			if (FPlatformTime::Seconds() < NextStepTime) return false;
			if (FPlatformTime::Seconds() > NextDiagnostic)
			{
				NextDiagnostic = FPlatformTime::Seconds() + 5.;
				UE_LOG(LogTemp, Display, TEXT("Dialogue click test step %d busy=%d clientNPC=%s choices=%d"), Step,
					NPC->NativeDialogue->bIsInConversation, *GetNameSafe(Remote->Conversation.NPC), Remote->Conversation.Choices.Num());
				for (TObjectIterator<UDialoguePresentationWidget> It; It; ++It)
					if (It->GetOwningPlayer() == ClientPC) UE_LOG(LogTemp, Display, TEXT("Modal viewport=%d choices=%d size=%s"), It->IsInViewport(), It->IsShowingChoices(), *It->GetCachedGeometry().GetLocalSize().ToString());
			}
			switch (Step)
			{
			case 0:
				RemotePC->GetPawn()->SetActorLocation(NPC->GetActorLocation() + FVector(-150, 0, 0));
				HostPC->GetPawn()->SetActorLocation(NPC->GetActorLocation() + FVector(0, 150, 0));
				NextStepTime = FPlatformTime::Seconds() + 1.;
				break;
			case 1:
				Test->TestTrue(TEXT("Tracker has no text before accepting a quest"), HasEmptyTracker(ClientPC));
				Remote->ServerTalkTo(ClientNPC);
				break;
			case 2:
				if (!Remote->Conversation.NPC) return false;
				if (!bCheckedVoice)
				{
					GreetingVoice = Remote->Conversation.Voice;
					Test->TestNotNull(TEXT("Greeting voice reaches participating client"), GreetingVoice.Get());
					Test->TestTrue(TEXT("Greeting recording is playing on client"), IsDialogueAudioPlaying(Client, GreetingVoice.Get()));
					Test->TestFalse(TEXT("Private dialogue recording does not play on host"), IsDialogueAudioPlaying(Server, GreetingVoice.Get()));
					bCheckedVoice = true;
				}
				Test->TestTrue(TEXT("Remote player owns server lock"), NPC->NativeDialogue->IsParticipant(ServerRemote));
				Host->ServerTalkTo(NPC);
				Test->TestTrue(TEXT("Host cannot steal remote player's conversation"), NPC->NativeDialogue->IsParticipant(ServerRemote));
				Test->TestNull(TEXT("Host receives no dialogue snapshot"), Host->Conversation.NPC.Get());
				if (!ClickChoice(ClientPC, 1)) return false; // No
				break;
			case 3:
				if (Remote->Conversation.NPC) return false;
				Test->TestFalse(TEXT("Closing dialogue stops its recording"), IsDialogueAudioPlaying(Client, GreetingVoice.Get()));
				Test->TestEqual(TEXT("No declines without accepting"), Remote->Quests.Num(), 0);
				NextStepTime = FPlatformTime::Seconds() + 0.5;
				break;
			case 4:
				Remote->ServerTalkTo(ClientNPC);
				break;
			case 5:
				if (!Remote->Conversation.NPC) return false;
				Test->TestEqual(TEXT("Decline restarts greeting"), Remote->Conversation.Text.ToString(), FString(TEXT("Hey, you want some help?")));
				if (!ClickChoice(ClientPC, 0)) return false; // Yes
				break;
			case 6:
				if (!Remote->Conversation.Text.ToString().StartsWith(TEXT("Go get"))) return false;
				Test->TestFalse(TEXT("Moving to another node stops previous recording"), IsDialogueAudioPlaying(Client, GreetingVoice.Get()));
				Test->TestEqual(TEXT("Yes alone does not accept quest"), Remote->Quests.Num(), 0);
				if (!ClickChoice(ClientPC, 1)) return false; // No way
				break;
			case 7:
				if (Remote->Conversation.NPC) return false;
				Test->TestEqual(TEXT("No way declines without accepting"), Remote->Quests.Num(), 0);
				NextStepTime = FPlatformTime::Seconds() + 0.5;
				break;
			case 8:
				Remote->ServerTalkTo(ClientNPC);
				break;
			case 9:
				if (!Remote->Conversation.NPC) return false;
				Test->TestEqual(TEXT("Second decline also restarts greeting"), Remote->Conversation.Text.ToString(), FString(TEXT("Hey, you want some help?")));
				if (!ClickChoice(ClientPC, 0)) return false;
				break;
			case 10:
				if (!Remote->Conversation.Text.ToString().StartsWith(TEXT("Go get"))) return false;
				if (!ClickChoice(ClientPC, 0)) return false; // Okay
				break;
			case 11:
			{
				if (Remote->Quests.Num() != 1 || Remote->Conversation.NPC) return false;
				Test->TestEqual(TEXT("Quest replicated only to accepting player"), Host->Quests.Num(), 0);
				AActor* Pickup = nullptr;
				for (TActorIterator<AActor> It(Client); It; ++It)
					if (It->FindComponentByClass<UQuestObjectiveComponent>()) { Pickup = *It; break; }
				if (!Pickup) return false;
				if (!bMovedToPickup)
				{
					RemotePC->GetPawn()->SetActorLocation(Pickup->GetActorLocation() + FVector(-90, 0, 40));
					bMovedToPickup = true;
					NextStepTime = FPlatformTime::Seconds() + 0.5;
					return false;
				}
				Test->TestEqual(TEXT("Walking near pickup does not collect it"), ServerRemote->Quests[0].Amount, 0);
				Test->TestEqual(TEXT("Pickup inherits existing interaction master"), Pickup->GetClass()->GetSuperClass()->GetName(), FString(TEXT("BP_InteractMaster_C")));
				UFunction* Interact = Pickup->FindFunction(TEXT("Interact"));
				if (!Test->TestNotNull(TEXT("Pickup implements interaction event"), Interact)) return true;
				FStructOnScope Params(Interact);
				auto* Interactor = FindFProperty<FObjectPropertyBase>(Interact, TEXT("InteractingPlayer"));
				if (!Test->TestNotNull(TEXT("Interaction carries player"), Interactor)) return true;
				Interactor->SetObjectPropertyValue_InContainer(Params.GetStructMemory(), ClientPC->GetPawn());
				Pickup->ProcessEvent(Interact, Params.GetStructMemory());
				Pickup->ProcessEvent(Interact, Params.GetStructMemory()); // duplicate input
				NextStepTime = FPlatformTime::Seconds() + 0.5;
				break;
			}
			case 12:
				if (Remote->Quests[0].State != EPlayerQuestState::ReadyToTurnIn) return false;
				RemotePC->GetPawn()->SetActorLocation(NPC->GetActorLocation() + FVector(-150, 0, 0));
				Remote->ServerTalkTo(ClientNPC);
				break;
			case 13:
				if (!Remote->Conversation.NPC || !Remote->Conversation.Text.ToString().StartsWith(TEXT("You got"))) return false;
				TurnInRevision = Remote->Conversation.Revision;
				if (!ClickChoice(ClientPC, 0)) return false;
				Remote->ServerChoose(0, TurnInRevision); // stale duplicate
				break;
			case 14:
				if (Remote->Quests[0].State != EPlayerQuestState::Completed) return false;
				Test->TestTrue(TEXT("Turn-in completed on server"), ServerRemote->Quests[0].State == EPlayerQuestState::Completed);
				Test->TestTrue(TEXT("Tracker clears after quest turn-in"), HasEmptyTracker(ClientPC));
				RemotePC->GetPawn()->SetActorLocation(NPC->GetActorLocation() + FVector(-2000, 0, 0));
				break;
			case 15:
				if (NPC->NativeDialogue->bIsInConversation || Remote->Conversation.NPC) return false;
				Host->ServerTalkTo(NPC);
				Test->TestTrue(TEXT("Walking away releases lock for another player"), NPC->NativeDialogue->IsParticipant(Host));
				NPC->NativeDialogue->Release(Host);
				return true;
			}
			UE_LOG(LogTemp, Display, TEXT("Quest network test completed step %d (server time %.2f)"), Step, Server->GetTimeSeconds());
			++Step;
			return false;
		}
	private:
		FAutomationTestBase* Test;
		double Deadline;
		double NextStepTime = 0.;
		double NextDiagnostic = 0.;
		int32 Step = 0;
		int32 TurnInRevision = 0;
		bool bMovedToPickup = false;
		bool bCheckedVoice = false;
		TWeakObjectPtr<USoundBase> GreetingVoice;
	};

	class FRestoreQuestPIESettings : public IAutomationLatentCommand
	{
	public:
		FRestoreQuestPIESettings()
		{
			auto* Settings = GetMutableDefault<ULevelEditorPlaySettings>();
			Settings->GetPlayNetMode(Mode);
			Settings->GetRunUnderOneProcess(bOneProcess);
			Settings->GetPlayNumberOfClients(Players);
			bSound = Settings->EnableGameSound;
			bEveryPlayerAudio = Settings->IsCreateAudioDeviceForEveryPlayer();
			bSoloFirstAudio = Settings->SoloAudioInFirstPIEClient;
			Settings->EnableGameSound = true;
			auto* AudioSetting = FindFProperty<FBoolProperty>(Settings->GetClass(), TEXT("CreateAudioDeviceForEveryPlayer"));
			check(AudioSetting);
			AudioSetting->SetPropertyValue_InContainer(Settings, true);
			Settings->SoloAudioInFirstPIEClient = false;
			Drivers = GEngine->NetDriverDefinitions;
			Settings->SetPlayNetMode(PIE_ListenServer);
			Settings->SetRunUnderOneProcess(true);
			Settings->SetPlayNumberOfClients(2);
			for (FNetDriverDefinition& Driver : GEngine->NetDriverDefinitions)
				if (Driver.DefName == TEXT("GameNetDriver")) Driver.DriverClassName = TEXT("/Script/OnlineSubsystemUtils.IpNetDriver");
		}
		virtual bool Update() override
		{
			auto* Settings = GetMutableDefault<ULevelEditorPlaySettings>();
			Settings->SetPlayNetMode(Mode);
			Settings->SetRunUnderOneProcess(bOneProcess);
			Settings->SetPlayNumberOfClients(Players);
			Settings->EnableGameSound = bSound;
			auto* AudioSetting = FindFProperty<FBoolProperty>(Settings->GetClass(), TEXT("CreateAudioDeviceForEveryPlayer"));
			check(AudioSetting);
			AudioSetting->SetPropertyValue_InContainer(Settings, bEveryPlayerAudio);
			Settings->SoloAudioInFirstPIEClient = bSoloFirstAudio;
			GEngine->NetDriverDefinitions = Drivers;
			return true;
		}
	private:
		EPlayNetMode Mode = PIE_Standalone;
		bool bOneProcess = true;
		int32 Players = 1;
		bool bSound = true;
		bool bEveryPlayerAudio = false;
		bool bSoloFirstAudio = false;
		TArray<FNetDriverDefinition> Drivers;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQuestReplicationTest, "ProductionGame.Quests.TwoPlayerReplication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FQuestReplicationTest::RunTest(const FString& Parameters)
{
	TSharedRef<FRestoreQuestPIESettings> Restore = MakeShared<FRestoreQuestPIESettings>();
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/OurStuff/Maps/DevLevels/L_Ben")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FQuestNetworkScenario(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	FAutomationTestFramework::Get().EnqueueLatentCommand(Restore);
	return true;
}
#endif
