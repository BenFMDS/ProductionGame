#pragma once

#include "CoreMinimal.h"
#include "AchievementTypes.h"
#include "Blueprint/UserWidget.h"
#include "RobberAchievementPopup.generated.h"

class UAchievementDefinitionAsset;
class UImage;
class USizeBox;
class UTextBlock;
class UWidget;
class UWrapBox;

UENUM()
enum class ERobberPopupKind : uint8
{
	None,
	/** A: a normal unlock, the front page spins in top-right. */
	FrontPage,
	/** G: a counted achievement passed a quarter, a draft proof slides in bottom-right. */
	DraftProof,
	/** E: a secret unlock, the ransom-note card drops in top-centre. */
	Secret,
};

/**
 * The in-game achievement popups from the Heist Team Board mock-ups (A, G and E), built in code in the Case File
 * look. One per local player, always in the viewport; popups queue and play one at a time.
 */
UCLASS()
class PRODUCTIONGAME_API URobberAchievementPopup : public UUserWidget
{
	GENERATED_BODY()

public:
	/** bProgress: a 25/50/75% milestone rather than the unlock itself. */
	void Enqueue(UAchievementDefinitionAsset* Asset, const FAchievementProgressState& State, bool bProgress);

	ERobberPopupKind GetShowingKind() const { return Showing.Kind; }
	FName GetShowingId() const;
	int32 GetQueuedCount() const { return Queue.Num(); }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	struct FEntry
	{
		TWeakObjectPtr<UAchievementDefinitionAsset> Asset;
		FAchievementProgressState State;
		ERobberPopupKind Kind = ERobberPopupKind::None;
	};

	void Build();
	void BuildFrontPage(class UCanvasPanel* Root);
	void BuildDraftProof(UCanvasPanel* Root);
	void BuildSecret(UCanvasPanel* Root);

	void Start(const FEntry& Entry);
	/** Seconds the current popup is up for, entrance and exit included. */
	float Duration() const;
	void Animate(const FVector2D& ScreenSize);
	void AnimateFrontPage(const FVector2D& ScreenSize);
	void AnimateDraftProof();
	void AnimateSecret();

	TArray<FEntry> Queue;
	FEntry Showing;
	float Clock = 0.f;

	// Draft proof: how far the red line runs, before and after this milestone (0-1).
	float InkFrom = 0.f;
	float InkTo = 0.f;

	UPROPERTY(Transient) TObjectPtr<UImage> Flash;

	UPROPERTY(Transient) TObjectPtr<UWidget> FrontRoot;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> FrontTitle;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> FrontBody;
	UPROPERTY(Transient) TObjectPtr<UWidget> FrontPic;
	UPROPERTY(Transient) TObjectPtr<UImage> FrontImage;

	UPROPERTY(Transient) TObjectPtr<UWidget> DraftRoot;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DraftTitle;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DraftBody;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DraftCount;
	UPROPERTY(Transient) TObjectPtr<UWidget> DraftCircle;
	UPROPERTY(Transient) TObjectPtr<UWidget> DraftStamp;
	UPROPERTY(Transient) TObjectPtr<USizeBox> DraftInk;
	/** The pencilled track the ink runs along; its laid-out width sets how long a full line is. */
	UPROPERTY(Transient) TObjectPtr<UWidget> DraftTrack;

	UPROPERTY(Transient) TObjectPtr<UWidget> SecretRoot;
	UPROPERTY(Transient) TObjectPtr<UWrapBox> SecretLetters;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SecretBody;
	UPROPERTY(Transient) TObjectPtr<UWidget> SecretPic;
	UPROPERTY(Transient) TObjectPtr<UImage> SecretImage;
	/** The cut-out letters, in order, for the one-at-a-time slap. */
	UPROPERTY(Transient) TArray<TObjectPtr<UWidget>> Letters;
};
