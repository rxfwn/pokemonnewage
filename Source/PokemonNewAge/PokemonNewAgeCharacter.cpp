// Copyright Epic Games, Inc. All Rights Reserved.

#include "PokemonNewAgeCharacter.h"
#include "PokemonNewAgePlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/OverlapResult.h"
#include "Core/Interactable.h"
#include "PokemonNewAge.h"

APokemonNewAgeCharacter::APokemonNewAgeCharacter()
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
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Smooth camera follow instead of a rigid one
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 10.f;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void APokemonNewAgeCharacter::BeginPlay()
{
	Super::BeginPlay();

	// WalkSpeed may have been changed in the Blueprint, so apply it here rather than only in the constructor
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;

	// Start from the distance set on the CameraBoom, within the allowed range
	DesiredCameraDistance = FMath::Clamp(CameraBoom->TargetArmLength, MinCameraDistance, MaxCameraDistance);
	CameraBoom->TargetArmLength = DesiredCameraDistance;
}

void APokemonNewAgeCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Dans un bâtiment, la caméra ne dépasse pas la limite de l'intérieur
	const float TargetDistance = CameraDistanceLimit > 0.f ? FMath::Min(DesiredCameraDistance, CameraDistanceLimit) : DesiredCameraDistance;
	if (!FMath::IsNearlyEqual(CameraBoom->TargetArmLength, TargetDistance))
	{
		CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetDistance, DeltaSeconds, ZoomInterpSpeed);
	}
}

void APokemonNewAgeCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APokemonNewAgeCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &APokemonNewAgeCharacter::Look);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &APokemonNewAgeCharacter::Look);

		// Sprinting (hold)
		if (SprintAction)
		{
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &APokemonNewAgeCharacter::DoSprintStart);
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &APokemonNewAgeCharacter::DoSprintEnd);
		}

		// Camera zoom
		if (ZoomAction)
		{
			EnhancedInputComponent->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &APokemonNewAgeCharacter::Zoom);
		}

		// Interaction (portes, PNJ...)
		SetupDefaultInteractInput();
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &APokemonNewAgeCharacter::DoInteract);

		// Touches choisies par le joueur (Options > Commandes) : les actions du personnage sont connues maintenant
		if (APokemonNewAgePlayerController* PokemonController = Cast<APokemonNewAgePlayerController>(GetController()))
		{
			PokemonController->ApplyKeyBindings();
		}
	}
	else
	{
		UE_LOG(LogPokemonNewAge, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void APokemonNewAgeCharacter::SetupDefaultInteractInput()
{
	if (InteractAction)
	{
		return;
	}

	// Pas d'IA_Interact réglé dans le Blueprint : on crée l'action et sa touche ici,
	// pour que ça marche sans rien configurer dans l'éditeur
	InteractAction = NewObject<UInputAction>(this, TEXT("IA_Interact_Default"));
	DefaultInteractContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Interact_Default"));
	DefaultInteractContext->MapKey(InteractAction, EKeys::E);
	DefaultInteractContext->MapKey(InteractAction, EKeys::Gamepad_FaceButton_Left);

	if (const APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultInteractContext, 1);
		}
	}
}

void APokemonNewAgeCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void APokemonNewAgeCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void APokemonNewAgeCharacter::Zoom(const FInputActionValue& Value)
{
	// input is a float (mouse wheel: +1 forward, -1 backward)
	DoZoom(Value.Get<float>());
}

void APokemonNewAgeCharacter::DoMove(float Right, float Forward)
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

void APokemonNewAgeCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// Sensibilité et sens de la caméra réglés dans Options (menu Pokédex)
		if (const APokemonNewAgePlayerController* PlayerController = Cast<APokemonNewAgePlayerController>(GetController()))
		{
			Yaw *= PlayerController->CameraSensitivity;
			Pitch *= PlayerController->CameraSensitivity * (PlayerController->bInvertCameraY ? -1.f : 1.f);
		}

		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void APokemonNewAgeCharacter::DoJumpStart()
{
	// signal the character to jump
	Jump();
}

void APokemonNewAgeCharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}

void APokemonNewAgeCharacter::DoSprintStart()
{
	bIsSprinting = true;
	GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
}

void APokemonNewAgeCharacter::DoSprintEnd()
{
	bIsSprinting = false;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}

void APokemonNewAgeCharacter::DoInteract()
{
	// Cherche dans une sphère un peu devant le personnage
	const FVector Center = GetActorLocation() + GetActorForwardVector() * InteractRadius * 0.5f;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(Interact), false, this);
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity,
		FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects), FCollisionShape::MakeSphere(InteractRadius), Params);

	// Garde la chose interactive la plus proche
	AActor* Best = nullptr;
	float BestDistance = TNumericLimits<float>::Max();
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (Actor && Actor->Implements<UInteractable>())
		{
			const float Distance = FVector::DistSquared(Actor->GetActorLocation(), GetActorLocation());
			if (Distance < BestDistance)
			{
				Best = Actor;
				BestDistance = Distance;
			}
		}
	}

	if (Best)
	{
		IInteractable::Execute_Interact(Best, this);
	}
}

void APokemonNewAgeCharacter::DoZoom(float Amount)
{
	// wheel forward brings the camera closer
	DesiredCameraDistance = FMath::Clamp(DesiredCameraDistance - Amount * ZoomStep, MinCameraDistance, MaxCameraDistance);
}
