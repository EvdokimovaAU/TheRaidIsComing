#include "UI/RICOverheadHealthBar.h"
#include "GameFramework/Actor.h"
#include "UObject/ConstructorHelpers.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

URICOverheadHealthBar::URICOverheadHealthBar()
{
    static ConstructorHelpers::FObjectFinder<UObject> Base(TEXT("/Game/Assets/UI/Buttons/Menu_Sprite_6.Menu_Sprite_6"));
    static ConstructorHelpers::FObjectFinder<UObject> Green(TEXT("/Game/Assets/UI/Buttons/Menu_Sprite_10.Menu_Sprite_10"));
    static ConstructorHelpers::FObjectFinder<UObject> Red(TEXT("/Game/Assets/UI/Buttons/Menu_Sprite_11.Menu_Sprite_11"));
    Frame.SetResourceObject(Base.Object);
    FriendlyFill.SetResourceObject(Green.Object);
    EnemyFill.SetResourceObject(Red.Object);
    SetWidgetSpace(EWidgetSpace::Screen);
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetCanEverAffectNavigation(false);
    SetPivot(FVector2D(0.5f, 1.f));
}

void URICOverheadHealthBar::Attach(AActor* Owner, FVector WorldAnchor, FIntPoint Size,
    TAttribute<FVector2D> Health, bool bFriendly, bool bBarrier)
{
    if (Owner->GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    auto* Bar = NewObject<URICOverheadHealthBar>(Owner);
    Owner->AddInstanceComponent(Bar);
    Bar->SetupAttachment(Owner->GetRootComponent());
    Bar->SetRelativeLocation(Owner->GetActorTransform().InverseTransformPosition(WorldAnchor));
    const float LabelWidth = bBarrier ? 125.f : 72.f;
    Bar->SetDrawSize(FVector2D(Size.X + LabelWidth, FMath::Max(20, Size.Y)));
    Bar->SetPivot(FVector2D(Size.X * 0.5f / (Size.X + LabelWidth), bBarrier ? 2.1f : 1.f));
    Bar->SetSlateWidget(
        SNew(SHorizontalBox).Visibility_Lambda([Health]()
        {
            return Health.Get().X > 0.0 ? EVisibility::HitTestInvisible : EVisibility::Hidden;
        })
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(SBox).WidthOverride(Size.X).HeightOverride(Size.Y)
            [
                SNew(SOverlay)
                + SOverlay::Slot()[SNew(SImage).Image(&Bar->Frame)]
                + SOverlay::Slot().Padding(2).HAlign(HAlign_Left)
                [
                    SNew(SBox).WidthOverride_Lambda([Health, Size]()
                    {
                        const FVector2D HP = Health.Get();
                        return FOptionalSize((Size.X - 4) * (HP.Y > 0 ? FMath::Clamp(HP.X / HP.Y, 0., 1.) : 0.));
                    })
                    [
                        SNew(SImage).Image(bFriendly || bBarrier ? &Bar->FriendlyFill : &Bar->EnemyFill)
                    ]
                ]
            ]
        ]
        + SHorizontalBox::Slot().AutoWidth().Padding(4, 0).VAlign(VAlign_Center)
        [
            SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
            .ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(FLinearColor::Black)
            .Text_Lambda([Health, bBarrier]()
            {
                const FVector2D HP = Health.Get();
                const FText Value = FText::FromString(FString::Printf(TEXT("%d/%d"),
                    FMath::CeilToInt32(FMath::Clamp(HP.X, 0.0, FMath::Max(0.0, HP.Y))),
                    FMath::CeilToInt32(FMath::Max(0.0, HP.Y))));
                return bBarrier ? FText::Format(NSLOCTEXT("Health", "Barrier", "Купол {0}"), Value) : Value;
            })
        ]);
    Bar->RegisterComponent();
}
