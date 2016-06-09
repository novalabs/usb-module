#include <Configuration.hpp>
#include <Module.hpp>

// --- MESSAGES ---------------------------------------------------------------
#include <common_msgs/Led.hpp>

// --- NODES ------------------------------------------------------------------
#include <led/Subscriber.hpp>
#include <led/Publisher.hpp>

// --- BOARD IMPL -------------------------------------------------------------

// --- MISC -------------------------------------------------------------------

// *** DO NOT MOVE ***
Module module;

// --- NODES ------------------------------------------------------------------
led::Subscriber led_subscriber("led_subscriber", Core::MW::Thread::PriorityEnum::LOWEST);
led::Publisher  led_publisher("led_publisher");

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

		led_publisher.configuration["topic"] = "led";
		led_publisher.configuration["led"]   = (uint32_t)1;

		led_subscriber.configuration["topic"] = led_publisher.configuration["topic"];

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

			module.shell(commands); // (Re)Spawn shell as needed...

			Core::MW::Thread::sleep(Core::MW::Time::ms(500));
		}

		return Core::MW::Thread::OK;
	} // main
}
