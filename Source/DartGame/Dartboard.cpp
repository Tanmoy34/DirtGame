#include "Dartboard.h"
#include "DartProjectile.h"
#include "DartGameMode.h"
#include "Kismet/GameplayStatics.h"

ADartboard::ADartboard()
{
	BoardMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Board"));
	RootComponent = BoardMesh;
}

void ADartboard::RegisterHit(FVector ImpactPoint, ADartProjectile* Dart)
{
	if (!HasAuthority()) return;

	FVector LocalHit = GetTransform().InverseTransformPosition(ImpactPoint);
	float Dist = FVector2D(LocalHit.Y, LocalHit.Z).Size();

	int32 Points = 0;
	if      (Dist <= BullseyeRadius) Points = 50;
	else if (Dist <= InnerRadius)    Points = 25;
	else if (Dist <= MiddleRadius)   Points = 10;
	else if (Dist <= OuterRadius)    Points = 5;

	if (ADartGameMode* GM = Cast<ADartGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		// TODO: resolve which player index owns this dart
		GM->AddScore(0, Points);
	}
}