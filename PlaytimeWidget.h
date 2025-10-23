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
    /** �� ������ mm:ss �Ǵ� hh:mm:ss ���ڿ��� �ٲ� TextBlock�� ǥ�� */
    UFUNCTION(BlueprintCallable)
    void SetTimeSeconds(float Seconds);

protected:
    /** �����̳ʿ��� ���� TextBlock�� ���ε� (�̸�: TimeText) */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TimeText;
};