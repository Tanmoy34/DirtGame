#include "DartProjectile.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Dartboard.h"

ADartProjectile::ADartProjectile()
{
	bReplicates = true;
	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	CollisionComp->InitSphereRadius(5.f);
	CollisionComp->SetCollisionProfileName("Projectile");
	CollisionComp->OnComponentHit.AddDynamic(this, &ADartProjectile::OnHit);
	RootComponent = CollisionComp;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComp->SetupAttachment(RootComponent);

	MovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	MovementComp->InitialSpeed = 0.f;
	MovementComp->MaxSpeed = 4000.f;
	MovementComp->bRotationFollowsVelocity = true;
	MovementComp->ProjectileGravityScale = 0.1f; // Slight arc
}

void ADartProjectile::Launch(FVector Direction, float Speed)
{
	MovementComp->Velocity = Direction * Speed;
}

void ADartProjectile::OnHit(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, FVector, const FHitResult& Hit)
{
	if (ADartboard* Board = Cast<ADartboard>(OtherActor))
	{
		Board->RegisterHit(Hit.ImpactPoint, this);
	}
	SetActorEnableCollision(false);
}