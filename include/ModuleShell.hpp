/* COPYRIGHT (c) 2016 Nova Labs SRL
 *
 * All rights reserved. All use of this software and documentation is
 * subject to the License Agreement located in the file LICENSE.
 */

#pragma once

#include <ModuleConfiguration.hpp>
#include <core/mw/CoreModule.hpp>
#include <core/mw/IOChannel.hpp>
#include "shell.h"


class Module:
   public core::mw::CoreModule
{
public:
// --- DEVICES ----------------------------------------------------------------
// ----------------------------------------------------------------------------

   static core::mw::IOChannel& stream;
   static core::mw::IOChannel& serial;

   static bool
   initialize();

   static void
   shell(
      const ShellCommand* commands
   );


   Module();
   virtual ~Module() {}
};
