/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */

extern "C" {
#include <ovsdb_wrapper.h>
};
#include <ovsdb_client_idl.h>
#include <physical_switch_ovsdb.h>
#include <ovsdb_types.h>

using OVSDB::PhysicalSwitchEntry;
using OVSDB::PhysicalSwitchTable;

PhysicalSwitchEntry::PhysicalSwitchEntry(PhysicalSwitchTable *table,
        const std::string &name) : OvsdbEntry(table), name_(name),
    tunnel_ip_() {
}

PhysicalSwitchEntry::~PhysicalSwitchEntry() {
}

Ip4Address &PhysicalSwitchEntry::tunnel_ip() {
    return tunnel_ip_;
}

void PhysicalSwitchEntry::set_tunnel_ip(std::string ip) {
    boost::system::error_code ec;
    tunnel_ip_ = Ip4Address::from_string(ip, ec);
}

bool PhysicalSwitchEntry::IsLess(const KSyncEntry &entry) const {
    const PhysicalSwitchEntry &ps_entry =
        static_cast<const PhysicalSwitchEntry&>(entry);
    return (name_ < ps_entry.name_);
}

KSyncEntry *PhysicalSwitchEntry::UnresolvedReference() {
    return NULL;
}

void PhysicalSwitchEntry::SendTrace(Trace event) const {
    SandeshPhysicalSwitchInfo info;
    if (event == ADD) {
        info.set_op("Add");
    } else {
        info.set_op("Delete");
    }
    info.set_name(name_);
    OVSDB_TRACE(PhysicalSwitch, info);
}

PhysicalSwitchTable::PhysicalSwitchTable(OvsdbClientIdl *idl) :
    OvsdbObject(idl) {
    idl->Register(OvsdbClientIdl::OVSDB_PHYSICAL_SWITCH,
                  boost::bind(&PhysicalSwitchTable::Notify, this, _1, _2));
}

PhysicalSwitchTable::~PhysicalSwitchTable() {
    client_idl_->UnRegister(OvsdbClientIdl::OVSDB_PHYSICAL_SWITCH);
}

void PhysicalSwitchTable::Notify(OvsdbClientIdl::Op op,
        struct ovsdb_idl_row *row) {
    PhysicalSwitchEntry key(this, ovsdb_wrapper_physical_switch_name(row));
    PhysicalSwitchEntry *entry =
        static_cast<PhysicalSwitchEntry *>(FindActiveEntry(&key));
    if (op == OvsdbClientIdl::OVSDB_DEL) {
        if (entry != NULL) {
            entry->SendTrace(PhysicalSwitchEntry::DEL);
            Delete(entry);
        }
    } else if (op == OvsdbClientIdl::OVSDB_ADD) {
        if (entry == NULL) {
            entry = static_cast<PhysicalSwitchEntry *>(Create(&key));
            entry->SendTrace(PhysicalSwitchEntry::ADD);
        }
        entry->set_tunnel_ip(ovsdb_wrapper_physical_switch_tunnel_ip(row));
    } else {
        assert(0);
    }
}

KSyncEntry *PhysicalSwitchTable::Alloc(const KSyncEntry *key, uint32_t index) {
    const PhysicalSwitchEntry *k_entry =
        static_cast<const PhysicalSwitchEntry *>(key);
    PhysicalSwitchEntry *entry = new PhysicalSwitchEntry(this, k_entry->name_);
    return entry;
}

