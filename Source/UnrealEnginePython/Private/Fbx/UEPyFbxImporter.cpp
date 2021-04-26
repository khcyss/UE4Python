#include "UEPyFbxImporter.h"

#if ENGINE_MINOR_VERSION > 12
#if WITH_EDITOR

#include "UEPyFbx.h"
#include "UEPyFbxNode.h"
#include "Engine/StaticMesh.h"
#include "ObjectMacros.h"

static FbxString GetNodeNameWithoutNamespace(FbxNode* Node)
{
	FbxString NodeName = Node->GetName();

	// Namespaces are marked with colons so find the last colon which will mark the start of the actual name
	int32 LastNamespceIndex = NodeName.ReverseFind(':');

	if (LastNamespceIndex == -1)
	{
		// No namespace
		return NodeName;
	}
	else
	{
		// chop off the namespace
		return NodeName.Right(NodeName.GetLen() - (LastNamespceIndex + 1));
	}
}


static PyObject *py_ue_fbx_importer_get_anim_stack_count(ue_PyFbxImporter *self, PyObject *args)
{
	return PyLong_FromLong(self->fbx_importer->GetAnimStackCount());
}

static PyObject *py_ue_fbx_importer_get_take_local_time_span(ue_PyFbxImporter *self, PyObject *args)
{
	int index;
	if (!PyArg_ParseTuple(args, "i", &index))
	{
		return nullptr;
	}

	FbxTakeInfo *take_info = self->fbx_importer->GetTakeInfo(index);
	if (!take_info)
		return PyErr_Format(PyExc_Exception, "unable to get FbxTakeInfo for index %d", index);

	FbxTimeSpan time_span = take_info->mLocalTimeSpan;
	return Py_BuildValue((char *)"(ff)", time_span.GetStart().GetSecondDouble(), time_span.GetStop().GetSecondDouble());
}

static PyObject *py_ue_fbx_importer_initialize(ue_PyFbxImporter *self, PyObject *args)
{
	char *filename;
	PyObject *py_object;
	if (!PyArg_ParseTuple(args, "sO", &filename, &py_object))
	{
		return nullptr;
	}

	ue_PyFbxIOSettings *py_fbx_io_settings = py_ue_is_fbx_io_settings(py_object);
	if (!py_fbx_io_settings)
	{
		return PyErr_Format(PyExc_Exception, "argument is not a FbxIOSettings");
	}

	if (self->fbx_importer->Initialize(filename, -1, py_fbx_io_settings->fbx_io_settings))
	{
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

static PyObject *py_ue_fbx_importer_import(ue_PyFbxImporter *self, PyObject *args)
{
	PyObject *py_object;
	if (!PyArg_ParseTuple(args, "O", &py_object))
	{
		return nullptr;
	}

	ue_PyFbxScene *py_fbx_scene = py_ue_is_fbx_scene(py_object);
	if (!py_fbx_scene)
	{
		return PyErr_Format(PyExc_Exception, "argument is not a FbxScene");
	}

	if (self->fbx_importer->Import(py_fbx_scene->fbx_scene))
	{
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

static PyObject* FillCollisionModelList(ue_PyFbxImporter* self, PyObject* args)
{
	PyObject* py_object;
	if (!PyArg_ParseTuple(args, "O", &py_object))
	{
		Py_RETURN_FALSE;
	}

	ue_PyFbxNode* py_fbx_Node = py_ue_is_fbx_node(py_object);
	if (!py_fbx_Node)
	{
		PyErr_SetString(PyExc_Exception, "argument is not a FbxNode");
		Py_RETURN_FALSE;
	}

	FbxString NodeName = GetNodeNameWithoutNamespace(py_fbx_Node->fbx_node);

	if (NodeName.Find("UCX") != -1 || NodeName.Find("MCDCX") != -1 ||
		NodeName.Find("UBX") != -1 || NodeName.Find("USP") != -1 || NodeName.Find("UCP") != -1)
	{
		// Get name of static mesh that the collision model connect to
		uint32 StartIndex = NodeName.Find('_') + 1;
		int32 TmpEndIndex = NodeName.Find('_', StartIndex);
		int32 EndIndex = TmpEndIndex;
		// Find the last '_' (underscore)
		while (TmpEndIndex >= 0)
		{
			EndIndex = TmpEndIndex;
			TmpEndIndex = NodeName.Find('_', EndIndex + 1);
		}

		const int32 NumMeshNames = 2;
		FbxString MeshName[NumMeshNames];
		if (EndIndex >= 0)
		{
			// all characters between the first '_' and the last '_' are the FBX mesh name
			// convert the name to upper because we are case insensitive
			MeshName[0] = NodeName.Mid(StartIndex, EndIndex - StartIndex).Upper();

			// also add a version of the mesh name that includes what follows the last '_'
			// in case that's not a suffix but, instead, is part of the mesh name
			if (StartIndex < (int32)NodeName.GetLen())
			{
				MeshName[1] = NodeName.Mid(StartIndex).Upper();
			}
		}
		else if (StartIndex < (int32)NodeName.GetLen())
		{
			MeshName[0] = NodeName.Mid(StartIndex).Upper();
		}

		for (int32 NameIdx = 0; NameIdx < NumMeshNames; ++NameIdx)
		{
			if ((int32)MeshName[NameIdx].GetLen() > 0)
			{
				FbxMap<FbxString, TSharedPtr<FbxArray<ue_PyFbxNode* > > >::RecordType const* Models = self->CollisionModels.Find(MeshName[NameIdx]);
				TSharedPtr< FbxArray<ue_PyFbxNode* > > Record;
				if (!Models)
				{
					Record = MakeShareable(new FbxArray<ue_PyFbxNode*>());
					self->CollisionModels.Insert(MeshName[NameIdx], Record);
				}
				else
				{
					Record = Models->GetValue();
				}

				//Unique add
				Record->AddUnique(py_fbx_Node);
			}
		}

		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

static PyObject* ImportMesh(ue_PyFbxImporter* self, PyObject* args)
{
	PyObject* py_object;
	PyObject* py_Inclassobject;
	PyObject* py_inParentobject;
	char* py_name;
	if (!PyArg_ParseTuple(args, "OOOs", &py_object,&py_Inclassobject,&py_inParentobject,&py_name))
	{
		Py_RETURN_FALSE;
	}	


	ue_PyFbxNode* py_fbx_Node = py_ue_is_fbx_node(py_object);
	if (!py_fbx_Node)
	{
		PyErr_SetString(PyExc_Exception, "argument is not a FbxNode");
		Py_RETURN_FALSE;
	}
	UClass* Inclass = ue_py_check_type<UClass>(py_Inclassobject);
	if (!Inclass)
	{
		PyErr_SetString(PyExc_Exception, "argument is not a FbxNode");
		Py_RETURN_FALSE;
	}

	UObject* InParent = ue_py_check_type<UObject>(py_inParentobject);
	if (!InParent)
	{
		PyErr_SetString(PyExc_Exception, "argument is not a FbxNode");
		Py_RETURN_FALSE;
	}

	FString MeshName(py_name);

	// Parent package to place new meshes
	UPackage* Package = NULL;
	if (InParent != nullptr && InParent->IsA(UPackage::StaticClass()))
	{
		Package = StaticCast<UPackage*>(InParent);
	}

	//UStaticMesh* StaticMesh = NewObject<UStaticMesh>(Package, FName(*MeshName), Flags | RF_Public);

	//if (StaticMesh->SourceModels.Num() < LODIndex + 1)
	//{
	//	// Add one LOD 
	//	StaticMesh->AddSourceModel();

	//	if (StaticMesh->SourceModels.Num() < LODIndex + 1)
	//	{
	//		LODIndex = StaticMesh->SourceModels.Num() - 1;
	//	}
	//}

	//FMeshDescription* MeshDescription = StaticMesh->GetMeshDescription(LODIndex);
	//if (MeshDescription == nullptr)
	//{
	//	MeshDescription = StaticMesh->CreateMeshDescription(LODIndex);
	//	check(MeshDescription != nullptr);
	//	StaticMesh->CommitMeshDescription(LODIndex);
	//	//Make sure an imported mesh do not get reduce if there was no mesh data before reimport.
	//	//In this case we have a generated LOD convert to a custom LOD
	//	StaticMesh->SourceModels[LODIndex].ReductionSettings.MaxDeviation = 0.0f;
	//	StaticMesh->SourceModels[LODIndex].ReductionSettings.PercentTriangles = 1.0f;
	//	StaticMesh->SourceModels[LODIndex].ReductionSettings.PercentVertices = 1.0f;
	//}
	//else if (InStaticMesh != NULL && LODIndex > 0)
	//{
	//	// clear out the old mesh data
	//	MeshDescription->Empty();
	//}

	//FStaticMeshSourceModel& SrcModel = StaticMesh->SourceModels[LODIndex];

	//// make sure it has a new lighting guid
	//StaticMesh->LightingGuid = FGuid::NewGuid();

	//// Set it to use textured lightmaps. Note that Build Lighting will do the error-checking (texcoordindex exists for all LODs, etc).
	//StaticMesh->LightMapResolution = 64;
	//StaticMesh->LightMapCoordinateIndex = 1;
}




static PyMethodDef ue_PyFbxImporter_methods[] = {
	{ "initialize", (PyCFunction)py_ue_fbx_importer_initialize, METH_VARARGS, "" },
	{ "_import", (PyCFunction)py_ue_fbx_importer_import, METH_VARARGS, "" },
	{ "get_anim_stack_count", (PyCFunction)py_ue_fbx_importer_get_anim_stack_count, METH_VARARGS, "" },
	{ "get_take_local_time_span", (PyCFunction)py_ue_fbx_importer_get_take_local_time_span, METH_VARARGS, "" },
	{ "fill_collision_model_list", (PyCFunction)FillCollisionModelList, METH_VARARGS, "" },
	{ NULL }  /* Sentinel */
};

static PyTypeObject ue_PyFbxImporterType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"unreal_engine.FbxImporter", /* tp_name */
	sizeof(ue_PyFbxImporter),    /* tp_basicsize */
	0,                         /* tp_itemsize */
	0,   /* tp_dealloc */
	0,                         /* tp_print */
	0,                         /* tp_getattr */
	0,                         /* tp_setattr */
	0,                         /* tp_reserved */
	0,                         /* tp_repr */
	0,                         /* tp_as_number */
	0,                         /* tp_as_sequence */
	0,                         /* tp_as_mapping */
	0,                         /* tp_hash  */
	0,                         /* tp_call */
	0,                         /* tp_str */
	0,                         /* tp_getattro */
	0,                         /* tp_setattro */
	0,                         /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,        /* tp_flags */
	"Unreal Engine FbxImporter", /* tp_doc */
	0,                         /* tp_traverse */
	0,                         /* tp_clear */
	0,                         /* tp_richcompare */
	0,                         /* tp_weaklistoffset */
	0,                         /* tp_iter */
	0,                         /* tp_iternext */
	ue_PyFbxImporter_methods,    /* tp_methods */
	0,   /* tp_members */
	0,                         /* tp_getset */
};

static int py_ue_fbx_importer_init(ue_PyFbxImporter *self, PyObject * args)
{
	PyObject *py_object;
	char *name;
	if (!PyArg_ParseTuple(args, "Os", &py_object, &name))
	{
		return -1;
	}

	ue_PyFbxManager *py_fbx_manager = py_ue_is_fbx_manager(py_object);
	if (!py_fbx_manager)
	{
		PyErr_SetString(PyExc_Exception, "argument is not a FbxManager");
		return -1;
	}

	self->fbx_importer = FbxImporter::Create(py_fbx_manager->fbx_manager, name);
	return 0;
}

void ue_python_init_fbx_importer(PyObject *ue_module)
{
	ue_PyFbxImporterType.tp_new = PyType_GenericNew;;
	ue_PyFbxImporterType.tp_init = (initproc)py_ue_fbx_importer_init;
	if (PyType_Ready(&ue_PyFbxImporterType) < 0)
		return;

	Py_INCREF(&ue_PyFbxImporterType);
	PyModule_AddObject(ue_module, "FbxImporter", (PyObject *)&ue_PyFbxImporterType);
}

#endif
#endif
