// Fill out your copyright notice in the Description page of Project Settings.


#include "PyGameInstanceSubsystem.h"

void UPyGameInstanceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	if (FUnrealEnginePythonModule::Get())
	{
		FUnrealEnginePythonModule::Get()->RegisterPyDebug();
	}
}

void UPyGameInstanceSubsystem::Deinitialize()
{
	if (FUnrealEnginePythonModule::Get())
	{
		FUnrealEnginePythonModule::Get()->UnRegisterPyDebug();
	}
}


