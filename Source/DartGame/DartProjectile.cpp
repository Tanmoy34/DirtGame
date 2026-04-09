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

	
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionComp->SetCollisionObjectType(ECC_GameTraceChannel1); 
	CollisionComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);  
	CollisionComp->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block); 
	

	CollisionComp->OnComponentHit.AddDynamic(this, &ADartProjectile::OnHit);
	RootComponent = CollisionComp;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComp->SetupAttachment(RootComponent);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	MovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	MovementComp->InitialSpeed = 0.f;
	MovementComp->MaxSpeed = 4000.f;
	MovementComp->bRotationFollowsVelocity = true;
	MovementComp->ProjectileGravityScale = 0.1f;
	MovementComp->bAutoActivate = false;
	MovementComp->SetIsReplicated(true);
	MovementComp->SetUpdatedComponent(CollisionComp);

	
	bIsStuck = false;

	
	InitialLifeSpan = 5.0f;

	
	bNotified = false;
}

void ADartProjectile::BeginPlay()
{
	Super::BeginPlay();

	
	if (MeshComp)
	{
		MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (CollisionComp)
	{
		
		CollisionComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}
}

void ADartProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	
	if (HasAuthority() && !bIsStuck && !bNotified)
	{
	
		AController *OwnerController = nullptr;
		if (AActor *OwnerActor = GetOwner())
		{
			if (APawn *OwnerPawn = Cast<APawn>(OwnerActor))
			{
				OwnerController = OwnerPawn->GetController();
			}
		}

		if (OwnerController)
		{
			if (ADartGameMode *GM = Cast<ADartGameMode>(UGameplayStatics::GetGameMode(this)))
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
		
		if (AActor *OwnerActor = GetOwner())
		{
			CollisionComp->IgnoreActorWhenMoving(OwnerActor, true);
		}
		if (AActor *Inst = GetInstigator())
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
		
		if (MovementComp)
		{
			MovementComp->StopMovementImmediately();
			MovementComp->Deactivate();
		}
	}
}

void ADartProjectile::OnHit(UPrimitiveComponent *HitComp, AActor *OtherActor,
							UPrimitiveComponent *OtherComp, FVector NormalImpulse, const FHitResult &Hit)
{
	
	if (!HasAuthority())
		return;

	
	if (bIsStuck)
		return;

	
	if (OtherActor == GetOwner() || OtherActor == GetInstigator())
		return;

	
	if (ADartboard *Board = Cast<ADartboard>(OtherActor))
	{
		Board->RegisterHit(Hit.ImpactPoint, this);

		
		bIsStuck = true;
		bNotified = true; 

		
		if (MovementComp)
		{
			MovementComp->StopMovementImmediately();
			MovementComp->Deactivate();
		}
		const FVector ImpactPoint = Hit.ImpactPoint;
		const FRotator StuckRot = FRotationMatrix::MakeFromX(-Hit.ImpactNormal).Rotator();
		SetActorLocationAndRotation(ImpactPoint, StuckRot, false, nullptr, ETeleportType::TeleportPhysics);
		if (CollisionComp)
			CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (MeshComp)
			MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return;
	}

	
	if (!bNotified)
	{
		AController *OwnerController = nullptr;
		if (AActor *OwnerActor = GetOwner())
		{
			if (APawn *OwnerPawn = Cast<APawn>(OwnerActor))
			{
				OwnerController = OwnerPawn->GetController();
			}
		}

		if (OwnerController)
		{
			if (ADartGameMode *GM = Cast<ADartGameMode>(UGameplayStatics::GetGameMode(this)))
			{
				GM->RegisterThrow(Cast<APlayerController>(OwnerController), 0);
			}
		}
		bNotified = true;
	}

	
	SetActorEnableCollision(false);
	Destroy();
}