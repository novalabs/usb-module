#include <ModuleConfiguration.hpp>
#include <Module.hpp>

// --- MESSAGES ---------------------------------------------------------------
#include <core/common_msgs/Led.hpp>

// --- NODES ------------------------------------------------------------------
#include <core/led/Subscriber.hpp>
#include <core/led/Publisher.hpp>

// --- BOARD IMPL -------------------------------------------------------------

// --- MISC -------------------------------------------------------------------

// *** DO NOT MOVE ***
Module module;

// --- NODES ------------------------------------------------------------------
core::led::Subscriber led_subscriber("led_subscriber", core::os::Thread::PriorityEnum::LOWEST);
core::led::Publisher  led_publisher("led_publisher");

/*
 * Application entry point.
 */
extern "C" {
   int
   main(
      void
   )
   {
      module.initialize();

      // Led publisher node
      core::led::PublisherConfiguration led_publisher_configuration;
      led_publisher_configuration.topic = "led";
      led_publisher_configuration.led   = 1;
      led_publisher.setConfiguration(led_publisher_configuration);
      module.add(led_publisher);

      // Led subscriber node
      core::led::SubscriberConfiguration led_subscriber_configuration;
      led_subscriber_configuration.topic = "led";
      led_subscriber.setConfiguration(led_subscriber_configuration);
      module.add(led_subscriber);

      // Add nodes to the node manager (== board)...
      module.add(led_subscriber);
      module.add(led_publisher);

      // ... and let's play!
      module.setup();
      module.run();

      // Is everything going well?
      for (;;) {
         if (!module.isOk()) {
            module.halt("This must not happen!");
         }

         core::os::Thread::sleep(core::os::Time::ms(500));
      }

      return core::os::Thread::OK;
   } // main
}
