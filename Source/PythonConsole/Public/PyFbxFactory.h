#pragma once

#include "UnrealEd.h"
#include <fbxsdk.h>
#include "FbxImporter.h"
#include "scene/fbxscene.h"
#include "scene/geometry/fbxnode.h"
#include "core/fbxobject.h"
#include "scene/shading/fbxsurfacematerial.h"
#include "Materials/MaterialInterface.h"
#include "scene/shading/fbxfiletexture.h"
#include "PyFbxFactory.generated.h"






UCLASS(hidecategories = Object)
class UPyFbxFactory : public UFbxFactory
{
	GENERATED_BODY()

	UPyFbxFactory(const FObjectInitializer& ObjectInitializer);

	virtual bool ConfigureProperties() override;
	virtual void PostInitProperties() override;
	virtual UObject * FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;
	virtual UObject * FactoryCreateBinary
		(
			UClass * InClass,
			UObject * InParent,
			FName InName,
			EObjectFlags Flags,
			UObject * Context,
			const TCHAR * Type,
			const uint8 *& Buffer,
			const uint8 * BufferEnd,
			FFeedbackContext * Warn,
			bool & bOutOperationCanceled) override;

	virtual int32 CreateNodeMaterials(FbxNode* FbxNode, TArray<UMaterialInterface*>& OutMaterials, TArray<FString>& UVSets, bool bForSkeletalMesh);


	void initImportOptions(UnFbx::FBXImportOptions* Options);




	TArray<UStaticMesh*> ImportSataticMesh(FbxNode* ParentNode,UObject* Outer, EObjectFlags Flags, UnFbx::FFbxImporter* FbxImporter,const FString& Suffix);

	/**
	 * Create Unreal material from Fbx material.
	 * Only setup channels that connect to texture, and setup the UV coordinate of texture.
	 * If diffuse channel has no texture, one default node will be created with constant.
	 *
	 * @param KFbxSurfaceMaterial*  Fbx material
	 * @param outMaterials Unreal Materials we created
	 * @param outUVSets
	 */
	void CreateUnrealMaterial(FbxSurfaceMaterial& FbxMaterial, TArray<class UMaterialInterface*>& OutMaterials, TArray<FString>& UVSets, bool bForSkeletalMesh);



	/**
	* Create and link texture to the right material parameter value
	*
	* @param FbxMaterial	Fbx material object
	* @param UnrealMaterial
	* @param MaterialProperty The material component to import
	* @param ParameterValue
	* @param bSetupAsNormalMap
	* @return bool
	*/
	bool LinkMaterialProperty(FbxSurfaceMaterial& FbxMaterial,
		UMaterialInstanceConstant* UnrealMaterial,
		const char* MaterialProperty,
		FName ParameterValue,
		bool bSetupAsNormalMap);

	/**
	 * Generate Unreal texture object from FBX texture.
	 *
	 * @param FbxTexture FBX texture object to import.
	 * @param bSetupAsNormalMap Flag to import this texture as normal map.
	 * @return UTexture* Unreal texture object generated.
	 */
	UTexture* ImportTexture(FbxFileTexture* FbxTexture, bool bSetupAsNormalMap);

	/**
	* Make material Unreal asset name from the Fbx material
	*
	* @param FbxMaterial Material from the Fbx node
	* @return Sanitized asset name
	*/
	FString GetMaterialFullName(FbxSurfaceMaterial& FbxMaterial);


	bool CanUseMaterialWithInstance(FbxSurfaceMaterial& FbxMaterial, const char* MaterialProperty, FString ParameterValueName, UMaterialInterface* BaseMaterial, TArray<FString>& UVSet);

private:
	UnFbx::FBXImportOptions* ImportOptions;
	TWeakObjectPtr<UObject> Parent;
	FString FileBasePath;
	UnFbx::FImportedMaterialData ImportedMaterialData;
};

