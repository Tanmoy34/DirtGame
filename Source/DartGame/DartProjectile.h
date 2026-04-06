#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DartProjectile.generated.h"

class UProjectileMovementComponent;
class USphereComponent;

UCLASS()
class DARTGAME_API ADartProjectile : public AActor
{
	GENERATED_BODY()
public:
	ADartProjectile();

	UPROPERTY(VisibleAnywhere) USphereComponent* CollisionComp;
	UPROPERTY(VisibleAnywhere) UProjectileMovementComponent* MovementComp;
	UPROPERTY(VisibleAnywhere) UStaticMeshComponent* MeshComp;

	void Launch(FVector Direction, float Speed);

protected:
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
			   UPrimitiveComponent* OtherComp, FVector NormalImpulse,
			   const FHitResult& Hit);
};
