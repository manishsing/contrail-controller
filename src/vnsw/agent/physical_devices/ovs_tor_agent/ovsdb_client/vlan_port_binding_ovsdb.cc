/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */

extern "C" {
#include <ovsdb_wrapper.h>
};
#include <logical_switch_ovsdb.h>
#include <physical_port_ovsdb.h>
#include <vlan_port_binding_ovsdb.h>

#include <oper/vn.h>
#include <oper/interface.h>
#include <oper/vm_interface.h>
#include <physical_devices/tables/physical_device.h>
#include <physical_devices/tables/physical_device_vn.h>
#include <physical_devices/tables/logical_port.h>

using OVSDB::VlanPortBindingEntry;
using OVSDB::VlanPortBindingTable;
using OVSDB::PhysicalPortEntry;
using OVSDB::LogicalSwitchEntry;

VlanPortBindingEntry::VlanPortBindingEntry(VlanPortBindingTable *table,
        const AGENT::VlanLogicalPortEntry *entry) : OvsdbDBEntry(table_) {
    logical_switch_name_ = entry->vm_interface()->vn()->GetName();
    physical_port_name_ = entry->physical_port()->name();
    vlan_ = entry->vlan();
}

VlanPortBindingEntry::VlanPortBindingEntry(VlanPortBindingTable *table,
        const VlanPortBindingEntry *key) : OvsdbDBEntry(table) {
    logical_switch_name_ = key->logical_switch_name_;
    physical_port_name_ = key->physical_port_name_;
    vlan_ = key->vlan_;
}

void VlanPortBindingEntry::AddMsg(struct ovsdb_idl_txn *txn) {
    PhysicalPortTable *p_table = table_->client_idl()->physical_port_table();
    PhysicalPortEntry key(p_table, physical_port_name_.c_str());
    physical_port_ = p_table->GetReference(&key);
    LogicalSwitchTable *l_table = table_->client_idl()->logical_switch_table();
    LogicalSwitchEntry ls_key(l_table, logical_switch_name_.c_str());
    logical_switch_ = l_table->GetReference(&ls_key);

    PhysicalPortEntry *port =
        static_cast<PhysicalPortEntry *>(physical_port_.get());
    port->AddBinding(vlan_,
            static_cast<LogicalSwitchEntry *>(logical_switch_.get()));
    port->Encode(txn);
}

void VlanPortBindingEntry::ChangeMsg(struct ovsdb_idl_txn *txn) {
    if (physical_port_) {
        PhysicalPortEntry *port =
            static_cast<PhysicalPortEntry *>(physical_port_.get());
        port->DeleteBinding(vlan_,
                static_cast<LogicalSwitchEntry *>(logical_switch_.get()));
        physical_port_ = NULL;
        logical_switch_ = NULL;
    }
    AddMsg(txn);
}

void VlanPortBindingEntry::DeleteMsg(struct ovsdb_idl_txn *txn) {
    if (!physical_port_) {
        return;
    }
    PhysicalPortEntry *port =
        static_cast<PhysicalPortEntry *>(physical_port_.get());
    port->DeleteBinding(vlan_,
            static_cast<LogicalSwitchEntry *>(logical_switch_.get()));
    physical_port_ = NULL;
    logical_switch_ = NULL;
    port->Encode(txn);
}

bool VlanPortBindingEntry::Sync(DBEntry *db_entry) {
    return false;
}

bool VlanPortBindingEntry::IsLess(const KSyncEntry &entry) const {
    const VlanPortBindingEntry &vps_entry =
        static_cast<const VlanPortBindingEntry&>(entry);
    if (vlan_ != vps_entry.vlan_)
        return vlan_ < vps_entry.vlan_;
    if (physical_port_name_ != vps_entry.physical_port_name_)
        return physical_port_name_ < vps_entry.physical_port_name_;

    return logical_switch_name_ < vps_entry.logical_switch_name_;
}

KSyncEntry *VlanPortBindingEntry::UnresolvedReference() {
    PhysicalPortTable *p_table = table_->client_idl()->physical_port_table();
    PhysicalPortEntry key(p_table, physical_port_name_.c_str());
    PhysicalPortEntry *p_port =
        static_cast<PhysicalPortEntry *>(p_table->GetReference(&key));
    if (!p_port->IsResolved()) {
        return p_port;
    }
    LogicalSwitchTable *l_table = table_->client_idl()->logical_switch_table();
    LogicalSwitchEntry ls_key(l_table, logical_switch_name_.c_str());
    LogicalSwitchEntry *ls_entry =
        static_cast<LogicalSwitchEntry *>(l_table->GetReference(&ls_key));
    if (!ls_entry->IsResolved()) {
        return ls_entry;
    }
    return NULL;
}

VlanPortBindingTable::VlanPortBindingTable(OvsdbClientIdl *idl, DBTable *table) :
    OvsdbDBObject(idl, table) {
}

VlanPortBindingTable::~VlanPortBindingTable() {
}

void VlanPortBindingTable::OvsdbNotify(OvsdbClientIdl::Op op,
        struct ovsdb_idl_row *row) {
}

KSyncEntry *VlanPortBindingTable::Alloc(const KSyncEntry *key, uint32_t index) {
    const VlanPortBindingEntry *k_entry =
        static_cast<const VlanPortBindingEntry *>(key);
    VlanPortBindingEntry *entry = new VlanPortBindingEntry(this, k_entry);
    return entry;
}

KSyncEntry *VlanPortBindingTable::DBToKSyncEntry(const DBEntry* db_entry) {
    const AGENT::VlanLogicalPortEntry *entry =
        static_cast<const AGENT::VlanLogicalPortEntry *>(db_entry);
    VlanPortBindingEntry *key = new VlanPortBindingEntry(this, entry);
    return static_cast<KSyncEntry *>(key);
}

