/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */

#include <util.h>
#include <json.h>
#include <jsonrpc.h>
#include <vtep-idl.h>
#include <ovsdb-idl.h>

struct ovsdb_idl *
ovsdb_api_idl_create() {
    vteprec_init();
    return ovsdb_idl_create(NULL, &vteprec_idl_class, true, false);
}

const struct vteprec_global *
ovsdb_api_vteprec_global_first(struct ovsdb_idl *idl) {
    return vteprec_global_first(idl);
}

/*
void
ovsdb_api_idl_callback_1(void *idl_base, int op, struct ovsdb_idl_row *row) {
    if (row->table->class->columns == vteprec_logical_switch_columns) {
        struct vteprec_logical_switch *ls = row ? CONTAINER_OF(row, struct vteprec_logical_switch, header_) : NULL;
        printf("Received %s for Logical Switch %s VxLAN %ld\n",
                op == 0 ? "Delete" : "Add/Change", ls->name, *ls->tunnel_key);
    }
}
*/

int
ovsdb_api_row_type(struct ovsdb_idl_row *row) {
    if (row->table->class->columns == vteprec_physical_switch_columns) {
        return 0;
    } else if (row->table->class->columns == vteprec_logical_switch_columns) {
        return 1;
    }
    return 100;
}

bool
ovsdb_api_msg_echo_req(struct jsonrpc_msg *msg) {
    return (msg->type == JSONRPC_REQUEST && !strcmp(msg->method, "echo"));
}

bool
ovsdb_api_msg_echo_reply(struct jsonrpc_msg *msg) {
    return (msg->type == JSONRPC_REPLY && msg->id &&
            msg->id->type == JSON_STRING && !strcmp(msg->id->u.string, "echo"));
}

struct jsonrpc_msg *
ovsdb_api_jsonrpc_create_reply(struct jsonrpc_msg *msg) {
    return jsonrpc_create_reply(json_clone(msg->params), msg->id);
}

char *
ovsdb_api_physical_switch_name(struct ovsdb_idl_row *row) {
    struct vteprec_physical_switch *ps = row ? CONTAINER_OF(row, struct vteprec_physical_switch, header_) : NULL;
    return ps->name;
}

char *
ovsdb_api_logical_switch_name(struct ovsdb_idl_row *row) {
    struct vteprec_logical_switch *ls = row ? CONTAINER_OF(row, struct vteprec_logical_switch, header_) : NULL;
    return ls->name;
}

int64_t
ovsdb_api_logical_switch_tunnel_key(struct ovsdb_idl_row *row) {
    struct vteprec_logical_switch *ls = row ? CONTAINER_OF(row, struct vteprec_logical_switch, header_) : NULL;
    return *ls->tunnel_key;
}

void
ovsdb_api_add_logical_switch(struct ovsdb_idl_txn *txn, const char *name,
        int64_t vxlan) {
    struct vteprec_logical_switch *ls = vteprec_logical_switch_insert(txn);
    vteprec_logical_switch_set_name(ls, name);
    vteprec_logical_switch_set_tunnel_key(ls, &vxlan, 1);
}

