/* COPYRIGHT (c) 2016-2017 Nova Labs SRL
 *
 * All rights reserved. All use of this software and documentation is
 * subject to the License Agreement located in the file LICENSE.
 */

#include <core/mw/Middleware.hpp>
#include <core/mw/transport/RTCANTransport.hpp>
#if CORE_USE_BRIDGE_MODE
	#include <core/mw/transport/DebugTransport.hpp>
#endif

#include "ch.h"
#include "hal.h"
#include "usbcfg.h"

#include <core/hw/GPIO.hpp>
#include <core/hw/SD.hpp>
#include <core/hw/SDU.hpp>
#include <core/hw/UID.hpp>
#include <core/os/Thread.hpp>
#include <Module.hpp>
#include "chprintf.h"

//core::mw::Middleware& Module::mw = core::mw::Middleware::instance;

using LED_PAD = core::hw::Pad_<core::hw::GPIO_C, 13>;
static LED_PAD _led;

#if CORE_USE_BRIDGE_MODE
static char dbgtra_namebuf[64];
static core::mw::DebugTransport dbgtra("SD3", reinterpret_cast<BaseChannel*>(core::hw::SD_3::driver), dbgtra_namebuf);
static core::os::Thread::Stack<2048> debug_transport_rx_stack;
static core::os::Thread::Stack<2048> debug_transport_tx_stack;
#else
using SDU_1_STREAM = core::os::SDChannelTraits<core::hw::SDU_1>;
using SD_3_STREAM  = core::os::SDChannelTraits<core::hw::SD_3>;

using STREAM = core::os::IOChannel_<SDU_1_STREAM, core::os::IOChannel::DefaultTimeout::INFINITE>;
using SERIAL = core::os::IOChannel_<SD_3_STREAM, core::os::IOChannel::DefaultTimeout::INFINITE>;

static STREAM        _stream;
core::os::IOChannel& Module::stream = _stream;

static SERIAL        _serial;
core::os::IOChannel& Module::serial = _serial;

static ShellConfig usb_shell_cfg = {
   reinterpret_cast<BaseSequentialStream*>(_stream.rawChannel()), nullptr
};
#endif // if CORE_USE_BRIDGE_MODE

//core::os::IOChannel_<core::mw::SDChannelTraits<core::hw::SD_3>, core::os::IOChannel::DefaultTimeout::IMMEDIATE> _cazzo;

#define SHELL_WA_SIZE   THD_WORKING_AREA_SIZE(2048)
thread_t* usb_shelltp = NULL;

void
usb_lld_disconnect_bus(
   USBDriver* usbp
)
{
   (void)usbp;
   palClearPort(GPIOA, (1 << GPIOA_USB_DM) | (1 << GPIOA_USB_DP));
   palSetPadMode(GPIOA, GPIOA_USB_DM, PAL_MODE_OUTPUT_PUSHPULL);
   palSetPadMode(GPIOA, GPIOA_USB_DP, PAL_MODE_OUTPUT_PUSHPULL);
}

void
usb_lld_connect_bus(
   USBDriver* usbp
)
{
   (void)usbp;
   palClearPort(GPIOA, (1 << GPIOA_USB_DM) | (1 << GPIOA_USB_DP));
   palSetPadMode(GPIOA, GPIOA_USB_DM, PAL_MODE_ALTERNATE(14));
   palSetPadMode(GPIOA, GPIOA_USB_DP, PAL_MODE_ALTERNATE(14));
}

static core::mw::RTCANTransport      rtcantra(&RTCAND1);
static core::os::Thread::Stack<2048> management_thread_stack;

RTCANConfig rtcan_config = {
   1000000, 100, 60
};

#ifndef CORE_MODULE_NAME
#define CORE_MODULE_NAME "USB"
#endif

#if CORE_USE_BRIDGE_MODE
enum {
   PUBSUB_BUFFER_LENGTH = 16
};

core::mw::Middleware::PubSubStep pubsub_buf[PUBSUB_BUFFER_LENGTH];
core::mw::Middleware core::mw::Middleware::instance(CORE_MODULE_NAME, "BOOT_" CORE_MODULE_NAME, pubsub_buf, PUBSUB_BUFFER_LENGTH);
#else
core::mw::Middleware core::mw::Middleware::instance(CORE_MODULE_NAME, "BOOT_" CORE_MODULE_NAME);
#endif

#if CORE_IS_BOOTLOADER_BRIDGE
core::mw::BootMasterMsg::MessageType _bootMasterMessageType = core::mw::BootMasterMsg::MessageType::ADVERTISE;

void Module::setBootloaderMasterType(core::mw::BootMasterMsg::MessageType type) {
	 _bootMasterMessageType = type;
}

void
bootloader_master_node(
		void* arg
)
{
	core::mw::Node node("bootmaster");
	core::mw::Publisher<core::mw::BootMasterMsg> pub;
	core::mw::BootMasterMsg* msgp;

	node.advertise(pub, BOOTLOADER_MASTER_TOPIC_NAME, core::os::Time::INFINITE);

	(void)arg;
	chRegSetThreadName("bootmaster");

	while(true) {
			if (pub.alloc(msgp)) {
				msgp->type = _bootMasterMessageType;
				pub.publish_remotely(*msgp);
			}

		core::os::Thread::sleep(core::os::Time::ms(500));

	}
}
#endif


Module::Module()
{}


bool
Module::initialize()
{
//	CORE_ASSERT(core::mw::Middleware::instance.is_stopped()); // TODO: capire perche non va...

   static bool initialized = false;

   if (!initialized) {
      halInit();
      chSysInit();

      /*
       * Initializes a serial-over-USB CDC driver.
       */
      sduObjectInit(core::hw::SDU_1::driver);
      sduStart(core::hw::SDU_1::driver, &serusbcfg);
      sdStart(core::hw::SD_3::driver, nullptr);

      /*
       * Activates the USB driver and then the USB bus pull-up on D+.
       * Note, a delay is inserted in order to not have to disconnect the cable
       * after a reset.
       */
      usbDisconnectBus(serusbcfg.usbp);
      chThdSleepMilliseconds(1500);
      usbStart(serusbcfg.usbp, &usbcfg);
      usbConnectBus(serusbcfg.usbp);

      core::mw::Middleware::instance.initialize(management_thread_stack, management_thread_stack.size(), core::os::Thread::LOWEST);

#if CORE_USE_BRIDGE_MODE
      dbgtra.initialize(debug_transport_rx_stack, debug_transport_rx_stack.size(), core::os::Thread::NORMAL,
    		  	  	    debug_transport_tx_stack, debug_transport_tx_stack.size(), core::os::Thread::NORMAL);
#endif
      rtcantra.initialize(rtcan_config);

      core::mw::Middleware::instance.start();

#if CORE_IS_BOOTLOADER_BRIDGE
  	  core::os::Thread::create_heap(NULL, 1024, core::os::Thread::PriorityEnum::NORMAL, bootloader_master_node, nullptr);
#endif

      initialized = true;
   }

   if (initialized) {
      CoreModule::disableBootloader();
   }

   return initialized;
} // Board::initialize

#if CORE_USE_BRIDGE_MODE
#else
void
Module::shell(
   const ShellCommand* commands
)
{
   usb_shell_cfg.sc_commands = commands;

   if (!usb_shelltp && (core::hw::SDU_1::driver->config->usbp->state == USB_ACTIVE)) {
      usb_shelltp = shellCreate(&usb_shell_cfg, SHELL_WA_SIZE, NORMALPRIO);
   } else if (chThdTerminatedX(usb_shelltp)) {
      chThdRelease(usb_shelltp); /* Recovers memory of the previous shell.   */
      usb_shelltp = NULL; /* Triggers spawning of a new shell.        */
   }
}
#endif

// ----------------------------------------------------------------------------
// CoreModule HW specific implementation
// ----------------------------------------------------------------------------

void
core::mw::CoreModule::Led::toggle()
{
    _led.toggle();
}

void
core::mw::CoreModule::Led::write(
    unsigned on
)
{
    _led.write(on);
}

void
core::mw::CoreModule::reset()
{
}

void
core::mw::CoreModule::keepAlive()
{
}

void
core::mw::CoreModule::disableBootloader()
{
}

void
core::mw::CoreModule::enableBootloader()
{
}

