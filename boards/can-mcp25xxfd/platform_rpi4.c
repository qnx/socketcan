/*
 * Copyright (c) 2025, BlackBerry Limited. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <sys/mman.h>

#include <hw/libsockcan.h>
#include <sockcan_debug.h>

#include "platform.h"

// These #defines are copied from the BSP's bcm2711.h
#define BCM2711_GPIO_BASE                   0xfe200000
#define BCM2711_GPIO_SIZE                   0x100
#define BCM2711_GPIO_FSEL0                  (0x00)
#define BCM2711_GPIO_FSEL1                  (0x04)
#define BCM2711_GPIO_FSEL2                  (0x08)
#define BCM2711_GPIO_FSEL3                  (0x0c)
#define BCM2711_GPIO_FSEL4                  (0x10)
#define BCM2711_GPIO_FSEL5                  (0x14)
#define BCM2711_GPIO_SET0                   (0x1c)
#define BCM2711_GPIO_SET1                   (0x20)
#define BCM2711_GPIO_CLR0                   (0x28)
#define BCM2711_GPIO_CLR1                   (0x2c)
#define BCM2711_GPIO_LEV0                   (0x34)
#define BCM2711_GPIO_LEV1                   (0x38)
#define BCM2711_GPIO_EDS0                   (0x40)
#define BCM2711_GPIO_EDS1                   (0x44)
#define BCM2711_GPIO_REN0                   (0x4c)
#define BCM2711_GPIO_REN1                   (0x50)
#define BCM2711_GPIO_FEN0                   (0x58)
#define BCM2711_GPIO_FEN1                   (0x5c)
#define BCM2711_GPIO_HEN0                   (0x64)
#define BCM2711_GPIO_HEN1                   (0x68)
#define BCM2711_GPIO_LEN0                   (0x70)
#define BCM2711_GPIO_LEN1                   (0x74)
#define BCM2711_GPIO_AREN0                  (0x7c)
#define BCM2711_GPIO_AREN1                  (0x80)
#define BCM2711_GPIO_AFEN0                  (0x88)
#define BCM2711_GPIO_AFEN1                  (0x8c)
#define BCM2711_GPIO_PULL0                  (0xe4)
#define BCM2711_GPIO_PULL1                  (0xe8)
#define BCM2711_GPIO_PULL2                  (0xec)
#define BCM2711_GPIO_PULL3                  (0xf0)
#define BCM2711_GPIO_IRQ0                   (96+49)
#define BCM2711_GPIO_IRQ1                   (96+50)
#define BCM2711_GPIO_IRQ2                   (96+51)
#define BCM2711_GPIO_IRQ3                   (96+52)

#define BCM2711_GPIO_MIN        0
#define BCM2711_GPIO_MAX        27

_Static_assert(BCM2711_GPIO_MIN == 0, "GPIO asserts at runtime assume BCM2711_GPIO_MIN == 0");
_Static_assert(BCM2711_GPIO_MAX <= 27, "Code only supports GPIOs <= 27");

static volatile uint32_t *Rpi4Regs = NULL;

static int rpi4_initialize(void) {
    int rc;

    if (Rpi4Regs != NULL) {
        // Already initialized.
        return EOK;
    }

    Rpi4Regs = mmap(0, __PAGESIZE, PROT_NOCACHE|PROT_READ|PROT_WRITE, MAP_PHYS | MAP_SHARED, NOFD, BCM2711_GPIO_BASE);
    if (Rpi4Regs == MAP_FAILED) {
        rc = errno;
        Rpi4Regs = NULL;
        CAN_LOG_ERR("Unable to map RPI4's GPIO registers. Error=%s", strerror(rc));
        return rc;
    }

    return EOK;
}

static void rpi4_deinitialize(void) {
    if (Rpi4Regs != NULL) {
        munmap((void*)Rpi4Regs, __PAGESIZE);
        Rpi4Regs = NULL;
    }
}

// Clear the interrupt bit
static void rpi4_gpio_irq_clear(uint32_t gpio) {
    assert(gpio <= BCM2711_GPIO_MAX);

    Rpi4Regs[BCM2711_GPIO_EDS0 / 4] = (1 << gpio);
}

static int rpi4_setup_gpio(uint32_t gpio) {
    int reg;
    int shift;

    // Before I do anything, make sure the gpio I want is valid for this platform.
    if (gpio > BCM2711_GPIO_MAX) {
        CAN_LOG_ERR("GPIO %d is out of range for RPI4. Must be between %d and %d.",
               gpio, BCM2711_GPIO_MIN, BCM2711_GPIO_MAX);
        return EINVAL;
    }
    // Program gpio to be an pulled-up input with a low level trigger'ed interrupt
    // Function (want input GPIO = 0b000)
    reg = (BCM2711_GPIO_FSEL0 + (gpio / 10)) / 4;
    shift = (gpio % 10) * 3;
    Rpi4Regs[reg] = (Rpi4Regs[reg] & ~(0x7 << shift));
    // Pull-up (0b01)
    reg = (BCM2711_GPIO_PULL0 + (gpio / 16)) / 4;
    shift = (gpio % 16) * 2;
    Rpi4Regs[reg] = (Rpi4Regs[reg] & ~(0x3 << shift)) | (0x01 << shift);
    // low-level trigger'ed interrupt. Disables the other types of interrupts
    // for this GPIO.
    Rpi4Regs[BCM2711_GPIO_REN0/4] &= ~(1 << gpio);
    Rpi4Regs[BCM2711_GPIO_FEN0/4] &= ~(1 << gpio);
    Rpi4Regs[BCM2711_GPIO_HEN0/4] &= ~(1 << gpio);
    Rpi4Regs[BCM2711_GPIO_LEN0/4] |= 1 << gpio;

    // Clear an interrupt event if there was one.
    // If it was legit, it will be raised again as the level from the MCP2515
    // will still be low.
    rpi4_gpio_irq_clear(gpio);

    return EOK;
}


static int rpi4_gpio_irq_get(uint32_t gpio) {
    // Get the IRQ associated with the gpio
    assert(gpio <= BCM2711_GPIO_MAX);

    // On the RPI4, GPIO interrupt's are routed via four lines, which are then
    // exposed via a specific interrupt in the GIC-400 from the video-core block.
    // The BCM2711_GPIO_IRQ* #defines are the actual GIC-400 interrupt number
    // (as seen by the processor).
    // GPIO_IRQ0 is for bank0 GPIOs (0-27)
    // GPIO_IRQ1 is for bank1 GPIOs (28-45)
    // GPIO_IRQ2 is for bank1 GPIOs (46-57)
    // GPIO_IRQ3 is for ALL GPIOs
    //
    // We only support GPIOS 0-27 so only need IRQ0
    return BCM2711_GPIO_IRQ0;
}

// Check if the GPIO's interrupt is 'active'
static int rpi4_gpio_irq_active(uint32_t gpio) {
    int bit;

    assert(gpio <= BCM2711_GPIO_MAX);

    bit = Rpi4Regs[BCM2711_GPIO_EDS0 / 4] & (1 << gpio);
    return bit != 0;
}

const mcp25xxfd_platform_t Mcp25xxfdPlatformRpi4 = {
    .initialize = rpi4_initialize,
    .deinitialize = rpi4_deinitialize,
    .setup_gpio = rpi4_setup_gpio,
    .gpio_irq_get = rpi4_gpio_irq_get,
    .gpio_irq_active = rpi4_gpio_irq_active,
    .gpio_irq_clear = rpi4_gpio_irq_clear
};
