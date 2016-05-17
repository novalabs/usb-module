#pragma once

#include <Configuration.hpp>
#include <Core/MW/CoreModule.hpp>
#include <Core/MW/IOStream.hpp>
#include "shell.h"


class Module:
	public Core::MW::CoreModule
{
public:
// --- DEVICES ----------------------------------------------------------------
// ----------------------------------------------------------------------------

	static Core::MW::IOStream& stream;

	static bool
	initialize();

	static void
	shell(
			const ShellCommand* commands
	);


	Module();
	virtual ~Module() {}
};
