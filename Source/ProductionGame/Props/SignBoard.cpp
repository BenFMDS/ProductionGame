#include "Props/SignBoard.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr double TitleGap = 10.;

	UStaticMeshComponent* MakeSignPiece(ASignBoard* Owner, USceneComponent* Parent, const TCHAR* Name, UStaticMesh* Cube)
	{
		UStaticMeshComponent* Piece = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Piece->SetupAttachment(Parent);
		Piece->SetStaticMesh(Cube);
		Piece->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		return Piece;
	}

	UTextRenderComponent* MakeSignText(ASignBoard* Owner, USceneComponent* Parent, const TCHAR* Name, bool bBack, bool bBody)
	{
		UTextRenderComponent* Text = Owner->CreateDefaultSubobject<UTextRenderComponent>(Name);
		Text->SetupAttachment(Parent);
		// Text faces its +X and reads along -Y, so the back copy is turned round to face -X.
		Text->SetRelativeRotation(FRotator(0., bBack ? 180. : 0., 0.));
		Text->SetHorizontalAlignment(bBody ? EHTA_Left : EHTA_Center);
		Text->SetVerticalAlignment(EVRTA_TextTop);
		Text->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		return Text;
	}
}

ASignBoard::ASignBoard()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cube = CubeFinder.Object;
	Board = MakeSignPiece(this, Root, TEXT("Board"), Cube);
	Post = MakeSignPiece(this, Root, TEXT("Post"), Cube);
	RodLeft = MakeSignPiece(this, Root, TEXT("RodLeft"), Cube);
	RodRight = MakeSignPiece(this, Root, TEXT("RodRight"), Cube);

	TitleFront = MakeSignText(this, Root, TEXT("TitleFront"), false, false);
	TitleBack = MakeSignText(this, Root, TEXT("TitleBack"), true, false);
	BodyFront = MakeSignText(this, Root, TEXT("BodyFront"), false, true);
	BodyBack = MakeSignText(this, Root, TEXT("BodyBack"), true, true);
}

void ASignBoard::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Rebuild();
}

FString ASignBoard::WrapText(const FString& Text, int32 Width)
{
	if (Width <= 0)
	{
		return Text;
	}
	TArray<FString> Paragraphs;
	Text.ParseIntoArray(Paragraphs, TEXT("\n"), false);
	TArray<FString> Lines;
	for (const FString& Paragraph : Paragraphs)
	{
		TArray<FString> Words;
		Paragraph.ParseIntoArray(Words, TEXT(" "));
		FString Line;
		for (const FString& Word : Words)
		{
			if (!Line.IsEmpty() && Line.Len() + 1 + Word.Len() > Width)
			{
				Lines.Add(Line);
				Line.Reset();
			}
			Line += (Line.IsEmpty() ? TEXT("") : TEXT(" ")) + Word;
		}
		Lines.Add(Line);
	}
	return FString::Join(Lines, TEXT("\n"));
}

void ASignBoard::Rebuild()
{
	const FText WrappedBody = FText::FromString(WrapText(Body.ToString(), WrapAt));
	const bool bHasBody = !Body.IsEmpty();
	for (UTextRenderComponent* Text : { TitleFront.Get(), TitleBack.Get(), BodyFront.Get(), BodyBack.Get() })
	{
		const bool bIsBody = Text == BodyFront || Text == BodyBack;
		Text->SetText(bIsBody ? WrappedBody : Title);
		Text->SetWorldSize(bIsBody ? BodySize : TitleSize);
		Text->SetTextRenderColor(TextColour);
		Text->SetVisibility((bIsBody ? bHasBody : !Title.IsEmpty()) && (bBothSides || Text == TitleFront || Text == BodyFront));
	}

	// Sized from the font's real glyph metrics; the board grows to fit and never shrinks below MinBoardSize.
	const FVector TitleExtent = Title.IsEmpty() ? FVector::ZeroVector : TitleFront->GetTextLocalSize();
	const FVector BodyExtent = bHasBody ? BodyFront->GetTextLocalSize() : FVector::ZeroVector;
	const double BodyTop = Margin + TitleExtent.Z + (bHasBody ? TitleGap : 0.);
	BoardSize.X = FMath::Max(MinBoardSize.X, FMath::Max(TitleExtent.Y, BodyExtent.Y) + 2. * Margin);
	BoardSize.Y = FMath::Max(MinBoardSize.Y, BodyTop + BodyExtent.Z + Margin);
	const double Width = BoardSize.X, Height = BoardSize.Y;

	// Standing: pivot at the foot of the post. Hanging: pivot at the top of the rods.
	const double BoardTop = bHanging ? -StemLength : StemLength + Height;
	Board->SetRelativeLocation(FVector(0., 0., BoardTop - Height / 2.));
	Board->SetRelativeScale3D(FVector(Thickness, Width, Height) / 100.);

	Post->SetVisibility(!bHanging && StemLength > 0.f);
	Post->SetRelativeLocation(FVector(0., 0., StemLength / 2.));
	Post->SetRelativeScale3D(FVector(8., 8., FMath::Max(StemLength, 1.f)) / 100.);
	for (UStaticMeshComponent* Rod : { RodLeft.Get(), RodRight.Get() })
	{
		const double Side = Rod == RodLeft ? 1. : -1.;
		Rod->SetVisibility(bHanging && StemLength > 0.f);
		Rod->SetRelativeLocation(FVector(0., Side * (Width / 2. - 20.), -StemLength / 2.));
		Rod->SetRelativeScale3D(FVector(4., 4., FMath::Max(StemLength, 1.f)) / 100.);
	}

	if (BoardMaterial)
	{
		Board->SetMaterial(0, BoardMaterial);
	}
	if (FrameMaterial)
	{
		for (UStaticMeshComponent* Piece : { Post.Get(), RodLeft.Get(), RodRight.Get() })
		{
			Piece->SetMaterial(0, FrameMaterial);
		}
	}

	// Half a unit off the board so the text doesn't z-fight with it.
	const double Face = Thickness / 2. + 0.5;
	TitleFront->SetRelativeLocation(FVector(Face, 0., BoardTop - Margin));
	TitleBack->SetRelativeLocation(FVector(-Face, 0., BoardTop - Margin));
	// Left-aligned body starts on the reader's left: +Y for someone facing the front, -Y from behind.
	BodyFront->SetRelativeLocation(FVector(Face, Width / 2. - Margin, BoardTop - BodyTop));
	BodyBack->SetRelativeLocation(FVector(-Face, -(Width / 2. - Margin), BoardTop - BodyTop));
}
