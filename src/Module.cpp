/* COPYRIGHT (c) 2016 Nova Labs SRL
 *
 * All rights reserved. All use of this software and documentation is
 * subject to the License Agreement located in the file LICENSE.
 */

#include <core/mw/Middleware.hpp>
#include <core/mw/transport/RTCANTransport.hpp>

#include "ch.h"
#include "hal.h"
#include "usbcfg.h"

#include <core/hw/GPIO.hpp>
#include <core/hw/SD.hpp>
#include <core/hw/SDU.hpp>
#include <core/hw/UID.hpp>
#include <core/os/Thread.hpp>
#include <Module.hpp>

//core::mw::Middleware& Module::mw = core::mw::Middleware::instance;

using LED_PAD = core::hw::Pad_<core::hw::GPIO_C, LED_PIN>;
static LED_PAD _led;

#if CORE_USE_BRIDGE_MODE
static char dbgtra_namebuf[64];
static core::mw::DebugTransport dbgtra("SD1", reinterpret_cast<BaseChannel*>(core::hw::SD_3::driver), dbgtra_namebuf);
static THD_WORKING_AREA(wa_rx_dbgtra, 1024);
static THD_WORKING_AREA(wa_tx_dbgtra, 1024);
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
      chThdSleepMilliseconds(500);
      usbStart(serusbcfg.usbp, &usbcfg);
      usbConnectBus(serusbcfg.usbp);

      core::mw::Middleware::instance.initialize(management_thread_stack, management_thread_stack.size(), core::os::Thread::LOWEST);

#if CORE_USE_BRIDGE_MODE
      dbgtra.initialize(wa_rx_dbgtra, sizeof(wa_rx_dbgtra), core::mw::Thread::LOWEST,
                        wa_tx_dbgtra, sizeof(wa_tx_dbgtra), core::mw::Thread::LOWEST);
#endif
      rtcantra.initialize(rtcan_config);

      core::mw::Middleware::instance.start();

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

// Leftover from CoreBoard (where LED_PAD cannot be defined
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
