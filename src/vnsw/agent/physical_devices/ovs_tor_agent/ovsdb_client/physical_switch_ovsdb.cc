/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */

extern "C" {
#include <ovsdb_wrapper.h>
};
#include <ovsdb_client_idl.h>
#include <physical_switch_ovsdb.h>

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
    return (name_.compare(ps_entry.name_) < 0);
}

KSyncEntry *PhysicalSwitchEntry::UnresolvedReference() {
    return NULL;
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
    if (op == OvsdbClientIdl::OVSDB_DEL) {
        printf("Delete of Physical Switch %s\n",
                ovsdb_wrapper_physical_switch_name(row));
        PhysicalSwitchEntry key(this, ovsdb_wrapper_physical_switch_name(row));
        PhysicalSwitchEntry *entry =
            static_cast<PhysicalSwitchEntry *>(Find(&key));
        if (entry != NULL)
            Delete(entry);
    } else if (op == OvsdbClientIdl::OVSDB_ADD) {
        printf("Add/Change of Physical Switch %s\n",
                ovsdb_wrapper_physical_switch_name(row));
        PhysicalSwitchEntry key(this, ovsdb_wrapper_physical_switch_name(row));
        PhysicalSwitchEntry *entry =
            static_cast<PhysicalSwitchEntry *>(Find(&key));
        if (entry == NULL) {
            Create(&key);
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

