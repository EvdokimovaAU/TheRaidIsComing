#include "RICPassage.h"

#include "RICBuilding.h"
#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"

ARICPassage::ARICPassage()
{
	PrimaryActorTick.bCanEverTick = false;

	PassageBounds = CreateDefaultSubobject<UBoxComponent>(
		TEXT("PassageBounds")
	);

	SetRootComponent(PassageBounds);

	PassageBounds->SetBoxExtent(HalfSize);
	PassageBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PassageBounds->SetGenerateOverlapEvents(false);
	PassageBounds->SetCanEverAffectNavigation(false);
	PassageBounds->SetHiddenInGame(true);

	DirectionArrow = CreateDefaultSubobject<UArrowComponent>(
		TEXT("DirectionArrow")
	);

	DirectionArrow->SetupAttachment(PassageBounds);
	DirectionArrow->SetHiddenInGame(true);
}

void ARICPassage::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (HalfSize.ContainsNaN())
	{
		HalfSize = FVector(100.0f, 150.0f, 100.0f);
	}

	HalfSize.X = FMath::Max(1.0f, HalfSize.X);
	HalfSize.Y = FMath::Max(1.0f, HalfSize.Y);
	HalfSize.Z = FMath::Max(1.0f, HalfSize.Z);

	RingId = FMath::Max(0, RingId);

	PassageBounds->SetBoxExtent(HalfSize);
}

void ARICPassage::BeginPlay()
{
	Super::BeginPlay();

	PassageId = FGuid::NewGuid();

	UE_LOG(
		LogTemp,
		Log,
		TEXT("Passage initialized: %s. ID: %s. Ring: %d. Open: %s"),
		*GetName(),
		*PassageId.ToString(),
		RingId,
		IsOpen() ? TEXT("Yes") : TEXT("No")
	);
}

bool ARICPassage::IsOpen() const
{
	return !IsValid(InstalledBarrier.Get()) ||
		InstalledBarrier->IsActorBeingDestroyed();
}

bool ARICPassage::RegisterBarrier(ARICBuilding* Barrier)
{
	if (!IsOpen() ||
		!IsValid(Barrier) ||
		Barrier->IsActorBeingDestroyed() ||
		Barrier->BuildingType != ERICBuildingType::Barrier)
	{
		return false;
	}

	// Убираем предыдущую подписку, если ссылка ещё существует.
	if (IsValid(InstalledBarrier.Get()))
	{
		InstalledBarrier->OnDestroyed.RemoveDynamic(
			this,
			&ARICPassage::OnBarrierDestroyed
		);
	}

	InstalledBarrier = Barrier;

	InstalledBarrier->OnDestroyed.AddDynamic(
		this,
		&ARICPassage::OnBarrierDestroyed
	);

	UE_LOG(
		LogTemp,
		Log,
		TEXT("Passage closed: %s. Barrier: %s"),
		*PassageId.ToString(),
		*Barrier->GetName()
	);

	return true;
}

void ARICPassage::OnBarrierDestroyed(AActor* DestroyedActor)
{
	if (DestroyedActor != InstalledBarrier.Get())
	{
		return;
	}

	InstalledBarrier = nullptr;

	UE_LOG(
		LogTemp,
		Log,
		TEXT("Passage reopened: %s"),
		*PassageId.ToString()
	);
}

void ARICPassage::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(InstalledBarrier.Get()))
	{
		InstalledBarrier->OnDestroyed.RemoveDynamic(
			this,
			&ARICPassage::OnBarrierDestroyed
		);
	}

	InstalledBarrier = nullptr;

	Super::EndPlay(EndPlayReason);
}