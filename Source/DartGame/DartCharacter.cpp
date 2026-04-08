#include "DartCharacter.h"
#include "DartProjectile.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Engine/Engine.h"   // GEngine->AddOnScreenDebugMessage
#include "Math/UnrealMathUtility.h"
#include "Net/UnrealNetwork.h" // added for replication

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

ADartCharacter::ADartCharacter()
{
    bReplicates = true;
    PrimaryActorTick.bCanEverTick = false;

    // Spring Arm
    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(RootComponent);
    SpringArm->TargetArmLength        = 300.f;
    SpringArm->bUsePawnControlRotation = true;
    SpringArm->SetRelativeLocation(FVector(0.f, 0.f, 60.f));

    // Camera
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
    Camera->bUsePawnControlRotation = false;

    // Movement
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate              = FRotator(0.f, 500.f, 0.f);
    GetCharacterMovement()->MaxWalkSpeed              = 400.f;
    GetCharacterMovement()->JumpZVelocity             = 500.f;

    bUseControllerRotationYaw   = false;
    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll  = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// BeginPlay
// ─────────────────────────────────────────────────────────────────────────────

void ADartCharacter::BeginPlay()
{
    Super::BeginPlay();

    // Ensure the HUD gets correct initial values when the widget binds
    if (IsLocallyControlled())
    {
        // Fire Blueprint events so the widget can initialize
        BP_OnDartsRemainingUpdated(DartsRemaining);
        BP_OnPlayerScoreUpdated(PlayerScore);
        BP_OnRoundScoreUpdated(RoundScore);

        GEngine->AddOnScreenDebugMessage(1, 5.f, FColor::Cyan,
            FString::Printf(TEXT("Darts Remaining: %d"), DartsRemaining));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Input
// ─────────────────────────────────────────────────────────────────────────────

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

// ─────────────────────────────────────────────────────────────────────────────
// Movement helpers
// ─────────────────────────────────────────────────────────────────────────────

void ADartCharacter::MoveForward(float Value)
{
    if (Controller && Value != 0.f)
    {
        const FRotator Rot(0.f, Controller->GetControlRotation().Yaw, 0.f);
        AddMovementInput(FRotationMatrix(Rot).GetUnitAxis(EAxis::X), Value);
    }
}

void ADartCharacter::MoveRight(float Value)
{
    if (Controller && Value != 0.f)
    {
        const FRotator Rot(0.f, Controller->GetControlRotation().Yaw, 0.f);
        AddMovementInput(FRotationMatrix(Rot).GetUnitAxis(EAxis::Y), Value);
    }
}

void ADartCharacter::Turn   (float Value) { AddControllerYawInput(Value);   }
void ADartCharacter::LookUp (float Value) { AddControllerPitchInput(Value); }

// ─────────────────────────────────────────────────────────────────────────────
// Aim start
// ─────────────────────────────────────────────────────────────────────────────

void ADartCharacter::StartAim()
{
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

    GetWorldTimerManager().SetTimer(
        AimTimerHandle,
        this,
        &ADartCharacter::AimTick,
        1.f,   // interval
        true,  // looping
        1.f    // first fire delay
    );
}

// ─────────────────────────────────────────────────────────────────────────────
// Aim tick (every second while aiming)
// ─────────────────────────────────────────────────────────────────────────────

void ADartCharacter::AimTick()
{
    if (!bIsAiming) { ClearAimTimer(); return; }

    --AimCountdown;

    if (IsLocallyControlled())
    {
        GEngine->AddOnScreenDebugMessage(3, 1.1f, FColor::Yellow,
            FString::Printf(TEXT("Aim timer: %d"), AimCountdown));
    }

    if (AimCountdown <= 0)
    {
        ClearAimTimer();
        bIsAiming = false;

        if (DartsRemaining > 0) { --DartsRemaining; }

        if (IsLocallyControlled())
        {
            GEngine->AddOnScreenDebugMessage(2, 4.f, FColor::Orange,
                TEXT("DART FORFEITED — ran out of time!"));
            GEngine->AddOnScreenDebugMessage(1, 5.f, FColor::Cyan,
                FString::Printf(TEXT("Darts Remaining: %d"), DartsRemaining));
        }

        UE_LOG(LogTemp, Warning,
               TEXT("Dart forfeited — timer expired. Darts left: %d"), DartsRemaining);
    }
}

void ADartCharacter::ClearAimTimer()
{
    GetWorldTimerManager().ClearTimer(AimTimerHandle);
}

// ─────────────────────────────────────────────────────────────────────────────
// Throw  (local client)
// ─────────────────────────────────────────────────────────────────────────────

void ADartCharacter::Throw()
{
    if (!bIsAiming) return;
    bIsAiming = false;

    ClearAimTimer();
    AimCountdown = 0;

    if (DartsRemaining <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Throw() — no darts remaining"));
        return;
    }

    if (!ProjectileClass)
    {
        UE_LOG(LogTemp, Error,
               TEXT("ProjectileClass is NULL — assign BP_DartProjectile in defaults!"));
        return;
    }

    --DartsRemaining;

    if (IsLocallyControlled())
    {
        GEngine->AddOnScreenDebugMessage(1, 5.f, FColor::Cyan,
            FString::Printf(TEXT("Darts Remaining: %d"), DartsRemaining));
    }

    // Capture the widget accuracy value at the moment the button is released.
    // TimingAccuracy is BlueprintReadWrite so the widget sets it every frame
    // while the ping-pong animation runs.
    const float CapturedAccuracy = FMath::Clamp(TimingAccuracy, 0.f, 1.f);

    const FVector Origin    = Camera->GetComponentLocation()
                            + Camera->GetForwardVector() * 50.f;
    const FVector Direction = Camera->GetForwardVector();
    const float   Speed     = MaxThrowSpeed * FMath::Max(CapturedAccuracy, 0.3f);

    UE_LOG(LogTemp, Warning,
           TEXT("Throw() → Server_Throw | Accuracy=%.3f | Speed=%.1f"),
           CapturedAccuracy, Speed);

    // Send raw camera direction + accuracy to server.
    // The SERVER applies the deflection so the result is authoritative and
    // replicates identically to every client.
    Server_Throw(Origin, Direction, Speed, CapturedAccuracy);
}

// ─────────────────────────────────────────────────────────────────────────────
// Server_Throw  (runs on server only)
// ─────────────────────────────────────────────────────────────────────────────

void ADartCharacter::Server_Throw_Implementation(
    FVector Origin, FVector Direction, float Speed, float Accuracy)
{
    UE_LOG(LogTemp, Warning,
           TEXT("[Server] Server_Throw received | Accuracy=%.3f"), Accuracy);

    if (!ProjectileClass)
    {
        UE_LOG(LogTemp, Error, TEXT("[Server] ProjectileClass is NULL"));
        return;
    }

    // ── 1. Calculate inaccuracy ─────────────────────────────────────────────
    //
    //   Inaccuracy  = 1 - Accuracy   (0 = perfect, 1 = worst)
    //   DeviationDeg = Inaccuracy * MaxInaccuracyAngle
    //
    //   We then pick a random direction within a cone of that half-angle around
    //   the intended Direction.  FMath::VRandCone gives us exactly that: a unit
    //   vector uniformly distributed inside a cone of the given half-angle.
    //
    //   This is all done on the server so the seed is authoritative; clients
    //   never compute a competing direction.

    const float Inaccuracy   = 1.f - Accuracy;                        // 0–1
    const float DeviationDeg = Inaccuracy * MaxInaccuracyAngle;       // 0–MaxAngle
    const float DeviationRad = FMath::DegreesToRadians(DeviationDeg);

    FVector FinalDirection;
    if (DeviationRad > SMALL_NUMBER)
    {
        // VRandCone: returns a unit vector within a cone of HalfAngleRad around
        // the Dir axis, using the server's authoritative RNG stream.
        FinalDirection = FMath::VRandCone(Direction, DeviationRad);
    }
    else
    {
        // Perfect accuracy — no deviation at all
        FinalDirection = Direction;
    }
    FinalDirection.Normalize();

    // ── 2. Spawn & launch the dart ───────────────────────────────────────────

    FActorSpawnParameters Params;
    Params.Owner      = this;
    Params.Instigator = GetInstigator();
    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ADartProjectile* Dart = GetWorld()->SpawnActor<ADartProjectile>(
        ProjectileClass,
        Origin,
        FinalDirection.Rotation(),
        Params
    );

    if (Dart)
    {
        Dart->Launch(FinalDirection, Speed);
        UE_LOG(LogTemp, Warning,
               TEXT("[Server] Dart launched | FinalDir=%s | Deviation=%.2f°"),
               *FinalDirection.ToString(), DeviationDeg);
    }
    else
    {
        UE_LOG(LogTemp, Error,
               TEXT("[Server] SpawnActor returned NULL"));
    }

    // ── 3. Send diagnostic values back to the owning client for display ──────
    //
    //   We do NOT call GEngine here because Server_Throw runs on the server
    //   process; on a dedicated server there is no viewport.  Sending a Client
    //   RPC guarantees the message appears on the correct player's screen.

    Client_PrintThrowDiagnostics(Accuracy, Inaccuracy, DeviationDeg);
}

// ─────────────────────────────────────────────────────────────────────────────
// Client_PrintThrowDiagnostics  (runs on owning client)
// ─────────────────────────────────────────────────────────────────────────────

void ADartCharacter::Client_PrintThrowDiagnostics_Implementation(
    float Accuracy, float Inaccuracy, float DeviationDeg)
{
    // Slot 4 — accuracy line (overwrites previous throw's value)
    GEngine->AddOnScreenDebugMessage(4, 6.f, FColor::Green,
        FString::Printf(
            TEXT("Throw Accuracy : %.0f%%  (raw %.3f)"),
            Accuracy * 100.f, Accuracy
        ));

    // Slot 5 — inaccuracy / deviation line
    if (DeviationDeg < 0.1f)
    {
        GEngine->AddOnScreenDebugMessage(5, 6.f, FColor::Green,
            TEXT("Inaccuracy     : 0.00°  — PERFECT THROW!"));
    }
    else
    {
        // Colour grades: green < 5°, yellow 5–10°, red > 10°
        const FColor DeviationColor =
            (DeviationDeg < 5.f)  ? FColor::Green  :
            (DeviationDeg < 10.f) ? FColor::Yellow :
                                    FColor::Red;

        GEngine->AddOnScreenDebugMessage(5, 6.f, DeviationColor,
            FString::Printf(
                TEXT("Inaccuracy     : %.2f°  (factor %.3f)"),
                DeviationDeg, Inaccuracy
            ));
    }

    UE_LOG(LogTemp, Warning,
           TEXT("[Client] Throw diagnostics | Accuracy=%.3f | Inaccuracy=%.3f | Deviation=%.2f°"),
           Accuracy, Inaccuracy, DeviationDeg);
}

// Server RPC implementation: consume one dart (server-authoritative)
void ADartCharacter::Server_ConsumeDart_Implementation()
{
    if (DartsRemaining <= 0) return;

    --DartsRemaining;

    // On server the variable changes will replicate to clients and trigger OnRep
    UE_LOG(LogTemp, Log, TEXT("[Server] Consumed one dart. Remaining=%d"), DartsRemaining);

    // Optionally, notify server-side UI (if any) by calling the Blueprint event on server too
    BP_OnDartsRemainingUpdated(DartsRemaining);
}

// OnRep handlers — call Blueprint events so UMG updates
void ADartCharacter::OnRep_DartsRemaining()
{
    BP_OnDartsRemainingUpdated(DartsRemaining);

    if (IsLocallyControlled())
    {
        GEngine->AddOnScreenDebugMessage(1, 3.f, FColor::Cyan,
            FString::Printf(TEXT("Darts Remaining (replicated): %d"), DartsRemaining));
    }
}

void ADartCharacter::OnRep_PlayerScore()
{
    BP_OnPlayerScoreUpdated(PlayerScore);

    if (IsLocallyControlled())
    {
        GEngine->AddOnScreenDebugMessage(6, 3.f, FColor::Green,
            FString::Printf(TEXT("Score (replicated): %d"), PlayerScore));
    }
}

void ADartCharacter::OnRep_RoundScore()
{
    BP_OnRoundScoreUpdated(RoundScore);

    if (IsLocallyControlled())
    {
        GEngine->AddOnScreenDebugMessage(7, 3.f, FColor::Yellow,
            FString::Printf(TEXT("Round Score (replicated): %d"), RoundScore));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Server_AddRoundScore  (server only — called by DartGameMode::AddScore)
//
//  1. Accumulates Points into RoundScore (this round's running total).
//  2. Also adds to PlayerScore (career/session total).
//  3. When all 3 darts are used (DartsRemaining == 0), the round is over:
//     RoundScore is reset to 0 ready for the next round.
//     (DartsRemaining itself is reset by AdvanceTurn inside GameMode.)
// ─────────────────────────────────────────────────────────────────────────────

void ADartCharacter::Server_AddRoundScore(int32 Points)
{
    // This must only run on the server; GameMode already guarantees that.
    check(HasAuthority());

    // Accumulate into round total
    RoundScore  += Points;

    // Also keep the lifetime score in sync
    PlayerScore += Points;

    UE_LOG(LogTemp, Log,
        TEXT("[DartCharacter] RoundScore=%d | PlayerScore=%d (added %d pts)"),
        RoundScore, PlayerScore, Points);

    // OnRep won't fire on the server itself, so broadcast the BP event manually
    // so server-side widgets (listen-server host) also update.
    BP_OnRoundScoreUpdated(RoundScore);
    BP_OnPlayerScoreUpdated(PlayerScore);

    // If the player just threw their last dart, reset RoundScore for next round.
    // DartsRemaining has already been decremented before this call (in Throw()),
    // so 0 means all darts in this round are spent.
    if (DartsRemaining <= 0)
    {
        UE_LOG(LogTemp, Log,
            TEXT("[DartCharacter] Round complete — resetting RoundScore (was %d)"), RoundScore);

        RoundScore = 0;

        // Notify again so HUD shows 0 at the start of the next round
        BP_OnRoundScoreUpdated(RoundScore);
    }
}

// Replication registration
void ADartCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ADartCharacter, DartsRemaining);
    DOREPLIFETIME(ADartCharacter, PlayerScore);
    DOREPLIFETIME(ADartCharacter, RoundScore);
}