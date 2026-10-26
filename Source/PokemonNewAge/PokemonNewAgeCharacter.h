// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "PokemonNewAgeCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class APokemonNewAgeCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
protected:

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

	/** Sprint Input Action (hold to run) */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* SprintAction;

	/** Max speed while walking, matches the mannequin walk animation (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement", meta = (ClampMin = 0, Units = "cm/s"))
	float WalkSpeed = 250.f;

	/** Max speed while running, matches the mannequin jog animation (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement", meta = (ClampMin = 0, Units = "cm/s"))
	float SprintSpeed = 500.f;

	/** True while the sprint input is held */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Movement")
	bool bIsSprinting = false;

	/** Camera Zoom Input Action (mouse wheel, Axis1D) */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* ZoomAction;

	/** Closest the camera can get to the character (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera", meta = (ClampMin = 50, Units = "cm"))
	float MinCameraDistance = 200.f;

	/** Farthest the camera can get from the character (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera", meta = (ClampMin = 50, Units = "cm"))
	float MaxCameraDistance = 800.f;

	/** Distance change for one mouse wheel notch (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera", meta = (ClampMin = 0, Units = "cm"))
	float ZoomStep = 50.f;

	/** How fast the camera reaches the requested distance */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera", meta = (ClampMin = 0))
	float ZoomInterpSpeed = 8.f;

	/** Camera distance requested by the player, reached smoothly in Tick */
	float DesiredCameraDistance = 400.f;

public:

	/** Constructor */
	APokemonNewAgeCharacter();	

protected:

	/** Applies the walk speed and camera distance once Blueprint defaults are loaded */
	virtual void BeginPlay() override;

	/** Smoothly moves the camera towards the requested distance */
	virtual void Tick(float DeltaSeconds) override;

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Called for camera zoom input */
	void Zoom(const FInputActionValue& Value);

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

	/** Handles sprint pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoSprintStart();

	/** Handles sprint released inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoSprintEnd();

	/** Handles camera zoom inputs from either controls or UI interfaces. Positive brings the camera closer */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoZoom(float Amount);

	/** Returns true while the character is running */
	UFUNCTION(BlueprintPure, Category="Movement")
	bool IsSprinting() const { return bIsSprinting; }

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};

