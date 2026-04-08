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

	// server-only guard
	bIsStuck = false;
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

void ADartProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// Only the server should decide to stick the dart and update transforms.
	if (!HasAuthority()) return;

	// Avoid processing the same hit multiple times
	if (bIsStuck) return;

	// Ignore owner/instigator collisions
	if (OtherActor == GetOwner() || OtherActor == GetInstigator()) return;

	// If we hit a dartboard, register the hit and STICK the dart into the board
	if (ADartboard* Board = Cast<ADartboard>(OtherActor))
	{
		Board->RegisterHit(Hit.ImpactPoint, this);

		// Mark stuck to avoid re-processing
		bIsStuck = true;

		// Stop movement and freeze the projectile on the server.
		if (MovementComp)
		{
			MovementComp->StopMovementImmediately();
			MovementComp->Deactivate();
		}

		// Position the dart exactly at impact point and orient it so its forward
		// points into the board (visually "stuck").
		const FVector ImpactPoint = Hit.ImpactPoint;
		// Make the dart's X axis point opposite to the surface normal (so it "enters" the board)
		const FRotator StuckRot = FRotationMatrix::MakeFromX(-Hit.ImpactNormal).Rotator();

		// Teleport to the impact transform on the server; this transform will replicate to clients.
		SetActorLocationAndRotation(ImpactPoint, StuckRot, false, nullptr, ETeleportType::TeleportPhysics);

		// Disable collisions so the stuck dart doesn't interfere with players or other darts.
		if (CollisionComp) CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (MeshComp)      MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// Do NOT Destroy() — keep the actor so all clients can see the stuck dart.
		return;
	}

	// Fallback: for other hits (walls/floor) we can keep existing behaviour if desired.
	// If you want darts to stick into world static as well, handle similar to above.
	// For now, preserve previous behavior for non-board hits:
	SetActorEnableCollision(false);
	Destroy();
}

