#include "DartProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Dartboard.h"

ADartProjectile::ADartProjectile()
{
	SetReplicates(true);
	SetReplicateMovement(true);

	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	CollisionComp->InitSphereRadius(5.f);

	// ---------------------------------------------------------------
	// Collision setup:
	//   • Start from a clean "no collision" slate, then opt-in only to
	//     what we need so there are no surprise channel responses.
	//   • We want to HIT WorldDynamic (the dartboard is WorldDynamic).
	//   • We want to IGNORE Pawns so the dart never jolts the thrower.
	// ---------------------------------------------------------------
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionComp->SetCollisionObjectType(ECC_GameTraceChannel1); // "Projectile" object channel
	CollisionComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComp->SetCollisionResponseToChannel(ECC_WorldStatic,  ECR_Block); // walls, floor
	CollisionComp->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block); // dartboard
	// Pawns are explicitly left as ECR_Ignore — no jerk on the owner or any other player

	CollisionComp->OnComponentHit.AddDynamic(this, &ADartProjectile::OnHit);
	RootComponent = CollisionComp;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComp->SetupAttachment(RootComponent);
	// Visual mesh must never contribute physics collisions
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	MovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	MovementComp->InitialSpeed = 0.f;
	MovementComp->MaxSpeed = 4000.f;
	MovementComp->bRotationFollowsVelocity = true;
	MovementComp->ProjectileGravityScale = 0.1f;
	MovementComp->bAutoActivate = false;
	MovementComp->SetIsReplicated(true);
	MovementComp->SetUpdatedComponent(CollisionComp);
}

void ADartProjectile::BeginPlay()
{
	Super::BeginPlay();

	// Re-apply on both server and clients in case a Blueprint default overrides us
	if (MeshComp)
	{
		MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (CollisionComp)
	{
		// Guarantee pawn channel is always ignored regardless of profile overrides
		CollisionComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}
}

void ADartProjectile::Launch(FVector Direction, float Speed)
{
	if (HasAuthority())
	{
		// Ignore the owning actor and instigator immediately — this is the main
		// guard against the "jerk" caused by the projectile briefly overlapping
		// the character at spawn before the ignore list takes effect.
		if (AActor* OwnerActor = GetOwner())
		{
			CollisionComp->IgnoreActorWhenMoving(OwnerActor, true);
		}
		if (AActor* Inst = GetInstigator())
		{
			CollisionComp->IgnoreActorWhenMoving(Inst, true);
		}

		if (MovementComp)
		{
			MovementComp->Velocity = Direction * Speed;
			MovementComp->Activate(true);
		}
	}
	else
	{
		// Client: stay inert; server-replicated state will drive movement
		if (MovementComp)
		{
			MovementComp->StopMovementImmediately();
			MovementComp->Deactivate();
		}
	}
}

void ADartProjectile::OnHit(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, FVector, const FHitResult& Hit)
{
	if (!HasAuthority()) return;

	if (OtherActor == GetOwner() || OtherActor == GetInstigator()) return;

	if (ADartboard* Board = Cast<ADartboard>(OtherActor))
	{
		Board->RegisterHit(Hit.ImpactPoint, this);
	}

	SetActorEnableCollision(false);
	Destroy();
}