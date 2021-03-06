/* COPYRIGHT (c) 2016-2018 Nova Labs SRL
 *
 * All rights reserved. All use of this software and documentation is
 * subject to the License Agreement located in the file LICENSE.
 */

#include <core/snippets/CortexMxFaultHandlers.h>

#include <core/mw/Middleware.hpp>
#include <core/mw/transport/RTCANTransport.hpp>
#if CORE_USE_BRIDGE_MODE
  #include <core/mw/transport/DebugTransport.hpp>
#endif

#include "ch.h"
#include "hal.h"
//#include "usbcfg.h"

#include <core/hw/GPIO.hpp>
#include <core/hw/SD.hpp>
#include <core/hw/SDU.hpp>
#include <core/hw/UID.hpp>
#include <core/hw/IWDG.hpp>
#include <core/os/Thread.hpp>
#include <Module.hpp>
#include "chprintf.h"

#include <core/snippets/CortexMxFaultHandlers.h>

// LED
using LED_PAD = core::hw::Pad_<core::hw::GPIO_C, 13>;
static LED_PAD _led;

#if CORE_USE_BRIDGE_MODE
static char dbgtra_namebuf[64];
static core::mw::DebugTransport      dbgtra("SDU_1", reinterpret_cast<BaseChannel*>(core::hw::SDU_1::driver), dbgtra_namebuf);
static core::os::Thread::Stack<2048> debug_transport_rx_stack;
static core::os::Thread::Stack<2048> debug_transport_tx_stack;

core::hw::SDU _sdu;

#else

// SERIAL DEVICES
using SDU_1_STREAM = core::os::SDChannelTraits<core::hw::SDU_1>;
using SD_3_STREAM  = core::os::SDChannelTraits<core::hw::SD_3>;
using STREAM       = core::os::IOChannel_<SDU_1_STREAM, core::os::IOChannel::DefaultTimeout::INFINITE>;
using SERIAL       = core::os::IOChannel_<SD_3_STREAM, core::os::IOChannel::DefaultTimeout::INFINITE>;
static STREAM _stream;
static SERIAL _serial;

// MODULE DEVICES
core::hw::SDU _sdu;
core::os::IOChannel& Module::stream = _stream;
core::os::IOChannel& Module::serial = _serial;


static ShellConfig usb_shell_cfg = {
    reinterpret_cast<BaseSequentialStream*>(_stream.rawChannel()), nullptr
};
#endif // if CORE_USE_BRIDGE_MODE

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

// SYSTEM STUFF
static core::os::Thread::Stack<1024> management_thread_stack;
static core::mw::RTCANTransport      rtcantra(&RTCAND1);

RTCANConfig rtcan_config = {
    1000000, 100, 60
};

#if CORE_IS_BOOTLOADER_BRIDGE
core::mw::bootloader::MessageType _bootMasterMessageType = core::mw::bootloader::MessageType::MASTER_ADVERTISE;

#if 0
void
Module::setBootloaderMasterType(
    core::mw::bootloader::MessageType type
)
{
    _bootMasterMessageType = type;
}
#endif

void
bootloader_master_node(
    void* arg
)
{
    core::mw::Node node("bootmaster");
    core::mw::Publisher<core::mw::bootloader::BootMasterMsg> pub;
    core::mw::bootloader::BootMasterMsg* msgp;

    node.advertise(pub, BOOTLOADER_MASTER_TOPIC_NAME, core::os::Time::INFINITE);

    (void)arg;
    chRegSetThreadName("bootmaster");

    while (true) {
        if (pub.alloc(msgp)) {
            msgp->command    = _bootMasterMessageType;
            msgp->sequenceId = 0;

            msgp->data[0] = 0;
            msgp->data[1] = 0;
            msgp->data[2] = 0;
            msgp->data[3] = 0;
            msgp->data[4] = 0;
            msgp->data[5] = 0;

            pub.publish_remotely(*msgp);
        }

        core::os::Thread::sleep(core::os::Time::ms(500));
    }
} // bootloader_master_node
#endif // if CORE_IS_BOOTLOADER_BRIDGE


Module::Module()
{}


bool
Module::initialize()
{
#ifdef _DEBUG
    FAULT_HANDLERS_ENABLE(true);
#else
    FAULT_HANDLERS_ENABLE(false);
#endif

    static bool initialized = false;

    FAULT_HANDLERS_ENABLE(false);

    if (!initialized) {
        core::mw::CoreModule::initialize();

        _sdu.setDescriptors(core::hw::SDUDefaultDescriptors::static_callback());
        _sdu.init();
        _sdu.start();

#if CORE_USE_BRIDGE_MODE
#else
        shellInit();
#endif

        /*
         * Initializes a serial-over-USB CDC driver.
         */
        //sduObjectInit(core::hw::SDU_1::driver);
        //sduStart(core::hw::SDU_1::driver, &serusbcfg);
        sdStart(core::hw::SD_3::driver, nullptr);

        /*
         * Activates the USB driver and then the USB bus pull-up on D+.
         * Note, a delay is inserted in order to not have to disconnect the cable
         * after a reset.
         */
        usbDisconnectBus(&USBD1);
        chThdSleepMilliseconds(1500);
        usbStart(&USBD1, _sdu.usbcfg());
        usbConnectBus(&USBD1);

        core::mw::Middleware::instance().initialize(name(), management_thread_stack, management_thread_stack.size(), core::os::Thread::LOWEST);

#if CORE_USE_BRIDGE_MODE
        while (usbGetDriverStateI(&USBD1) != USB_ACTIVE) {
            chThdSleepMilliseconds(1);
        }

        dbgtra.initialize(debug_transport_rx_stack, debug_transport_rx_stack.size(), core::os::Thread::NORMAL,
                          debug_transport_tx_stack, debug_transport_tx_stack.size(), core::os::Thread::NORMAL);
#endif
        rtcantra.initialize(rtcan_config, canID());

        core::mw::Middleware::instance().start();

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
// CoreModule STM32FlashConfigurationStorage
// ----------------------------------------------------------------------------
#include <core/snippets/CoreModuleSTM32FlashConfigurationStorage.hpp>
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// CoreModule HW specific implementation
// ----------------------------------------------------------------------------
#include <core/snippets/CoreModuleHWSpecificImplementation.hpp>
// ----------------------------------------------------------------------------
