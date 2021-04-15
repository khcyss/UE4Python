// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UnrealEnginePython.h"
#include "PyGameInstanceSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class UNREALENGINEPYTHON_API UPyGameInstanceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
	/** Implement this for initialization of instances of the system */
	virtual void Initialize(FSubsystemCollectionBase& Collection);

	/** Implement this for deinitialization of instances of the system */
	virtual void Deinitialize();

};
