/* COPYRIGHT (c) 2016 Nova Labs SRL
 *
 * All rights reserved. All use of this software and documentation is
 * subject to the License Agreement located in the file LICENSE.
 */
 
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
   static Core::MW::IOStream& serial;

   static bool
   initialize();

   static void
   shell(
      const ShellCommand* commands
   );


   Module();
   virtual ~Module() {}
};
