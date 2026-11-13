#pragma once

#include "CoreMinimal.h"
#include "Heist/RobberCaseFileUI.h"
#include "RobberPowerCut.generated.h"

/**
 * Logic for WBP_PowerCut, the generator's breaker panel. Trip all the breakers by catching a sweeping needle in the
 * green band; a miss makes noise and one breaker pops back up. Only finishing it runs BP_PowerGenerator's CutPower.
 * Widgets it uses, by name (all optional): PanelBox, MeterBand, MeterNeedle, StatusText, TripButton, LeaveButton,
 * LightsOut, and Lamp_N, Handle_N, State_N for each breaker from 0.
 */
UCLASS(Abstract)
class PRODUCTIONGAME_API URobberPowerCut : public URobberCaseFileWidgetBase
{
	GENERATED_BODY()

public:
	URobberPowerCut(const FObjectInitializer& Initializer);

	/** From BP_PowerGenerator's Interact, on every machine that runs it. On the server, opens the panel for whoever pressed. */
	UFUNCTION(BlueprintCallable, Category = "Robber|Power")
	static void BeginFromGenerator(AActor* Generator, AActor* InteractingPlayer);

	static constexpr int32 BreakerCount = 4;
	static constexpr float MissHeat = 8.f;

	void OnOpened(AActor* InGenerator);

	/** The trip key or button. Ignored during the lockout after a miss. */
	void Press();

	int32 GetBreakersDown() const { return BreakersDown; }
	float GetBandCentre() const { return BandCentre; }
	bool IsDone() const { return bDone; }
	bool IsLockedOut() const { return Lockout > 0.f; }
	void SetNeedle(float Position) { Needle = Position; }

	/** Needle travel per second for each breaker in turn, as a fraction of the meter. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power Cut")
	TArray<float> NeedleSpeeds = { 0.8f, 1.f, 1.25f, 1.5f };

	/** Width of the green band for each breaker in turn, as a fraction of the meter. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power Cut")
	TArray<float> BandWidths = { 0.26f, 0.22f, 0.18f, 0.15f };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power Cut")
	float LockoutSeconds = 0.5f;

	/** How long "LIGHTS OUT" stays up before the panel closes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power Cut")
	float CloseDelay = 1.f;

	/** Must match the width of MeterFrame in the WBP. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power Cut")
	float MeterWidth = 640.f;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;

private:
	UFUNCTION() void OnTrip();
	UFUNCTION() void OnLeave();

	float CurrentSpeed() const;
	float CurrentBandWidth() const;
	void PickBand();
	void Draw();
	void Say(const FText& Text);
	void SayNextBreaker();

	TWeakObjectPtr<AActor> Generator;
	int32 BreakersDown = 0;
	float Needle = 0.f;
	float Direction = 1.f;
	float BandCentre = 0.5f;
	float Lockout = 0.f;
	float CloseIn = 0.f;
	bool bDone = false;
	bool bCutWhenOpened = false;
};
