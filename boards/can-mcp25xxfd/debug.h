/*
 * Copyright (c) 2025-2026, BlackBerry Limited. All rights reserved.
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
#ifndef MCP25xxFD_DEBUG_H_
#define MCP25xxFD_DEBUG_H_

#include <stdint.h>

#include "state.h"

struct dbuffer {
    char *orig;
    char *cur;
    size_t size;
};
typedef struct dbuffer dbuffer_t;

#define DBUFFER_INIT(p, s) { .orig = p, .cur = p, .size = s }
#define DBUFFER_ALLOCA(name, size) char db_buffer_##__LINE__[size]; dbuffer_t name = DBUFFER_INIT(db_buffer_##__LINE__, size)

static inline dbuffer_t* mcp25xxfd_dbuffer_init(dbuffer_t *db, char *p, size_t s) {
    db->orig = p;
    db->cur = p;
    db->size = s;
    return db;
}

int mcp25xxfd_dbuffer_append(dbuffer_t *db, const char *fmt, ...);

int mcp25xxfd_debug_dump_specific_regs(mcp25xxfd_dev_t *ifc, dbuffer_t *db);
int mcp25xxfd_debug_dump_controller_regs(mcp25xxfd_dev_t *ifc, dbuffer_t *db);
int mcp25xxfd_debug_dump_controller_tef_regs(mcp25xxfd_dev_t *ifc, dbuffer_t *db);
int mcp25xxfd_debug_dump_controller_txq_regs(mcp25xxfd_dev_t *ifc, dbuffer_t *db);
int mcp25xxfd_debug_dump_controller_fifo_regs(mcp25xxfd_dev_t *ifc, dbuffer_t *db);
int mcp25xxfd_debug_dump_controller_filter_regs(mcp25xxfd_dev_t *ifc, dbuffer_t *db);

#endif // MCP25xxFD_DEBUG_H_
