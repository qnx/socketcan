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
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "platform.h"

#define MAX_GPIOS 16

typedef struct gpio_state {
    uint32_t gpio;

    bool used;
    bool active;
} gpio_state_t;

static bool Initialized = false;
static gpio_state_t States[MAX_GPIOS];

static int generic_initialize(void) {
    if (!Initialized) {
        memset(States, 0, sizeof(States));
        Initialized = true;
    }

    return EOK;
}

static void generic_deinitialize(void) {
    Initialized = false;
}

// Clear the interrupt bit
static void generic_gpio_irq_clear(uint32_t gpio) {
    // Nothing to do. Interrupt is managed by the kernel and the
    // sockcan library will automatically call InterruptUnmask()
    // for us.
    //
    // However use this as a kick to set the 'active' state of
    // a gpio back to true. In theory, this means that the next
    // call to generic_gpio_irq_active will do the right thing.
    for (int i = 0; i < MAX_GPIOS; ++i) {
        if (States[i].used && States[i].gpio == gpio) {
            States[i].active = true;
            break;
        }
    }
}

static int generic_setup_gpio(uint32_t gpio) {
    int idx = -1;

    for (int i = 0; i < MAX_GPIOS; ++i) {
        if (!States[i].used) {
            idx = i;
            // Keep looking to make sure I've not already set this one up
        } else if (States[i].gpio == gpio) {
            // Already set up. Re/use this one.
            idx = i;
            break;
        }
    }
    if (idx == -1) {
        // Too many GPIOs
        return ENOMEM;
    }
    States[idx].gpio = gpio;
    States[idx].used = true;
    States[idx].active = true;
    return EOK;
}


static int generic_gpio_irq_get(uint32_t gpio) {
    // Assume gpio == irq when this platform is used.
    return (int)gpio;
}

// Check if the GPIO's interrupt is 'active'
static int generic_gpio_irq_active(uint32_t gpio) {
    // I don't have a way to query the kernel to tell if an IRQ
    // is active or not. It doesn't really make sense like it does
    // for a GPIO based IRQ.
    //
    // Instead, assume it is active when this API is first called.
    // It then goes inactive until the 'clear' routine is called.
    //
    // This should get the behaviour I want.
    int active;

    for (int i = 0; i < MAX_GPIOS; ++i) {
        if (States[i].used && States[i].gpio == gpio) {
            active = States[i].active ? 1 : 0;
            States[i].active = false;
            return active;
        }
    }
    // GPIO not known, can't be active
    return 0;
}

const mcp25xxfd_platform_t Mcp25xxfdPlatformGeneric = {
    .initialize = generic_initialize,
    .deinitialize = generic_deinitialize,
    .setup_gpio = generic_setup_gpio,
    .gpio_irq_get = generic_gpio_irq_get,
    .gpio_irq_active = generic_gpio_irq_active,
    .gpio_irq_clear = generic_gpio_irq_clear
};
