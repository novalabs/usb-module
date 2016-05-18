/* COPYRIGHT (c) 2016 Nova Labs SRL
 *
 * All rights reserved. All use of this software and documentation is
 * subject to the License Agreement located in the file LICENSE.
 */
 
#pragma once

#include <Core/MW/Time.hpp>

namespace Configuration {
// --- CONSTANTS --------------------------------------------------------------
   static const Core::MW::Time PUBLISHER_RETRY_DELAY = Core::MW::Time::ms(500);
   static const Core::MW::Time SUBSCRIBER_SPIN_TIME  = Core::MW::Time::ms(2000);

   static const std::size_t SUBSCRIBER_QUEUE_LENGTH = 5;
}
