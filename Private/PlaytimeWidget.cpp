#include "PlaytimeWidget.h"
#include "Components/TextBlock.h"

void UPlaytimeWidget::SetTimeSeconds(float Seconds)
{
    Seconds = FMath::Max(0.f, Seconds);

    int32 Total = FMath::FloorToInt(Seconds);
    int32 H = Total / 3600;
    int32 M = (Total % 3600) / 60;
    int32 S = Total % 60;

    FText Out;
    if (H > 0)
    {
        Out = FText::FromString(FString::Printf(TEXT("%02d:%02d:%02d"), H, M, S));
    }
    else
    {
        Out = FText::FromString(FString::Printf(TEXT("%02d:%02d"), M, S));
    }

    if (TimeText) TimeText->SetText(Out);
}