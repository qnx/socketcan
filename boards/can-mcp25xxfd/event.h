/**
 * Copyright (c) 2026, BlackBerry Limited. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MCP25xxFD_EVENT_H_
#define MCP25xxFD_EVENT_H_

#include "state.h"

int can_register_event(mcp25xxfd_dev_t *dev);

/**
 * Shut down the interrupt-service thread started by can_register_event: sends
 * it a shutdown pulse, waits for it to exit, and detaches the GPIO interrupt.
 * A no-op if can_register_event was never called (or already unregistered).
 */
void can_unregister_event(mcp25xxfd_dev_t *dev);

#endif // MCP25xxFD_EVENT_H_