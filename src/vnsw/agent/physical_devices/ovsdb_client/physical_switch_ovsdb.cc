/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */

#include <ovsdb_api.h>
#include <physical_switch_ovsdb.h>

bool PhysicalSwitchEntry::IsLess(const KSyncEntry &entry) const {
    const PhysicalSwitchEntry &ps_entry =
        static_cast<const PhysicalSwitchEntry&>(entry);
    return (name_.compare(ps_entry.name_) < 0);
}

KSyncObject *PhysicalSwitchEntry::GetObject() {
    return table_;
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
        PhysicalSwitchEntry key(this, ovsdb_api_physical_switch_name(row));
        PhysicalSwitchEntry *entry =
            static_cast<PhysicalSwitchEntry *>(Find(&key));
        if (entry != NULL)
            Delete(entry);
    } else if (op == OvsdbClientIdl::OVSDB_ADD) {
        PhysicalSwitchEntry key(this, ovsdb_api_physical_switch_name(row));
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

