#include "DartProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Dartboard.h"

ADartProjectile::ADartProjectile()
{
	// Ensure the actor replicates and movement is replicated to clients
	SetReplicates(true);
	SetReplicateMovement(true);

	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	CollisionComp->InitSphereRadius(5.f);

	// Use a query/blocking collision preset for world objects, but ignore pawns by default
	CollisionComp->SetCollisionProfileName(TEXT("Projectile"));
	CollisionComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	CollisionComp->OnComponentHit.AddDynamic(this, &ADartProjectile::OnHit);
	RootComponent = CollisionComp;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComp->SetupAttachment(RootComponent);

	// Visual mesh shouldn't add physics collisions that might push the player
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	MovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	MovementComp->InitialSpeed = 0.f;
	MovementComp->MaxSpeed = 4000.f;
	MovementComp->bRotationFollowsVelocity = true;
	MovementComp->ProjectileGravityScale = 0.1f; // Slight arc

	// Do not auto-activate; we'll activate on server Launch so the initial velocity replicates properly
	MovementComp->bAutoActivate = false;

	// Ensure the movement component replicates its velocity/state to clients
	MovementComp->SetIsReplicated(true);

	// Make sure the movement component updates the collision root
	MovementComp->SetUpdatedComponent(CollisionComp);
}

void ADartProjectile::BeginPlay()
{
	Super::BeginPlay();
	// Safety: ensure mesh stays non-colliding and pawn channel ignored on clients/servers
	if (MeshComp)
	{
		MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (CollisionComp)
	{
		CollisionComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}
}

void ADartProjectile::Launch(FVector Direction, float Speed)
{
	// Only the server should set the authoritative initial velocity and activate movement.
	// Clients will receive movement via replication.
	if (HasAuthority())
	{
		// Prevent the projectile from colliding with its owner (stops the "jerk")
		if (AActor* OwnerActor = GetOwner())
		{
			CollisionComp->IgnoreActorWhenMoving(OwnerActor, true);
			// also ignore the instigator if set
			if (AActor* Inst = GetInstigator())
			{
				CollisionComp->IgnoreActorWhenMoving(Inst, true);
			}
		}

		// Set velocity and activate movement component so it replicates to clients
		if (MovementComp)
		{
			MovementComp->Velocity = Direction * Speed;
			MovementComp->Activate(true);
		}
	}
	else
	{
		// Clients should not attempt to simulate projectile motion locally (avoid falling straight).
		// Keep them inert until the server-spawned/replicated state arrives.
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
	// Only handle hits on the server to keep authoritative behavior
	if (!HasAuthority())
	{
		return;
	}

	// Ignore owner/instigator hits to avoid self-collisions
	if (OtherActor == GetOwner() || OtherActor == GetInstigator())
	{
		return;
	}

	if (ADartboard* Board = Cast<ADartboard>(OtherActor))
	{
		Board->RegisterHit(Hit.ImpactPoint, this);
	}

	// Disable collision and destroy on server; clients will replicate the destruction
	SetActorEnableCollision(false);
	Destroy();
}