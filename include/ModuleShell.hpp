/* COPYRIGHT (c) 2016-2017 Nova Labs SRL
 *
 * All rights reserved. All use of this software and documentation is
 * subject to the License Agreement located in the file LICENSE.
 */

#pragma once

#include <ModuleConfiguration.hpp>
#include <core/mw/CoreModule.hpp>
#include <core/os/IOChannel.hpp>
#include "shell.h"


class Module:
   public core::mw::CoreModule
{
public:
// --- DEVICES ----------------------------------------------------------------
// ----------------------------------------------------------------------------

   static core::os::IOChannel& stream;
   static core::os::IOChannel& serial;

   static bool
   initialize();

   static void
   shell(
      const ShellCommand* commands
   );


   Module();
   virtual ~Module() {}
};
