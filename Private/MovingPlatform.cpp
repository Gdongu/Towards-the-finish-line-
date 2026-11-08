// Fill out your copyright notice in the Description page of Project Settings.


#include "MovingPlatform.h"

// Sets default values
AMovingPlatform::AMovingPlatform()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

int MyTestFunction(float MyFloatParam, int MyIntParam)
{
	UE_LOG(LogTemp, Display, TEXT("MyFloatParam is %f, and MyIntParam is %d"), MyFloatParam, MyIntParam);

	return 40;
}

// Called when the game starts or when spawned
void AMovingPlatform::BeginPlay()
{
	Super::BeginPlay();

	int ReturnValue = MyTestFunction(3.5f, 10);
	UE_LOG(LogTemp, Display, TEXT("ReturnValue is %d"), ReturnValue);

	StartLocation = GetActorLocation();
}

// Called every frame
void AMovingPlatform::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	MovePlatform(DeltaTime);

	RotatePlatform(DeltaTime);
}

void AMovingPlatform::MovePlatform(float DeltaTime)
{
	DistanceMoved = GetDistanceMoved();

	if (DistanceMoved >= MoveDistance) 
	{
		float OverShoot = DistanceMoved - MoveDistance;
		FString PlatformName = GetName();
		UE_LOG(LogTemp, Display, TEXT("%s Overshoot by %f"), *PlatformName, OverShoot);

		FVector MoveDirection = PlatformVelocity.GetSafeNormal();
		FVector NewStartLocation = StartLocation + MoveDirection * MoveDistance;
		SetActorLocation(NewStartLocation);
		StartLocation = NewStartLocation;

		PlatformVelocity = -PlatformVelocity;
	}
	else 
	{
		FVector CurrentLocation = GetActorLocation();

		CurrentLocation += PlatformVelocity * DeltaTime;

		SetActorLocation(CurrentLocation);
	}
}

void AMovingPlatform::RotatePlatform(float DeltaTime)
{
	FRotator RotationToAdd = RotationVelocity * DeltaTime;
	AddActorLocalRotation(RotationToAdd);
}

float AMovingPlatform::GetDistanceMoved()
{
	return FVector::Dist(StartLocation, GetActorLocation());
}

