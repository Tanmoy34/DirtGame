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
	// (now exposed to Blueprint and returns the points for the hit)
	UFUNCTION(BlueprintCallable, Category="Dart|Scoring")
	int32 RegisterHit(const FVector& ImpactPoint, ADartProjectile* Dart);

	// ── Scoring Zone Radii (edit these in Blueprint to match your mesh) ──────
	//
	//  Zone layout (matches your collision mesh, inner → outer):
	//
	//    [0 – BullseyeRadius]               Red dot centre    → 100 pts
	//    [BullseyeRadius – SkyBlueRadius]   Cyan / sky-blue   →  80 pts
	//    [SkyBlueRadius  – DeepBlueRadius]  Deep blue ring    →  40 pts
	//    [DeepBlueRadius – GreenRadius]     Green outer ring  →  10 pts
	//    [> GreenRadius]                    Miss              →   0 pts
	//
	//  Default values are in Unreal units (cm). Adjust to your mesh size.

	UPROPERTY(EditAnywhere, Category="Dartboard|Scoring",
	          meta=(ClampMin="0.0", ToolTip="Radius of the red bullseye dot (100 pts)"))
	float BullseyeRadius = 5.f;

	UPROPERTY(EditAnywhere, Category="Dartboard|Scoring",
	          meta=(ClampMin="0.0", ToolTip="Outer radius of the cyan / sky-blue ring (80 pts)"))
	float SkyBlueRadius = 15.f;

	UPROPERTY(EditAnywhere, Category="Dartboard|Scoring",
	          meta=(ClampMin="0.0", ToolTip="Outer radius of the deep-blue ring (40 pts)"))
	float DeepBlueRadius = 30.f;

	UPROPERTY(EditAnywhere, Category="Dartboard|Scoring",
	          meta=(ClampMin="0.0", ToolTip="Outer radius of the green ring (10 pts). Beyond this = miss (0 pts)"))
	float GreenRadius = 50.f;

protected:
	UPROPERTY(VisibleAnywhere) UStaticMeshComponent* BoardMesh;

private:
	// Returns the score for a given 2-D radial distance from board centre.
	int32 CalculatePoints(float DistFromCentre) const;

	// Prints the hit result to the owning player's screen (server-side helper).
	void PrintHitResult(int32 Points, float Dist, AController* OwnerController) const;

public:
	// Cumulative score tracked by the board (exposed to Blueprint).
	UPROPERTY(BlueprintReadOnly, Category="Dart|Scoring")
	int32 LastScore = 0;

	// Blueprint hook called whenever LastScore is updated (total after the hit).
	UFUNCTION(BlueprintImplementableEvent, Category="Dart|Scoring")
	void BP_OnLastScoreUpdated(int32 NewTotal);
};