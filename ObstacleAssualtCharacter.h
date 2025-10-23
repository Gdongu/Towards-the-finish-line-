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

	UPROPERTY() FVector WallImpactPoint = FVector::ZeroVector;   // 전면 벽 히트 지점
	UPROPERTY() FVector WallNormal = FVector::ForwardVector; // 벽의 외향 법선
	UPROPERTY() FVector LedgeTopPoint = FVector::ZeroVector;   // 올라설 상면 지점
	UPROPERTY() float   LedgeHeightWorld = 0.f;                   // 상면 월드 Z
	UPROPERTY() AActor* HitActor = nullptr;               // 맞은 액터

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
	FLedgeInfo CurrentLedge;

	// 탐지, 행동 함수들
	bool FindLedge(FLedgeInfo& OutInfo) const;    
	void EnterHang(const FLedgeInfo& Info);        
	void ClimbUpFromLedge();
	void DropFromLedge();

	UFUNCTION()
		void OnCapsuleHit(
			UPrimitiveComponent* HitComponent,
			AActor* OtherActor,
			UPrimitiveComponent* OtherComp,
			FVector NormalImpulse,
			const FHitResult& Hit 
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

	/** 서버에서만 전역 타임 딜레이션 설정 */
	UFUNCTION(Server, Reliable)
	void ServerSetSlowMo(bool bEnable, float NewGlobalDilation);

	/** 전역 시간 배율 (플레이어 포함 전체 느려짐) */
	UPROPERTY(EditDefaultsOnly, Category = "SlowMo")
	float GlobalTimeDilation = 0.25f;

	/** Enhanced Input 에셋들 */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> PlayerIMC;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> SlowMoAction;

	/** 슬로우 중 중복 호출 방지용 */
	bool bIsSlowMo = false;

	// BGM(2D)
	UPROPERTY(EditAnywhere, Category = "Audio|BGM")
	TObjectPtr<USoundBase> BGM;

	/** 2D 음악을 관리할 오디오 컴포넌트 */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> BGMComponent;

	/** 평소 피치 */
	UPROPERTY(EditAnywhere, Category = "Audio|BGM")
	float NormalPitch = 1.0f;

	/** Post Process 머티리얼 (M_Desaturate_PP) */
	UPROPERTY(EditDefaultsOnly, Category = "PostProcess")
	TObjectPtr<UMaterialInterface> DesaturatePPMaterial = nullptr;

	/** 동적 인스턴스 (DesatAmount 파라미터 제어) */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DesaturatePPMID = nullptr;

	/** 목표 디새츄레이션 값(0~1), 틱에서 부드럽게 보간 */
	float TargetDesat = 0.f;

	/** 보간 속도 (값/초) */
	UPROPERTY(EditDefaultsOnly, Category = "PostProcess")
	float DesatInterpSpeed = 6.0f;

	/** 위젯 BP 클래스 (WBP_Playtime) */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UPlaytimeWidget> PlaytimeWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UPlaytimeWidget> PlaytimeWidget;

	/** 시작 기준 시간 (게임 시간/실시간 중 선택) */
	float StartGameSeconds = 0.f;
	float StartRealSeconds = 0.f;

	/** true면 슬로우/일시정지 영향을 받는 ‘인게임 시간’, false면 실시간 */
	UPROPERTY(EditAnywhere, Category = "UI")
	bool bUseGameTime = false;

	// === 자동 오토클라임 설정 ===
	UPROPERTY(EditAnywhere, Category = "Ledge|Auto")
	bool bAutoClimbEnabled = true;

	UPROPERTY(EditAnywhere, Category = "Ledge|Auto", meta = (ClampMin = "0.0"))
	float AutoClimbCooldown = 0.6f;    // 연속 트리거 방지

	UPROPERTY(EditAnywhere, Category = "Ledge|Auto", meta = (ClampMin = "0.0"))
	float MinImpactSpeed = 150.f;       // 너무 살짝 스치면 무시

	UPROPERTY(EditAnywhere, Category = "Ledge|Auto", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float MinApproachDot = 0.6f;        // 정면 충돌 정도 (Forward · -WallNormal)

	UPROPERTY(EditAnywhere, Category = "Ledge|Auto")
	bool bRequireAirborne = true;       // 공중(점프/낙하)에서만 자동 발동할지

	UPROPERTY(EditAnywhere, Category = "Ledge|Auto")
	bool bUseActorTagFilter = true;

	UPROPERTY(EditAnywhere, Category = "Ledge|Auto", meta = (EditCondition = "bUseActorTagFilter"))
	FName ClimbableTag = TEXT("Climbable");

	UPROPERTY(VisibleAnywhere, Category = "Ledge|State")
	bool bClimbInProgress = false;

	float LastAutoClimbTime = -1000.f;

	// ====== Ledge Detect Params ======
	UPROPERTY(EditAnywhere, Category = "Ledge|Trace")
	float ForwardCheckDistance = 70.f;   // 앞벽 감지 거리(가슴 위치 기준)

	UPROPERTY(EditAnywhere, Category = "Ledge|Trace")
	float UpCheckHeight = 90.f;          // 벽 위로 얼마나 올라가서 내려찍을지

	UPROPERTY(EditAnywhere, Category = "Ledge|Trace")
	float DownCheckDepth = 120.f;        // 위에서 아래로 내려찍는 거리

	UPROPERTY(EditAnywhere, Category = "Ledge|Trace")
	float MinLedgeHeight = 60.f;         // 너무 낮은 턱은 무시

	UPROPERTY(EditAnywhere, Category = "Ledge|Trace")
	float MaxLedgeHeight = 180.f;        // 너무 높은 건 무시

	UPROPERTY(EditAnywhere, Category = "Ledge|Snap")
	float HangOffsetFromEdge = 35.f;     // 엣지에서 뒤로 떨어지는 거리(벽 반대로)

	UPROPERTY(EditAnywhere, Category = "Ledge|Snap")
	float HangZOffset = -40.f;

	UPROPERTY(EditDefaultsOnly, Category = "Ledge|Anim")
	TObjectPtr<UAnimMontage> ClimbUpMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Ledge|Anim")
	bool bClimbUsesRootMotion = false;

	// 시퀀스 제어
	void StartClimbUpSequence();    // 몽타주 재생 시작
	UFUNCTION(BlueprintCallable)    // AnimNotify에서 호출할 커밋 함수
		void ClimbUpCommit();           // 정확 타이밍에 위치를 엣지 위로 이동
	void FinishClimbUpSequence();   // 종료 정리(이동모드 복귀)

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	// 등반 종료 시 바닥으로 스냅
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


