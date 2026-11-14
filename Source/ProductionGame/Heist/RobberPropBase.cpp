#include "Heist/RobberPropBase.h"

#include "Heist/RobberHighlight.h"
#include "UObject/UnrealType.h"

ARobberPropBase::ARobberPropBase()
{
	Highlight = CreateDefaultSubobject<URobberHighlightComponent>(TEXT("RobberHighlight"));
	InteractComponent = CreateDefaultSubobject<URobberInteractComponent>(TEXT("RobberInteract"));
}

FText ARobberPropBase::GetInteractText(const ARobberGameState* GameState, const APlayerState* Viewer) const
{
	// The Blueprint function isn't const, but GetInteractData only reads.
	return FText::FromString(GetInteractData(const_cast<ARobberPropBase*>(this)));
}

FString ARobberPropBase::GetInteractData(AActor* Actor)
{
	// BPI_Interaction is a Blueprint interface, so C++ can only reach it by name.
	UFunction* Function = Actor ? Actor->FindFunction(TEXT("GetInteractData")) : nullptr;
	if (!Function)
	{
		return FString();
	}
	uint8* Params = static_cast<uint8*>(FMemory_Alloca_Aligned(Function->ParmsSize, Function->GetMinAlignment()));
	Function->InitializeStruct(Params);
	Actor->ProcessEvent(Function, Params);
	FString Text;
	for (TFieldIterator<FStrProperty> It(Function); It; ++It)
	{
		if (It->HasAnyPropertyFlags(CPF_OutParm))
		{
			Text = It->GetPropertyValue_InContainer(Params);
			break;
		}
	}
	Function->DestroyStruct(Params);
	return Text;
}
