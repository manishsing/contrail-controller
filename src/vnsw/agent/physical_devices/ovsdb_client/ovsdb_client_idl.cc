/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */

#include <assert.h>
#include <cstddef>
#include <string.h>
#include <stdlib.h>

#include <ovsdb_api.h>
#include <ovsdb_client_idl.h>
#include <physical_switch_ovsdb.h>
#include <logical_switch_ovsdb.h>

#include <physical_devices/tables/device_manager.h>

namespace AGENT {
class PhysicalDeviceTable;
class PhysicalPortTable;
class LogicalPortTable;
class PhysicalDeviceVnTable;
}

void ovsdb_api_idl_callback(void *idl_base, int op, struct ovsdb_idl_row *row) {
    OvsdbClientIdl *client_idl = (OvsdbClientIdl *) idl_base;
    int i = ovsdb_api_row_type(row);
    if (i >= OvsdbClientIdl::OVSDB_TYPE_COUNT)
        return;
    if (client_idl->callback_[i] != NULL)
        client_idl->callback_[i]((OvsdbClientIdl::Op)op, row);
}

OvsdbClientIdl::OvsdbClientIdl(OvsdbClientSession *session, Agent *agent) :
    idl_(ovsdb_api_idl_create()), session_(session) {
    vtep_global_= ovsdb_api_vteprec_global_first(idl_);
    ovsdb_idl_set_callback(idl_, (void *)this, ovsdb_api_idl_callback);
    parser_ = NULL;
    for (int i = 0; i < OVSDB_TYPE_COUNT; i++) {
        callback_[i] = NULL;
    }
    physical_switch_table_.reset(new PhysicalSwitchTable(this));
    logical_switch_table_.reset(new LogicalSwitchTable(this,
               (DBTable *)agent->device_manager()->physical_device_vn_table()));
}

OvsdbClientIdl::~OvsdbClientIdl() {
    ovsdb_idl_destroy(idl_);
}

void OvsdbClientIdl::SendMointorReq() {
    SendJsonRpc(ovsdb_idl_encode_monitor_request(idl_));
}

void OvsdbClientIdl::SendJsonRpc(struct jsonrpc_msg *msg) {
    struct json *json_msg = jsonrpc_msg_to_json(msg);
    char *s = json_to_string(json_msg, 0);
    json_destroy(json_msg);

    session_->SendMsg((u_int8_t *)s, strlen(s));
}

void OvsdbClientIdl::MessageProcess(const u_int8_t *buf, std::size_t len) {
    if (parser_ == NULL) {
        parser_ = json_parser_create(0);
    }
    json_parser_feed(parser_, (const char *)buf, len);

    /* If we have complete JSON, attempt to parse it as JSON-RPC. */
    if (json_parser_is_done(parser_)) {
        struct json *json = json_parser_finish(parser_);
        parser_ = NULL;
        struct jsonrpc_msg *msg;
        char *error = jsonrpc_msg_from_json(json, &msg);
        if (error) {
            free(error);
            assert(0);
            //return;
        }

        if (ovsdb_api_msg_echo_req(msg)) {
            /* Echo request.  Send reply. */
            struct jsonrpc_msg *reply;
            reply = ovsdb_api_jsonrpc_create_reply(msg);
            SendJsonRpc(reply);
            //jsonrpc_session_send(s, reply);
        } else if (ovsdb_api_msg_echo_reply(msg)) {
            /* It's a reply to our echo request.  Suppress it. */
        } else {
            ovsdb_idl_msg_process(idl_, msg);
            return;
        }
        jsonrpc_msg_destroy(msg);
    }
}

struct ovsdb_idl_txn *OvsdbClientIdl::CreateTxn() {
    return ovsdb_idl_txn_create(idl_);
}

