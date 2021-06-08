
#include "PyFbxFactory.h"
#include "FbxMeshUtils.h"
#include "Runtime/Launch/Resources/Version.h"
#include "UnrealString.h"
#include "scene/geometry/fbxlayer.h"
#include "AssetToolsModule.h"
#include "ModuleManager.h"
#include "scene/shading/fbxlayeredtexture.h"
#include "scene/shading/fbxfiletexture.h"
#include "ObjectTools.h"
#include "PackageTools.h"
#include "Engine/Texture.h"
#include "EditorFramework/AssetImportData.h"
#include "AssetRegistryModule.h"
#include "Package.h"
#include "FileHelper.h"
#include "StringConv.h"


using namespace fbxsdk;

UPyFbxFactory::UPyFbxFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// disable automatic detection of the factory
	ImportPriority = 120;
	ImportOptions = nullptr;
}

bool UPyFbxFactory::ConfigureProperties() {
	bDetectImportTypeOnImport = false;
	bShowOption = false;

	return true;
}

void UPyFbxFactory::PostInitProperties() {

	Super::PostInitProperties();
	ImportUI->MeshTypeToImport = FBXIT_MAX;
}

UObject * UPyFbxFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
#if ENGINE_MINOR_VERSION >= 20
	if (ImportUI->MeshTypeToImport == FBXIT_MAX)
	{
		if (!DetectImportType(UFactory::CurrentFilename))
		{
			return nullptr;
		}
	}
	FbxMeshUtils::SetImportOption(ImportUI);
	// ensure auto-detect is skipped
	bDetectImportTypeOnImport = false;
#endif
	Parent = InParent;
	FString FileExtension = FPaths::GetExtension(Filename);
	FileBasePath = FPaths::GetPath(Filename);
	FString Name = FPaths::GetBaseFilename(Filename);
	const TCHAR* Type = *FileExtension;
	UnFbx::FFbxImporter* FbxImporter = UnFbx::FFbxImporter::GetInstance();
	ImportOptions = FbxImporter->GetImportOptions();
	FbxImporter->ImportFromFile(Filename, Type);
	FbxNode* RootNode = FbxImporter->Scene->GetRootNode();
	TArray<UStaticMesh*> ImportedObjects;
	for (int i = 0; i < RootNode->GetChildCount(); i++)
	{
		if (RootNode->GetChild(i)->GetMesh())
		{
			TArray<FbxNode*> FbxMeshArray;
			FbxMeshArray.Add(RootNode->GetChild(i));
			UStaticMesh* Mesh = FbxImporter->ImportStaticMesh(InParent, RootNode->GetChild(i), *FString::Printf(TEXT("%s_%s"), *Name, *FString(RootNode->GetChild(i)->GetName())), Flags, nullptr);
			if (Mesh)
			{
				ImportedObjects.Add(Mesh);
				//Build the staticmesh
				FbxImporter->PostImportStaticMesh(Mesh, FbxMeshArray);
				UFbxStaticMeshImportData* ImportData = Cast<UFbxStaticMeshImportData>(Mesh->AssetImportData);
				if (ImportData)
				{
					ImportData->ImportMaterialOriginalNameData.Empty();
					ImportData->ImportMeshLodData.Empty();

					for (const FStaticMaterial& Material : Mesh->StaticMaterials)
					{
						ImportData->ImportMaterialOriginalNameData.Add(Material.ImportedMaterialSlotName);
					}
					for (int32 LODResoureceIndex = 0; LODResoureceIndex < Mesh->RenderData->LODResources.Num(); ++LODResoureceIndex)
					{
						ImportData->ImportMeshLodData.AddZeroed();
						FStaticMeshLODResources& LOD = Mesh->RenderData->LODResources[LODResoureceIndex];
						int32 NumSections = LOD.Sections.Num();
						for (int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
						{
							int32 MaterialLodSectionIndex = LOD.Sections[SectionIndex].MaterialIndex;
							if (Mesh->SectionInfoMap.GetSectionNumber(LODResoureceIndex) > SectionIndex)
							{
								//In case we have a different ordering then the original fbx order use the sectioninfomap
								const FMeshSectionInfo& SectionInfo = Mesh->SectionInfoMap.Get(LODResoureceIndex, SectionIndex);
								MaterialLodSectionIndex = SectionInfo.MaterialIndex;
							}
							if (ImportData->ImportMaterialOriginalNameData.IsValidIndex(MaterialLodSectionIndex))
							{
								ImportData->ImportMeshLodData[LODResoureceIndex].SectionOriginalMaterialName.Add(ImportData->ImportMaterialOriginalNameData[MaterialLodSectionIndex]);
							}
							else
							{
								ImportData->ImportMeshLodData[LODResoureceIndex].SectionOriginalMaterialName.Add(TEXT("InvalidMaterialIndex"));
							}
						}
					}
				}


				//FbxImporter->UpdateStaticMeshImportData(Mesh, nullptr);
			}
		}
			
	}
	if (ImportedObjects.Num() > 0)
		return ImportedObjects[0];
	return nullptr;//Super::FactoryCreateFile(InClass, InParent, InName, Flags, Filename, Parms, Warn, bOutOperationCanceled);
}

UObject *UPyFbxFactory::FactoryCreateBinary
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
	bool & bOutOperationCanceled) {

	if (ImportUI->MeshTypeToImport == FBXIT_MAX) {
		if (!DetectImportType(UFactory::CurrentFilename)) {
			return nullptr;
		}
	}

	FbxMeshUtils::SetImportOption(ImportUI);

	// ensure auto-detect is skipped
	bDetectImportTypeOnImport = false;

	return Super::FactoryCreateBinary(InClass, InParent, InName, Flags, Context, Type, Buffer, BufferEnd, Warn, bOutOperationCanceled);
}

int32 UPyFbxFactory::CreateNodeMaterials(FbxNode* FbxNode, TArray<UMaterialInterface*>& OutMaterials, TArray<FString>& UVSets, bool bForSkeletalMesh)
{
	int32 MaterialCount = FbxNode->GetMaterialCount();
	TArray<FbxSurfaceMaterial*> UsedSurfaceMaterials;
	FbxMesh* MeshNode = FbxNode->GetMesh();
	TSet<int32> UsedMaterialIndexes;
	if (MeshNode)
	{
		for (int32 ElementMaterialIndex = 0; ElementMaterialIndex < MeshNode->GetElementMaterialCount(); ++ElementMaterialIndex)
		{
			FbxGeometryElementMaterial* ElementMaterial = MeshNode->GetElementMaterial(ElementMaterialIndex);
			switch (ElementMaterial->GetMappingMode())
			{
			case FbxLayerElement::eAllSame:
			{
				if (ElementMaterial->GetIndexArray().GetCount() > 0)
				{
					UsedMaterialIndexes.Add(ElementMaterial->GetIndexArray()[0]);
				}
			}
			break;
			case FbxLayerElement::eByPolygon:
			{
				for (int32 MaterialIndex = 0; MaterialIndex < ElementMaterial->GetIndexArray().GetCount(); ++MaterialIndex)
				{
					UsedMaterialIndexes.Add(ElementMaterial->GetIndexArray()[MaterialIndex]);
				}
			}
			break;
			}
		}
	}
	for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
	{
		//Create only the material used by the mesh element material
		if (MeshNode == nullptr || UsedMaterialIndexes.Contains(MaterialIndex))
		{
			FbxSurfaceMaterial* FbxMaterial = FbxNode->GetMaterial(MaterialIndex);

			if (FbxMaterial)
			{
				CreateUnrealMaterial(*FbxMaterial, OutMaterials, UVSets, bForSkeletalMesh);
			}
		}
		else
		{
			OutMaterials.Add(nullptr);
		}
	}
	return MaterialCount;
}

void UPyFbxFactory::CreateUnrealMaterial(FbxSurfaceMaterial& FbxMaterial, TArray<UMaterialInterface*>& OutMaterials, TArray<FString>& UVSets, bool bForSkeletalMesh)
{
	// Make sure we have a parent
	if (!ensure(Parent.IsValid()))
	{
		return;
	}
	if (ImportOptions->OverrideMaterials.Contains(FbxMaterial.GetUniqueID()))
	{
		UMaterialInterface* FoundMaterial = *(ImportOptions->OverrideMaterials.Find(FbxMaterial.GetUniqueID()));
		if (ImportedMaterialData.IsUnique(FbxMaterial, FName(*FoundMaterial->GetPathName())) == false)
		{
			ImportedMaterialData.AddImportedMaterial(FbxMaterial, *FoundMaterial);
		}
		// The material is override add the existing one
		OutMaterials.Add(FoundMaterial);
		return;
	}
	FString MaterialFullName = GetMaterialFullName(FbxMaterial);
	FString BasePackageName = FPackageName::GetLongPackagePath(Parent->GetOutermost()->GetName());
	if (ImportOptions->MaterialBasePath != NAME_None)
	{
		BasePackageName = ImportOptions->MaterialBasePath.ToString();
	}
	else
	{
		BasePackageName += TEXT("/");
	}
	BasePackageName += MaterialFullName;

	BasePackageName = UPackageTools::SanitizePackageName(BasePackageName);

	// The material could already exist in the project
	FName ObjectPath = *(BasePackageName + TEXT(".") + MaterialFullName);

	if (ImportedMaterialData.IsUnique(FbxMaterial, ObjectPath))
	{
		UMaterialInterface* FoundMaterial = ImportedMaterialData.GetUnrealMaterial(FbxMaterial);
		if (FoundMaterial)
		{
			// The material was imported from this FBX.  Reuse it
			OutMaterials.Add(FoundMaterial);
			return;
		}
	}
	else
	{
		FBXImportOptions* FbxImportOptions = GetImportOptions();

		FText Error;
		UMaterialInterface* FoundMaterial = UMaterialImportHelpers::FindExistingMaterialFromSearchLocation(MaterialFullName, BasePackageName, FbxImportOptions->MaterialSearchLocation, Error);

		if (!Error.IsEmpty())
		{
			AddTokenizedErrorMessage(
				FTokenizedMessage::Create(EMessageSeverity::Warning,
					FText::Format(LOCTEXT("FbxMaterialImport_MultipleMaterialsFound", "While importing '{0}': {1}"),
						FText::FromString(Parent->GetOutermost()->GetName()),
						Error)),
				FFbxErrors::Generic_LoadingSceneFailed);
		}
		// do not override existing materials
		if (FoundMaterial)
		{
			ImportedMaterialData.AddImportedMaterial(FbxMaterial, *FoundMaterial);
			OutMaterials.Add(FoundMaterial);
			return;
		}
	}

	const FString Suffix(TEXT(""));
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	FString FinalPackageName;
	AssetToolsModule.Get().CreateUniqueAssetName(BasePackageName, Suffix, FinalPackageName, MaterialFullName);

	UPackage* Package = CreatePackage(NULL, *FinalPackageName);

	// Check if we can use the specified base material to instance from it
	FBXImportOptions* FbxImportOptions = GetImportOptions();
	bool bCanInstance = false;
	if (FbxImportOptions->BaseMaterial)
	{
		bCanInstance = false;
		// try to use the material as a base for the new material to instance from
		FbxProperty FbxDiffuseProperty = FbxMaterial.FindProperty(FbxSurfaceMaterial::sDiffuse);
		if (FbxDiffuseProperty.IsValid())
		{
			bCanInstance = CanUseMaterialWithInstance(FbxMaterial, FbxSurfaceMaterial::sDiffuse, FbxImportOptions->BaseDiffuseTextureName, FbxImportOptions->BaseMaterial, UVSets);
		}
		else
		{
			bCanInstance = !FbxImportOptions->BaseColorName.IsEmpty();
		}
		FbxProperty FbxEmissiveProperty = FbxMaterial.FindProperty(FbxSurfaceMaterial::sEmissive);
		if (FbxDiffuseProperty.IsValid())
		{
			bCanInstance &= CanUseMaterialWithInstance(FbxMaterial, FbxSurfaceMaterial::sEmissive, FbxImportOptions->BaseEmmisiveTextureName, FbxImportOptions->BaseMaterial, UVSets);
		}
		else
		{
			bCanInstance &= !FbxImportOptions->BaseEmissiveColorName.IsEmpty();
		}
		bCanInstance &= CanUseMaterialWithInstance(FbxMaterial, FbxSurfaceMaterial::sSpecular, FbxImportOptions->BaseSpecularTextureName, FbxImportOptions->BaseMaterial, UVSets);
		bCanInstance &= CanUseMaterialWithInstance(FbxMaterial, FbxSurfaceMaterial::sNormalMap, FbxImportOptions->BaseNormalTextureName, FbxImportOptions->BaseMaterial, UVSets);
	}

	UMaterialInterface* UnrealMaterialFinal = nullptr;
	if (bCanInstance) {
		auto MaterialInstanceFactory = NewObject<UMaterialInstanceConstantFactoryNew>();
		MaterialInstanceFactory->InitialParent = FbxImportOptions->BaseMaterial;
		UMaterialInstanceConstant* UnrealMaterialConstant = (UMaterialInstanceConstant*)MaterialInstanceFactory->FactoryCreateNew(UMaterialInstanceConstant::StaticClass(), Package, *MaterialFullName, RF_Standalone | RF_Public, NULL, GWarn);
		if (UnrealMaterialConstant != NULL)
		{
			UnrealMaterialFinal = UnrealMaterialConstant;
			// Notify the asset registry
			FAssetRegistryModule::AssetCreated(UnrealMaterialConstant);

			// Set the dirty flag so this package will get saved later
			Package->SetDirtyFlag(true);

			//UnrealMaterialConstant->SetParentEditorOnly(FbxImportOptions->BaseMaterial);


			// textures and properties
			bool bDiffuseTextureCreated = LinkMaterialProperty(FbxMaterial, UnrealMaterialConstant, FbxSurfaceMaterial::sDiffuse, FName(*FbxImportOptions->BaseDiffuseTextureName), false);
			bool bEmissiveTextureCreated = LinkMaterialProperty(FbxMaterial, UnrealMaterialConstant, FbxSurfaceMaterial::sEmissive, FName(*FbxImportOptions->BaseEmmisiveTextureName), false);
			LinkMaterialProperty(FbxMaterial, UnrealMaterialConstant, FbxSurfaceMaterial::sSpecular, FName(*FbxImportOptions->BaseSpecularTextureName), false);
			if (!LinkMaterialProperty(FbxMaterial, UnrealMaterialConstant, FbxSurfaceMaterial::sNormalMap, FName(*FbxImportOptions->BaseNormalTextureName), true))
			{
				LinkMaterialProperty(FbxMaterial, UnrealMaterialConstant, FbxSurfaceMaterial::sBump, FName(*FbxImportOptions->BaseNormalTextureName), true); // no bump in unreal, use as normal map
			}

			// If we only have colors and its different from the base material
			if (!bDiffuseTextureCreated)
			{
				FbxDouble3 DiffuseColor;
				bool OverrideColor = false;

				if (FbxMaterial.GetClassId().Is(FbxSurfacePhong::ClassId))
				{
					DiffuseColor = ((FbxSurfacePhong&)(FbxMaterial)).Diffuse.Get();
					OverrideColor = true;
				}
				else if (FbxMaterial.GetClassId().Is(FbxSurfaceLambert::ClassId))
				{
					DiffuseColor = ((FbxSurfaceLambert&)(FbxMaterial)).Diffuse.Get();
					OverrideColor = true;
				}
				if (OverrideColor)
				{
					FLinearColor LinearColor((float)DiffuseColor[0], (float)DiffuseColor[1], (float)DiffuseColor[2]);
					FLinearColor CurrentLinearColor;
					if (UnrealMaterialConstant->GetVectorParameterValue(FName(*FbxImportOptions->BaseColorName), CurrentLinearColor))
					{
						//Alpha is not consider for diffuse color
						LinearColor.A = CurrentLinearColor.A;
						if (!CurrentLinearColor.Equals(LinearColor))
						{
							UnrealMaterialConstant->SetVectorParameterValueEditorOnly(FName(*FbxImportOptions->BaseColorName), LinearColor);
						}
					}
				}
			}
			if (!bEmissiveTextureCreated)
			{
				FbxDouble3 EmissiveColor;
				bool OverrideColor = false;

				if (FbxMaterial.GetClassId().Is(FbxSurfacePhong::ClassId))
				{
					EmissiveColor = ((FbxSurfacePhong&)(FbxMaterial)).Emissive.Get();
					OverrideColor = true;
				}
				else if (FbxMaterial.GetClassId().Is(FbxSurfaceLambert::ClassId))
				{
					EmissiveColor = ((FbxSurfaceLambert&)(FbxMaterial)).Emissive.Get();
					OverrideColor = true;
				}
				if (OverrideColor)
				{
					FLinearColor LinearColor((float)EmissiveColor[0], (float)EmissiveColor[1], (float)EmissiveColor[2]);
					FLinearColor CurrentLinearColor;
					if (UnrealMaterialConstant->GetVectorParameterValue(FName(*FbxImportOptions->BaseEmissiveColorName), CurrentLinearColor))
					{
						//Alpha is not consider for emissive color
						LinearColor.A = CurrentLinearColor.A;
						if (!CurrentLinearColor.Equals(LinearColor))
						{
							UnrealMaterialConstant->SetVectorParameterValueEditorOnly(FName(*FbxImportOptions->BaseEmissiveColorName), LinearColor);
						}
					}
				}
			}
		}
	}
	if (UnrealMaterialFinal)
	{
		// let the material update itself if necessary
		UnrealMaterialFinal->PreEditChange(NULL);
		UnrealMaterialFinal->PostEditChange();

		ImportedMaterialData.AddImportedMaterial(FbxMaterial, *UnrealMaterialFinal);

		OutMaterials.Add(UnrealMaterialFinal);
	}
}

bool UPyFbxFactory::LinkMaterialProperty(FbxSurfaceMaterial& FbxMaterial, UMaterialInstanceConstant* UnrealMaterial, const char* MaterialProperty, FName ParameterValue, bool bSetupAsNormalMap)
{
	bool bCreated = false;
	FbxProperty FbxProperty = FbxMaterial.FindProperty(MaterialProperty);
	if (FbxProperty.IsValid())
	{
		int32 LayeredTextureCount = FbxProperty.GetSrcObjectCount<FbxLayeredTexture>();
		if (LayeredTextureCount > 0)
		{
			//UE_LOG(LogFbxMaterialImport, Warning, TEXT("Layered Textures are not supported (material %s)"), UTF8_TO_TCHAR(FbxMaterial.GetName()));
		}
		else
		{
			int32 TextureCount = FbxProperty.GetSrcObjectCount<FbxTexture>();
			if (TextureCount > 0)
			{
				for (int32 TextureIndex = 0; TextureIndex < TextureCount; ++TextureIndex)
				{
					FbxFileTexture* FbxTexture = FbxProperty.GetSrcObject<FbxFileTexture>(TextureIndex);

					// create an unreal texture asset
					UTexture* UnrealTexture = ImportTexture(FbxTexture, bSetupAsNormalMap);

					if (UnrealTexture)
					{
						UnrealMaterial->SetTextureParameterValueEditorOnly(ParameterValue, UnrealTexture);
						bCreated = true;
					}
				}
			}
		}
	}

	return bCreated;
}

UTexture* UPyFbxFactory::ImportTexture(FbxFileTexture* FbxTexture, bool bSetupAsNormalMap)
{
	
	if (!FbxTexture)
	{
		return nullptr;
	}

	// create an unreal texture asset
	UTexture* UnrealTexture = NULL;
	FString AbsoluteFilename = UTF8_TO_TCHAR(FbxTexture->GetFileName());
	FString Extension = FPaths::GetExtension(AbsoluteFilename).ToLower();
	// name the texture with file name
	FString TextureName = FPaths::GetBaseFilename(AbsoluteFilename);

	TextureName = ObjectTools::SanitizeObjectName(TextureName);

	// set where to place the textures
	FString BasePackageName = FPackageName::GetLongPackagePath(Parent->GetOutermost()->GetName()) / TextureName;
	BasePackageName = UPackageTools::SanitizePackageName(BasePackageName);

	UTexture* ExistingTexture = NULL;
	UPackage* TexturePackage = NULL;
	// First check if the asset already exists.
	{
		FString ObjectPath = BasePackageName + TEXT(".") + TextureName;
		ExistingTexture = LoadObject<UTexture>(NULL, *ObjectPath, nullptr, LOAD_Quiet | LOAD_NoWarn);
	}


	if (!ExistingTexture)
	{
		const FString Suffix(TEXT(""));

		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
		FString FinalPackageName;
		AssetToolsModule.Get().CreateUniqueAssetName(BasePackageName, Suffix, FinalPackageName, TextureName);

		TexturePackage = CreatePackage(NULL, *FinalPackageName);
	}
	else
	{
		TexturePackage = ExistingTexture->GetOutermost();
	}

	FString FinalFilePath;
	if (IFileManager::Get().FileExists(*AbsoluteFilename))
	{
		// try opening from absolute path
		FinalFilePath = AbsoluteFilename;
	}
	else if (IFileManager::Get().FileExists(*(FileBasePath / UTF8_TO_TCHAR(FbxTexture->GetRelativeFileName()))))
	{
		// try fbx file base path + relative path
		FinalFilePath = FileBasePath / UTF8_TO_TCHAR(FbxTexture->GetRelativeFileName());
	}
	else if (IFileManager::Get().FileExists(*(FileBasePath / AbsoluteFilename)))
	{
		// Some fbx files dont store the actual absolute filename as absolute and it is actually relative.  Try to get it relative to the FBX file we are importing
		FinalFilePath = FileBasePath / AbsoluteFilename;
	}
	else
	{
		//UE_LOG(LogFbxMaterialImport, Warning, TEXT("Unable to find Texture file %s"), *AbsoluteFilename);
	}

	TArray<uint8> DataBinary;
	if (!FinalFilePath.IsEmpty())
	{
		FFileHelper::LoadFileToArray(DataBinary, *FinalFilePath);
	}

	if (DataBinary.Num() > 0)
	{
		//UE_LOG(LogFbxMaterialImport, Verbose, TEXT("Loading texture file %s"), *FinalFilePath);
		const uint8* PtrTexture = DataBinary.GetData();
		auto TextureFact = NewObject<UTextureFactory>();
		TextureFact->AddToRoot();

		// save texture settings if texture exist
		TextureFact->SuppressImportOverwriteDialog();
		const TCHAR* TextureType = *Extension;

		// Unless the normal map setting is used during import, 
		//	the user has to manually hit "reimport" then "recompress now" button
		if (bSetupAsNormalMap)
		{
			if (!ExistingTexture)
			{
				TextureFact->LODGroup = TEXTUREGROUP_WorldNormalMap;
				TextureFact->CompressionSettings = TC_Normalmap;
				TextureFact->bFlipNormalMapGreenChannel = ImportOptions->bInvertNormalMap;
			}
			else
			{
				//UE_LOG(LogFbxMaterialImport, Warning, TEXT("Manual texture reimport and recompression may be needed for %s"), *TextureName);
			}
		}

		UnrealTexture = (UTexture*)TextureFact->FactoryCreateBinary(
			UTexture2D::StaticClass(), TexturePackage, *TextureName,
			RF_Standalone | RF_Public, NULL, TextureType,
			PtrTexture, PtrTexture + DataBinary.Num(), GWarn);

		if (UnrealTexture != NULL)
		{
			//Make sure the AssetImportData point on the texture file and not on the fbx files since the factory point on the fbx file
			UnrealTexture->AssetImportData->Update(IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FinalFilePath));

			// Notify the asset registry
			FAssetRegistryModule::AssetCreated(UnrealTexture);

			// Set the dirty flag so this package will get saved later
			TexturePackage->SetDirtyFlag(true);
		}
		TextureFact->RemoveFromRoot();
	}

	return UnrealTexture;
}

FString UPyFbxFactory::GetMaterialFullName(FbxSurfaceMaterial& FbxMaterial)
{
	FString MaterialFullName = UTF8_TO_TCHAR(MakeName(FbxMaterial.GetName()));

	if (MaterialFullName.Len() > 6)
	{
		int32 Offset = MaterialFullName.Find(TEXT("_SKIN"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		if (Offset != INDEX_NONE)
		{
			// Chop off the material name so we are left with the number in _SKINXX
			FString SkinXXNumber = MaterialFullName.Right(MaterialFullName.Len() - (Offset + 1)).RightChop(4);

			if (SkinXXNumber.IsNumeric())
			{
				// remove the '_skinXX' suffix from the material name					
				MaterialFullName = MaterialFullName.LeftChop(MaterialFullName.Len() - Offset);
			}
		}
	}

	MaterialFullName = ObjectTools::SanitizeObjectName(MaterialFullName);

	return MaterialFullName;
}
