#include "DartCharacter.h"
#include "DartProjectile.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"

ADartCharacter::ADartCharacter()
{
    bReplicates = true;
    PrimaryActorTick.bCanEverTick = false;

    // --- Spring Arm ---
    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(RootComponent);
    SpringArm->TargetArmLength = 300.f;
    SpringArm->bUsePawnControlRotation = true;  // Arm rotates with controller
    SpringArm->SetRelativeLocation(FVector(0.f, 0.f, 60.f)); // Eye height

    // --- Camera ---
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
    Camera->bUsePawnControlRotation = false;

    // --- Movement Settings ---
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.f, 500.f, 0.f);
    GetCharacterMovement()->MaxWalkSpeed = 400.f;
    GetCharacterMovement()->JumpZVelocity = 500.f;

    // Prevent character rotating with camera (camera rotates independently)
    bUseControllerRotationYaw   = false;
    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll  = false;
}

void ADartCharacter::BeginPlay()
{
    Super::BeginPlay();
}

void ADartCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);

    // Movement axes
    Input->BindAxis("MoveForward", this, &ADartCharacter::MoveForward);
    Input->BindAxis("MoveRight",   this, &ADartCharacter::MoveRight);

    // Mouse look axes
    Input->BindAxis("Turn",   this, &ADartCharacter::Turn);
    Input->BindAxis("LookUp", this, &ADartCharacter::LookUp);

    // Jump (optional but nice to have)
    Input->BindAction("Jump", IE_Pressed,  this, &ACharacter::Jump);
    Input->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

    // Dart throwing
    Input->BindAction("Aim", IE_Pressed,  this, &ADartCharacter::StartAim);
    Input->BindAction("Aim", IE_Released, this, &ADartCharacter::Throw);
}

// --- Movement ---

void ADartCharacter::MoveForward(float Value)
{
    if (Controller && Value != 0.f)
    {
        // Get forward direction from controller yaw only (ignore pitch)
        const FRotator Rot = FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f);
        const FVector Dir  = FRotationMatrix(Rot).GetUnitAxis(EAxis::X);
        AddMovementInput(Dir, Value);
    }
}

void ADartCharacter::MoveRight(float Value)
{
    if (Controller && Value != 0.f)
    {
        const FRotator Rot = FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f);
        const FVector Dir  = FRotationMatrix(Rot).GetUnitAxis(EAxis::Y);
        AddMovementInput(Dir, Value);
    }
}

void ADartCharacter::Turn(float Value)
{
    AddControllerYawInput(Value);
}

void ADartCharacter::LookUp(float Value)
{
    AddControllerPitchInput(Value);
}

// --- Throwing ---

void ADartCharacter::StartAim()
{
    bIsAiming = true;
    // HUD widget will start animating the timing bar when it sees bIsAiming
    // You can expose bIsAiming as BlueprintReadOnly if needed
}

void ADartCharacter::Throw()
{
    if (!bIsAiming) return;
    bIsAiming = false;

    UE_LOG(LogTemp, Warning, TEXT("Throw() called"));

    if (!ProjectileClass)
    {
        UE_LOG(LogTemp, Error, TEXT("ProjectileClass is NULL - assign BP_DartProjectile in defaults!"));
        return;
    }

    FVector Origin    = Camera->GetComponentLocation() + Camera->GetForwardVector() * 50.f;
    FVector Direction = Camera->GetForwardVector();
    float Speed       = MaxThrowSpeed * FMath::Max(TimingAccuracy, 0.3f);

    UE_LOG(LogTemp, Warning, TEXT("Calling Server_Throw — Origin: %s  Speed: %f"), *Origin.ToString(), Speed);

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
    Params.Owner            = this;
    Params.Instigator       = GetInstigator();
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    UE_LOG(LogTemp, Warning, TEXT("Spawning projectile now..."));

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