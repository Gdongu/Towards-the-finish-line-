// Copyright Epic Games, Inc. All Rights Reserved.

#include "ObstacleAssualtCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputAction.h"
#include "ObstacleAssualt.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"
#include "ObstacleAssualtPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "PlaytimeWidget.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Animation/AnimInstance.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetMathLibrary.h"

AObstacleAssualtCharacter::AObstacleAssualtCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void AObstacleAssualtCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (SlowMoAction)
		{
			// 버튼 누를 때 → Started, 뗄 때 → Completed
			EIC->BindAction(SlowMoAction, ETriggerEvent::Started, this, &AObstacleAssualtCharacter::StartSlowMo);
			EIC->BindAction(SlowMoAction, ETriggerEvent::Completed, this, &AObstacleAssualtCharacter::StopSlowMo);
		}
	}
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AObstacleAssualtCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AObstacleAssualtCharacter::Look);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AObstacleAssualtCharacter::Look);
	}
	else
	{
		UE_LOG(LogObstacleAssualt, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AObstacleAssualtCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 로컬 플레이어에 IMC 적용
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsys = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
			{
				if (PlayerIMC)
				{
					// 우선순위 0 이상이면 OK. 필요 시 더 높은 값 사용.
					Subsys->AddMappingContext(PlayerIMC, /*Priority=*/0);
				}
			}
		}
	}

	if (BGM)
	{
		// SpawnSound2D는 월드 어디서나 들리는 2D 음악을 생성 + 재생
		// 반환된 AudioComponent를 잡아서 피치/볼륨 제어에 사용
		BGMComponent = UGameplayStatics::SpawnSound2D(this, BGM, /*VolumeMultiplier=*/1.0f, /*PitchMultiplier=*/NormalPitch, /*StartTime=*/0.0f, /*ConcurrencySettings=*/nullptr, /*bAutoDestroy=*/false);
		if (BGMComponent)
		{
			BGMComponent->bIsUISound = false; // 굳이 UI사운드로 취급하지 않음(믹싱 분리 시 바꿔도 됨)
			BGMComponent->SetUISound(false);
			// 루프는 사운드 에셋 쪽(큐/웨이브)에서 설정하는 것을 권장
			if (!BGMComponent->IsPlaying())
			{
				BGMComponent->Play();
			}
		}
	}

	if (!FollowCamera)
	{
		// 캐릭터에 달린 모든 UCameraComponent 중 첫 번째 사용
		TArray<UCameraComponent*> Cams;
		GetComponents<UCameraComponent>(Cams);
		if (Cams.Num() > 0)
		{
			// 이름이 "FollowCamera"인 걸 우선 선택
			for (UCameraComponent* Cam : Cams)
			{
				if (Cam && Cam->GetName().Contains(TEXT("FollowCamera")))
				{
					FollowCamera = Cam;
					break;
				}
			}
			if (!FollowCamera) FollowCamera = Cams[0];
		}
	}

	// ★ 포스트프로세스 MID 만들어 FollowCamera에 붙이기
	if (FollowCamera && DesaturatePPMaterial)
	{
		DesaturatePPMID = UMaterialInstanceDynamic::Create(DesaturatePPMaterial, this);
		// BlendWeight는 1로 고정, 세기는 머티리얼 파라미터(DesatAmount)로 조절
		FollowCamera->PostProcessSettings.AddBlendable(DesaturatePPMID, 1.0f);
		DesaturatePPMID->SetScalarParameterValue(TEXT("DesatAmount"), 0.0f); // 평상시 컬러
	}

	// 시작 시각 저장
	StartGameSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	StartRealSeconds = UGameplayStatics::GetRealTimeSeconds(GetWorld());

	// 위젯 생성 & 화면 추가
	if (IsLocallyControlled() && PlaytimeWidgetClass)
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			PlaytimeWidget = CreateWidget<UPlaytimeWidget>(PC, PlaytimeWidgetClass);
		}
		else
		{
			PlaytimeWidget = CreateWidget<UPlaytimeWidget>(GetWorld(), PlaytimeWidgetClass);
		}

		if (PlaytimeWidget)
		{
			PlaytimeWidget->AddToViewport(999);
			PlaytimeWidget->SetTimeSeconds(0.f);
		}
	}

	if (UCapsuleComponent* Cap = GetCapsuleComponent())
	{
		// Hit 이벤트가 나오려면 'Simulation Generates Hit Events'가 켜져야 함
		Cap->SetNotifyRigidBodyCollision(true);
		Cap->OnComponentHit.AddDynamic(this, &AObstacleAssualtCharacter::OnCapsuleHit);
	}
}

void AObstacleAssualtCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void AObstacleAssualtCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AObstacleAssualtCharacter::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void AObstacleAssualtCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AObstacleAssualtCharacter::DoJumpStart()
{
	// signal the character to jump
	Jump();
}

void AObstacleAssualtCharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}

void AObstacleAssualtCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// DesatAmount를 부드럽게 보간 (0 ↔ 1)
	if (DesaturatePPMID)
	{
		float Current = 0.f;
		DesaturatePPMID->GetScalarParameterValue(TEXT("DesatAmount"), Current);

		const float NewValue = FMath::FInterpTo(Current, TargetDesat, DeltaSeconds, DesatInterpSpeed);
		if (!FMath::IsNearlyEqual(NewValue, Current, 0.001f))
		{
			DesaturatePPMID->SetScalarParameterValue(TEXT("DesatAmount"), NewValue);
		}
	}
	if (!PlaytimeWidget) return;

	float Now = 0.f;
	float Elapsed = 0.f;

	if (bUseGameTime)
	{
		// 슬로우(전역 Time Dilation)나 일시정지의 영향을 받는 '인게임' 경과시간
		Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		Elapsed = Now - StartGameSeconds;
	}
	else
	{
		// 실제 경과시간(슬로우 무시, 일시정지 중에도 증가)
		Now = UGameplayStatics::GetRealTimeSeconds(GetWorld());
		Elapsed = Now - StartRealSeconds;
	}

	if (PlaytimeWidget)
	{
		PlaytimeWidget->SetTimeSeconds(Elapsed);
	}
}

void AObstacleAssualtCharacter::StartSlowMo()
{
	if (bIsSlowMo) return;
	bIsSlowMo = true;

	// 전역 타임 딜레이션은 서버 권한에서 적용
	ServerSetSlowMo(/*bEnable=*/true, GlobalTimeDilation);

	TargetDesat = 0.4f;

	if (BGMComponent)
	{
		BGMComponent->SetPitchMultiplier(FMath::Max(0.01f, GlobalTimeDilation));
	}

	// 플레이어도 같이 느려지게 하므로 보정 없음
	// 혹시 예전에 보정 썼다면 안전하게 원복
	CustomTimeDilation = 1.f;
	if (AController* C = GetController())
		if (AActor* AsActor = Cast<AActor>(C))
			AsActor->CustomTimeDilation = 1.f;
}

void AObstacleAssualtCharacter::StopSlowMo()
{
	if (!bIsSlowMo) return;
	bIsSlowMo = false;

	ServerSetSlowMo(/*bEnable=*/false, /*ignored*/1.f);

	TargetDesat = 0.0f;

	// 음악 피치 복구
	if (BGMComponent)
	{
		BGMComponent->SetPitchMultiplier(NormalPitch);
	}

	CustomTimeDilation = 1.f;
	if (AController* C = GetController())
		if (AActor* AsActor = Cast<AActor>(C))
			AsActor->CustomTimeDilation = 1.f;
}

void AObstacleAssualtCharacter::ServerSetSlowMo_Implementation(bool bEnable, float NewGlobalDilation)
{
	UWorld* World = GetWorld();
	if (!World) return;

	if (bEnable)
	{
		// 전체(플레이어 포함) 느려짐
		UGameplayStatics::SetGlobalTimeDilation(World, FMath::Clamp(NewGlobalDilation, 0.01f, 1.f));
	}
	else
	{
		// 복구
		UGameplayStatics::SetGlobalTimeDilation(World, 1.f);
	}
}

void AObstacleAssualtCharacter::OnCapsuleHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit)
{

	if (!bAutoClimbEnabled || bIsHanging || bClimbInProgress) return;
	if (!OtherActor || OtherActor == this) return;

	// 로컬 폰에서만 처리(멀티플레이 최적화)
	if (!IsLocallyControlled()) return;
	UWorld* World = GetWorld();
	if (!World) return;

	const float Now = World->GetTimeSeconds();
	if (Now - LastAutoClimbTime < AutoClimbCooldown) return;

	// 공중에서만 자동 발동
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	if (bRequireAirborne && Move && !Move->IsFalling()) return;

	// 태그 필터(선택)
	if (bUseActorTagFilter && !OtherActor->ActorHasTag(ClimbableTag)) return;

	// ‘벽’ 성격 판정: 법선이 수직에 가깝고(Z 작아야), 정면 접근이어야
	const FVector WallNormal = Hit.ImpactNormal.GetSafeNormal();
	if (WallNormal.Z > 0.3f) return; // 경사/바닥은 제외

	const float Approach = FVector::DotProduct(GetActorForwardVector(), -WallNormal);
	if (Approach < MinApproachDot) return;

	// 너무 살짝 닿은 경우 무시
	if (Move && Move->Velocity.SizeSquared() < (MinImpactSpeed * MinImpactSpeed)) return;

	// 실제 올라설 수 있는 엣지인지 정밀 탐지
	FLedgeInfo Info;
	if (!FindLedge(Info)) return;

	// 온리업 느낌: 닿자마자 등반
	EnterHang(Info);
	LastAutoClimbTime = Now;
	//ClimbUpFromLedge();
	StartClimbUpSequence();
}

bool AObstacleAssualtCharacter::FindLedge(FLedgeInfo& OutInfo) const
{
	OutInfo = FLedgeInfo{};

	const UCapsuleComponent* Cap = GetCapsuleComponent();
	if (!Cap || !GetWorld()) return false;

	const float HalfHeight = Cap->GetScaledCapsuleHalfHeight();
	const float Radius = Cap->GetScaledCapsuleRadius();

	const FVector Loc = GetActorLocation();
	const FRotator YawRot(0.f, GetActorRotation().Yaw, 0.f);
	const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);

	// 1) 가슴 높이에서 전방 라인트레이스 -> 벽 맞기
	const FVector Chest = Loc + FVector(0, 0, HalfHeight * 0.5f);
	const FVector Start = Chest;
	const FVector End = Chest + Forward * (ForwardCheckDistance + Radius);

	FHitResult WallHit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LedgeWall), false, this);
	const bool bHitWall = GetWorld()->LineTraceSingleByChannel(WallHit, Start, End, ECC_Visibility, Params);
	if (!bHitWall) return false;

	// 벽 성격: 거의 수직(법선 Z가 작아야)
	if (WallHit.ImpactNormal.Z > 0.3f) return false;

	// 2) 벽 위로 올라가서 아래로 캐스트 -> 상면 찾기
	const FVector Up = FVector::UpVector;
	const FVector OverTopStart = WallHit.ImpactPoint + Up * UpCheckHeight - WallHit.ImpactNormal * 10.f;
	const FVector OverTopEnd = OverTopStart - Up * DownCheckDepth;

	FHitResult TopHit;
	const bool bHitTop = GetWorld()->LineTraceSingleByChannel(TopHit, OverTopStart, OverTopEnd, ECC_Visibility, Params);
	if (!bHitTop) return false;

	// 높이 범위 체크
	const float EdgeHeight = TopHit.ImpactPoint.Z;
	const float CharFeetZ = Loc.Z - HalfHeight;
	const float HeightDelta = EdgeHeight - CharFeetZ;
	if (HeightDelta < MinLedgeHeight || HeightDelta > MaxLedgeHeight) return false;

	OutInfo.WallImpactPoint = WallHit.ImpactPoint;
	OutInfo.WallNormal = WallHit.ImpactNormal.GetSafeNormal();
	OutInfo.LedgeTopPoint = TopHit.ImpactPoint;
	OutInfo.LedgeHeightWorld = EdgeHeight;
	OutInfo.HitActor = TopHit.GetActor() ? TopHit.GetActor() : WallHit.GetActor();
	return true;
}

void AObstacleAssualtCharacter::EnterHang(const FLedgeInfo& Info)
{
	bIsHanging = true;
	CurrentLedge = Info;

	// 이동/중력 잠금
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
		Move->GravityScale = 0.f;
		Move->SetMovementMode(MOVE_Flying); // 외력 제거
	}

	// 레지 정렬 & 스냅
	const UCapsuleComponent* Cap = GetCapsuleComponent();
	const float HalfHeight = Cap ? Cap->GetScaledCapsuleHalfHeight() : 88.f;

	const FVector OutFromWall = -Info.WallNormal; // 벽을 바라보도록
	const FVector HangBase =
		FVector(Info.LedgeTopPoint.X, Info.LedgeTopPoint.Y, Info.LedgeTopPoint.Z) +
		OutFromWall * HangOffsetFromEdge +
		FVector(0, 0, HangZOffset + HalfHeight);

	const FRotator TargetRot = FRotationMatrix::MakeFromXZ(OutFromWall, FVector::UpVector).Rotator();
	SetActorLocation(HangBase, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorRotation(FRotator(0.f, TargetRot.Yaw, 0.f));
}

void AObstacleAssualtCharacter::ClimbUpFromLedge()
{
	if (!bIsHanging) return;

	// 간단 버전: 바로 위로 스냅 (연출은 나중에 몽타주/노티파이로)
	const float StepForward = 30.f; // 벽 넘어 조금 전진
	const FVector Forward = (-CurrentLedge.WallNormal);

	FVector Target = CurrentLedge.LedgeTopPoint + Forward * StepForward;

	if (const UCapsuleComponent* Cap = GetCapsuleComponent())
	{
		Target.Z += Cap->GetScaledCapsuleHalfHeight() + 2.f; // 발이 상면 위로
	}

	SetActorLocation(Target, false, nullptr, ETeleportType::TeleportPhysics);

	DropFromLedge();
}

void AObstacleAssualtCharacter::DropFromLedge()
{
	bIsHanging = false;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->GravityScale = 1.f;
		Move->SetMovementMode(MOVE_Walking);
		Move->SetDefaultMovementMode();
	}
}

void AObstacleAssualtCharacter::StartClimbUpSequence()
{
	bClimbInProgress = true; // 시퀀스 시작

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->SetMovementMode(MOVE_Flying);              // 외력 제거
		Move->bUseControllerDesiredRotation = false;
		Move->bOrientRotationToMovement = false;

		if (bClimbUsesRootMotion)
		{
			// ★ enum은 ::Type 사용 + 헤더 포함 필요: "Animation/AnimTypes.h"
			//Move->SetRootMotionMode(ERootMotionMode::Type::RootMotionFromMontagesOnly);
		}
	}

	if (UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		if (ClimbUpMontage)
		{
			Anim->Montage_Play(ClimbUpMontage, 1.f);

			// 끝나면 마무리
			FOnMontageEnded OnEnd;
			OnEnd.BindLambda([this](UAnimMontage*, bool) { FinishClimbUpSequence(); });
			Anim->Montage_SetEndDelegate(OnEnd, ClimbUpMontage);
		}
		else
		{
			// 몽타주가 비어있으면 즉시 커밋+종료
			ClimbUpCommit();
			FinishClimbUpSequence();
		}
	}
}

// AnimNotify(ClimbCommit)에서 부르는 함수
void AObstacleAssualtCharacter::ClimbUpCommit()
{
	if (bClimbUsesRootMotion) return; // 루트모션이면 이동 안 함

	const float StepForward = 30.f;
	const FVector Forward = (-CurrentLedge.WallNormal);

	FVector Target = CurrentLedge.LedgeTopPoint + Forward * StepForward;
	if (const UCapsuleComponent* Cap = GetCapsuleComponent())
	{
		Target.Z += Cap->GetScaledCapsuleHalfHeight() + 2.f;
	}

	SetActorLocation(Target, false, nullptr, ETeleportType::TeleportPhysics);

	// 시선 정렬(선택)
	const FRotator Face = FRotationMatrix::MakeFromXZ(Forward, FVector::UpVector).Rotator();
	SetActorRotation(FRotator(0.f, Face.Yaw, 0.f));

	SnapCapsuleToFloor(/*DownTrace=*/150.f, /*UpTolerance=*/12.f);
}

void AObstacleAssualtCharacter::FinishClimbUpSequence()
{
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		// 루트모션 모드 원복
		//Move->SetRootMotionMode(ERootMotionMode::Type::NoRootMotionExtraction);

		Move->GravityScale = 1.f;
		Move->SetMovementMode(MOVE_Walking);
		Move->SetDefaultMovementMode();
		Move->bUseControllerDesiredRotation = true;    // 프로젝트 스타일에 맞게
		Move->bOrientRotationToMovement = true;     // 필요하면 false
	}
	SnapCapsuleToFloor(150.f, 12.f);

	bIsHanging = false;
	bClimbInProgress = false;
}

void AObstacleAssualtCharacter::SnapCapsuleToFloor(float DownTrace, float UpTolerance)
{
	UCapsuleComponent* Cap = GetCapsuleComponent();
	if (!Cap || !GetWorld()) return;

	const float HalfHeight = Cap->GetScaledCapsuleHalfHeight();
	const float Radius = Cap->GetScaledCapsuleRadius();

	// 캡슐 중심에서 약간 위로 올린 지점에서 아래로 스윕
	const FVector Start = GetActorLocation() + FVector(0, 0, UpTolerance);
	const FVector End = Start - FVector(0, 0, DownTrace);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ClimbSnapFloor), false, this);
	FHitResult Hit;
	const bool bHit = GetWorld()->SweepSingleByChannel(
		Hit,
		Start, End,
		FQuat::Identity,
		ECollisionChannel::ECC_Visibility, // 필요하면 전용 채널
		FCollisionShape::MakeCapsule(Radius, HalfHeight),
		Params
	);

	if (bHit)
	{
		// 워커블 바닥만 인정(너무 가파르면 스킵)
		const UCharacterMovementComponent* Move = GetCharacterMovement();
		const float MaxWalkableCos = Move ? FMath::Cos(FMath::DegreesToRadians(Move->GetWalkableFloorAngle())) : FMath::Cos(FMath::DegreesToRadians(44.f));
		if (FVector::DotProduct(Hit.ImpactNormal, FVector::UpVector) >= MaxWalkableCos)
		{
			// 캡슐 바닥이 지면 위로 오도록 Z 조정
			FVector NewLoc = GetActorLocation();
			NewLoc.Z = Hit.ImpactPoint.Z + HalfHeight;
			SetActorLocation(NewLoc, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
	else
	{
		// 바닥을 못 찾았으면, 너무 뜨지 않도록 살짝만 내려주기(옵션)
		AddActorWorldOffset(FVector(0, 0, -UpTolerance * 0.5f));
	}
}