#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "QuestDialogueCharacter.generated.h"

class UDialogueComponent;
class UDialogueDefinition;
class UQuestGiverComponent;
class UQuestDefinition;

/** Native parent for the existing talking character Blueprints; keeps their visual components. */
UCLASS(Blueprintable)
class PRODUCTIONGAME_API AQuestDialogueCharacter : public ACharacter
{
	GENERATED_BODY()
public:
	AQuestDialogueCharacter();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dialogue") TObjectPtr<UDialogueComponent> NativeDialogue;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Quest") TObjectPtr<UQuestGiverComponent> NativeQuestGiver;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dialogue") TObjectPtr<UDialogueDefinition> NativeDialogueDefinition;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest") TArray<TObjectPtr<UQuestDefinition>> NativeQuestDefinitions;
protected:
	virtual void BeginPlay() override;
};
