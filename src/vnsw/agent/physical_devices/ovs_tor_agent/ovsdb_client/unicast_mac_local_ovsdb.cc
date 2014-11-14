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
#include <logical_switch_ovsdb.h>
#include <unicast_mac_local_ovsdb.h>

using OVSDB::UnicastMacLocalOvsdb;
using OVSDB::UnicastMacLocalEntry;
using std::string;

UnicastMacLocalEntry::UnicastMacLocalEntry(UnicastMacLocalOvsdb *table,
        const UnicastMacLocalEntry *key) : OvsdbEntry(table), mac_(key->mac_),
    logical_switch_name_(key->logical_switch_name_), dest_ip_(key->dest_ip_) {
    LogicalSwitchTable *l_table = table_->client_idl()->logical_switch_table();
    LogicalSwitchEntry l_key(l_table, logical_switch_name_.c_str());
    logical_switch_ = l_table->GetReference(&l_key);
}

UnicastMacLocalEntry::UnicastMacLocalEntry(UnicastMacLocalOvsdb *table,
        struct ovsdb_idl_row *row) : OvsdbEntry(table),
    mac_(ovsdb_wrapper_ucast_mac_local_mac(row)),
    logical_switch_name_(ovsdb_wrapper_ucast_mac_local_logical_switch(row)),
    dest_ip_() {
    if (ovsdb_wrapper_ucast_mac_local_dst_ip(row))
        dest_ip_ = ovsdb_wrapper_ucast_mac_local_dst_ip(row);
}

UnicastMacLocalEntry::~UnicastMacLocalEntry() {
}

bool UnicastMacLocalEntry::Add() {
    UnicastMacLocalOvsdb *table = static_cast<UnicastMacLocalOvsdb *>(table_);
    OVSDB_TRACE(Trace, "Adding Route " + mac_ + " VN uuid " +
            logical_switch_name_ + " destination IP " + dest_ip_);
    boost::uuids::uuid ls_uuid = StringToUuid(logical_switch_name_);
    boost::system::error_code err;
    Ip4Address dest = Ip4Address::from_string(dest_ip_, err);
    table->peer()->AddOvsRoute(ls_uuid, MacAddress(mac_), dest);
    return true;
}

bool UnicastMacLocalEntry::Delete() {
    UnicastMacLocalOvsdb *table = static_cast<UnicastMacLocalOvsdb *>(table_);
    OVSDB_TRACE(Trace, "Deleting Route " + mac_ + " VN uuid " +
            logical_switch_name_ + " destination IP " + dest_ip_);
    boost::uuids::uuid ls_uuid = StringToUuid(logical_switch_name_);
    table->peer()->DeleteOvsRoute(ls_uuid, MacAddress(mac_));
    return true;
}

bool UnicastMacLocalEntry::IsLess(const KSyncEntry& entry) const {
    const UnicastMacLocalEntry &ucast =
        static_cast<const UnicastMacLocalEntry&>(entry);
    if (mac_ != ucast.mac_)
        return mac_ < ucast.mac_;
    return logical_switch_name_ < ucast.logical_switch_name_;
}

KSyncEntry *UnicastMacLocalEntry::UnresolvedReference() {
    LogicalSwitchTable *l_table = table_->client_idl()->logical_switch_table();
    LogicalSwitchEntry key(l_table, logical_switch_name_.c_str());
    LogicalSwitchEntry *l_switch =
        static_cast<LogicalSwitchEntry *>(l_table->GetReference(&key));
    if (!l_switch->IsResolved()) {
        return l_switch;
    }
    return NULL;
}

UnicastMacLocalOvsdb::UnicastMacLocalOvsdb(OvsdbClientIdl *idl, OvsPeer *peer) :
    OvsdbObject(idl), peer_(peer) {
    idl->Register(OvsdbClientIdl::OVSDB_UCAST_MAC_LOCAL,
            boost::bind(&UnicastMacLocalOvsdb::Notify, this, _1, _2));
}

UnicastMacLocalOvsdb::~UnicastMacLocalOvsdb() {
    client_idl_->UnRegister(OvsdbClientIdl::OVSDB_UCAST_MAC_LOCAL);
}

OvsPeer *UnicastMacLocalOvsdb::peer() {
    return peer_;
}

void UnicastMacLocalOvsdb::Notify(OvsdbClientIdl::Op op,
        struct ovsdb_idl_row *row) {
    const char *ls_name = ovsdb_wrapper_ucast_mac_local_logical_switch(row);
    const char *dest_ip = ovsdb_wrapper_ucast_mac_local_dst_ip(row);
    /* ignore if ls_name is not present */
    if (ls_name == NULL) {
        return;
    }
    UnicastMacLocalEntry key(this, row);
    UnicastMacLocalEntry *entry =
        static_cast<UnicastMacLocalEntry *>(FindActiveEntry(&key));
    /* trigger delete if dest ip is not available */
    if (op == OvsdbClientIdl::OVSDB_DEL || dest_ip == NULL) {
        if (entry != NULL) {
            Delete(entry);
        }
    } else if (op == OvsdbClientIdl::OVSDB_ADD) {
        if (entry == NULL) {
            entry = static_cast<UnicastMacLocalEntry *>(Create(&key));
        }
    } else {
        assert(0);
    }
}

KSyncEntry *UnicastMacLocalOvsdb::Alloc(const KSyncEntry *key, uint32_t index) {
    const UnicastMacLocalEntry *k_entry =
        static_cast<const UnicastMacLocalEntry *>(key);
    UnicastMacLocalEntry *entry = new UnicastMacLocalEntry(this, k_entry);
    return entry;
}

