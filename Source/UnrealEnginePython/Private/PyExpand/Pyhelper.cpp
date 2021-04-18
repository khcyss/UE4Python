#include "Pyhelper.h"

void Pyhelper::AddSystemPath(const FString& InPath)
{
	/*FPyObjectPtr PySysModule = FPyObjectPtr::StealReference(PyImport_ImportModule("sys"));
	if (PySysModule)
	{
		PyObject* PySysDict = PyModule_GetDict(PySysModule);

		PyObject* PyPathList = PyDict_GetItemString(PySysDict, "path");
		if (PyPathList)
		{
			FPyObjectPtr PyPath;
			if (PyConversion::Pythonize(InPath, PyPath.Get(), PyConversion::ESetErrorState::No))
			{
				if (PySequence_Contains(PyPathList, PyPath) != 1)
				{
					PyList_Append(PyPathList, PyPath);
				}
			}
		}
	}*/
}

