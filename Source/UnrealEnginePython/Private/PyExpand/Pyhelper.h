#pragma once

#include "PythonInclude.h"


namespace Pyhelper
{
	/**
	 * Ensure that the given path is on the sys.path list.
	 */
	void AddSystemPath(const FString& InPath);
}
