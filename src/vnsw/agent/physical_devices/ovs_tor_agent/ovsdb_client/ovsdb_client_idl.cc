/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */

#include <assert.h>
#include <cstddef>
#include <string.h>
#include <stdlib.h>

extern "C" {
#include <ovsdb_wrapper.h>
};
#include <oper/agent_sandesh.h>
#include <ovsdb_types.h>
#include <ovsdb_client_idl.h>
#include <ovsdb_route_peer.h>
#include <ovsdb_entry.h>
#include <physical_switch_ovsdb.h>
#include <logical_switch_ovsdb.h>
#include <physical_port_ovsdb.h>
#include <vlan_port_binding_ovsdb.h>
#include <unicast_mac_local_ovsdb.h>
#include <unicast_mac_remote_ovsdb.h>

#include <physical_devices/tables/device_manager.h>

SandeshTraceBufferPtr OvsdbTraceBuf(SandeshTraceBufferCreate("Ovsdb", 5000));

namespace AGENT {
class PhysicalDeviceTable;
class PhysicalPortTable;
class LogicalPortTable;
class PhysicalDeviceVnTable;
}

using OVSDB::OvsdbClientIdl;
using OVSDB::OvsdbClientSession;
using OVSDB::OvsdbEntryBase;

namespace OVSDB {
void ovsdb_wrapper_idl_callback(void *idl_base, int op,
        struct ovsdb_idl_row *row) {
    OvsdbClientIdl *client_idl = (OvsdbClientIdl *) idl_base;
    int i = ovsdb_wrapper_row_type(row);
    if (i >= OvsdbClientIdl::OVSDB_TYPE_COUNT)
        return;
    if (client_idl->callback_[i] != NULL)
        client_idl->callback_[i]((OvsdbClientIdl::Op)op, row);
}

void ovsdb_wrapper_idl_txn_ack(void *idl_base, struct ovsdb_idl_txn *txn) {
    OvsdbClientIdl *client_idl = (OvsdbClientIdl *) idl_base;
    OvsdbEntryBase *entry = client_idl->pending_txn_[txn];
    bool success = ovsdb_wrapper_is_txn_success(txn);
    client_idl->DeleteTxn(txn);
    if (!success) {
        OVSDB_TRACE(Error, "Transaction failed");
    }
    if (entry)
        entry->Ack(success);
}
};

OvsdbClientIdl::OvsdbClientIdl(OvsdbClientSession *session, Agent *agent,
        OvsPeerManager *manager) : idl_(ovsdb_wrapper_idl_create()),
    session_(session), pending_txn_() {
    vtep_global_= ovsdb_wrapper_vteprec_global_first(idl_);
    ovsdb_wrapper_idl_set_callback(idl_, (void *)this,
            ovsdb_wrapper_idl_callback, ovsdb_wrapper_idl_txn_ack);
    parser_ = NULL;
    for (int i = 0; i < OVSDB_TYPE_COUNT; i++) {
        callback_[i] = NULL;
    }
    route_peer_.reset(manager->Allocate(IpAddress()));
    physical_switch_table_.reset(new PhysicalSwitchTable(this));
    logical_switch_table_.reset(new LogicalSwitchTable(this,
               (DBTable *)agent->device_manager()->physical_device_vn_table()));
    physical_port_table_.reset(new PhysicalPortTable(this));
    vlan_port_table_.reset(new VlanPortBindingTable(this,
                (DBTable *)agent->device_manager()->logical_port_table()));
    unicast_mac_local_ovsdb_.reset(new UnicastMacLocalOvsdb(this,
                route_peer()));
    vrf_ovsdb_.reset(new VrfOvsdbObject(this, (DBTable *)agent->vrf_table()));
}

OvsdbClientIdl::~OvsdbClientIdl() {
    ovsdb_wrapper_idl_destroy(idl_);
}

void OvsdbClientIdl::SendMointorReq() {
    SendJsonRpc(ovsdb_wrapper_idl_encode_monitor_request(idl_));
}

void OvsdbClientIdl::SendJsonRpc(struct jsonrpc_msg *msg) {
    struct json *json_msg = ovsdb_wrapper_jsonrpc_msg_to_json(msg);
    char *s = ovsdb_wrapper_json_to_string(json_msg, 0);
    ovsdb_wrapper_json_destroy(json_msg);

    session_->SendMsg((u_int8_t *)s, strlen(s));
}

void OvsdbClientIdl::MessageProcess(const u_int8_t *buf, std::size_t len) {
    if (parser_ == NULL) {
        parser_ = ovsdb_wrapper_json_parser_create(0);
    }
    ovsdb_wrapper_json_parser_feed(parser_, (const char *)buf, len);

    /* If we have complete JSON, attempt to parse it as JSON-RPC. */
    if (ovsdb_wrapper_json_parser_is_done(parser_)) {
        struct json *json = ovsdb_wrapper_json_parser_finish(parser_);
        parser_ = NULL;
        struct jsonrpc_msg *msg;
        char *error = ovsdb_wrapper_jsonrpc_msg_from_json(json, &msg);
        if (error) {
            free(error);
            assert(0);
            //return;
        }

        if (ovsdb_wrapper_msg_echo_req(msg)) {
            /* Echo request.  Send reply. */
            struct jsonrpc_msg *reply;
            reply = ovsdb_wrapper_jsonrpc_create_reply(msg);
            SendJsonRpc(reply);
            //jsonrpc_session_send(s, reply);
        } else if (ovsdb_wrapper_msg_echo_reply(msg)) {
            /* It's a reply to our echo request.  Suppress it. */
        } else {
            ovsdb_wrapper_idl_msg_process(idl_, msg);
            return;
        }
        ovsdb_wrapper_jsonrpc_msg_destroy(msg);
    }
}

struct ovsdb_idl_txn *OvsdbClientIdl::CreateTxn(OvsdbEntryBase *entry) {
    struct ovsdb_idl_txn *txn =  ovsdb_wrapper_idl_txn_create(idl_);
    pending_txn_[txn] = entry;
    return txn;
}

void OvsdbClientIdl::DeleteTxn(struct ovsdb_idl_txn *txn) {
    pending_txn_.erase(txn);
    ovsdb_wrapper_idl_txn_destroy(txn);
}

Ip4Address OvsdbClientIdl::tsn_ip() {
    return session_->tsn_ip();
}

