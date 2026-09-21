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

#ifndef _DUMMY_H_
#define _DUMMY_H_

/*
 * Interface between the two halves of the can-dummy driver:
 *
 *   dummy_module.c - io-sock/newbus module glue (boilerplate; copy for a new driver)
 *   dummy.c        - the actual driver implementation (what you write)
 *
 * The glue calls candummy_attach() at module load and candummy_detach() at unload.
 */

/* Set up the driver at module load. Returns 0 on success or an errno. */
int candummy_attach(void);

/* Tear the driver down at module unload. */
void candummy_detach(void);

#endif /* _DUMMY_H_ */
