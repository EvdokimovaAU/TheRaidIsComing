
#pragma once

#include "CoreMinimal.h"
#include "AI/EnemyAI.h"
#include "EnemyMageAI.generated.h"

class AEnemyBarrier;
class AEnemySwordsmanAI;
class ARICBuilding;
class UAnimMontage;
class ANavigationData;

UENUM(BlueprintType)
enum class EMageState : uint8
{
    CreatingBarrier UMETA(DisplayName = "Creating Barrier"),
    FollowingSwordsmen UMETA(DisplayName = "Following Swordsmen"),
    MovingAlone UMETA(DisplayName = "Moving Alone"),
    WaitingForBarrier UMETA(DisplayName = "Waiting For Barrier"),
    Attacking UMETA(DisplayName = "Attacking"),
    Regrouping UMETA(DisplayName = "Regrouping")
};

UCLASS()
class RAIDISCOMING_API AEnemyMageAI : public AEnemyAI
{
    GENERATED_BODY()

public:

    AEnemyMageAI();

    virtual void ApplyEnemyBalance() override;
    virtual void MoveToTarget() override;
    virtual void Die() override;

    // ==========================================
    // MAGIC SETTINGS
    // ==========================================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Magic")
    float AllyDetectionRadius = 800.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Magic")
    int32 MaxProtectedSwordsmen = 2;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Magic")
    float BarrierHP = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Magic")
    float BarrierCooldown = 5.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Magic")
    float BarrierDuration = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Magic")
    TSubclassOf<AEnemyBarrier> BarrierClass;

    // ==========================================
    // ANIMATION
    // ==========================================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Animation")
    TObjectPtr<UAnimMontage> BarrierMontage = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Animation")
    TObjectPtr<UAnimMontage> AttackMontage = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Animation")
    float BarrierSpawnDelay = 0.45f;

    // ==========================================
    // BARRIER STATE
    // ==========================================

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Magic")
    TObjectPtr<AEnemyBarrier> ActiveBarrier = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Magic")
    EMageState MageState = EMageState::MovingAlone;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Magic")
    TArray<TObjectPtr<AEnemyAI>> AlliesAhead;

    // ==========================================
    // MOVEMENT SETTINGS
    // ==========================================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Movement")
    float FollowDistance = 120.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Movement")
    float RegroupDistance = 300.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Movement")
    float AllyReleaseDistance = 1200.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Movement")
    float MageMovementSpeed = 320.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Movement")
    float NormalMovementSpeed = 250.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Movement")
    float MageAcceleration = 350.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Movement")
    float MageBrakingDeceleration = 300.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Movement")
    float SpeedChangeRate = 150.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Movement")
    float FollowUpdateInterval = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Movement")
    float RepathDistance = 100.f;

    // Fraction of barrier radius used as safe escort area.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Movement")
    float BarrierSafetyFraction = 0.65f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Movement")
    float EscortMinSwordsmanSpeed = 80.f;

    // ==========================================
    // COMBAT SETTINGS
    // ==========================================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat")
    float WallDetectionRadius = 220.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat")
    float WallAttackDistance = 140.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat", meta = (ClampMin = "0.0", ClampMax = "100.0"))
    float SoloWallAttackChance = 50.f;

    // ==========================================
    // PUBLIC METHODS
    // ==========================================

    UFUNCTION(BlueprintCallable, Category = "AI|Magic")
    void FindAlliesAhead();

    UFUNCTION(BlueprintCallable, Category = "AI|Magic")
    bool CanCreateBarrier() const;

    UFUNCTION(BlueprintCallable, Category = "AI|Magic")
    void Barrier();

    UFUNCTION(BlueprintCallable, Category = "AI|Magic")
    void SpawnBarrierFromAnimation();

    UFUNCTION(BlueprintCallable, Category = "AI|Magic")
    void RemoveBarrier();

    UFUNCTION(BlueprintPure, Category = "AI|Magic")
    bool HasActiveBarrier() const;

    bool OwnsEscortMovement() const;
    bool CanSelectEscortObstacle(const AEnemySwordsmanAI* Ally) const;
    bool FindEscortMarchDestination(AEnemySwordsmanAI* Ally, FVector& Destination);
    bool BindSpawnCompanion(AEnemySwordsmanAI* Ally);
    bool IsEscortMarching() const { return bEscortMarchActive; }

protected:

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason
    ) override;

private:

    friend class FEnemyEscortObstacleTest;
    friend class FEnemyEscortMarchTest;
    friend class FEnemyLevelOneTest;
    friend class FObserveLevelOneCommand;
    UFUNCTION()
    void OnCombatNavigationChanged(ANavigationData* NavData);
    FVector RejectedFromPosition = FVector::ZeroVector;
    TWeakObjectPtr<ARICBuilding> MarchBuilding;
    FVector MarchAnchor = FVector::ZeroVector;
    TMap<TWeakObjectPtr<AEnemySwordsmanAI>, FVector> MarchDestinations;
    bool EnsureMarchAnchor();
    void ClearMarchPlan();
    bool BuildEscortRoute();
    void UpdateEscortMarch();
    bool UpdateRecruitMeeting();
    bool ReserveRecruit(AEnemySwordsmanAI* Ally);
    void CancelRecruitMeeting(const TCHAR* Reason);
    FVector PointOnEscortRoute(float Progress) const;
    float ProjectOntoEscortRoute(const FVector& Position, float MinProgress, float MaxProgress) const;
    void IssueEscortMove(AEnemyAI* Member, const FVector& Goal, float Speed);
    struct FMarchMove
    {
        FVector Goal = FVector::ZeroVector;
        float Progress = 0.f;
        bool bIssued = false;
    };
    TMap<TWeakObjectPtr<AEnemyAI>, FMarchMove> MarchMoves;
    TArray<FVector> EscortRoute;
    TArray<float> EscortRouteDistances;
    bool bEscortMarchActive = false;
    bool bSpawnGroupNeedsBarrier = false;
    bool bFinishingEscortMarch = false;
    TWeakObjectPtr<AEnemySwordsmanAI> MeetingRecruit;
    TMap<TWeakObjectPtr<AEnemyAI>, FVector> MeetingSlots;
    float MeetingProgress = 0.f;
    bool bApproachingMeeting = false;

    // ==========================================
    // TIMERS
    // ==========================================

    FTimerHandle BehaviorTimer;
    FTimerHandle BarrierSpawnTimer;
    FTimerHandle BarrierCastEndTimer;
    FTimerHandle AttackTimer;

    // ==========================================
    // ESCORT GROUP
    // ==========================================

    TArray<TWeakObjectPtr<AEnemySwordsmanAI>> EscortGroup;

    TArray<TWeakObjectPtr<AEnemySwordsmanAI>> WaitingSwordsmen;

    TWeakObjectPtr<AEnemySwordsmanAI> FollowedSwordsman;

    TWeakObjectPtr<AActor> CurrentMoveTarget;

    TWeakObjectPtr<ARICBuilding> CombatTarget;

    // ==========================================
    // INTERNAL STATES
    // ==========================================

    bool bCastingBarrier = false;
    bool bBarrierSpawnedThisCast = false;

    bool bAttacking = false;
    bool bAlliesNeedToAttack = false;

    bool bBarrierDestroyed = false;
    bool bCombatWasWall = false;
    bool bWaitingForPostCombatBarrier = false;
    bool bRecruitBarrierRefreshPending = false;
    bool bRecruitBarrierRefreshCasting = false;
    TWeakObjectPtr<ARICBuilding> PendingGroupTarget;

    bool bMovingToCapitol = false;
    bool bFollowingSwordsman = false;

    float NextBarrierTime = 0.f;
    float CurrentDesiredSpeed = 250.f;

    // ==========================================
    // SPEED CONTROL
    // ==========================================

    TWeakObjectPtr<AEnemySwordsmanAI> SpeedControlledSwordsman;

    float SavedSwordsmanSpeed = 0.f;

    TMap<TWeakObjectPtr<AEnemySwordsmanAI>, float> OriginalEscortSpeeds;

    void RestoreAllEscortSpeeds();
    void RestoreSwordsmanSpeed();

    // ==========================================
    // MAIN BEHAVIOR
    // ==========================================

    void UpdateBehavior();

    // ==========================================
    // ALLY MANAGEMENT
    // ==========================================

    void RefreshEscortGroup();

    void SelectSwordsman();

    void ReleaseEscortGroup();

    void StopNearbySwordsmen();

    void ReleaseWaitingSwordsmen();

    AEnemySwordsmanAI* FindRegroupTarget() const;

    AEnemySwordsmanAI* FindFollowTarget() const;

    // ==========================================
    // MOVEMENT
    // ==========================================

    void UpdateMovement();

    void UpdateFollowSpeed(float DeltaTime);

    void RequestMoveToActor(
        AActor* MoveTarget,
        float InAcceptanceRadius
    );

    void ResetMovementRequest();

    // ==========================================
    // GROUP COMBAT
    // ==========================================

    bool AreProtectedAlliesAttacking() const;

    void UpdateGroupCombatState();

    bool StageEscortAtWall(ARICBuilding* Wall);
    bool BeginObstacleAction(ARICBuilding* Wall);
    void UpdateObstacleAction();
    void ResetObstacleAction(const TCHAR* Reason, bool bRestoreBarrier);
    bool IsAllyOnRoute(const AEnemySwordsmanAI* Ally) const;
    enum class EObstaclePhase : uint8 { None, Approaching, Attacking, Restoring };
    EObstaclePhase ObstaclePhase = EObstaclePhase::None;
    struct FObstacleSlot
    {
        TWeakObjectPtr<AEnemyAI> Member;
        FVector Destination = FVector::ZeroVector;
        FVector LastPosition = FVector::ZeroVector;
        float LastProgressTime = 0.f;
        bool bMoveIssued = false;
        bool bArrived = false;
    };
    TArray<FObstacleSlot> ObstacleSlots;
    float LastGroupProgressTime = 0.f;
    TWeakObjectPtr<ARICBuilding> FailedApproachWall;
    TArray<FVector> FailedApproachAnchors;
    TSet<TWeakObjectPtr<AEnemySwordsmanAI>> JoinedEscort;
    TMap<TWeakObjectPtr<ARICBuilding>, float> RejectedWalls;
    TWeakObjectPtr<ARICBuilding> SoloAvoidedWall;
    float NextSoloDecisionTime = 0.f;
    TWeakObjectPtr<ARICBuilding> EscortStagingWall;
    float NextWallNavRetryTime = 0.f;

    // ==========================================
    // COMBAT
    // ==========================================

    void UpdateCombat();

    ARICBuilding* FindCombatTarget() const;

    void StartCombat(ARICBuilding* TargetBuilding);

    void StopCombat();

    void PerformAttack();

    // ==========================================
    // BARRIER
    // ==========================================

    void SpawnBarrierInternal();

    void FinishBarrierCast();

    void CancelBarrierCast();

    void RemoveBarrierInternal(bool bDestroyed);

    void UpdateBarrierState();

    // ==========================================
    // HELPERS
    // ==========================================

    bool IsSwordsman(const AEnemyAI* Enemy) const;
};
