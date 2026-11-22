#if WITH_EDITOR
#include "UI/RICWaveCounterText.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/Blueprint.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

static FAutoConsoleCommand InstallWaveCounter(TEXT("ric.InstallWaveCounter"),
    TEXT("Install the editable wave counter in WBP_GameHUD"), FConsoleCommandDelegate::CreateLambda([]
    {
        auto* BP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/UI/All_WBP/WBP_GameHUD.WBP_GameHUD"));
        auto* Tree = FindObject<UWidgetTree>(BP, TEXT("WidgetTree"));
        auto* Canvas = Tree ? Cast<UCanvasPanel>(Tree->RootWidget) : nullptr;
        if (!Canvas || Tree->FindWidget(TEXT("WaveCounter"))) return;

        BP->Modify();
        Tree->Modify();
        Canvas->Modify();
        auto* Counter = Tree->ConstructWidget<URICWaveCounterText>(URICWaveCounterText::StaticClass(), TEXT("WaveCounter"));
        Counter->SetText(NSLOCTEXT("Waves", "CounterPreview", "Волна 0 / 1"));
        Counter->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 20));
        Counter->SetShadowOffset(FVector2D(1, 2));
        Counter->SetShadowColorAndOpacity(FLinearColor::Black);
        Counter->SetVisibility(ESlateVisibility::HitTestInvisible);
        UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Counter);
        Slot->SetAnchors(FAnchors(0.5f, 0.0f));
        Slot->SetAlignment(FVector2D(0.5f, 0.0f));
        Slot->SetPosition(FVector2D(0, 24));
        Slot->SetAutoSize(true);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FKismetEditorUtilities::CompileBlueprint(BP);
        UE_LOG(LogTemp, Display, TEXT("WAVE_HUD installed, status %d"), int32(BP->Status));
    }));
#endif
