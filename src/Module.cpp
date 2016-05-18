/* COPYRIGHT (c) 2016 Nova Labs SRL
 *
 * All rights reserved. All use of this software and documentation is
 * subject to the License Agreement located in the file LICENSE.
 */
 
#include <Core/MW/Middleware.hpp>

#include "ch.h"
#include "hal.h"
#include "usbcfg.h"

#include <Core/HW/GPIO.hpp>
#include <Core/MW/Thread.hpp>
#include <Module.hpp>


using LED_PAD = Core::HW::Pad_<Core::HW::GPIO_C, LED_PIN>;
static LED_PAD _led;

// TODO: Move SDU stuff in Core::HW - as it is not a top priority task it is left here
template <std::size_t N>
struct SDUTraits {};

template <>
struct SDUTraits<1> {
   static constexpr auto device = &SDU1;
};

template <class _SDU>
struct SDUStreamTraits {
   static constexpr auto stream = (BaseSequentialStream*)_SDU::device;
};

#ifdef CORE_USE_BRIDGE_MODE
using SDU_1 = SDUTraits<1>;
static char dbgtra_namebuf[64];
static Core::MW::DebugTransport dbgtra("SD2", reinterpret_cast<BaseChannel*>(SDU_1::device), dbgtra_namebuf);
static THD_WORKING_AREA(wa_rx_dbgtra, 1024);
static THD_WORKING_AREA(wa_tx_dbgtra, 1024);
#else
using SDU_1        = SDUTraits<1>;
using SDU_1_STREAM = SDUStreamTraits<SDU_1>;
using STREAM       = Core::MW::IOStream_<SDU_1_STREAM>;

static STREAM       _stream;
Core::MW::IOStream& Module::stream = _stream;


static ShellConfig usb_shell_cfg = {
   SDU_1_STREAM::stream, nullptr
};
#endif // ifdef CORE_USE_BRIDGE_MODE

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

static THD_WORKING_AREA(wa_info, 1024);
static Core::MW::RTCANTransport rtcantra(RTCAND1);

RTCANConfig rtcan_config = {
   1000000, 100, 60
};

#ifndef CORE_MODULE_NAME
#define CORE_MODULE_NAME "USB"
#endif

#ifdef CORE_USE_BRIDGE_MODE
enum {
   PUBSUB_BUFFER_LENGTH = 16
};

Core::MW::Middleware::PubSubStep pubsub_buf[PUBSUB_BUFFER_LENGTH];
Core::MW::Middleware Core::MW::Middleware::instance(CORE_MODULE_NAME, "BOOT_" CORE_MODULE_NAME, pubsub_buf, PUBSUB_BUFFER_LENGTH);
#else
Core::MW::Middleware Core::MW::Middleware::instance(CORE_MODULE_NAME, "BOOT_" CORE_MODULE_NAME);
#endif

Module::Module()
{}

bool
Module::initialize()
{
//	CORE_ASSERT(Core::MW::Middleware::instance.is_stopped()); // TODO: capire perche non va...

   static bool initialized = false;

   if (!initialized) {
      halInit();
      chSysInit();

      /*
       * Initializes a serial-over-USB CDC driver.
       */
      sduObjectInit(SDU_1::device);
      sduStart(SDU_1::device, &serusbcfg);

      /*
       * Activates the USB driver and then the USB bus pull-up on D+.
       * Note, a delay is inserted in order to not have to disconnect the cable
       * after a reset.
       */
      usbDisconnectBus(serusbcfg.usbp);
      chThdSleepMilliseconds(500);
      usbStart(serusbcfg.usbp, &usbcfg);
      usbConnectBus(serusbcfg.usbp);

      Core::MW::Middleware::instance.initialize(wa_info, sizeof(wa_info), Core::MW::Thread::LOWEST);

#ifdef CORE_USE_BRIDGE_MODE
      dbgtra.initialize(wa_rx_dbgtra, sizeof(wa_rx_dbgtra), Core::MW::Thread::LOWEST,
                        wa_tx_dbgtra, sizeof(wa_tx_dbgtra), Core::MW::Thread::LOWEST);
#endif
      rtcantra.initialize(rtcan_config);

      Core::MW::Middleware::instance.start();

      initialized = true;
   }

   if (initialized) {
      CoreModule::disableBootloader();
   }

   return initialized;
} // Board::initialize

#ifdef CORE_USE_BRIDGE_MODE
#else
void
Module::shell(
   const ShellCommand* commands
)
{
   usb_shell_cfg.sc_commands = commands;

   if (!usb_shelltp && (SDU_1::device->config->usbp->state == USB_ACTIVE)) {
      usb_shelltp = shellCreate(&usb_shell_cfg, SHELL_WA_SIZE, NORMALPRIO);
   } else if (chThdTerminatedX(usb_shelltp)) {
      chThdRelease(usb_shelltp); /* Recovers memory of the previous shell.   */
      usb_shelltp = NULL; /* Triggers spawning of a new shell.        */
   }
}
#endif

// Leftover from CoreBoard (where LED_PAD cannot be defined
void
Core::MW::CoreModule::Led::toggle()
{
   _led.toggle();
}

void
Core::MW::CoreModule::Led::write(
   unsigned on
)
{
   _led.write(on);
}
