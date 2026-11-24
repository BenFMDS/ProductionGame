#include "Heist/RobberSpaceProps.h"

#include "Camera/CameraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace RobberSpaceShapes
{
	// Engine basic shapes: sphere radius 50, cube 100, cylinder and cone 100 across and 100 tall along Z.
	constexpr double Unit = 50.;

	/** (x, y, radius) as Ben drew them: x right, y down, in radii of the ball, on the side facing the camera. */
	const FVector3f Continents[] = {
		// Top: the tall wiggly one.
		{-.22f, -.58f, .15f}, {-.30f, -.44f, .13f}, {-.20f, -.32f, .12f}, {-.26f, -.18f, .10f}, {-.14f, -.10f, .08f}, {-.08f, -.46f, .09f},
		// Bottom left: the big one.
		{-.46f, .16f, .12f}, {-.32f, .22f, .16f}, {-.20f, .38f, .15f}, {-.36f, .40f, .12f}, {-.12f, .52f, .10f},
		// Right: the small one.
		{.24f, .04f, .11f}, {.28f, .18f, .13f}, {.22f, .32f, .11f},
	};
	const FVector3f Craters[] = {{-.30f, -.25f, .24f}, {.35f, .12f, .18f}, {-.05f, .42f, .15f}, {.18f, -.45f, .1f}, {-.5f, .2f, .09f}};

	/** A point on the camera side of the ball (-X faces the camera, +Y is screen right, +Z up). */
	FVector FacingCamera(const FVector3f& Spot)
	{
		const double Depth = FMath::Sqrt(FMath::Max(0., 1. - FMath::Square(Spot.X) - FMath::Square(Spot.Y)));
		return FVector(-Depth, Spot.X, -Spot.Y);
	}

	/** A flattened ball lying on the surface of a ball of radius Unit: a continent or a crater. */
	void LayOnSurface(UStaticMeshComponent* Patch, const FVector3f& Spot, double Thickness, double Sink)
	{
		const FVector Out = FacingCamera(Spot);
		Patch->SetRelativeRotation(FRotationMatrix::MakeFromZ(Out).Rotator());
		Patch->SetRelativeScale3D(FVector(Spot.Z, Spot.Z, Spot.Z * Thickness));
		Patch->SetRelativeLocation(Out * Unit * (1. - Spot.Z * Sink));
	}

	/**
	 * Lays a Z-up shape (cylinder, cone) along the rocket's X axis with its base at BaseX, pointing forward (+X) or back.
	 * Works from the mesh bounds, so it doesn't matter where the shape's pivot is.
	 */
	void AlongX(UStaticMeshComponent* Part, double BaseX, double Length, double Diameter, bool bForward)
	{
		const UStaticMesh* Mesh = Part->GetStaticMesh();
		if (!Mesh)
		{
			return;
		}
		const FBoxSphereBounds Bounds = Mesh->GetBounds();
		const double Height = FMath::Max(Bounds.BoxExtent.Z * 2., 1.);
		const double Across = FMath::Max(Bounds.BoxExtent.X * 2., 1.);
		const double ScaleZ = Length / Height;
		const double Bottom = (Bounds.Origin.Z - Bounds.BoxExtent.Z) * ScaleZ;
		// Pitch -90 turns the mesh's +Z to +X, pitch 90 turns it to -X.
		Part->SetRelativeRotation(FRotator(bForward ? -90. : 90., 0., 0.));
		Part->SetRelativeScale3D(FVector(Diameter / Across, Diameter / Across, ScaleZ));
		Part->SetRelativeLocation(FVector(bForward ? BaseX - Bottom : BaseX + Bottom, 0., 0.));
	}
}

// ---------------------------------------------------------------------------------------------------------------------

ARobberSpaceProp::ARobberSpaceProp()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	// Only the ending shows it, on its own machine.
	SetHidden(true);
	SetCanBeDamaged(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

UStaticMesh* ARobberSpaceProp::Shape(const TCHAR* Name)
{
	ConstructorHelpers::FObjectFinder<UStaticMesh> Finder(*FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Name, Name));
	return Finder.Object;
}

UStaticMeshComponent* ARobberSpaceProp::AddPart(FName Name, UStaticMesh* Mesh, USceneComponent* Parent)
{
	UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Part->SetStaticMesh(Mesh);
	Part->SetupAttachment(Parent ? Parent : RootComponent.Get());
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Part->SetGenerateOverlapEvents(false);
	Part->CanCharacterStepUpOn = ECB_No;
	// Unlit and miles from anything: keep it out of the level's lighting entirely.
	Part->SetCastShadow(false);
	Part->bAffectDistanceFieldLighting = false;
	Part->bAffectDynamicIndirectLighting = false;
	Part->bVisibleInReflectionCaptures = false;
	Part->bVisibleInRealTimeSkyCaptures = false;
	Part->bVisibleInRayTracing = false;
	Part->bReceivesDecals = false;
	return Part;
}

UMaterialInstanceDynamic* ARobberSpaceProp::Paint(UPrimitiveComponent* Part, const FLinearColor& Colour, const FLinearColor& Shade, float Glow)
{
	if (!Part || !FlatMaterial)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(FlatMaterial, this);
	Material->SetVectorParameterValue(TEXT("Color"), Colour);
	Material->SetVectorParameterValue(TEXT("ShadeColor"), Shade);
	Material->SetVectorParameterValue(TEXT("LightFrom"), FLinearColor(LightFrom.GetSafeNormal()));
	Material->SetScalarParameterValue(TEXT("Glow"), Glow);
	Part->SetMaterial(0, Material);
	return Material;
}

float ARobberSpaceProp::TravelAlpha(float Seconds) const
{
	const float Linear = FMath::Clamp(Seconds / TravelSeconds, 0.f, 1.f);
	return 1.f - FMath::Square(1.f - Linear);
}

void ARobberSpaceProp::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Rebuild();
}

void ARobberSpaceProp::BeginPlay()
{
	Super::BeginPlay();
	// A cooked level doesn't rerun construction, and the materials it made in the editor are throwaway.
	Rebuild();
	Home = GetActorTransform();
}

void ARobberSpaceProp::ShowAll(UWorld* World, bool bShow)
{
	if (!World)
	{
		return;
	}
	for (TActorIterator<ARobberSpaceProp> It(World); It; ++It)
	{
		It->ShowInShot(bShow);
	}
}

void ARobberSpaceProp::ShowInShot(bool bShow)
{
	if (bShow == bInShot)
	{
		return;
	}
	bInShot = bShow;
	SetActorTransform(Home);
	ShotTime = 0.f;
	Animate(0.f);
	SetActorHiddenInGame(!bShow);
	SetActorTickEnabled(bShow);
}

void ARobberSpaceProp::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ShotTime += DeltaSeconds;
	if (!TravelTo.IsNearlyZero())
	{
		SetActorLocation(Home.TransformPosition(TravelTo * TravelAlpha(ShotTime)));
	}
	Animate(ShotTime);
}

// ---------------------------------------------------------------------------------------------------------------------

ARobberSpaceEarth::ARobberSpaceEarth()
{
	using namespace RobberSpaceShapes;
	UStaticMesh* Sphere = Shape(TEXT("Sphere"));
	Globe = CreateDefaultSubobject<USceneComponent>(TEXT("Globe"));
	Globe->SetupAttachment(RootComponent);
	Sea = AddPart(TEXT("Sea"), Sphere, Globe);

	Air = AddPart(TEXT("Air"), Shape(TEXT("Cylinder")), RootComponent);
	// Edge on to the camera through the middle of the globe, so only the bit sticking out past the edge shows.
	Air->SetRelativeRotation(FRotator(90., 0., 0.));
	// The globe's outline sits a touch outside this disc (perspective), so a little over 1 is barely a hairline;
	// the nudge puts the rim on the sun's side only.
	Air->SetRelativeScale3D(FVector(1.03, 1.03, .01));
	Air->SetRelativeLocation(FVector(0., Unit * .035, Unit * .017));

	int32 Index = 0;
	for (const FVector3f& Spot : RobberSpaceShapes::Continents)
	{
		UStaticMeshComponent* Land = AddPart(*FString::Printf(TEXT("Land%d"), Index++), Sphere, Globe);
		LayOnSurface(Land, Spot, .3, .12);
		Continents.Add(Land);
	}
}

void ARobberSpaceEarth::Rebuild()
{
	Paint(Sea, SeaColour, NightColour);
	Paint(Air, AirColour, AirColour);
	for (UStaticMeshComponent* Land : Continents)
	{
		Paint(Land, LandColour, LandShadeColour);
	}
}

void ARobberSpaceEarth::Animate(float Seconds)
{
	Globe->SetRelativeRotation(FRotator(0., -SpinSpeed * Seconds, 0.));
}

// ---------------------------------------------------------------------------------------------------------------------

ARobberSpaceSun::ARobberSpaceSun()
{
	Disc = AddPart(TEXT("Disc"), Shape(TEXT("Sphere")), RootComponent);
}

void ARobberSpaceSun::Rebuild()
{
	DiscPaint = Paint(Disc, SunColour, SunColour, Glow);
}

void ARobberSpaceSun::Animate(float Seconds)
{
	if (DiscPaint)
	{
		DiscPaint->SetScalarParameterValue(TEXT("Glow"), Glow * (.88f + .12f * FMath::Sin(Seconds * 1.7f)));
	}
}

// ---------------------------------------------------------------------------------------------------------------------

ARobberSpaceMoon::ARobberSpaceMoon()
{
	using namespace RobberSpaceShapes;
	UStaticMesh* Sphere = Shape(TEXT("Sphere"));
	Ball = AddPart(TEXT("Ball"), Sphere, RootComponent);
	int32 Index = 0;
	for (const FVector3f& Spot : RobberSpaceShapes::Craters)
	{
		UStaticMeshComponent* Crater = AddPart(*FString::Printf(TEXT("Crater%d"), Index++), Sphere, RootComponent);
		LayOnSurface(Crater, Spot, .15, .06);
		Craters.Add(Crater);
	}
}

void ARobberSpaceMoon::Rebuild()
{
	Paint(Ball, MoonColour, MoonShadeColour);
	for (UStaticMeshComponent* Crater : Craters)
	{
		Paint(Crater, CraterColour, MoonShadeColour * .8f);
	}
}

// ---------------------------------------------------------------------------------------------------------------------

namespace RobberRocketSize
{
	// In rocket units (scale the actor to size it): body 100 across, 380 long, nose to tail along X.
	constexpr double BodyLength = 380.;
	constexpr double Tail = -BodyLength / 2.;
	constexpr double FlameLength = 170.;
	constexpr double FlameCoreLength = 100.;
}

ARobberSpaceRocket::ARobberSpaceRocket()
{
	using namespace RobberSpaceShapes;
	using namespace RobberRocketSize;
	UStaticMesh* Sphere = Shape(TEXT("Sphere"));
	UStaticMesh* Cube = Shape(TEXT("Cube"));
	UStaticMesh* Cone = Shape(TEXT("Cone"));
	Craft = CreateDefaultSubobject<USceneComponent>(TEXT("Craft"));
	Craft->SetupAttachment(RootComponent);

	Body = AddPart(TEXT("Body"), Shape(TEXT("Cylinder")), Craft);
	AlongX(Body, Tail, BodyLength, 100., true);
	Nose = AddPart(TEXT("Nose"), Cone, Craft);
	AlongX(Nose, -Tail, 130., 100., true);

	// Top, bottom and both sides, swept back off the tail.
	for (int32 Index = 0; Index < 4; ++Index)
	{
		UStaticMeshComponent* Fin = AddPart(*FString::Printf(TEXT("Fin%d"), Index), Cube, Craft);
		const double Roll = 90. * Index;
		Fin->SetRelativeRotation(FRotator(0., 0., Roll));
		Fin->SetRelativeScale3D(FVector(.95, .07, .75));
		Fin->SetRelativeLocation(FRotator(0., 0., Roll).RotateVector(FVector(Tail + 40., 0., 75.)));
		Fins.Add(Fin);
	}

	// A porthole on each side, so whichever way it's turned the camera sees one.
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		UStaticMeshComponent* Rim = AddPart(*FString::Printf(TEXT("WindowRim%d"), Side > 0), Sphere, Craft);
		Rim->SetRelativeRotation(FRotationMatrix::MakeFromZ(FVector(0., Side, 0.)).Rotator());
		Rim->SetRelativeScale3D(FVector(.5, .5, .12));
		Rim->SetRelativeLocation(FVector(55., Side * 48., 0.));
		WindowRims.Add(Rim);
		UStaticMeshComponent* Window = AddPart(*FString::Printf(TEXT("Window%d"), Side > 0), Sphere, Craft);
		Window->SetRelativeRotation(FRotationMatrix::MakeFromZ(FVector(0., Side, 0.)).Rotator());
		Window->SetRelativeScale3D(FVector(.36, .36, .12));
		Window->SetRelativeLocation(FVector(55., Side * 50., 0.));
		Windows.Add(Window);
	}

	Flame = AddPart(TEXT("Flame"), Cone, Craft);
	AlongX(Flame, Tail + 5., FlameLength, 85., false);
	FlameCore = AddPart(TEXT("FlameCore"), Cone, Craft);
	AlongX(FlameCore, Tail + 4., FlameCoreLength, 55., false);
}

void ARobberSpaceRocket::Rebuild()
{
	Paint(Body, HullColour, HullShadeColour);
	Paint(Nose, TrimColour, HullShadeColour * .7f);
	for (UStaticMeshComponent* Fin : Fins)
	{
		Paint(Fin, TrimColour, HullShadeColour * .7f);
	}
	for (UStaticMeshComponent* Rim : WindowRims)
	{
		Paint(Rim, FLinearColor(.9f, .9f, .92f), HullShadeColour);
	}
	for (UStaticMeshComponent* Window : Windows)
	{
		Paint(Window, WindowColour, WindowColour * .6f);
	}
	// Over 1 so the flame blooms a little on the space camera.
	Paint(Flame, FlameColour, FlameColour, 2.f);
	Paint(FlameCore, FlameCoreColour, FlameCoreColour, 2.5f);
}

void ARobberSpaceRocket::Animate(float Seconds)
{
	using namespace RobberRocketSize;
	Craft->SetRelativeLocation(FVector(0., 0., 6. * FMath::Sin(Seconds * 1.3f)));
	const double Flicker = 1. + .22 * FMath::Sin(Seconds * 31.f) + .1 * FMath::Sin(Seconds * 53.f);
	RobberSpaceShapes::AlongX(Flame, Tail + 5., FlameLength * Flicker, 85., false);
	RobberSpaceShapes::AlongX(FlameCore, Tail + 4., FlameCoreLength * (2. - Flicker), 55., false);
}

// ---------------------------------------------------------------------------------------------------------------------

ARobberSpaceStars::ARobberSpaceStars()
{
	Sky = AddPart(TEXT("Sky"), Shape(TEXT("Sphere")), RootComponent);
	UStaticMesh* Cube = Shape(TEXT("Cube"));
	Stars = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Stars"));
	Stars->SetStaticMesh(Cube);
	Stars->SetupAttachment(RootComponent);
	Stars->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Stars->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Stars->SetGenerateOverlapEvents(false);
	Stars->SetCastShadow(false);
	Stars->bAffectDistanceFieldLighting = false;
	Stars->bAffectDynamicIndirectLighting = false;
	Stars->bVisibleInReflectionCaptures = false;
	Stars->bVisibleInRealTimeSkyCaptures = false;
	Stars->bVisibleInRayTracing = false;
	Stars->bReceivesDecals = false;
}

void ARobberSpaceStars::Rebuild()
{
	Sky->SetRelativeScale3D(FVector(SkyRadius / RobberSpaceShapes::Unit));
	Paint(Sky, FLinearColor::Black, FLinearColor::Black, 0.f);
	if (UMaterialInstanceDynamic* Twinkle = Paint(Stars, FLinearColor::White, FLinearColor::White, 1.4f))
	{
		Twinkle->SetScalarParameterValue(TEXT("Twinkle"), 1.f);
	}

	Stars->ClearInstances();
	// One pixel at 1080p with the space camera's 70 degree view, this far out.
	const double Pixel = StarDistance * FMath::Tan(FMath::DegreesToRadians(35.)) * 2. / 1920.;
	FRandomStream Random(Seed);
	TArray<FTransform> Placed;
	Placed.Reserve(StarCount);
	for (int32 Index = 0; Index < StarCount; ++Index)
	{
		// Only in front of the camera (which looks along +X), a little wider than it can see so the drift has spare.
		const FVector Out = FRotator(Random.FRandRange(-40.f, 40.f), Random.FRandRange(-62.f, 62.f), 0.f).Vector();
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Out).GetSafeNormal();
		const FVector Up = FVector::CrossProduct(Out, Right);
		const double Angle = Random.FRand() * UE_PI;
		const FVector Along = Right * FMath::Cos(Angle) + Up * FMath::Sin(Angle);
		// Ben drew them as little dashes, some longer than others.
		const double Length = (2. + Random.FRand() * 6.) * Pixel;
		const double Thick = 2.5 * Pixel;
		Placed.Add(FTransform(FRotationMatrix::MakeFromXZ(Along, Out).Rotator(), Out * StarDistance, FVector(Length, Thick, Thick) / 100.));
	}
	Stars->AddInstances(Placed, false);
}

void ARobberSpaceStars::Animate(float Seconds)
{
	Sky->SetRelativeRotation(FRotator(0., -DriftSpeed * Seconds, 0.));
	Stars->SetRelativeRotation(FRotator(0., -DriftSpeed * Seconds, 0.));
}

// ---------------------------------------------------------------------------------------------------------------------

ARobberSpaceCamera::ARobberSpaceCamera()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	UCameraComponent* Lens = GetCameraComponent();
	Lens->SetFieldOfView(70.f);
	Lens->SetConstraintAspectRatio(false);
	FPostProcessSettings& Look = Lens->PostProcessSettings;
	// Fixed exposure: the flat colours come out as picked, whatever the level's own post process does.
	Look.bOverride_AutoExposureMethod = true;
	Look.AutoExposureMethod = AEM_Manual;
	Look.bOverride_AutoExposureBias = true;
	Look.AutoExposureBias = 0.f;
	Look.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	Look.AutoExposureApplyPhysicalCameraExposure = false;
	Look.bOverride_BloomIntensity = true;
	Look.BloomIntensity = 1.2f;
	Look.bOverride_MotionBlurAmount = true;
	Look.MotionBlurAmount = 0.f;
	Look.bOverride_LensFlareIntensity = true;
	Look.LensFlareIntensity = 0.f;
	Look.bOverride_VignetteIntensity = true;
	Look.VignetteIntensity = .35f;
	Lens->PostProcessBlendWeight = 1.f;
}

ARobberSpaceCamera* ARobberSpaceCamera::Find(UWorld* World)
{
	if (World)
	{
		for (TActorIterator<ARobberSpaceCamera> It(World); It; ++It)
		{
			return *It;
		}
	}
	return nullptr;
}

void ARobberSpaceCamera::StartShot()
{
	if (!bShooting)
	{
		Home = GetActorLocation();
		bShooting = true;
	}
	ShotTime = 0.f;
	SetActorTickEnabled(true);
}

void ARobberSpaceCamera::EndShot()
{
	if (bShooting)
	{
		SetActorLocation(Home);
		bShooting = false;
	}
	SetActorTickEnabled(false);
}

void ARobberSpaceCamera::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ShotTime += DeltaSeconds;
	const float Linear = FMath::Clamp(ShotTime / PushSeconds, 0.f, 1.f);
	SetActorLocation(Home + GetActorForwardVector() * PushIn * (1.f - FMath::Square(1.f - Linear)));
}
