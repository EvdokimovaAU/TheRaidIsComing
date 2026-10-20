#include "RICPassage.h"

#include "RICBuilding.h"
#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"

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

	HighlightMesh = CreateDefaultSubobject<UStaticMeshComponent>(
		TEXT("HighlightMesh")
	);

	HighlightMesh->SetupAttachment(PassageBounds);
	HighlightMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HighlightMesh->SetGenerateOverlapEvents(false);
	HighlightMesh->SetCanEverAffectNavigation(false);
	HighlightMesh->SetCastShadow(false);
	HighlightMesh->SetVisibility(false);
	HighlightMesh->SetHiddenInGame(true);
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

	// Используем стандартный Plane размером 100 x 100 см.
// Origin прохода располагаем на уровне пола.
	HighlightMesh->SetRelativeLocation(FVector(0.0, 0.0, 2.0));
	HighlightMesh->SetRelativeRotation(FRotator::ZeroRotator);
	HighlightMesh->SetRelativeScale3D(
		FVector(HalfSize.X / 50.0, HalfSize.Y / 50.0, 1.0)
	);

	HighlightMesh->SetMaterial(0, HighlightMaterial.Get());

	RefreshHighlight();
}

void ARICPassage::BeginPlay()
{
	Super::BeginPlay();
	// Проверка в редакторе не должна оставлять подсветку включённой в игре.
	SetHighlighted(false);
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
	return GetInstalledBarrier() == nullptr;
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
	RefreshHighlight();
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
	RefreshHighlight();
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
void ARICPassage::SetHighlighted(bool bEnabled)
{
	bHighlightRequested = bEnabled;
	RefreshHighlight();
}

void ARICPassage::RefreshHighlight()
{
	if (!HighlightMesh)
	{
		return;
	}

	const bool bShow = bHighlightRequested && IsOpen();

	HighlightMesh->SetVisibility(bShow);
	HighlightMesh->SetHiddenInGame(!bShow);
}

void ARICPassage::ShowHighlight()
{
	SetHighlighted(true);
}

void ARICPassage::HideHighlight()
{
	SetHighlighted(false);
}

ARICBuilding* ARICPassage::GetInstalledBarrier() const
{
	ARICBuilding* Barrier = InstalledBarrier.Get();
	return IsValid(Barrier) && !Barrier->IsActorBeingDestroyed()
		? Barrier : nullptr;
}

FRICPassageData ARICPassage::GetPassageData() const
{
	FRICPassageData Data;
	Data.Passage = const_cast<ARICPassage*>(this);
	Data.PassageId = PassageId;
	Data.RingId = RingId;
	Data.PassageLocation = GetActorLocation();

	ARICBuilding* Barrier = GetInstalledBarrier();
	Data.bIsOpen = (Barrier == nullptr);
	if (Barrier)
	{
		Data.Barrier = Barrier;
		Data.BarrierId = Barrier->BuildingId;
		Data.BarrierLocation = Barrier->GetActorLocation();
		Data.BarrierHP = Barrier->CurrentHP;
		Data.BarrierMaxHP = Barrier->MaxHP;
	}
	return Data;
}
