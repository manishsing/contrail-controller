/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */

#include <ovsdb_api.h>
#include <physical_switch_ovsdb.h>
#include <logical_switch_ovsdb.h>

#include <oper/vn.h>
#include <physical_devices/tables/physical_device.h>
#include <physical_devices/tables/physical_device_vn.h>

using namespace AGENT;

LogicalSwitchEntry::LogicalSwitchEntry(LogicalSwitchTable *table,
        const AGENT::PhysicalDeviceVnEntry *entry) : OvsdbDBEntry(),
    table_(table) {
    name_ = entry->vn()->GetName();
    vxlan_id_ = entry->vn()->GetVxLanId();
    device_name_ = entry->device()->name();
}

LogicalSwitchEntry::LogicalSwitchEntry(LogicalSwitchTable *table,
        const LogicalSwitchEntry *entry) : OvsdbDBEntry(), table_(table) {
    name_ = entry->name_;
    vxlan_id_ = entry->vxlan_id_;;
    device_name_ = entry->device_name_;
}

void LogicalSwitchEntry::AddMsg(struct ovsdb_idl_txn *txn) {
    ovsdb_api_add_logical_switch(txn, name_.c_str(), vxlan_id_);
}

void LogicalSwitchEntry::ChangeMsg(struct ovsdb_idl_txn *txn) {
    AddMsg(txn);
}

void LogicalSwitchEntry::DeleteMsg(struct ovsdb_idl_txn *txn) {
    //TODO add deleted logic need to delete dependent objects
    // from ovsdb local table
    //ovsdb_api_del_logical_switch(txn, name_.c_str(), vxlan_id_);
}

bool LogicalSwitchEntry::Sync(DBEntry *db_entry) {
    PhysicalDeviceVnEntry *entry =
        static_cast<PhysicalDeviceVnEntry *>(db_entry);
    if (vxlan_id_ != entry->vn()->GetVxLanId()) {
        vxlan_id_ = entry->vn()->GetVxLanId();
        return true;
    }
    return false;
}

bool LogicalSwitchEntry::IsLess(const KSyncEntry &entry) const {
    const LogicalSwitchEntry &ps_entry =
        static_cast<const LogicalSwitchEntry&>(entry);
    return (name_.compare(ps_entry.name_) < 0);
}

KSyncObject *LogicalSwitchEntry::GetObject() {
    return table_;
}

KSyncEntry *LogicalSwitchEntry::UnresolvedReference() {
    PhysicalSwitchTable *p_table = table_->client_idl()->physical_switch_table();
    PhysicalSwitchEntry key(p_table, device_name_.c_str());
    PhysicalSwitchEntry *p_switch =
        static_cast<PhysicalSwitchEntry *>(p_table->GetReference(&key));
    if (!p_switch->IsResolved()) {
        return p_switch;
    }
    return NULL;
}

LogicalSwitchTable::LogicalSwitchTable(OvsdbClientIdl *idl, DBTable *table) :
    OvsdbDBObject(idl, table) {
    idl->Register(OvsdbClientIdl::OVSDB_LOGICAL_SWITCH,
                  boost::bind(&LogicalSwitchTable::Notify, this, _1, _2));
}

LogicalSwitchTable::~LogicalSwitchTable() {
    client_idl_->UnRegister(OvsdbClientIdl::OVSDB_PHYSICAL_SWITCH);
}

void LogicalSwitchTable::Notify(OvsdbClientIdl::Op op,
        struct ovsdb_idl_row *row) {
    if (op == OvsdbClientIdl::OVSDB_DEL) {
        LogicalSwitchEntry key(this, ovsdb_api_logical_switch_name(row));
        LogicalSwitchEntry *entry =
            static_cast<LogicalSwitchEntry *>(Find(&key));
        if (entry != NULL)
            NotifyEvent(entry, KSyncEntry::DEL_ACK);
    } else if (op == OvsdbClientIdl::OVSDB_ADD) {
        LogicalSwitchEntry key(this, ovsdb_api_logical_switch_name(row));
        LogicalSwitchEntry *entry =
            static_cast<LogicalSwitchEntry *>(Find(&key));
        if (entry != NULL) {
            NotifyEvent(entry, KSyncEntry::ADD_ACK);
        } else {
            // TODO trigger delete of entry.
        }
    } else {
        assert(0);
    }
}

KSyncEntry *LogicalSwitchTable::Alloc(const KSyncEntry *key, uint32_t index) {
    const LogicalSwitchEntry *k_entry =
        static_cast<const LogicalSwitchEntry *>(key);
    LogicalSwitchEntry *entry = new LogicalSwitchEntry(this, k_entry);
    return entry;
}

KSyncEntry *LogicalSwitchTable::DBToKSyncEntry(const DBEntry* db_entry) {
    const PhysicalDeviceVnEntry *entry =
        static_cast<const PhysicalDeviceVnEntry *>(db_entry);
    LogicalSwitchEntry *key = new LogicalSwitchEntry(this, entry);
    return static_cast<KSyncEntry *>(key);
}

