
#include "AI/EnemyBarrier.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"

#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"

AEnemyBarrier::AEnemyBarrier()
{
    PrimaryActorTick.bCanEverTick = false;

    // Корневой компонент барьера.
    SphereCollision =
        CreateDefaultSubobject<USphereComponent>(
            TEXT("SphereCollision"));

    RootComponent = SphereCollision;

    // Барьер не должен мешать движению AI.
    SphereCollision->SetCollisionEnabled(
        ECollisionEnabled::NoCollision);

    SphereCollision->SetGenerateOverlapEvents(false);

    // Визуальная сфера.
    SphereVisual =
        CreateDefaultSubobject<UStaticMeshComponent>(
            TEXT("SphereVisual"));

    SphereVisual->SetupAttachment(RootComponent);

    SphereVisual->SetCollisionEnabled(
        ECollisionEnabled::NoCollision);

    SphereVisual->SetGenerateOverlapEvents(false);
    SphereVisual->SetCastShadow(false);

    SphereVisual->SetMobility(
        EComponentMobility::Movable);

    // Стандартная сфера Unreal Engine.
    static ConstructorHelpers::FObjectFinder<UStaticMesh>
        SphereMesh(
            TEXT("/Engine/BasicShapes/Sphere.Sphere"));

    if (SphereMesh.Succeeded())
    {
        SphereVisual->SetStaticMesh(
            SphereMesh.Object);
    }

    SphereVisual->SetVisibility(false);
}

void AEnemyBarrier::BeginPlay()
{
    Super::BeginPlay();

    UpdateBarrierSize();

    if (BarrierMaterial)
    {
        SphereVisual->SetMaterial(
            0, BarrierMaterial);
    }

    SphereVisual->SetVisibility(false);
}

// ==========================================
// РАЗМЕР БАРЬЕРА
// ==========================================

void AEnemyBarrier::UpdateBarrierSize()
{
    SphereCollision->SetSphereRadius(
        BarrierRadius);

    // Стандартная Sphere имеет радиус 50 см.
    const float Scale =
        BarrierRadius / 50.0f;

    SphereVisual->SetRelativeScale3D(
        FVector(Scale));
}

// ==========================================
// АКТИВАЦИЯ
// ==========================================

void AEnemyBarrier::ActivateBarrier(
    float InHP,
    float InDuration)
{
    GetWorldTimerManager().ClearTimer(
        ExpirationTimer);

    CurrentBarrierHP =
        FMath::Max(0.0f, InHP);

    bActive = CurrentBarrierHP > 0.0f;

    SphereVisual->SetVisibility(bActive);

    if (bActive && InDuration > 0.0f)
    {
        GetWorldTimerManager().SetTimer(
            ExpirationTimer,
            this,
            &AEnemyBarrier::RemoveBarrier,
            InDuration,
            false);
    }

    UE_LOG(LogTemp, Log,
        TEXT("[Barrier] Activated. HP: %.1f | Radius: %.1f"),
        CurrentBarrierHP,
        BarrierRadius);
}

// ==========================================
// ПОГЛОЩЕНИЕ УРОНА
// ==========================================

float AEnemyBarrier::AbsorbDamage(
    float DamageAmount)
{
    if (!bActive || DamageAmount <= 0.0f)
        return DamageAmount;

    const float AbsorbedDamage =
        FMath::Min(
            CurrentBarrierHP,
            DamageAmount);

    CurrentBarrierHP -= AbsorbedDamage;

    if (CurrentBarrierHP <= 0.0f)
    {
        RemoveBarrier();
    }

    return DamageAmount - AbsorbedDamage;
}

// ==========================================
// СНЯТИЕ БАРЬЕРА
// ==========================================

void AEnemyBarrier::RemoveBarrier()
{
    GetWorldTimerManager().ClearTimer(
        ExpirationTimer);

    bActive = false;
    CurrentBarrierHP = 0.0f;

    SphereVisual->SetVisibility(false);

    UE_LOG(LogTemp, Log,
        TEXT("[Barrier] Deactivated"));
}

// ==========================================
// ПРИКРЕПЛЕНИЕ К МАГУ
// ==========================================

void AEnemyBarrier::AttachToMage(
    AActor* Mage)
{
    if (!IsValid(Mage))
        return;

    AttachToActor(
        Mage,
        FAttachmentTransformRules::
        SnapToTargetNotIncludingScale);

    // Центр сферы совпадает с положением мага.
    SetActorRelativeLocation(
        FVector::ZeroVector);

    UE_LOG(LogTemp, Log,
        TEXT("[Barrier] Attached to mage: %s"),
        *Mage->GetName());
}

// ==========================================
// ПРОВЕРКА АКТИВНОСТИ
// ==========================================

bool AEnemyBarrier::IsBarrierActive() const
{
    return bActive;
}

// ==========================================
// ПРОВЕРКА ПОПАДАНИЯ В ОБЛАСТЬ
// ==========================================

bool AEnemyBarrier::ContainsLocation(
    FVector Location) const
{
    if (!bActive)
        return false;

    return FVector::DistSquared(
        GetActorLocation(),
        Location) <=
        FMath::Square(BarrierRadius);
}

void AEnemyBarrier::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(
        ExpirationTimer);

    Super::EndPlay(EndPlayReason);
}
