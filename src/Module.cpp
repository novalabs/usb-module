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

using SDU_1        = SDUTraits<1>;
using SDU_1_STREAM = SDUStreamTraits<SDU_1>;
using STREAM       = Core::MW::IOStream_<SDU_1_STREAM>;

static STREAM       _stream;
Core::MW::IOStream& Module::stream = _stream;


static ShellConfig usb_shell_cfg = {
	SDU_1_STREAM::stream, nullptr
};

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

Core::MW::Middleware Core::MW::Middleware::instance(CORE_MODULE_NAME, "BOOT_" CORE_MODULE_NAME);

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
		rtcantra.initialize(rtcan_config);
		Core::MW::Middleware::instance.start();

		initialized = true;
	}

	if(initialized) {
		CoreModule::disableBootloader();
	}

	return initialized;
} // Board::initialize

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
