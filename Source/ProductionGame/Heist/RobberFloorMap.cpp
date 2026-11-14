#include "Heist/RobberFloorMap.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ARobberFloorMap::ARobberFloorMap()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Board = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Board"));
	Board->SetupAttachment(Root);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Cube = CubeMesh.Object;
	Board->SetStaticMesh(Cube);
	Tags.Add(TEXT("YouAreHere"));
}

void ARobberFloorMap::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Rebuild();
}

void ARobberFloorMap::BeginPlay()
{
	Super::BeginPlay();
	// The plan's parts aren't saved, and a cooked level doesn't rerun construction, so build them here too.
	if (Generated.IsEmpty())
	{
		Rebuild();
	}
}

UStaticMeshComponent* ARobberFloorMap::AddBlock(const FVector& Location, const FVector& Size, const FQuat& Rotation, FName MaterialName)
{
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
	Part->SetupAttachment(Root);
	Part->SetStaticMesh(Cube);
	Part->SetMaterial(0, Materials.FindRef(MaterialName));
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetRelativeTransform(FTransform(Rotation, Location, Size / 100.));
	Part->RegisterComponent();
	Generated.Add(Part);
	return Part;
}

UTextRenderComponent* ARobberFloorMap::AddText(const FString& Text, const FVector& Location, float Size, const FColor& Colour, EHorizTextAligment Align)
{
	UTextRenderComponent* Part = NewObject<UTextRenderComponent>(this, NAME_None, RF_Transient);
	Part->SetupAttachment(Root);
	Part->SetRelativeLocation(Location);
	Part->SetText(FText::FromString(Text));
	Part->SetWorldSize(Size);
	Part->SetTextRenderColor(Colour);
	Part->SetHorizontalAlignment(Align);
	Part->SetVerticalAlignment(EVRTA_TextCenter);
	Part->RegisterComponent();
	Generated.Add(Part);
	return Part;
}

void ARobberFloorMap::Rebuild()
{
	for (USceneComponent* Part : Generated)
	{
		if (Part)
		{
			Part->DestroyComponent();
		}
	}
	Generated.Reset();

	// Board space: X out of the face, Y to the reader's left (text reads along -Y), Z up from the floor.
	const double Width = BoardSize.Y, Height = BoardSize.Z, Face = BoardSize.X / 2.;
	Board->SetRelativeTransform(FTransform(FQuat::Identity, FVector(0., 0., Height / 2.), BoardSize / 100.));
	Board->SetMaterial(0, Materials.FindRef(TEXT("sign")));
	if (!bDrawPlan || !Cube)
	{
		return;
	}
	const double MapLeft = Width * 0.487, MapRight = -Width * 0.053, MapBottom = Height * 0.09, MapTop = Height * 0.84;
	const double LegendY = -Width * 0.084, LegendTop = Height * 0.85, LegendLine = 10.;
	AddText(FloorName.ToString().ToUpper(), FVector(Face + 1., (MapLeft + MapRight) / 2., Height - 12.), 20.f, Ink);

	// Plan to board: u runs to the reader's right, v away from them. The reader faces into the board.
	const FVector2D Facing = FVector2D(GetActorForwardVector()).GetSafeNormal();
	const FVector2D Forward = -Facing;
	const FVector2D Right(-Forward.Y, Forward.X);
	const auto Plan = [&](const FVector2D& P) { return FVector2D(FVector2D::DotProduct(P, Right), FVector2D::DotProduct(P, Forward)); };
	const auto Corners = [](const FBox2D& Box) { return TArray<FVector2D>{ Box.Min, Box.Max, FVector2D(Box.Min.X, Box.Max.Y), FVector2D(Box.Max.X, Box.Min.Y) }; };

	FBox2D Extent(ForceInit);
	for (const FRobberFloorMapRoom& Room : Rooms)
	{
		for (const FBox2D& Rect : Room.Rects)
		{
			for (const FVector2D& Corner : Corners(Rect)) Extent += Plan(Corner);
		}
	}
	if (!Extent.bIsValid)
	{
		return;
	}
	const FVector2D Size = Extent.GetSize();
	const double Scale = FMath::Min((MapLeft - MapRight) / FMath::Max(1., Size.X), (MapTop - MapBottom) / FMath::Max(1., Size.Y));
	const FVector2D Centre = Extent.GetCenter();
	const auto OnBoard = [&](const FVector2D& Point, double X)
	{
		const FVector2D UV = Plan(Point);
		return FVector(X, (MapLeft + MapRight) / 2. - (UV.X - Centre.X) * Scale, (MapTop + MapBottom) / 2. + (UV.Y - Centre.Y) * Scale);
	};
	// World X on the board, as an angle round the board's X axis, so the plan turns with the actor however it's rotated.
	const FQuat Turn(FVector::ForwardVector, FMath::Atan2(Forward.X, -Right.X));
	const auto DrawRect = [&](const FBox2D& Rect, FName MaterialName, double X, double Depth)
	{
		const FVector Mid = OnBoard(Rect.GetCenter(), X);
		const FVector2D RectSize = Rect.GetSize();
		AddBlock(Mid, FVector(Depth, FMath::Max(1., RectSize.X * Scale - 1.), FMath::Max(1., RectSize.Y * Scale - 1.)), Turn, MaterialName);
		return Mid;
	};

	double LegendZ = LegendTop;
	for (int32 Index = 0; Index < Rooms.Num(); ++Index)
	{
		const FRobberFloorMapRoom& Room = Rooms[Index];
		for (const FBox2D& Rect : Room.Rects)
		{
			DrawRect(Rect, Room.Kind, Face + 1.5, 2.);
		}
		if (Room.Rects.IsEmpty())
		{
			continue;
		}
		const FVector Mid = OnBoard(Room.Rects[0].GetCenter(), Face + 3.);
		AddText(FString::FromInt(Index + 1), Mid, 9.f, Ink);
		AddText(FString::Printf(TEXT("%d  %s"), Index + 1, *Room.Name), FVector(Face + 1., LegendY, LegendZ), 8.f, Ink, EHTA_Left);
		LegendZ -= LegendLine;
	}
	for (const FBox2D& Stair : Stairs)
	{
		// Just proud of the rooms but behind their numbers; white so it reads on the dark block.
		const FVector Mid = DrawRect(Stair, TEXT("rail"), Face + 2., 1.);
		AddText(TEXT("S"), FVector(Face + 4., Mid.Y, Mid.Z), 8.f, FColor::White);
	}
	if (!Stairs.IsEmpty())
	{
		AddText(TEXT("S  Stairs"), FVector(Face + 1., LegendY, LegendZ - 4.), 8.f, Ink, EHTA_Left);
	}

	const FVector2D Reader = FVector2D(GetActorLocation()) / 100. + Facing * ReaderDistance;
	HereOnBoard = OnBoard(Reader, Face + 4.);
	AddBlock(HereOnBoard, FVector(3., 9., 9.), FQuat::Identity, TEXT("red"));
	AddText(TEXT("YOU ARE HERE"), HereOnBoard + FVector(0.5, 0., -10.), 7.f, FColor(0xc0, 0x1a, 0x1a));
}
