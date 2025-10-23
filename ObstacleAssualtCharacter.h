// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Animation/AnimTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Logging/LogMacros.h"
#include "ObstacleAssualtCharacter.generated.h"

USTRUCT(BlueprintType)
struct FLedgeInfo
{
	GENERATED_BODY()

	UPROPERTY() FVector WallImpactPoint = FVector::ZeroVector;   // ���� �� ��Ʈ ����
	UPROPERTY() FVector WallNormal = FVector::ForwardVector; // ���� ���� ����
	UPROPERTY() FVector LedgeTopPoint = FVector::ZeroVector;   // �ö� ��� ����
	UPROPERTY() float   LedgeHeightWorld = 0.f;                   // ��� ���� Z(�����)
	UPROPERTY() AActor* HitActor = nullptr;               // ���� ����(�ɼ�)

	bool IsValid() const { return HitActor != nullptr; }
};

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;
class UPlaytimeWidget;
class UPrimitiveComponent;
class UCapsuleComponent;
class USoundBase;
class UAudioComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class AObstacleAssualtCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
protected:
	bool bIsHanging = false;
	FLedgeInfo CurrentLedge; // �� ����� ����� ��� ok (���ǰ� ���� �����Ƿ�)

	// Ž��/�ൿ �Լ���
	bool FindLedge(FLedgeInfo& OutInfo) const;     // by-ref
	void EnterHang(const FLedgeInfo& Info);         // by-const-ref or by-value ok
	void ClimbUpFromLedge();
	void DropFromLedge();

	UFUNCTION() // �� �ݵ�� �ʿ�
		void OnCapsuleHit(
			UPrimitiveComponent* HitComponent,
			AActor* OtherActor,
			UPrimitiveComponent* OtherComp,
			FVector NormalImpulse,
			const FHitResult& Hit   // �� const FHitResult& ���� ��
		);
	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;


public:

	/** Constructor */
	AObstacleAssualtCharacter();	

protected:

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UFUNCTION()
	void StartSlowMo();

	UFUNCTION()
	void StopSlowMo();

	/** ���������� ���� Ÿ�� �����̼� ���� */
	UFUNCTION(Server, Reliable)
	void ServerSetSlowMo(bool bEnable, float NewGlobalDilation);

	/** ���� �ð� ���� (�÷��̾� ���� ��ü ������) */
	UPROPERTY(EditDefaultsOnly, Category = "SlowMo")
	float GlobalTimeDilation = 0.25f;

	/** Enhanced Input ���µ� (BP���� ���� ����) */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> PlayerIMC;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> SlowMoAction;

	/** (����) ���ο� �� �ߺ� ȣ�� ������ */
	bool bIsSlowMo = false;

	// BGM(2D)
	UPROPERTY(EditAnywhere, Category = "Audio|BGM")
	TObjectPtr<USoundBase> BGM;

	/** 2D ������ ������ ����� ������Ʈ (BeginPlay���� Spawn) */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> BGMComponent;

	/** ��� ��ġ (���� 1.0) */
	UPROPERTY(EditAnywhere, Category = "Audio|BGM")
	float NormalPitch = 1.0f;

	/** Post Process ��Ƽ���� (M_Desaturate_PP) */
	UPROPERTY(EditDefaultsOnly, Category = "PostProcess")
	TObjectPtr<UMaterialInterface> DesaturatePPMaterial = nullptr;

	/** ���� �ν��Ͻ� (DesatAmount �Ķ���� ����) */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DesaturatePPMID = nullptr;

	/** ��ǥ ������̼� ��(0~1), ƽ���� �ε巴�� ���� */
	float TargetDesat = 0.f;

	/** ���� �ӵ� (��/��) */
	UPROPERTY(EditDefaultsOnly, Category = "PostProcess")
	float DesatInterpSpeed = 6.0f; // ������ 0.15~0.25�� ����

	/** ���� BP Ŭ���� (WBP_Playtime ����) */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UPlaytimeWidget> PlaytimeWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UPlaytimeWidget> PlaytimeWidget;

	/** ���� ���� �ð�(���� �ð�/�ǽð� �� ����) */
	float StartGameSeconds = 0.f;
	float StartRealSeconds = 0.f;

	/** true�� ���ο�/�Ͻ����� ������ �޴� ���ΰ��� �ð���, false�� �ǽð� */
	UPROPERTY(EditAnywhere, Category = "UI")
	bool bUseGameTime = false;

	// === �ڵ� ����Ŭ���� ���� ===
	UPROPERTY(EditAnywhere, Category = "Ledge|Auto")
	bool bAutoClimbEnabled = true;

	UPROPERTY(EditAnywhere, Category = "Ledge|Auto", meta = (ClampMin = "0.0"))
	float AutoClimbCooldown = 0.6f;    // ���� Ʈ���� ����

	UPROPERTY(EditAnywhere, Category = "Ledge|Auto", meta = (ClampMin = "0.0"))
	float MinImpactSpeed = 150.f;       // �ʹ� ��¦ ��ġ�� ����

	UPROPERTY(EditAnywhere, Category = "Ledge|Auto", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float MinApproachDot = 0.6f;        // ���� �浹 ����(Forward �� -WallNormal)

	UPROPERTY(EditAnywhere, Category = "Ledge|Auto")
	bool bRequireAirborne = true;       // ����(����/����)������ �ڵ� �ߵ�����

	UPROPERTY(EditAnywhere, Category = "Ledge|Auto")
	bool bUseActorTagFilter = true;

	UPROPERTY(EditAnywhere, Category = "Ledge|Auto", meta = (EditCondition = "bUseActorTagFilter"))
	FName ClimbableTag = TEXT("Climbable");

	UPROPERTY(VisibleAnywhere, Category = "Ledge|State")
	bool bClimbInProgress = false;

	float LastAutoClimbTime = -1000.f;

	// ====== Ledge Detect Params ======
	UPROPERTY(EditAnywhere, Category = "Ledge|Trace")
	float ForwardCheckDistance = 70.f;   // �պ� ���� �Ÿ�(���� ��ġ ����)

	UPROPERTY(EditAnywhere, Category = "Ledge|Trace")
	float UpCheckHeight = 90.f;          // �� ���� �󸶳� �ö󰡼� ����������

	UPROPERTY(EditAnywhere, Category = "Ledge|Trace")
	float DownCheckDepth = 120.f;        // ������ �Ʒ��� ������� �Ÿ�

	UPROPERTY(EditAnywhere, Category = "Ledge|Trace")
	float MinLedgeHeight = 60.f;         // �ʹ� ���� ���� ����

	UPROPERTY(EditAnywhere, Category = "Ledge|Trace")
	float MaxLedgeHeight = 180.f;        // �ʹ� ���� �� ����

	UPROPERTY(EditAnywhere, Category = "Ledge|Snap")
	float HangOffsetFromEdge = 35.f;     // �������� �ڷ� �������� �Ÿ�(�� �ݴ��)

	UPROPERTY(EditAnywhere, Category = "Ledge|Snap")
	float HangZOffset = -40.f;

	// ��Ÿ�� ����(�����Ϳ��� �Ҵ�)
	UPROPERTY(EditDefaultsOnly, Category = "Ledge|Anim")
	TObjectPtr<UAnimMontage> ClimbUpMontage = nullptr;

	// ��Ʈ��� ��� ����(��Ÿ�ְ� ��Ʈ��� �����̸� true ����)
	UPROPERTY(EditDefaultsOnly, Category = "Ledge|Anim")
	bool bClimbUsesRootMotion = false;

	// ������ ����
	void StartClimbUpSequence();    // ��Ÿ�� ��� ����
	UFUNCTION(BlueprintCallable)    // AnimNotify���� ȣ���� Ŀ�� �Լ�
		void ClimbUpCommit();           // ��Ȯ Ÿ�ֿ̹� ��ġ�� ���� ���� �̵�
	void FinishClimbUpSequence();   // ���� ����(�̵���� ����)

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	// ��� ���� �� �ٴ����� ����
	void SnapCapsuleToFloor(float DownTrace = 120.f, float UpTolerance = 10.f);

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

	void BeginPlay() override;

	void Tick(float DeltaTime) override;

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};

