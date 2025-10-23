#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlaytimeWidget.generated.h"

class UTextBlock;

UCLASS()
class UPlaytimeWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable)
    void SetTimeSeconds(float Seconds);

protected:
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TimeText;

};
