#include "DartCharacter.h"
#include "DartProjectile.h"
#include "DartGameState.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Engine/Engine.h" 
#include "Math/UnrealMathUtility.h"
#include "Net/UnrealNetwork.h"     
#include "Kismet/GameplayStatics.h" 
#include "GameFramework/PlayerState.h"
#include "DartGameMode.h"



ADartCharacter::ADartCharacter()
{
    bReplicates = true;
    PrimaryActorTick.bCanEverTick = false;

    // Spring Arm
    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(RootComponent);
    SpringArm->TargetArmLength = 300.f;
    SpringArm->bUsePawnControlRotation = true;
    SpringArm->SetRelativeLocation(FVector(0.f, 0.f, 60.f));

    // Camera
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
    Camera->bUsePawnControlRotation = false;

    // Movement
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.f, 500.f, 0.f);
    GetCharacterMovement()->MaxWalkSpeed = 400.f;
    GetCharacterMovement()->JumpZVelocity = 500.f;

    bUseControllerRotationYaw = false;
    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll = false;
}


// BeginPlay


void ADartCharacter::BeginPlay()
{
    Super::BeginPlay();

    // Ensure the HUD gets correct initial values when the widget binds
    if (IsLocallyControlled())
    {
     
        BP_OnDartsRemainingUpdated(DartsRemaining);
        BP_OnPlayerScoreUpdated(PlayerScore);
        BP_OnRoundScoreUpdated(RoundScore);

        
        BP_OnLastScoredUpdated(LastScoredPoints);

        GEngine->AddOnScreenDebugMessage(1, 5.f, FColor::Cyan,
                                         FString::Printf(TEXT("Darts Remaining: %d"), DartsRemaining));
    }

    // ── Server-side
    if (HasAuthority())
    {
        if (AGameStateBase *GSBase = GetWorld() ? GetWorld()->GetGameState() : nullptr)
        {
            const int32 NumPlayers = GSBase->PlayerArray.Num();
            if (NumPlayers == 1)
            {
                
                bIsMyTurn = true;

                // For a listen-server host the local UI 
                if (IsLocallyControlled())
                {
                    const int32 SlotIndex = 0; 
                    BP_OnTurnStarted(SlotIndex);

                    GEngine->AddOnScreenDebugMessage(9, 5.f, FColor::Green,
                                                     TEXT("YOUR TURN — aim and throw! (initial assignment)"));
                }

                UE_LOG(LogTemp, Log, TEXT("[DartCharacter] Assigned initial turn to %s (players=%d)"),
                       *GetName(), NumPlayers);
            }
        }
    }

    // Server: initialize display name from PlayerState so it replicates to clients
    if (HasAuthority())
    {
        if (APlayerState *PS = GetPlayerState())
        {
            PlayerDisplayName = PS->GetPlayerName();
        }
    }

   
    if (IsLocallyControlled())
    {
        BP_OnPlayerNameUpdated(PlayerDisplayName);
    }
}


// Input


void ADartCharacter::SetupPlayerInputComponent(UInputComponent *Input)
{
    Super::SetupPlayerInputComponent(Input);

    Input->BindAxis("MoveForward", this, &ADartCharacter::MoveForward);
    Input->BindAxis("MoveRight", this, &ADartCharacter::MoveRight);
    Input->BindAxis("Turn", this, &ADartCharacter::Turn);
    Input->BindAxis("LookUp", this, &ADartCharacter::LookUp);

    Input->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
    Input->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

    // Right mouse button: hold to aim, release to throw
    Input->BindAction("Aim", IE_Pressed, this, &ADartCharacter::StartAim);
    Input->BindAction("Aim", IE_Released, this, &ADartCharacter::Throw);
}


// Movement helpers


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

void ADartCharacter::Turn(float Value) { AddControllerYawInput(Value); }
void ADartCharacter::LookUp(float Value) { AddControllerPitchInput(Value); }


// Aim start

{
    
    
    if (!bIsMyTurn)
    {
        if (IsLocallyControlled())
        {
            GEngine->AddOnScreenDebugMessage(2, 3.f, FColor::Red,
                                             TEXT("Not your turn!"));
        }
        return;
    }

    if (DartsRemaining <= 0)
    {
        if (IsLocallyControlled())
        {
            GEngine->AddOnScreenDebugMessage(2, 3.f, FColor::Red,
                                             TEXT("No darts remaining!"));
        }
        return;
    }

    bIsAiming = true;
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
        1.f,  
        true, 
        1.f   
    );
}



void ADartCharacter::AimTick()
{
    if (!bIsAiming)
    {
        ClearAimTimer();
        return;
    }

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

      
        Server_ConsumeDart(true);

        if (IsLocallyControlled())
        {
            GEngine->AddOnScreenDebugMessage(2, 4.f, FColor::Orange,
                                             TEXT("DART FORFEITED — ran out of time!"));
            
        }

        UE_LOG(LogTemp, Warning,
               TEXT("Dart forfeited — timer expired. Requesting server consume."));
    }
}

void ADartCharacter::ClearAimTimer()
{
    GetWorldTimerManager().ClearTimer(AimTimerHandle);
}


// Throw  (local client)


void ADartCharacter::Throw()
{
    if (!bIsAiming)
        return;
    bIsAiming = false;

    ClearAimTimer();
    AimCountdown = 0;

    // ── Turn-order lock 
    if (!bIsMyTurn)
    {
        UE_LOG(LogTemp, Warning, TEXT("Throw() — not this player's turn, ignoring."));
        return;
    }

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

    // Server-authoritative consume: 
    Server_ConsumeDart(false);

    if (IsLocallyControlled())
    {
        GEngine->AddOnScreenDebugMessage(1, 5.f, FColor::Cyan,
                                         FString::Printf(TEXT("Darts Remaining: %d (pending server update)"), DartsRemaining));
    }

    
    const float CapturedAccuracy = FMath::Clamp(TimingAccuracy, 0.f, 1.f);

    const FVector Origin = Camera->GetComponentLocation() + Camera->GetForwardVector() * 50.f;
    const FVector Direction = Camera->GetForwardVector();
    const float Speed = MaxThrowSpeed * FMath::Max(CapturedAccuracy, 0.3f);

    UE_LOG(LogTemp, Warning,
           TEXT("Throw() → Server_Throw | Accuracy=%.3f | Speed=%.1f"),
           CapturedAccuracy, Speed);

    
    Server_Throw(Origin, Direction, Speed, CapturedAccuracy);
}


// Server_Throw  (runs on server only)


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

   

    const float Inaccuracy = 1.f - Accuracy;                    
    const float DeviationDeg = Inaccuracy * MaxInaccuracyAngle; 
    const float DeviationRad = FMath::DegreesToRadians(DeviationDeg);

    FVector FinalDirection;
    if (DeviationRad > SMALL_NUMBER)
    {
       
       
        FinalDirection = FMath::VRandCone(Direction, DeviationRad);
    }
    else
    {
        
        FinalDirection = Direction;
    }
    FinalDirection.Normalize();

    // ── 2. Spawn & launch the dart 

    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.Instigator = GetInstigator();
    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ADartProjectile *Dart = GetWorld()->SpawnActor<ADartProjectile>(
        ProjectileClass,
        Origin,
        FinalDirection.Rotation(),
        Params);

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

   

    Client_PrintThrowDiagnostics(Accuracy, Inaccuracy, DeviationDeg);
}


// Client_PrintThrowDiagnostics  (runs on owning client)


void ADartCharacter::Client_PrintThrowDiagnostics_Implementation(
    float Accuracy, float Inaccuracy, float DeviationDeg)
{
   
    GEngine->AddOnScreenDebugMessage(4, 6.f, FColor::Green,
                                     FString::Printf(
                                         TEXT("Throw Accuracy : %.0f%%  (raw %.3f)"),
                                         Accuracy * 100.f, Accuracy));

    
    if (DeviationDeg < 0.1f)
    {
        GEngine->AddOnScreenDebugMessage(5, 6.f, FColor::Green,
                                         TEXT("Inaccuracy     : 0.00°  — PERFECT THROW!"));
    }
    else
    {
       
        const FColor DeviationColor =
            (DeviationDeg < 5.f) ? FColor::Green : (DeviationDeg < 10.f) ? FColor::Yellow
                                                                         : FColor::Red;

        GEngine->AddOnScreenDebugMessage(5, 6.f, DeviationColor,
                                         FString::Printf(
                                             TEXT("Inaccuracy     : %.2f°  (factor %.3f)"),
                                             DeviationDeg, Inaccuracy));
    }

    UE_LOG(LogTemp, Warning,
           TEXT("[Client] Throw diagnostics | Accuracy=%.3f | Inaccuracy=%.3f | Deviation=%.2f°"),
           Accuracy, Inaccuracy, DeviationDeg);
}

// Server RPC implementation: consume one dart (server-authoritative)
void ADartCharacter::Server_ConsumeDart_Implementation(bool bForfeit)
{
    if (DartsRemaining <= 0)
        return;

    --DartsRemaining;

    
    UE_LOG(LogTemp, Log, TEXT("[Server] Consumed one dart. Remaining=%d (forfeit=%s)"),
           DartsRemaining, bForfeit ? TEXT("true") : TEXT("false"));

   
    BP_OnDartsRemainingUpdated(DartsRemaining);

   
    if (bForfeit)
    {
        if (AGameModeBase *GMBase = UGameplayStatics::GetGameMode(this))
        {
            if (ADartGameMode *GM = Cast<ADartGameMode>(GMBase))
            {
                if (APlayerController *PC = Cast<APlayerController>(GetController()))
                {
                    GM->ForfeitDart(PC);
                }
            }
        }
    }
}


void ADartCharacter::AddPoints(int32 Points)
{
    if (HasAuthority())
    {
        
        Server_AddRoundScore(Points);
    }
    else
    {
        
        Server_RequestAddPoints(Points);
    }
}

void ADartCharacter::Server_RequestAddPoints_Implementation(int32 Points)
{
    
    Server_AddRoundScore(Points);
}


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

// New: replica notify for LastScoredPoints
void ADartCharacter::OnRep_LastScoredPoints()
{
    BP_OnLastScoredUpdated(LastScoredPoints);

    if (IsLocallyControlled())
    {
        GEngine->AddOnScreenDebugMessage(8, 3.f, FColor::Orange,
                                         FString::Printf(TEXT("Last Hit Points (replicated): %d"), LastScoredPoints));
    }
}


void ADartCharacter::OnRep_PlayerDisplayName()
{
    
    BP_OnPlayerNameUpdated(PlayerDisplayName);

    if (IsLocallyControlled())
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan,
                                         FString::Printf(TEXT("Player name set: %s"), *PlayerDisplayName));
    }
}


void ADartCharacter::Server_AddRoundScore(int32 Points)
{
    
    check(HasAuthority());

    
    if (Points < 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DartCharacter] Negative points received: %d. Clamping to 0."), Points);
        Points = 0;
    }
    const int32 MaxPerHit = 100;
    if (Points > MaxPerHit)
    {
        UE_LOG(LogTemp, Warning, TEXT("[DartCharacter] Excessive points received: %d. Clamping to %d."), Points, MaxPerHit);
        Points = MaxPerHit;
    }

    
    LastScoredPoints = Points;

    
    RoundScore += Points;

    
    PlayerScore += Points;

    UE_LOG(LogTemp, Log,
           TEXT("[DartCharacter] RoundScore=%d | PlayerScore=%d (added %d pts)"),
           RoundScore, PlayerScore, Points);

    
    BP_OnRoundScoreUpdated(RoundScore);
    BP_OnPlayerScoreUpdated(PlayerScore);
    BP_OnLastScoredUpdated(LastScoredPoints);

    
    if (DartsRemaining <= 0)
    {
        UE_LOG(LogTemp, Log,
               TEXT("[DartCharacter] Round complete — resetting RoundScore (was %d)"), RoundScore);

        RoundScore = 0;

        
        BP_OnRoundScoreUpdated(RoundScore);
    }
}


// OnRep_bIsMyTurn  (owning client)


void ADartCharacter::OnRep_bIsMyTurn()
{
    if (bIsMyTurn)
    {
        
        int32 MySlot = 0;
        if (AGameStateBase *GSBase = GetWorld() ? GetWorld()->GetGameState() : nullptr)
        {
            if (ADartGameState *GS = Cast<ADartGameState>(GSBase))
            {
                if (AController *C = GetController())
                {
                    MySlot = GS->GetSlotIndexForController(Cast<APlayerController>(C));
                    if (MySlot < 0)
                        MySlot = 0;
                }
            }
        }

        BP_OnTurnStarted(MySlot);

        if (IsLocallyControlled())
        {
            GEngine->AddOnScreenDebugMessage(9, 5.f, FColor::Green,
                                             TEXT("YOUR TURN — aim and throw!"));
        }
    }
    else
    {
        
        BP_OnTurnEnded(0);

        if (IsLocallyControlled())
        {
            GEngine->AddOnScreenDebugMessage(9, 5.f, FColor::Orange,
                                             TEXT("Waiting for your turn…"));
        }
    }
}

// Replication registration
void ADartCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ADartCharacter, DartsRemaining);
    DOREPLIFETIME(ADartCharacter, PlayerScore);
    DOREPLIFETIME(ADartCharacter, RoundScore);
    DOREPLIFETIME(ADartCharacter, LastScoredPoints); 
    DOREPLIFETIME(ADartCharacter, bIsMyTurn);

    // New replicated fields
    DOREPLIFETIME(ADartCharacter, PlayerDisplayName);
    DOREPLIFETIME(ADartCharacter, PlayerSlot);
}