/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */

extern "C" {
#include <ovsdb_wrapper.h>
};
#include <base/util.h>
#include <net/mac_address.h>
#include <oper/agent_sandesh.h>
#include <ovsdb_types.h>
#include <ovsdb_route_peer.h>
#include <unicast_mac_local_ovsdb.h>

using OVSDB::UnicastMacLocalOvsdb;
using std::string;

UnicastMacLocalOvsdb::UnicastMacLocalOvsdb(OvsdbClientIdl *idl, OvsPeer *peer) :
    client_idl_(idl), peer_(peer) {
    client_idl_->Register(OvsdbClientIdl::OVSDB_UCAST_MAC_LOCAL,
            boost::bind(&UnicastMacLocalOvsdb::Notify, this, _1, _2));
}

UnicastMacLocalOvsdb::~UnicastMacLocalOvsdb() {
    client_idl_->UnRegister(OvsdbClientIdl::OVSDB_UCAST_MAC_LOCAL);
}

void UnicastMacLocalOvsdb::Notify(OvsdbClientIdl::Op op,
        struct ovsdb_idl_row *row) {
    const char *ls_name = ovsdb_wrapper_ucast_mac_local_logical_switch(row);
    const char *mac_str = ovsdb_wrapper_ucast_mac_local_mac(row);
    const char *dest_ip = ovsdb_wrapper_ucast_mac_local_dst_ip(row);
    /* ignore if ls_name is not present */
    if (ls_name == NULL) {
        return;
    }
    boost::uuids::uuid ls_uuid = StringToUuid(ls_name);
    MacAddress mac(mac_str);
    Ip4Address dest;
    /* trigger delete if dest ip is not available */
    bool delete_entry = true;
    if (dest_ip != NULL) {
        delete_entry = false;
        boost::system::error_code err;
        dest = Ip4Address::from_string(dest_ip, err);
    }
    if (op == OvsdbClientIdl::OVSDB_DEL || delete_entry) {
        OVSDB_TRACE(Trace, string("Deleting Route ") + string(mac_str) +
                string(" VN uuid ") + string(ls_name));
        peer_->DeleteOvsRoute(ls_uuid, mac);
    } else if (op == OvsdbClientIdl::OVSDB_ADD) {
        OVSDB_TRACE(Trace, string("Adding Route ") + string(mac_str) +
                string(" VN uuid ") + string(ls_name) +
                string(" destination IP ") + string(dest_ip));
        peer_->AddOvsRoute(ls_uuid, mac, dest);
    } else {
        assert(0);
    }
}

