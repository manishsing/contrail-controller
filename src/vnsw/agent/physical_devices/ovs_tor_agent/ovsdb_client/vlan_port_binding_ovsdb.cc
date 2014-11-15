/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */

extern "C" {
#include <ovsdb_wrapper.h>
};
#include <physical_switch_ovsdb.h>
#include <logical_switch_ovsdb.h>
#include <physical_port_ovsdb.h>
#include <vlan_port_binding_ovsdb.h>
#include <vm_interface_ksync.h>

#include <oper/vn.h>
#include <oper/interface.h>
#include <oper/vm_interface.h>
#include <physical_devices/tables/physical_device.h>
#include <physical_devices/tables/physical_device_vn.h>
#include <physical_devices/tables/logical_port.h>
#include <ovsdb_types.h>

using OVSDB::OvsdbDBEntry;
using OVSDB::VlanPortBindingEntry;
using OVSDB::VlanPortBindingTable;
using OVSDB::PhysicalSwitchEntry;
using OVSDB::PhysicalPortEntry;
using OVSDB::LogicalSwitchEntry;

VlanPortBindingEntry::VlanPortBindingEntry(VlanPortBindingTable *table,
        const AGENT::VlanLogicalPortEntry *entry) : OvsdbDBEntry(table_),
    logical_switch_name_(), physical_port_name_(entry->physical_port()->name()),
    physical_device_name_(entry->physical_port()->device()->name()),
    vlan_(entry->vlan()) {
}

VlanPortBindingEntry::VlanPortBindingEntry(VlanPortBindingTable *table,
        const VlanPortBindingEntry *key) : OvsdbDBEntry(table),
    physical_port_name_(key->physical_port_name_),
    physical_device_name_(key->physical_device_name_), vlan_(key->vlan_) {
}

void VlanPortBindingEntry::PreAddChange() {
    if (!logical_switch_name_.empty()) {
        LogicalSwitchTable *l_table =
            table_->client_idl()->logical_switch_table();
        LogicalSwitchEntry ls_key(l_table, logical_switch_name_.c_str());
        logical_switch_ = l_table->GetReference(&ls_key);
    } else {
        logical_switch_ = NULL;
    }
}

void VlanPortBindingEntry::PostDelete() {
    logical_switch_ = NULL;
}

void VlanPortBindingEntry::AddMsg(struct ovsdb_idl_txn *txn) {
    PhysicalPortTable *p_table = table_->client_idl()->physical_port_table();
    PhysicalPortEntry key(p_table, physical_port_name_.c_str());
    physical_port_ = p_table->GetReference(&key);
    PhysicalPortEntry *port =
        static_cast<PhysicalPortEntry *>(physical_port_.get());

    if (!logical_switch_name_.empty()) {
        port->AddBinding(vlan_,
                static_cast<LogicalSwitchEntry *>(logical_switch_.get()));
        OVSDB_TRACE(Trace, "Adding port vlan binding port " +
                physical_port_name_ + " vlan " + integerToString(vlan_) +
                " to Logical Switch " + logical_switch_name_);
    } else {
        OVSDB_TRACE(Trace, "Deleting port vlan binding port " +
                physical_port_name_ + " vlan " + integerToString(vlan_));
        port->DeleteBinding(vlan_, NULL);
    }
    port->Encode(txn);
}

void VlanPortBindingEntry::ChangeMsg(struct ovsdb_idl_txn *txn) {
    PhysicalPortEntry *port =
        static_cast<PhysicalPortEntry *>(physical_port_.get());
    OVSDB_TRACE(Trace, "Deleting port vlan binding port " +
            physical_port_name_ + " vlan " + integerToString(vlan_));
    port->DeleteBinding(vlan_, NULL);

    AddMsg(txn);
}

void VlanPortBindingEntry::DeleteMsg(struct ovsdb_idl_txn *txn) {
    if (!physical_port_) {
        return;
    }
    PhysicalPortEntry *port =
        static_cast<PhysicalPortEntry *>(physical_port_.get());
    OVSDB_TRACE(Trace, "Deleting port vlan binding port " +
            physical_port_name_ + " vlan " + integerToString(vlan_));
    port->DeleteBinding(vlan_,
            static_cast<LogicalSwitchEntry *>(logical_switch_.get()));
    port->Encode(txn);
}

bool VlanPortBindingEntry::Sync(DBEntry *db_entry) {
    AGENT::VlanLogicalPortEntry *entry =
        static_cast<AGENT::VlanLogicalPortEntry *>(db_entry);
    std::string ls_name;
    boost::uuids::uuid vmi_uuid;
    bool change = false;

    if (entry->vm_interface()) {
        vmi_uuid = entry->vm_interface()->GetUuid();
    }

    if (entry->vm_interface() && entry->vm_interface()->vn()) {
        ls_name = UuidToString(entry->vm_interface()->vn()->GetUuid());
    }

    if (vmi_uuid_ != vmi_uuid) {
        vmi_uuid_ = vmi_uuid;
        change = true;
    }

    if (ls_name != logical_switch_name_) {
        logical_switch_name_ = ls_name;
        change = true;
    }

    return change;
}

bool VlanPortBindingEntry::IsLess(const KSyncEntry &entry) const {
    const VlanPortBindingEntry &vps_entry =
        static_cast<const VlanPortBindingEntry&>(entry);
    if (vlan_ != vps_entry.vlan_)
        return vlan_ < vps_entry.vlan_;
    if (physical_device_name_ != vps_entry.physical_device_name_)
        return physical_device_name_ < vps_entry.physical_device_name_;
    return physical_port_name_ < vps_entry.physical_port_name_;
}

KSyncEntry *VlanPortBindingEntry::UnresolvedReference() {
    PhysicalPortTable *p_table = table_->client_idl()->physical_port_table();
    PhysicalPortEntry key(p_table, physical_port_name_.c_str());
    PhysicalPortEntry *p_port =
        static_cast<PhysicalPortEntry *>(p_table->GetReference(&key));
    if (!p_port->IsResolved()) {
        OVSDB_TRACE(Trace, "Physical Port unavailable for Port Vlan Binding " +
                physical_port_name_ + " vlan " + integerToString(vlan_) +
                " to Logical Switch " + logical_switch_name_);
        return p_port;
    }

    PhysicalSwitchTable *ps_table =
        table_->client_idl()->physical_switch_table();
    PhysicalSwitchEntry ps_key(ps_table, physical_device_name_.c_str());
    PhysicalSwitchEntry *p_switch =
        static_cast<PhysicalSwitchEntry *>(ps_table->GetReference(&ps_key));
    if (!p_switch->IsResolved()) {
        OVSDB_TRACE(Trace, "Physical Switch unavailable for Port Vlan Binding "+
                physical_port_name_ + " vlan " + integerToString(vlan_) +
                " to Logical Switch " + logical_switch_name_);
        return p_switch;
    }

    VMInterfaceKSyncObject *vm_intf_table =
        table_->client_idl()->vm_interface_table();
    VMInterfaceKSyncEntry vm_intf_key(vm_intf_table, vmi_uuid_);
    VMInterfaceKSyncEntry *vm_intf = static_cast<VMInterfaceKSyncEntry *>(
            vm_intf_table->GetReference(&vm_intf_key));
    if (!vm_intf->IsResolved()) {
        OVSDB_TRACE(Trace, "VM Interface unavailable for Port Vlan Binding " +
                physical_port_name_ + " vlan " + integerToString(vlan_) +
                " to Logical Switch " + logical_switch_name_);
        return vm_intf;
    } else if (logical_switch_name_.empty()) {
        // update latest name after resolution.
        logical_switch_name_ = vm_intf->vn_name();
    }

    if (!logical_switch_name_.empty()) {
        // Check only if logical switch name is present.
        LogicalSwitchTable *l_table =
            table_->client_idl()->logical_switch_table();
        LogicalSwitchEntry ls_key(l_table, logical_switch_name_.c_str());
        LogicalSwitchEntry *ls_entry =
            static_cast<LogicalSwitchEntry *>(l_table->GetReference(&ls_key));
        if (!ls_entry->IsResolved()) {
            OVSDB_TRACE(Trace, "Logical Switch  unavailable for Port Vlan "
                    "Binding " + physical_port_name_ + " vlan " +
                    integerToString(vlan_) + " to Logical Switch " +
                    logical_switch_name_);
            return ls_entry;
        }
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

OvsdbDBEntry *VlanPortBindingTable::AllocOvsEntry(struct ovsdb_idl_row *row) {
    return NULL;
}

KSyncDBObject::DBFilterResp VlanPortBindingTable::DBEntryFilter(
        const DBEntry *entry) {
    const AGENT::VlanLogicalPortEntry *l_port =
        static_cast<const AGENT::VlanLogicalPortEntry *>(entry);
    // Since we need physical port name and device name as key, ignore entry
    // if physical port or device is not yet present.
    if (l_port->physical_port() == NULL) {
        OVSDB_TRACE(Trace, "Ignoring Port Vlan Binding due to physical port "
                "unavailablity Logical port = " + l_port->name());
        return DBFilterIgnore; // TODO(Prabhjot) check if Delete is required.
    }
    if (l_port->physical_port()->device() == NULL) {
        OVSDB_TRACE(Trace, "Ignoring Port Vlan Binding due to device "
                "unavailablity Logical port = " + l_port->name());
        return DBFilterIgnore; // TODO(Prabhjot) check if Delete is required.
    }
    return DBFilterAccept;
}

