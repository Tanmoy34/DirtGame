#include "DartCharacter.h"
#include "DartProjectile.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Engine/Engine.h"   // GEngine->AddOnScreenDebugMessage

ADartCharacter::ADartCharacter()
{
    bReplicates = true;
    PrimaryActorTick.bCanEverTick = false;

    // --- Spring Arm ---
    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(RootComponent);
    SpringArm->TargetArmLength = 300.f;
    SpringArm->bUsePawnControlRotation = true;
    SpringArm->SetRelativeLocation(FVector(0.f, 0.f, 60.f));

    // --- Camera ---
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
    Camera->bUsePawnControlRotation = false;

    // --- Movement Settings ---
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.f, 500.f, 0.f);
    GetCharacterMovement()->MaxWalkSpeed = 400.f;
    GetCharacterMovement()->JumpZVelocity = 500.f;

    bUseControllerRotationYaw   = false;
    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll  = false;
}

void ADartCharacter::BeginPlay()
{
    Super::BeginPlay();

    // Print initial dart count for the local player only
    if (IsLocallyControlled())
    {
        GEngine->AddOnScreenDebugMessage(1, 5.f, FColor::Cyan,
            FString::Printf(TEXT("Darts Remaining: %d"), DartsRemaining));
    }
}

void ADartCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);

    Input->BindAxis("MoveForward", this, &ADartCharacter::MoveForward);
    Input->BindAxis("MoveRight",   this, &ADartCharacter::MoveRight);
    Input->BindAxis("Turn",        this, &ADartCharacter::Turn);
    Input->BindAxis("LookUp",      this, &ADartCharacter::LookUp);

    Input->BindAction("Jump", IE_Pressed,  this, &ACharacter::Jump);
    Input->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

    // Right mouse button: hold to aim, release to throw
    Input->BindAction("Aim", IE_Pressed,  this, &ADartCharacter::StartAim);
    Input->BindAction("Aim", IE_Released, this, &ADartCharacter::Throw);
}

// -----------------------------------------------------------------------
// Movement
// -----------------------------------------------------------------------

void ADartCharacter::MoveForward(float Value)
{
    if (Controller && Value != 0.f)
    {
        const FRotator Rot = FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f);
        AddMovementInput(FRotationMatrix(Rot).GetUnitAxis(EAxis::X), Value);
    }
}

void ADartCharacter::MoveRight(float Value)
{
    if (Controller && Value != 0.f)
    {
        const FRotator Rot = FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f);
        AddMovementInput(FRotationMatrix(Rot).GetUnitAxis(EAxis::Y), Value);
    }
}

void ADartCharacter::Turn(float Value)   { AddControllerYawInput(Value); }
void ADartCharacter::LookUp(float Value) { AddControllerPitchInput(Value); }

// -----------------------------------------------------------------------
// Throwing
// -----------------------------------------------------------------------

void ADartCharacter::StartAim()
{
    // Don't start aiming if no darts remain
    if (DartsRemaining <= 0)
    {
        if (IsLocallyControlled())
        {
            GEngine->AddOnScreenDebugMessage(2, 3.f, FColor::Red,
                TEXT("No darts remaining!"));
        }
        return;
    }

    bIsAiming    = true;
    AimCountdown = 30;

    if (IsLocallyControlled())
    {
        GEngine->AddOnScreenDebugMessage(3, 2.f, FColor::Yellow,
            FString::Printf(TEXT("Aim timer: %d"), AimCountdown));
    }

    // Fire AimTick every 1 second
    GetWorldTimerManager().SetTimer(
        AimTimerHandle,
        this,
        &ADartCharacter::AimTick,
        1.f,   // interval
        true,  // looping
        1.f    // first fire after 1 second
    );
}

void ADartCharacter::AimTick()
{
    if (!bIsAiming) { ClearAimTimer(); return; }

    --AimCountdown;

    if (IsLocallyControlled())
    {
        GEngine->AddOnScreenDebugMessage(3, 1.1f, FColor::Yellow,
            FString::Printf(TEXT("Aim timer: %d"), AimCountdown));
    }

    // Time is up — forfeit the dart
    if (AimCountdown <= 0)
    {
        ClearAimTimer();
        bIsAiming = false;

        if (DartsRemaining > 0)
        {
            --DartsRemaining;
        }

        if (IsLocallyControlled())
        {
            GEngine->AddOnScreenDebugMessage(2, 4.f, FColor::Orange,
                TEXT("DART FORFEITED — ran out of time!"));

            GEngine->AddOnScreenDebugMessage(1, 5.f, FColor::Cyan,
                FString::Printf(TEXT("Darts Remaining: %d"), DartsRemaining));
        }

        UE_LOG(LogTemp, Warning, TEXT("Dart forfeited — timer expired. Darts left: %d"), DartsRemaining);
    }
}

void ADartCharacter::ClearAimTimer()
{
    GetWorldTimerManager().ClearTimer(AimTimerHandle);
}

void ADartCharacter::Throw()
{
    if (!bIsAiming) return;
    bIsAiming = false;

    // Cancel the forfeit timer since the player threw in time
    ClearAimTimer();
    AimCountdown = 0;

    if (DartsRemaining <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Throw() called but no darts remaining"));
        return;
    }

    if (!ProjectileClass)
    {
        UE_LOG(LogTemp, Error, TEXT("ProjectileClass is NULL — assign BP_DartProjectile in defaults!"));
        return;
    }

    --DartsRemaining;

    if (IsLocallyControlled())
    {
        GEngine->AddOnScreenDebugMessage(1, 5.f, FColor::Cyan,
            FString::Printf(TEXT("Darts Remaining: %d"), DartsRemaining));
    }

    UE_LOG(LogTemp, Warning, TEXT("Throw() called — Darts left: %d"), DartsRemaining);

    FVector Origin    = Camera->GetComponentLocation() + Camera->GetForwardVector() * 50.f;
    FVector Direction = Camera->GetForwardVector();
    float   Speed     = MaxThrowSpeed * FMath::Max(TimingAccuracy, 0.3f);

    Server_Throw(Origin, Direction, Speed);
}

void ADartCharacter::Server_Throw_Implementation(FVector Origin, FVector Direction, float Speed)
{
    UE_LOG(LogTemp, Warning, TEXT("Server_Throw_Implementation fired"));

    if (!ProjectileClass)
    {
        UE_LOG(LogTemp, Error, TEXT("Server: ProjectileClass is NULL"));
        return;
    }

    FActorSpawnParameters Params;
    Params.Owner      = this;
    Params.Instigator = GetInstigator();
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ADartProjectile* Dart = GetWorld()->SpawnActor<ADartProjectile>(
        ProjectileClass,
        Origin,
        Direction.Rotation(),
        Params
    );

    if (Dart)
    {
        UE_LOG(LogTemp, Warning, TEXT("Dart spawned successfully!"));
        Dart->Launch(Direction, Speed);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("SpawnActor returned NULL — likely a collision block at spawn point"));
    }
}