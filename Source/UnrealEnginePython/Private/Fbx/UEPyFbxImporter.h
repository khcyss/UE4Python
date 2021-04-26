#pragma once
#include "UEPyModule.h"

#if WITH_EDITOR

#if PLATFORM_LINUX
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wnull-dereference"
#endif
#endif

#include <fbxsdk.h>
#include "UEPyFbxNode.h"

struct ue_PyFbxImporter
{
	PyObject_HEAD
		/* Type-specific fields go here. */
		FbxImporter *fbx_importer;
	/**
	 * Collision model list. The key is fbx node name
	 * If there is an collision model with old name format, the key is empty string("").
	 */
	FbxMap<FbxString, TSharedPtr< FbxArray<ue_PyFbxNode* > > > CollisionModels;
};


void ue_python_init_fbx_importer(PyObject *);

#endif
