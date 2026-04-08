#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DartProjectile.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent; // added forward declaration

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
	virtual void BeginPlay() override;

	// Add EndPlay so we can detect lifespan expiry and notify GameMode.
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
			   UPrimitiveComponent* OtherComp, FVector NormalImpulse,
			   const FHitResult& Hit);

private:
	// Server-only: once stuck we won't process further hits or destroy the actor.
	bool bIsStuck;

	// Has the projectile already notified the GameMode about this throw? Avoid double notifications.
	bool bNotified;
};
