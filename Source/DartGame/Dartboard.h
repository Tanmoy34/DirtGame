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

	
	UFUNCTION(BlueprintCallable, Category = "Dart|Scoring")
	int32 RegisterHit(const FVector &ImpactPoint, ADartProjectile *Dart);

	

	UPROPERTY(EditAnywhere, Category = "Dartboard|Scoring",
			  meta = (ClampMin = "0.0", ToolTip = "Radius of the red bullseye dot (100 pts)"))
	float BullseyeRadius = 5.f;

	UPROPERTY(EditAnywhere, Category = "Dartboard|Scoring",
			  meta = (ClampMin = "0.0", ToolTip = "Outer radius of the cyan / sky-blue ring (80 pts)"))
	float SkyBlueRadius = 15.f;

	UPROPERTY(EditAnywhere, Category = "Dartboard|Scoring",
			  meta = (ClampMin = "0.0", ToolTip = "Outer radius of the deep-blue ring (40 pts)"))
	float DeepBlueRadius = 30.f;

	UPROPERTY(EditAnywhere, Category = "Dartboard|Scoring",
			  meta = (ClampMin = "0.0", ToolTip = "Outer radius of the green ring (10 pts). Beyond this = miss (0 pts)"))
	float GreenRadius = 50.f;

protected:
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent *BoardMesh;

private:
	
	int32 CalculatePoints(float DistFromCentre) const;

	
	void PrintHitResult(int32 Points, float Dist, AController *OwnerController) const;

public:
	
	UPROPERTY(BlueprintReadOnly, Category = "Dart|Scoring")
	int32 LastScore = 0;


	UFUNCTION(BlueprintImplementableEvent, Category = "Dart|Scoring")
	void BP_OnLastScoreUpdated(int32 NewTotal);
};