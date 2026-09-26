#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "QuestObjectiveComponent.generated.h"

class UPlayerQuestComponent;

/** Server-validated quest collection for BP_InteractMaster children. No overlap collection. */
UCLASS(ClassGroup=(Quest), BlueprintType, meta=(BlueprintSpawnableComponent))
class PRODUCTIONGAME_API UQuestObjectiveComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest") FName ProgressEvent = TEXT("CollectShield");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest", meta=(ClampMin="1")) int32 Amount = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest", meta=(ClampMin="1")) float InteractionDistance = 350.f;
	/** Called by the owned player component after an interaction request reaches the server. */
	bool Collect(UPlayerQuestComponent* Player);
	bool HasBeenCollectedBy(const UPlayerQuestComponent* Player) const;
private:
	TSet<TWeakObjectPtr<UPlayerQuestComponent>> ClaimedBy;
};
