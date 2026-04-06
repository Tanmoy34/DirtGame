#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Dartboard.generated.h"

class ADartProjectile;

UCLASS()
class DARTGAME_API ADartboard : public AActor
{
	GENERATED_BODY()
public:
	ADartboard();

	// Called when a dart hits — calculates score from impact point
	void RegisterHit(FVector ImpactPoint, ADartProjectile* Dart);

	UPROPERTY(EditAnywhere) float BullseyeRadius = 5.f;
	UPROPERTY(EditAnywhere) float InnerRadius    = 15.f;
	UPROPERTY(EditAnywhere) float MiddleRadius   = 30.f;
	UPROPERTY(EditAnywhere) float OuterRadius    = 50.f;

protected:
	UPROPERTY(VisibleAnywhere) UStaticMeshComponent* BoardMesh;
};