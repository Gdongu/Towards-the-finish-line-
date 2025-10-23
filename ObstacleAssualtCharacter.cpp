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
			// ��ư ���� �� �� Started, �� �� �� Completed
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

	// ���� �÷��̾ IMC ����
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsys = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
			{
				if (PlayerIMC)
				{
					// �켱���� 0 �̻��̸� OK. �ʿ� �� �� ���� �� ���.
					Subsys->AddMappingContext(PlayerIMC, /*Priority=*/0);
				}
			}
		}
	}

	if (BGM)
	{
		// SpawnSound2D�� ���� ��𼭳� �鸮�� 2D ������ ���� + ���
		// ��ȯ�� AudioComponent�� ��Ƽ� ��ġ/���� ��� ���
		BGMComponent = UGameplayStatics::SpawnSound2D(this, BGM, /*VolumeMultiplier=*/1.0f, /*PitchMultiplier=*/NormalPitch, /*StartTime=*/0.0f, /*ConcurrencySettings=*/nullptr, /*bAutoDestroy=*/false);
		if (BGMComponent)
		{
			BGMComponent->bIsUISound = false; // ���� UI����� ������� ����(�ͽ� �и� �� �ٲ㵵 ��)
			BGMComponent->SetUISound(false);
			// ������ ���� ���� ��(ť/���̺�)���� �����ϴ� ���� ����
			if (!BGMComponent->IsPlaying())
			{
				BGMComponent->Play();
			}
		}
	}

	if (!FollowCamera)
	{
		// ĳ���Ϳ� �޸� ��� UCameraComponent �� ù ��° ���
		TArray<UCameraComponent*> Cams;
		GetComponents<UCameraComponent>(Cams);
		if (Cams.Num() > 0)
		{
			// �̸��� "FollowCamera"�� �� �켱 ����
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

	// �� ����Ʈ���μ��� MID ����� FollowCamera�� ���̱�
	if (FollowCamera && DesaturatePPMaterial)
	{
		DesaturatePPMID = UMaterialInstanceDynamic::Create(DesaturatePPMaterial, this);
		// BlendWeight�� 1�� ����, ����� ��Ƽ���� �Ķ����(DesatAmount)�� ����
		FollowCamera->PostProcessSettings.AddBlendable(DesaturatePPMID, 1.0f);
		DesaturatePPMID->SetScalarParameterValue(TEXT("DesatAmount"), 0.0f); // ���� �÷�
	}

	// ���� �ð� ����
	StartGameSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	StartRealSeconds = UGameplayStatics::GetRealTimeSeconds(GetWorld());

	// ���� ���� & ȭ�� �߰�
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
		// Hit �̺�Ʈ�� �������� 'Simulation Generates Hit Events'�� ������ ��
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

	// DesatAmount�� �ε巴�� ���� (0 �� 1)
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
		// ���ο�(���� Time Dilation)�� �Ͻ������� ������ �޴� '�ΰ���' ����ð�
		Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		Elapsed = Now - StartGameSeconds;
	}
	else
	{
		// ���� ����ð�(���ο� ����, �Ͻ����� �߿��� ����)
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

	// ���� Ÿ�� �����̼��� ���� ���ѿ��� ����
	ServerSetSlowMo(/*bEnable=*/true, GlobalTimeDilation);

	TargetDesat = 0.4f;

	if (BGMComponent)
	{
		BGMComponent->SetPitchMultiplier(FMath::Max(0.01f, GlobalTimeDilation));
	}

	// �÷��̾ ���� �������� �ϹǷ� ���� ����
	// Ȥ�� ������ ���� ��ٸ� �����ϰ� ����
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

	// ���� ��ġ ����
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
		// ��ü(�÷��̾� ����) ������
		UGameplayStatics::SetGlobalTimeDilation(World, FMath::Clamp(NewGlobalDilation, 0.01f, 1.f));
	}
	else
	{
		// ����
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

	// ���� �������� ó��(��Ƽ�÷��� ����ȭ)
	if (!IsLocallyControlled()) return;
	UWorld* World = GetWorld();
	if (!World) return;

	const float Now = World->GetTimeSeconds();
	if (Now - LastAutoClimbTime < AutoClimbCooldown) return;

	// ���߿����� �ڵ� �ߵ�
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	if (bRequireAirborne && Move && !Move->IsFalling()) return;

	// �±� ����(����)
	if (bUseActorTagFilter && !OtherActor->ActorHasTag(ClimbableTag)) return;

	// ������ ���� ����: ������ ������ ������(Z �۾ƾ�), ���� �����̾��
	const FVector WallNormal = Hit.ImpactNormal.GetSafeNormal();
	if (WallNormal.Z > 0.3f) return; // ���/�ٴ��� ����

	const float Approach = FVector::DotProduct(GetActorForwardVector(), -WallNormal);
	if (Approach < MinApproachDot) return;

	// �ʹ� ��¦ ���� ��� ����
	if (Move && Move->Velocity.SizeSquared() < (MinImpactSpeed * MinImpactSpeed)) return;

	// ���� �ö� �� �ִ� �������� ���� Ž��
	FLedgeInfo Info;
	if (!FindLedge(Info)) return;

	// �¸��� ����: ���ڸ��� ���
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

	// 1) ���� ���̿��� ���� ����Ʈ���̽� -> �� �±�
	const FVector Chest = Loc + FVector(0, 0, HalfHeight * 0.5f);
	const FVector Start = Chest;
	const FVector End = Chest + Forward * (ForwardCheckDistance + Radius);

	FHitResult WallHit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LedgeWall), false, this);
	const bool bHitWall = GetWorld()->LineTraceSingleByChannel(WallHit, Start, End, ECC_Visibility, Params);
	if (!bHitWall) return false;

	// �� ����: ���� ����(���� Z�� �۾ƾ�)
	if (WallHit.ImpactNormal.Z > 0.3f) return false;

	// 2) �� ���� �ö󰡼� �Ʒ��� ĳ��Ʈ -> ��� ã��
	const FVector Up = FVector::UpVector;
	const FVector OverTopStart = WallHit.ImpactPoint + Up * UpCheckHeight - WallHit.ImpactNormal * 10.f;
	const FVector OverTopEnd = OverTopStart - Up * DownCheckDepth;

	FHitResult TopHit;
	const bool bHitTop = GetWorld()->LineTraceSingleByChannel(TopHit, OverTopStart, OverTopEnd, ECC_Visibility, Params);
	if (!bHitTop) return false;

	// ���� ���� üũ
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

	// �̵�/�߷� ���
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
		Move->GravityScale = 0.f;
		Move->SetMovementMode(MOVE_Flying); // �ܷ� ����
	}

	// ���� ���� & ����
	const UCapsuleComponent* Cap = GetCapsuleComponent();
	const float HalfHeight = Cap ? Cap->GetScaledCapsuleHalfHeight() : 88.f;

	const FVector OutFromWall = -Info.WallNormal; // ���� �ٶ󺸵���
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

	// ���� ����: �ٷ� ���� ���� (������ ���߿� ��Ÿ��/��Ƽ���̷�)
	const float StepForward = 30.f; // �� �Ѿ� ���� ����
	const FVector Forward = (-CurrentLedge.WallNormal);

	FVector Target = CurrentLedge.LedgeTopPoint + Forward * StepForward;

	if (const UCapsuleComponent* Cap = GetCapsuleComponent())
	{
		Target.Z += Cap->GetScaledCapsuleHalfHeight() + 2.f; // ���� ��� ����
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
	bClimbInProgress = true; // ������ ����

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->SetMovementMode(MOVE_Flying);              // �ܷ� ����
		Move->bUseControllerDesiredRotation = false;
		Move->bOrientRotationToMovement = false;

		if (bClimbUsesRootMotion)
		{
			// �� enum�� ::Type ��� + ��� ���� �ʿ�: "Animation/AnimTypes.h"
			//Move->SetRootMotionMode(ERootMotionMode::Type::RootMotionFromMontagesOnly);
		}
	}

	if (UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		if (ClimbUpMontage)
		{
			Anim->Montage_Play(ClimbUpMontage, 1.f);

			// ������ ������
			FOnMontageEnded OnEnd;
			OnEnd.BindLambda([this](UAnimMontage*, bool) { FinishClimbUpSequence(); });
			Anim->Montage_SetEndDelegate(OnEnd, ClimbUpMontage);
		}
		else
		{
			// ��Ÿ�ְ� ��������� ��� Ŀ��+����
			ClimbUpCommit();
			FinishClimbUpSequence();
		}
	}
}

// AnimNotify(ClimbCommit)���� �θ��� �Լ�
void AObstacleAssualtCharacter::ClimbUpCommit()
{
	if (bClimbUsesRootMotion) return; // ��Ʈ����̸� �̵� �� ��

	const float StepForward = 30.f;
	const FVector Forward = (-CurrentLedge.WallNormal);

	FVector Target = CurrentLedge.LedgeTopPoint + Forward * StepForward;
	if (const UCapsuleComponent* Cap = GetCapsuleComponent())
	{
		Target.Z += Cap->GetScaledCapsuleHalfHeight() + 2.f;
	}

	SetActorLocation(Target, false, nullptr, ETeleportType::TeleportPhysics);

	// �ü� ����(����)
	const FRotator Face = FRotationMatrix::MakeFromXZ(Forward, FVector::UpVector).Rotator();
	SetActorRotation(FRotator(0.f, Face.Yaw, 0.f));

	SnapCapsuleToFloor(/*DownTrace=*/150.f, /*UpTolerance=*/12.f);
}

void AObstacleAssualtCharacter::FinishClimbUpSequence()
{
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		// ��Ʈ��� ��� ����
		//Move->SetRootMotionMode(ERootMotionMode::Type::NoRootMotionExtraction);

		Move->GravityScale = 1.f;
		Move->SetMovementMode(MOVE_Walking);
		Move->SetDefaultMovementMode();
		Move->bUseControllerDesiredRotation = true;    // ������Ʈ ��Ÿ�Ͽ� �°�
		Move->bOrientRotationToMovement = true;     // �ʿ��ϸ� false
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

	// ĸ�� �߽ɿ��� �ణ ���� �ø� �������� �Ʒ��� ����
	const FVector Start = GetActorLocation() + FVector(0, 0, UpTolerance);
	const FVector End = Start - FVector(0, 0, DownTrace);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ClimbSnapFloor), false, this);
	FHitResult Hit;
	const bool bHit = GetWorld()->SweepSingleByChannel(
		Hit,
		Start, End,
		FQuat::Identity,
		ECollisionChannel::ECC_Visibility, // �ʿ��ϸ� ���� ä��
		FCollisionShape::MakeCapsule(Radius, HalfHeight),
		Params
	);

	if (bHit)
	{
		// ��Ŀ�� �ٴڸ� ����(�ʹ� ���ĸ��� ��ŵ)
		const UCharacterMovementComponent* Move = GetCharacterMovement();
		const float MaxWalkableCos = Move ? FMath::Cos(FMath::DegreesToRadians(Move->GetWalkableFloorAngle())) : FMath::Cos(FMath::DegreesToRadians(44.f));
		if (FVector::DotProduct(Hit.ImpactNormal, FVector::UpVector) >= MaxWalkableCos)
		{
			// ĸ�� �ٴ��� ���� ���� ������ Z ����
			FVector NewLoc = GetActorLocation();
			NewLoc.Z = Hit.ImpactPoint.Z + HalfHeight;
			SetActorLocation(NewLoc, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
	else
	{
		// �ٴ��� �� ã������, �ʹ� ���� �ʵ��� ��¦�� �����ֱ�(�ɼ�)
		AddActorWorldOffset(FVector(0, 0, -UpTolerance * 0.5f));
	}
}