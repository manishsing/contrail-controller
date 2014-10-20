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
        const std::string &name) : OvsdbEntry(table), name_(name) {
}

PhysicalSwitchEntry::~PhysicalSwitchEntry() {
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
    const char *name = ovsdb_wrapper_physical_switch_name(row);
    if (op == OvsdbClientIdl::OVSDB_DEL) {
        PhysicalSwitchEntry key(this, name);
        PhysicalSwitchEntry *entry =
            static_cast<PhysicalSwitchEntry *>(Find(&key));
        if (entry != NULL) {
            entry->SendTrace(PhysicalSwitchEntry::DEL);
            Delete(entry);
        }
    } else if (op == OvsdbClientIdl::OVSDB_ADD) {
        PhysicalSwitchEntry key(this, name);
        PhysicalSwitchEntry *entry =
            static_cast<PhysicalSwitchEntry *>(Find(&key));
        if (entry == NULL) {
            entry = static_cast<PhysicalSwitchEntry *>(Create(&key));
            entry->SendTrace(PhysicalSwitchEntry::ADD);
        }
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

