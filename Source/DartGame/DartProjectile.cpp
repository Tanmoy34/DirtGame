#include "DartProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Dartboard.h"
#include "DartGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"

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

	// Ensure stray darts are cleaned up in a few seconds if they never hit anything.
	InitialLifeSpan = 5.0f;

	// Not yet notified GameMode about this throw
	bNotified = false;
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

void ADartProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// If the projectile is being removed without having stuck to the board and
	// without having notified game mode, treat this as a miss and notify GM.
	if (HasAuthority() && !bIsStuck && !bNotified)
	{
		// Resolve owner controller
		AController* OwnerController = nullptr;
		if (AActor* OwnerActor = GetOwner())
		{
			if (APawn* OwnerPawn = Cast<APawn>(OwnerActor))
			{
				OwnerController = OwnerPawn->GetController();
			}
		}

		if (OwnerController)
		{
			if (ADartGameMode* GM = Cast<ADartGameMode>(UGameplayStatics::GetGameMode(this)))
			{
				GM->RegisterThrow(Cast<APlayerController>(OwnerController), 0);
			}
		}
		bNotified = true;
	}

	Super::EndPlay(EndPlayReason);
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
		bNotified = true; // board.RegisterHit will inform GameMode via character

		// Stop movement and freeze the projectile on the server.
		if (MovementComp)
		{
			MovementComp->StopMovementImmediately();
			MovementComp->Deactivate();
		}
		const FVector ImpactPoint = Hit.ImpactPoint;
		const FRotator StuckRot = FRotationMatrix::MakeFromX(-Hit.ImpactNormal).Rotator();
		SetActorLocationAndRotation(ImpactPoint, StuckRot, false, nullptr, ETeleportType::TeleportPhysics);
		if (CollisionComp) CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (MeshComp)      MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return;
	}

	// Non-board hit (wall/floor): notify GameMode as a MISS (0 pts) then destroy.
	if (!bNotified)
	{
		AController* OwnerController = nullptr;
		if (AActor* OwnerActor = GetOwner())
		{
			if (APawn* OwnerPawn = Cast<APawn>(OwnerActor))
			{
				OwnerController = OwnerPawn->GetController();
			}
		}

		if (OwnerController)
		{
			if (ADartGameMode* GM = Cast<ADartGameMode>(UGameplayStatics::GetGameMode(this)))
			{
				GM->RegisterThrow(Cast<APlayerController>(OwnerController), 0);
			}
		}
		bNotified = true;
	}

	// Fallback: for other hits (walls/floor) destroy projectile after notifying.
	SetActorEnableCollision(false);
	Destroy();
}

