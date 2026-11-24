#include "Heist/RobberLaunchScene.h"

#include "Components/PoseableMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/SkeletalMesh.h"
#include "TwoBoneIK.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogRobberLaunchScene, Log, All);

namespace RobberLaunchSceneLook
{
	const TCHAR* PlainPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
	// Unlit Color x Glow, made by the NadaSet tool for the Mission Control screens.
	const TCHAR* GlowPath = TEXT("/Game/OurStuff/Maps/DevLevels/MuseumGreybox/M_Greybox_Glow.M_Greybox_Glow");
	// The key lock's lamp: red until the key is most of the way round.
	const FLinearColor LampOff(1.f, .02f, .02f);
	const FLinearColor LampOn(.05f, 1.f, .1f);
	constexpr float LampOnAt = .85f;
	constexpr float TurnDegrees = 90.f;

	UMaterialInstanceDynamic* Colour(UStaticMeshComponent* Part, UMaterialInterface* Material, const FLinearColor& Value)
	{
		UMaterialInstanceDynamic* Paint = Part && Material ? Part->CreateAndSetMaterialInstanceDynamicFromMaterial(0, Material) : nullptr;
		if (Paint)
		{
			Paint->SetVectorParameterValue(TEXT("Color"), Value);
		}
		return Paint;
	}
}

// ---- Playing the scene ----------------------------------------------------------------------------------------------

const FName RobberLaunchScene::Tag(TEXT("LaunchScene"));

ALevelSequenceActor* RobberLaunchScene::Find(UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ALevelSequenceActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(Tag))
		{
			return *It;
		}
	}
	return nullptr;
}

bool RobberLaunchScene::Play(UWorld* World)
{
	ALevelSequenceActor* Scene = Find(World);
	ULevelSequencePlayer* Player = Scene ? Scene->GetSequencePlayer() : nullptr;
	if (!Player)
	{
		UE_LOG(LogRobberLaunchScene, Display, TEXT("No launch scene in %s"), *GetNameSafe(World));
		return false;
	}
	ARobberSceneRobber::ShowAll(World, true);
	ARobberLaunchKey::DressScene(World, true);
	Player->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(FFrameTime(0), EUpdatePositionMethod::Jump));
	Player->Play();
	UE_LOG(LogRobberLaunchScene, Display, TEXT("Launch scene playing (%s, %.1f s)"), *GetNameSafe(Scene->GetSequence()), Player->GetDuration().AsSeconds());
	return true;
}

void RobberLaunchScene::Stop(UWorld* World)
{
	ALevelSequenceActor* Scene = Find(World);
	if (ULevelSequencePlayer* Player = Scene ? Scene->GetSequencePlayer() : nullptr)
	{
		Player->Stop();
	}
	ARobberSceneRobber::ShowAll(World, false);
	ARobberLaunchKey::DressScene(World, false);
}

bool RobberLaunchScene::IsPlaying(UWorld* World)
{
	ALevelSequenceActor* Scene = Find(World);
	ULevelSequencePlayer* Player = Scene ? Scene->GetSequencePlayer() : nullptr;
	return Player && Player->IsPlaying();
}

// ---- Stand-in robber ------------------------------------------------------------------------------------------------

ARobberSceneRobber::ARobberSceneRobber()
{
	PrimaryActorTick.bCanEverTick = true;
	// After every mesh has finished its animation for the frame, so Posed copies this frame's pose, not the last.
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(RootComponent);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCanEverAffectNavigation(false);
	// Never drawn, so it has to animate unseen; BeginPlay turns it off in game until the scene plays.
	Body->SetVisibility(false);
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	Posed = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("Posed"));
	Posed->SetupAttachment(Body);
	Posed->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Posed->SetCanEverAffectNavigation(false);

	// A flat disc over the face (the material masks the plane round); placed on the head bone each frame.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	Visor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visor"));
	Visor->SetupAttachment(Posed);
	Visor->SetStaticMesh(Plane.Object);
	Visor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visor->SetCanEverAffectNavigation(false);
	Visor->SetCastShadow(false);
	Visor->SetVisibility(false);

	// Looks out along the visor's face (the plane's +Z), just in front of it, its up the visor's +Y (the head's up), so
	// its right is the visor's +X.
	Mirror = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Mirror"));
	Mirror->SetupAttachment(Visor);
	Mirror->SetRelativeLocation(FVector(0.f, 0.f, 2.f));
	Mirror->SetRelativeRotation(FRotationMatrix::MakeFromXZ(FVector::UpVector, FVector::RightVector).Rotator());
	Mirror->SetUsingAbsoluteScale(true);
	// A domed visor shows a wide, bent view of the room; a wide lens on a flat disc reads the same.
	Mirror->FOVAngle = 80.f;
	Mirror->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	Mirror->bCaptureEveryFrame = false;
	Mirror->bCaptureOnMovement = false;

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}

void ARobberSceneRobber::BeginPlay()
{
	Super::BeginPlay();
	Show(false);
}

void ARobberSceneRobber::ShowAll(UWorld* World, bool bShow)
{
	if (!World)
	{
		return;
	}
	for (TActorIterator<ARobberSceneRobber> It(World); It; ++It)
	{
		It->Show(bShow);
	}
}

void ARobberSceneRobber::Show(bool bShow)
{
	SetActorHiddenInGame(!bShow);
	// Eight meshes animating all night for nobody: only while the scene plays.
	SetActorTickEnabled(bShow);
	Body->SetComponentTickEnabled(bShow);
	Body->bPauseAnims = bMannequin;
	if (bShow)
	{
		bVisorFitted = false;
		Hid.Reset();
		for (AActor* Actor : HideInScene)
		{
			if (Actor)
			{
				Hid.Emplace(Actor, Actor->IsHidden());
				Actor->SetActorHiddenInGame(true);
			}
		}
	}
	else
	{
		for (const TPair<TWeakObjectPtr<AActor>, bool>& Each : Hid)
		{
			if (AActor* Actor = Each.Key.Get())
			{
				Actor->SetActorHiddenInGame(Each.Value);
			}
		}
		Hid.Reset();
	}
	Dress();
}

void ARobberSceneRobber::SetReach(float Value)
{
	Reach = FMath::Clamp(Value, 0.f, 1.f);
}

void ARobberSceneRobber::SetSuit(float Value)
{
	Suit = FMath::Clamp(Value, 0.f, 1.f);
}

bool ARobberSceneRobber::IsMirroring() const
{
	return bMirrorVisor && IsSuited() && !IsHidden() && VisorMaterial;
}

void ARobberSceneRobber::Dress()
{
	const bool bSuited = IsSuited();
	auto Paint = [this](TObjectPtr<UMaterialInstanceDynamic>& Made, const FLinearColor& Colour) -> UMaterialInterface*
	{
		if (!Made && SolidMaterial)
		{
			Made = UMaterialInstanceDynamic::Create(SolidMaterial, this);
		}
		if (Made)
		{
			Made->SetVectorParameterValue(TEXT("Color"), Colour);
		}
		return Made;
	};
	UMaterialInterface* Over = bSuited ? Paint(SuitPaint, SuitColour) : (bMannequin ? Paint(MannequinPaint, MannequinColour) : nullptr);
	for (int32 Slot = 0; Slot < Body->GetNumMaterials(); ++Slot)
	{
		UMaterialInterface* Want = Over ? Over : Body->GetMaterial(Slot);
		if (Posed->GetMaterial(Slot) != Want)
		{
			Posed->SetMaterial(Slot, Want);
		}
	}

	Visor->SetVisibility(bSuited);
	if (bSuited && !VisorPaint && VisorMaterial)
	{
		VisorPaint = UMaterialInstanceDynamic::Create(VisorMaterial, this);
		Visor->SetMaterial(0, VisorPaint);
	}
	const bool bMirror = IsMirroring();
	if (bMirror && !Reflection)
	{
		Reflection = NewObject<UTextureRenderTarget2D>(this);
		Reflection->RenderTargetFormat = RTF_RGBA8_SRGB;
		Reflection->ClearColor = FLinearColor::Black;
		Reflection->InitAutoFormat(512, 512);
		Mirror->TextureTarget = Reflection;
		// Not the inside of his own helmet.
		Mirror->HiddenActors.AddUnique(this);
	}
	if (VisorPaint)
	{
		if (Reflection)
		{
			VisorPaint->SetTextureParameterValue(TEXT("Reflection"), Reflection);
		}
		VisorPaint->SetScalarParameterValue(TEXT("MirrorStrength"), bMirror ? 1.f : 0.f);
		// The glass's own orange would wash the reflection out.
		VisorPaint->SetScalarParameterValue(TEXT("BaseBrightness"), bMirror ? .12f : .6f);
	}
	// A second render of the room each frame: only while the visor is on screen (it's a frame late coming on, unseen).
	Mirror->bCaptureEveryFrame = bMirror && Visor->WasRecentlyRendered(.2f);
}

void ARobberSceneRobber::PlaceVisor()
{
	if (!Visor->IsVisible())
	{
		return;
	}
	const FName HeadBone(TEXT("Head"));
	if (Posed->GetBoneIndex(HeadBone) == INDEX_NONE)
	{
		return;
	}
	const FTransform Head = Posed->GetSocketTransform(HeadBone, RTS_World);
	if (!bVisorFitted)
	{
		// Standing as he is on his first frame, facing ahead: the disc upright in front of the face, its face (the
		// plane's +Z) looking where he looks and its +Y up. Kept relative to the head bone after that, so it nods and
		// turns with it.
		const FVector Forward = GetActorForwardVector(), Up = GetActorUpVector(), Right = GetActorRightVector();
		const FVector At = Head.GetLocation() + Forward * VisorOffset.X + Right * VisorOffset.Y + Up * VisorOffset.Z;
		const FTransform Wanted(FRotationMatrix::MakeFromZY(Forward, Up).ToQuat(), At, FVector(VisorSize / 100.f));
		VisorOnHead = Wanted.GetRelativeTransform(Head);
		bVisorFitted = true;
	}
	Visor->SetWorldTransform(VisorOnHead * Head);
}

void ARobberSceneRobber::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Pose();
}

void ARobberSceneRobber::Pose()
{
	USkeletalMesh* Mesh = Body->GetSkeletalMeshAsset();
	if (!Mesh || Body->GetBoneSpaceTransforms().IsEmpty())
	{
		return;
	}
	if (Posed->GetSkinnedAsset() != Mesh)
	{
		Posed->SetSkinnedAssetAndUpdate(Mesh);
	}
	Dress();
	Posed->CopyPoseFromSkeletalComponent(Body);

	const float Alpha = FMath::SmoothStep(0.f, 1.f, Reach);
	if (Alpha > KINDA_SMALL_NUMBER && ReachFor)
	{
		const FName Shoulder = bLeftHand ? TEXT("LeftArm") : TEXT("RightArm");
		const FName Elbow = bLeftHand ? TEXT("LeftForeArm") : TEXT("RightForeArm");
		const FName Wrist = bLeftHand ? TEXT("LeftHand") : TEXT("RightHand");
		const ARobberLaunchKey* Key = Cast<ARobberLaunchKey>(ReachFor);
		const FVector Grip = Key ? Key->GetGripLocation() : ReachFor->GetActorLocation();
		const FTransform& ToWorld = Posed->GetComponentTransform();
		auto Bone = [this](FName Name) { return Posed->GetBoneTransformByName(Name, EBoneSpaces::ComponentSpace); };

		// Too far for the arm: bend forward at the spine, toward the grip, as far as needed (and allowed).
		const FVector Up = ToWorld.InverseTransformVectorNoScale(FVector::UpVector);
		FVector ShoulderAt = Bone(Shoulder).GetLocation();
		const double ArmLength = (Bone(Elbow).GetLocation() - ShoulderAt).Size() + (Bone(Wrist).GetLocation() - Bone(Elbow).GetLocation()).Size();
		const FVector GripAt = ToWorld.InverseTransformPosition(Grip);
		const double Short = (GripAt - ShoulderAt).Size() + HandLength / ToWorld.GetScale3D().X - ArmLength * .95;
		const FVector Toward = FVector::VectorPlaneProject(GripAt - ShoulderAt, Up).GetSafeNormal();
		const FTransform Spine = Bone(TEXT("Spine"));
		const double SpineToShoulder = FMath::Max(1., (ShoulderAt - Spine.GetLocation()).Size());
		const double Lean = Alpha * FMath::Min(FMath::DegreesToRadians(MaxLean), FMath::Asin(FMath::Clamp(Short / SpineToShoulder, 0., 1.)));
		if (Lean > 0. && !Toward.IsNearlyZero())
		{
			// Up crossed with Toward tips the chest toward the grip.
			FTransform Bent = Spine;
			Bent.SetRotation(FQuat(FVector::CrossProduct(Up, Toward).GetSafeNormal(), Lean) * Spine.GetRotation());
			Posed->SetBoneTransformByName(TEXT("Spine"), Bent, EBoneSpaces::ComponentSpace);
			ShoulderAt = Bone(Shoulder).GetLocation();
		}

		FTransform Upper = Bone(Shoulder);
		FTransform Lower = Bone(Elbow);
		FTransform Hand = Bone(Wrist);
		const FQuat LowerWas = Lower.GetRotation();
		const FQuat HandWas = Hand.GetRotation();
		// The wrist stops a hand's length short of the grip, so the fingers are on it.
		const FVector Target = FMath::Lerp(Hand.GetLocation(), GripAt - (GripAt - ShoulderAt).GetSafeNormal() * (HandLength / ToWorld.GetScale3D().X), Alpha);
		// The elbow bends out to the side and a little down, as when reaching for something low in front.
		const FVector Side = FVector::CrossProduct(Toward, Up).GetSafeNormal() * (bLeftHand ? 1. : -1.);
		const FVector Pole = Lower.GetLocation() + (Side - Up * .5) * ArmLength;
		AnimationCore::SolveTwoBoneIK(Upper, Lower, Hand, Pole, Target, false, 1., 1.);
		// The hand keeps its angle to the forearm, then turns with the key.
		FQuat HandTurn = (Lower.GetRotation() * LowerWas.Inverse()) * HandWas;
		if (Key)
		{
			HandTurn = ToWorld.InverseTransformRotation(Key->GetTurnRotation() * ToWorld.TransformRotation(HandTurn));
		}
		Hand.SetRotation(HandTurn);
		Posed->SetBoneTransformByName(Shoulder, Upper, EBoneSpaces::ComponentSpace);
		Posed->SetBoneTransformByName(Elbow, Lower, EBoneSpaces::ComponentSpace);
		Posed->SetBoneTransformByName(Wrist, Hand, EBoneSpaces::ComponentSpace);
	}
	Posed->RefreshBoneTransforms();
	PlaceVisor();
}

// ---- Launch key -----------------------------------------------------------------------------------------------------

ARobberLaunchKey::ARobberLaunchKey()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Plain(RobberLaunchSceneLook::PlainPath);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(RobberLaunchSceneLook::GlowPath);
	PlainMaterial = Plain.Object;
	GlowMaterial = Glow.Object;

	// Sits on the desk with its origin on the desk top; the operator stands on its -X side.
	Plate = AddPart(TEXT("Plate"), Cube.Object, Root, FVector(0.f, 0.f, .6f), FVector(.14f, .14f, .012f));
	Barrel = AddPart(TEXT("Barrel"), Cylinder.Object, Root, FVector(0.f, 0.f, 2.f), FVector(.055f, .055f, .04f));
	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	Pivot->SetupAttachment(Root);
	Pivot->SetRelativeLocation(FVector(0.f, 0.f, 4.f));
	Blade = AddPart(TEXT("Blade"), Cube.Object, Pivot, FVector(0.f, 0.f, 1.5f), FVector(.006f, .022f, .03f));
	// The bow stands upright, flat face to the operator, so a quarter turn shows its edge.
	Bow = AddPart(TEXT("Bow"), Cylinder.Object, Pivot, FVector(0.f, 0.f, 5.2f), FVector(.055f, .055f, .008f), FRotator(0.f, 0.f, 90.f));
	KeyTag = AddPart(TEXT("Tag"), Cube.Object, Pivot, FVector(0.f, 0.f, 10.f), FVector(.004f, .045f, .06f), FRotator(0.f, 0.f, 12.f));

	Lamp = AddPart(TEXT("Lamp"), Sphere.Object, Root, FVector(0.f, 9.f, 1.2f), FVector(.025f));
}

UStaticMeshComponent* ARobberLaunchKey::AddPart(FName Name, UStaticMesh* Mesh, USceneComponent* Parent, const FVector& Location, const FVector& Scale, const FRotator& Rotation)
{
	UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Part->SetupAttachment(Parent);
	Part->SetStaticMesh(Mesh);
	Part->SetRelativeLocationAndRotation(Location, Rotation);
	Part->SetRelativeScale3D(Scale);
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetCanEverAffectNavigation(false);
	return Part;
}

void ARobberLaunchKey::DressScene(UWorld* World, bool bInScene)
{
	if (!World)
	{
		return;
	}
	for (TActorIterator<ARobberLaunchKey> It(World); It; ++It)
	{
		ARobberLaunchKey* Key = *It;
		if (!bInScene)
		{
			for (const TPair<TWeakObjectPtr<USceneComponent>, bool>& Each : Key->HiddenText)
			{
				if (Each.Key.IsValid())
				{
					Each.Key->SetHiddenInGame(Each.Value);
				}
			}
			Key->HiddenText.Reset();
			continue;
		}
		// The LAUNCH KEYS box's label floats right where the key stands, so it would run through the close-up.
		for (TActorIterator<AActor> Other(World); Other; ++Other)
		{
			if (*Other == Key || FVector::Dist(Other->GetActorLocation(), Key->GetActorLocation()) > 60.)
			{
				continue;
			}
			TArray<UTextRenderComponent*> Texts;
			Other->GetComponents(Texts);
			for (UTextRenderComponent* Text : Texts)
			{
				Key->HiddenText.Emplace(Text, Text->bHiddenInGame != 0);
				Text->SetHiddenInGame(true);
			}
		}
	}
}

FVector ARobberLaunchKey::GetGripLocation() const
{
	return Bow->GetComponentLocation();
}

FQuat ARobberLaunchKey::GetTurnRotation() const
{
	return FQuat(GetActorUpVector(), FMath::DegreesToRadians(Turn * RobberLaunchSceneLook::TurnDegrees));
}

void ARobberLaunchKey::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Paint();
	Apply();
}

void ARobberLaunchKey::BeginPlay()
{
	Super::BeginPlay();
	Paint();
	Apply();
}

void ARobberLaunchKey::Paint()
{
	using namespace RobberLaunchSceneLook;
	Colour(Plate, PlainMaterial, FLinearColor(.03f, .03f, .035f));
	Colour(Barrel, PlainMaterial, MetalColour * .6f);
	Colour(Blade, PlainMaterial, MetalColour);
	Colour(Bow, PlainMaterial, MetalColour);
	Colour(KeyTag, PlainMaterial, TagColour);
	LampPaint = Colour(Lamp, GlowMaterial, LampOff);
	if (LampPaint)
	{
		LampPaint->SetScalarParameterValue(TEXT("Glow"), 4.f);
	}
}

void ARobberLaunchKey::SetTurn(float Value)
{
	Turn = FMath::Clamp(Value, 0.f, 1.f);
	Apply();
}

void ARobberLaunchKey::Apply()
{
	using namespace RobberLaunchSceneLook;
	Pivot->SetRelativeRotation(FRotator(0.f, Turn * TurnDegrees, 0.f));
	if (LampPaint)
	{
		LampPaint->SetVectorParameterValue(TEXT("Color"), Turn >= LampOnAt ? LampOn : LampOff);
	}
}

// ---- Floodlight -----------------------------------------------------------------------------------------------------

ARobberFloodlight::ARobberFloodlight()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Plain(RobberLaunchSceneLook::PlainPath);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(RobberLaunchSceneLook::GlowPath);
	PlainMaterial = Plain.Object;
	GlowMaterial = Glow.Object;
	Post =CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Post"));
	Post->SetupAttachment(Root);
	Post->SetStaticMesh(Cylinder.Object);
	Post->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Aim = CreateDefaultSubobject<USceneComponent>(TEXT("Aim"));
	Aim->SetupAttachment(Root);
	Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	Head->SetupAttachment(Aim);
	Head->SetStaticMesh(Cube.Object);
	Head->SetRelativeLocation(FVector(-20.f, 0.f, 0.f));
	Head->SetRelativeScale3D(FVector(.5f, .8f, .6f));
	Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Lens = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Lens"));
	Lens->SetupAttachment(Aim);
	Lens->SetStaticMesh(Cube.Object);
	Lens->SetRelativeLocation(FVector(6.f, 0.f, 0.f));
	Lens->SetRelativeScale3D(FVector(.02f, .7f, .5f));
	Lens->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Lens->SetCastShadow(false);

	Spot = CreateDefaultSubobject<USpotLightComponent>(TEXT("Spot"));
	Spot->SetupAttachment(Aim);
	Spot->SetRelativeLocation(FVector(10.f, 0.f, 0.f));
	// Changed at run time by Sequencer, so it has to be Movable.
	Spot->SetMobility(EComponentMobility::Movable);
	Spot->SetIntensityUnits(ELightUnits::Candelas);
	Spot->SetIntensity(0.f);
	Spot->SetAttenuationRadius(8000.f);
	Spot->SetCastShadows(false);
}

void ARobberFloodlight::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Post->SetRelativeLocation(FVector(0.f, 0.f, PostHeight * .5f));
	Post->SetRelativeScale3D(FVector(.12f, .12f, PostHeight / 100.f));
	Aim->SetRelativeLocation(FVector(0.f, 0.f, PostHeight));
	const FVector Look = AimAt - FVector(0.f, 0.f, PostHeight);
	if (!Look.IsNearlyZero())
	{
		Aim->SetRelativeRotation(Look.Rotation());
	}
	// Shape only here, in the editor: a cone change at run time on a moving light upsets the GPU scene.
	Spot->SetOuterConeAngle(ConeAngle);
	Spot->SetInnerConeAngle(ConeAngle * .6f);
	Spot->SetLightColor(LightColour);
	Apply();
}

void ARobberFloodlight::BeginPlay()
{
	Super::BeginPlay();
	Apply();
}

void ARobberFloodlight::SetPower(float Value)
{
	Power = FMath::Max(0.f, Value);
	Apply();
}

void ARobberFloodlight::Apply()
{
	Spot->SetIntensity(Brightness * Power);
	Spot->SetVisibility(Power > 0.f);
	if (!LensPaint)
	{
		LensPaint = RobberLaunchSceneLook::Colour(Lens, GlowMaterial, LightColour);
		RobberLaunchSceneLook::Colour(Head, PlainMaterial, FLinearColor(.08f, .08f, .09f));
		RobberLaunchSceneLook::Colour(Post, PlainMaterial, FLinearColor(.25f, .25f, .27f));
	}
	if (LensPaint)
	{
		// A dull grey glass when off, a blown-out white face when on.
		LensPaint->SetScalarParameterValue(TEXT("Glow"), FMath::Lerp(.03f, 30.f, FMath::Min(Power, 1.f)));
	}
}
