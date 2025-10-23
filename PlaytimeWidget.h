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
    /** 초 단위를 mm:ss 또는 hh:mm:ss 문자열로 바꿔 TextBlock에 표시 */
    UFUNCTION(BlueprintCallable)
    void SetTimeSeconds(float Seconds);

protected:
    /** 디자이너에서 만든 TextBlock에 바인딩 (이름: TimeText) */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TimeText;
};