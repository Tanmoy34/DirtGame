#include "DartCharacter.h"
#include "DartProjectile.h"
#include "Camera/CameraComponent.h"

ADartCharacter::ADartCharacter() {}

void ADartCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
	Super::SetupPlayerInputComponent(Input);
	Input->BindAction("Aim", IE_Pressed,  this, &ADartCharacter::StartAim);
	Input->BindAction("Aim", IE_Released, this, &ADartCharacter::Throw);
}

void ADartCharacter::StartAim()
{
	bIsAiming = true;
	AimStartTime = GetWorld()->GetTimeSeconds();
	// Tell HUD to start the timing bar (via blueprint or interface)
}

void ADartCharacter::Throw()
{
	if (!bIsAiming) return;
	bIsAiming = false;

	FVector Origin = GetActorLocation() + GetActorForwardVector() * 50.f;
	FVector Direction = GetActorForwardVector();
	float Speed = MaxThrowSpeed * TimingAccuracy; // 0..1 from timing bar

	Server_Throw(Origin, Direction, Speed);
}

void ADartCharacter::Server_Throw_Implementation(FVector Origin, FVector Direction, float Speed)
{
	if (!ProjectileClass) return;
	FActorSpawnParameters Params;
	Params.Owner = this;
	if (ADartProjectile* Dart = GetWorld()->SpawnActor<ADartProjectile>(ProjectileClass, Origin,
		Direction.Rotation(), Params))
	{
		Dart->Launch(Direction, Speed);
	}
}