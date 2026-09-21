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

/*
 * A GPIO interrupt on the RPI5 goes through several HW blocks before
 * it hits the processor.
 * 1. It starts in the GPIO block on the RP1
 * 2. GPIO interrupts 0-27 (the 'public' ones exposed on the 40-pin connector)
 *    are all banked together in BANK0
 * 3. BANK0's single interrupt is routed over PCI as an MSIX interrupt to the CPU
 * 4. The MIP block in the CPU receives the MSIX interrupt and turns it into
 *    an interrupt to the GIC.
 * 5. The GIC is part of the CPU and manages interrupting the CPU itself.
 *
 * From the Kernel's point of view, only the GIC matters. So everything before
 * that is invisible to it.
 */

#define RPI5_GPIO_MIN                   0
#define RPI5_GPIO_MAX                   27
_Static_assert(RPI5_GPIO_MIN == 0, "GPIO asserts at runtime assume RPI5_GPIO_MIN == 0");
_Static_assert(RPI5_GPIO_MAX <= 27, "Code only supports GPIOs <= 27");

/* The SPI interrupt that MSIX interrupts are mapped to */
#define RPI5_PCIE_MSI_GIC_IRQ_BASE      0x80
/* The SPI interrupts start at interrupt vector 32 so I need to
 * add that the the GIC SPI interrupt to get the actual vector
 * known to the kernel
 */
#define RPI5_RP1_IO_BANK0_IRQ           (RPI5_PCIE_MSI_GIC_IRQ_BASE + 32)

/* GIC Interrupt Configuration Registers */
#define BCM2712_GICD_ADDR               0x107fff9000
#define BCM2712_GICD_SIZE               0x1000
#define GICD_ICFGR_SPI                  0xc08
#define RPI5_IRQ_TO_GICD_ICFGR_SPI_OFFSET(n) ((((n) + RPI5_PCIE_MSI_GIC_IRQ_BASE) / 16) * 4)
#define RPI5_IRQ_TO_GICD_ICFGR_SPI_MASK(n) (2u << ((((n) + RPI5_PCIE_MSI_GIC_IRQ_BASE) % 16) * 2))

/* MIP registers/constants */
#define BCM2712_MIP_ADDR                0x1000130000
#define BCM2712_MIP_SIZE                0xc0
#define MIP_INT_CFGL_HOST               0x20
#define MIP_INT_MASKL_HOST              0x40

/* RP1 related values */
#define RP1_RW_OFFSET                   0x0000
#define RP1_SET_OFFSET                  0x2000
#define RP1_CLR_OFFSET                  0x3000

/* MSIX registers/constants on the RP1, only care about IO_BANK0 */
#define RP1_PCIE_MSIX_ADDR              0x1f00108000
#define RP1_PCIE_MSIX_SIZE              0x4000
#define RP1_PCIE_MSIX_IRQ_0_IO_BANK0    0
#define RP1_PCIE_MSIX_CFG(irq)          (0x08 + ((irq) * 0x04))
/*
 * I don't know about these values, the datasheet implies that they
 * should be 0x0000, 0x2000, and 0x3000 (as above)
 * but a driver in the linux kernel (drivers/mfd/rp1.c) has
 * the different values below
 */
#define RP1_PCIE_MSIX_RW_OFFSET         0x000
#define RP1_PCIE_MSIX_SET_OFFSET        0x800
#define RP1_PCIE_MSIX_CLR_OFFSET        0xC00

#define RP1_PCIE_MSIX_CFG_IACK_EN       (1u << 3)
#define RP1_PCIE_MSIX_CFG_IACK          (1u << 2)
#define RP1_PCIE_MSIX_CFG_ENABLE        (1u << 0)

/* RP1 GPIO base address */
#define RP1_GPIO_ADDR                   0x1f000d0000
#define RP1_GPIO_SIZE                   0x2c000

/* GPIO configuration on the RP1 */
#define RP1_IO_BANK0_OFFSET             0x00000000
#define RP1_GPIO_IO_REG_BASE_OFFSET(gpio)       (RP1_IO_BANK0_OFFSET + ((gpio) * sizeof(uint32_t) * 2))
#define RP1_GPIO_IO_REG_CONTROL_OFFSET(gpio)    (RP1_GPIO_IO_REG_BASE_OFFSET(gpio) + (1 * sizeof(uint32_t)))

/* RP1 IRQ events*/
#define RP1_GPIO_FUNC_RIO                        5       /* ALT_5 is the RIO mode */
#define RP1_GPIO_CTRL_IRQ_RESET         (1u << 28)       /*  Reset interrupts - gpio_ctrl register set to 1 to reset the interrupt edge detector */
#define RP1_GPIO_CTRL_IRQMASK_LEVEL_LOW (1u << 22)       /* Masks low level interrupt into the interrupt output */

/* GPIO Interrupt->MSIX enable/status regs */
#define RP1_GPIO_IO_REG_PCIE_INTE_OFFSET        0x11c
#define RP1_GPIO_IO_REG_PCIE_INTS_OFFSET        0x124

/* GPIO sys_io/RIO configuration on the RP1 */
#define RP1_RIO_BANK0_OFFSET                    0x00010000
#define RP1_GPIO_RIO_REG_OE_OFFSET              0x4

/* GPIO Pad configuration on the RP1 */
#define RP1_PADS_BANK0_OFFSET                   0x00020000
#define RP1_GPIO_PADS_OFFSET(gpio)              (RP1_PADS_BANK0_OFFSET + 4 + sizeof(uint32_t) * (gpio))

#define RP1_PADS_OD_SET                         (1 << 7)
#define RP1_PADS_IE_SET                         (1 << 6)
#define RP1_PADS_PUE_SET                        (1 << 3)

#define REG32(addr) (volatile uint32_t*)(addr)
static uintptr_t Rp1MsixRegs = 0;
static uintptr_t Rp1GpioRegs = 0;
static uintptr_t Rpi5MipRegs = 0;
static uintptr_t Rpi5GicRegs = 0;

static void rpi5_deinitialize(void) {
    if (Rp1GpioRegs != 0) {
        munmap((void*)Rp1GpioRegs, RP1_GPIO_ADDR);
        Rp1GpioRegs = 0;
    }

    if (Rp1MsixRegs != 0) {
        munmap((void*)Rp1MsixRegs, RP1_PCIE_MSIX_SIZE);
        Rp1MsixRegs = 0;
    }

    if (Rpi5MipRegs != 0) {
        munmap((void*)Rpi5MipRegs, BCM2712_MIP_SIZE);
        Rpi5MipRegs = 0;
    }

    if (Rpi5GicRegs != 0) {
        munmap((void*)Rpi5GicRegs, BCM2712_GICD_SIZE);
        Rpi5GicRegs = 0;
    }
}

static int rpi5_initialize(void) {
    int rc;
    void *ptr;

    if (Rpi5GicRegs == 0) {
        ptr = mmap_device_memory(NULL, BCM2712_GICD_SIZE, PROT_NOCACHE|PROT_READ|PROT_WRITE, 0, BCM2712_GICD_ADDR);
        if (ptr != MAP_FAILED) {
            Rpi5GicRegs = (uintptr_t)ptr;
        } else {
            rc = errno;
            CAN_LOG_ERR("Unable to map RPI5's GIC registers. Error=%s", strerror(rc));
            goto exit;
        }
    }

    if (Rpi5MipRegs == 0) {
        ptr = mmap_device_memory(NULL, BCM2712_MIP_SIZE, PROT_NOCACHE|PROT_READ|PROT_WRITE, 0, BCM2712_MIP_ADDR);
        if (ptr != MAP_FAILED) {
            Rpi5MipRegs = (uintptr_t)ptr;
        } else {
            rc = errno;
            CAN_LOG_ERR("Unable to map RPI5's MIP registers. Error=%s", strerror(rc));
            goto exit;
        }
    }

    if (Rp1MsixRegs == 0) {
        ptr = mmap_device_memory(NULL, RP1_PCIE_MSIX_SIZE, PROT_NOCACHE|PROT_READ|PROT_WRITE, 0, RP1_PCIE_MSIX_ADDR);
        if (ptr != MAP_FAILED) {
            Rp1MsixRegs = (uintptr_t)ptr;
        } else {
            rc = errno;
            CAN_LOG_ERR("Unable to map RP1's MSIX registers. Error=%s", strerror(rc));
            goto exit;
        }
    }

    if (Rp1GpioRegs == 0) {
        ptr = mmap_device_memory(NULL, RP1_GPIO_SIZE, PROT_NOCACHE|PROT_READ|PROT_WRITE, 0, RP1_GPIO_ADDR);
        if (ptr != MAP_FAILED) {
            Rp1GpioRegs = (uintptr_t)ptr;
        } else {
            rc = errno;
            CAN_LOG_ERR("Unable to map RP1's GPIO registers. Error=%s", strerror(rc));
            goto exit;
        }
    }

    rc = EOK;
exit:
    if (rc != EOK) {
        // Something went wrong, deinitialize anything that got initialized.
        rpi5_deinitialize();
    }
    return rc;
}

static void rpi5_gpio_irq_clear(uint32_t gpio) {
    uintptr_t regOffset;
    assert(gpio <= RPI5_GPIO_MAX);

    /*
     * First clear the event, this should really only be needed for edge triggered
     * interrupts but is safe to do for level triggered as well.
     */
    regOffset = RP1_GPIO_IO_REG_CONTROL_OFFSET(gpio);
    *REG32(Rp1GpioRegs + RP1_SET_OFFSET + regOffset) = RP1_GPIO_CTRL_IRQ_RESET;

    /*
     * An MSIX interrupt are always edge triggered. This means that a GPIO level
     * triggered interrupt won't refire at the MSIX level until it de-asserts and
     * asserts again. To get around this, there is a specific bit to set that causes
     * the MSIX block to re-check the level and if it is still asserted, fire the
     * MSIX interrupt.
     *
     * This is the IACK bit.
     */
    regOffset = RP1_PCIE_MSIX_CFG(RP1_PCIE_MSIX_IRQ_0_IO_BANK0);
    *REG32(Rp1MsixRegs + RP1_PCIE_MSIX_SET_OFFSET + regOffset) = RP1_PCIE_MSIX_CFG_IACK;
}

static int rpi5_setup_gpio(uint32_t gpio) {
    uint32_t val;

    /* Before I do anything, make sure the gpio I want is valid for this platform. */
    if (gpio > RPI5_GPIO_MAX) {
        CAN_LOG_ERR("GPIO %d is out of range for RPI5. Must be between %d and %d.",
               gpio, RPI5_GPIO_MIN, RPI5_GPIO_MAX);
        return EINVAL;
    }

    /* Make sure the interrupt is edge enabled at the GIC.
     * Masking/Unmasking will be handled by the kernel when the interrupt
     * is attached.
     */
    val = *REG32(Rpi5GicRegs + GICD_ICFGR_SPI + RPI5_IRQ_TO_GICD_ICFGR_SPI_OFFSET(RP1_PCIE_MSIX_IRQ_0_IO_BANK0));
    val |= RPI5_IRQ_TO_GICD_ICFGR_SPI_MASK(RP1_PCIE_MSIX_IRQ_0_IO_BANK0);
    *REG32(Rpi5GicRegs + GICD_ICFGR_SPI + RPI5_IRQ_TO_GICD_ICFGR_SPI_OFFSET(RP1_PCIE_MSIX_IRQ_0_IO_BANK0)) = val;

    /* Make sure the interrupt is enabled, and edge-triggered at the MIP. */
    val = *REG32(Rpi5MipRegs + MIP_INT_CFGL_HOST);
    val |= (1u << RP1_PCIE_MSIX_IRQ_0_IO_BANK0);
    *REG32(Rpi5MipRegs + MIP_INT_CFGL_HOST) = val;
    /* Unmask it for the processor */
    val = *REG32(Rpi5MipRegs + MIP_INT_MASKL_HOST);
    val &= ~(1u << RP1_PCIE_MSIX_IRQ_0_IO_BANK0);
    *REG32(Rpi5MipRegs + MIP_INT_MASKL_HOST) = val;

    /* Enable MSIX on RP1 w/ IACK (for level triggered) */
    val = RP1_PCIE_MSIX_CFG_ENABLE | RP1_PCIE_MSIX_CFG_IACK_EN;
    *REG32(Rp1MsixRegs + RP1_PCIE_MSIX_SET_OFFSET + RP1_PCIE_MSIX_CFG(RP1_PCIE_MSIX_IRQ_0_IO_BANK0)) = val;

    /* Configure the GPIO's PAD for input and pull-up */
    val = RP1_PADS_OD_SET | RP1_PADS_IE_SET | RP1_PADS_PUE_SET;
    *REG32(Rp1GpioRegs + RP1_RW_OFFSET + RP1_GPIO_PADS_OFFSET(gpio)) = val;
    /* Set RIO (peripheral?) for the GPIO to be an input */
    *REG32(Rp1GpioRegs + RP1_CLR_OFFSET + RP1_RIO_BANK0_OFFSET + RP1_GPIO_RIO_REG_OE_OFFSET) = (1u << gpio);
    /* Configure the GPIO itself to use RIO and low level trigger'ed interrupt.
     * The MASK bit here is to mask that particular type of interrupt INTO what is reported,
     * instead of masking it out.
     */
    val = RP1_GPIO_FUNC_RIO | RP1_GPIO_CTRL_IRQMASK_LEVEL_LOW;
    *REG32(Rp1GpioRegs + RP1_RW_OFFSET + RP1_GPIO_IO_REG_CONTROL_OFFSET(gpio)) = val;

    /* Finally enable the interrupt from the GPIO block to MSIX block */
    val = (1u << gpio);
    *REG32(Rp1GpioRegs + RP1_SET_OFFSET + RP1_GPIO_IO_REG_PCIE_INTE_OFFSET) = val;

    /* Clear an interrupt event if there was one.
     * If it was legit, it will be raised again as the level from the MCP25xxFD
     * will still be low.
     */
    rpi5_gpio_irq_clear(gpio);

    return EOK;
}


static int rpi5_gpio_irq_get(uint32_t gpio) {
    assert(gpio <= RPI5_GPIO_MAX);

    /* On the RPI5, every gpio eventually gets banked together into
     * a single interrupt to the processor.
     * IO_BANK0 is for GPIOs 0-27
     */
    return RPI5_RP1_IO_BANK0_IRQ;
}

static int rpi5_gpio_irq_active(uint32_t gpio) {
    /* Check the status from the GPIO block */
    uint32_t value;

    assert(gpio <= RPI5_GPIO_MAX);

    value = *REG32(Rp1GpioRegs + RP1_RW_OFFSET + RP1_GPIO_IO_REG_PCIE_INTS_OFFSET);
    return (value & (1u << gpio)) != 0;
}

const mcp25xxfd_platform_t Mcp25xxfdPlatformRpi5 = {
    .initialize = rpi5_initialize,
    .deinitialize = rpi5_deinitialize,
    .setup_gpio = rpi5_setup_gpio,
    .gpio_irq_get = rpi5_gpio_irq_get,
    .gpio_irq_active = rpi5_gpio_irq_active,
    .gpio_irq_clear = rpi5_gpio_irq_clear
};
