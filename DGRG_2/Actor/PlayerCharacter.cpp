// Fill out your copyright notice in the Description page of Project Settings.

#include "PlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "DrawDebugHelpers.h"
#include "Manager/DGRG_GameInstance.h"
#include "GameFramework/SpringArmComponent.h"
#include "Delegate.h"
#include "Datas/DGRG_Macro.h"
#include "Kismet/KismetSystemLibrary.h"
//#include "MonsterBase.h"
#include "PortraitRenderActor.h"
#include "Components/MeshComponent.h"

APlayerCharacter::APlayerCharacter(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;
	//
	m_CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	m_CameraBoom->SetupAttachment(RootComponent);
	m_CameraBoom->TargetArmLength = 300.0f;
	m_CameraBoom->bUsePawnControlRotation = true;
	m_CameraBoom->bDoCollisionTest = false;
	// 
	m_FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	m_FollowCamera->SetupAttachment(m_CameraBoom, USpringArmComponent::SocketName);
	m_FollowCamera->bUsePawnControlRotation = false;
	//
	m_InteractObj = nullptr;
	m_CurrentAttackTarget = nullptr;
	m_PortraitActor = nullptr;
	m_MonsterCastHalfSizeWant = FVector(200, 200, 10);
	m_PortActorPosition = FVector(99999, 99999, 99999);
	m_PortActorZRot = 259.f;
}


void APlayerCharacter::BeginPlay()
{
	//GetGameInstance<UDGRG_GameInstance>()->SetPlayerActor(this);
	Super::BeginPlay();
	

	SetPortraitActor();
	
}

void APlayerCharacter::MoveForward(float Value)
{
	if (m_IsAttacking)
	{
		return;
	}
	if ((Controller != NULL) && (Value != 0.0f))
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void APlayerCharacter::MoveRight(float Value)
{
	if (m_IsAttacking)
	{
		return;
	}

	if ((Controller != NULL) && (Value != 0.0f))
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get right vector 
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}

void APlayerCharacter::SetupPlayerInputComponent(UInputComponent * PlayerInputComponent)
{
	check(PlayerInputComponent);
	PlayerInputComponent->BindAxis("MoveForward", this, &APlayerCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &APlayerCharacter::MoveRight);
}

void APlayerCharacter::SetNullInteract()
{
	m_InteractObj = nullptr;
}

void APlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	TryCheckInteractable();
	TryCheckTargetMonster();
}


void APlayerCharacter::SetPortraitActor()
{
	m_PortraitActor = GetWorld()->SpawnActor<APortraitRenderActor>(m_ClassPortRender, m_PortActorPosition, FRotator(0, m_PortActorZRot, 0));
	m_PortraitActor->SetCamTargetActor(this);
	m_PortraitActor->SetSkMesh(GetMesh()->SkeletalMesh);
	m_PortraitActor->SetTargetAnimation();
	for (auto* EquipSlotEle : m_EquipSlotArray)
	{
		EquipSlotEle->SetPortraitActor(m_PortraitActor);
	}
	m_OnStanceChanged.AddUObject(m_PortraitActor, &APortraitRenderActor::SetTargetAnimationDeleWrapper);
}

void APlayerCharacter::TryCheckInteractable()
{
	FHitResult HitResult;

	FVector StartTrace = GetCapsuleComponent()->GetComponentLocation();
	float Half = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	FVector EndTrace = (GetCapsuleComponent()->GetForwardVector() * 300) + StartTrace;

	StartTrace.Z += Half;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	if (m_InteractObj)
	{
		SetRenderFocusOff(m_InteractObj->GetOwner());
		m_InteractObj->SetFocusOut();
	}
	FCollisionObjectQueryParams CollObject= FCollisionObjectQueryParams(ECC_TO_BITFIELD(ECC_GameTraceChannel3));
	if (GetWorld()->
		LineTraceSingleByObjectType(HitResult, StartTrace, EndTrace, CollObject.ObjectTypesToQuery, QueryParams))
	{
		auto* InteractComponent = Cast<UInteractable>(HitResult.GetActor()->GetComponentByClass(UInteractable::StaticClass()));

		if (InteractComponent)
		{
			m_InteractObj = InteractComponent;

			if (!m_InteractObj->m_bUnlocked)
			{
				return;
			}
			
			OnFoundInteractBP.Broadcast();

			SetRenderFocusOn(m_InteractObj->GetOwner());
			m_InteractObj->SetFocusIn();

			return;
		}
	}

	m_InteractObj = nullptr;
	OnFailFoundInteractBP.Broadcast();
}

void APlayerCharacter::TryCheckTargetMonster()
{
	//monster 1
	FVector StartPos = GetActorLocation() + (GetActorForwardVector()*300.0f);
	FVector EndPos = GetActorLocation() + (GetActorForwardVector()*400.0f);
	FHitResult HitResult;

	if (UKismetSystemLibrary::BoxTraceSingle(GetWorld(),
		StartPos, EndPos,
		m_MonsterCastHalfSizeWant,
		GetActorRotation(),
		ETraceTypeQuery::TraceTypeQuery3,
		false,
		m_MonsterCastIgnore,
		EDrawDebugTrace::None,
		HitResult, true))
	{
		auto* MonsterCasted=Cast<AMonsterBase>(HitResult.Actor);

		if (MonsterCasted == m_CurrentAttackTarget)
		{
			return;
		}

		SetAttackTarget(MonsterCasted);
		return;
	}

	if (!m_CurrentAttackTarget)
	{
		return;
	}
	SetNullAttackTarget();
}

bool APlayerCharacter::InteractWithTarget()
{
	if (m_InteractObj)
	{
		m_InteractObj->Interact(this);
		return true;
	}

	return false;
}





void APlayerCharacter::SetNullAttackTarget()
{
	if (m_CurrentAttackTarget)
	{
		SetRenderFocusOff(m_CurrentAttackTarget);
		OnFoundEndMonsterBP.Broadcast(m_CurrentAttackTarget);
		m_CurrentAttackTarget = nullptr;
	}
}

void APlayerCharacter::SetAttackTarget(AMonsterBase * target)
{
	//if (!target)
	//{
	//	return;
	//}

	SetNullAttackTarget();

	OnFoundMonsterBP.Broadcast(target);
	m_CurrentAttackTarget = target;
	SetRenderFocusOn(m_CurrentAttackTarget);
}

AMonsterBase * APlayerCharacter::GetAttackTarget()
{
	return m_CurrentAttackTarget;
}

bool APlayerCharacter::TryAttack()
{
	if (GetAttackTarget()) {
		auto ROt = (GetAttackTarget()->GetActorLocation()) - GetActorLocation();
		SetActorRotation(ROt.Rotation());
	}

	return Super::TryAttack();
}

void APlayerCharacter::GetKill()
{
	Super::GetKill();
	DisableInput(Cast<APlayerController>( GetController()));
}

void APlayerCharacter::SetRenderFocusOn(AActor * target)
{
	UMeshComponent* MeshCompo =Cast<UMeshComponent>(target->GetComponentByClass(UMeshComponent::StaticClass()));
	MeshCompo->SetRenderCustomDepth(true);
}

void APlayerCharacter::SetRenderFocusOff(AActor * target)
{
	UMeshComponent* MeshCompo = Cast<UMeshComponent>(target->GetComponentByClass(UMeshComponent::StaticClass()));
	MeshCompo->SetRenderCustomDepth(false);
}
