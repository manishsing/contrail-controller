/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */

extern "C" {
#include <ovsdb_api.h>
};

#include <physical_switch_ovsdb.h>
#include <physical_locator_ovsdb.h>

#include <oper/vn.h>
#include <oper/tunnel_nh.h>
#include <physical_devices/tables/physical_device.h>
#include <physical_devices/tables/physical_device_vn.h>

using namespace AGENT;

PhysicalLocatorEntry::PhysicalLocatorEntry(PhysicalLocatorTable *table,
        const char *dip_str) : OvsdbDBEntry(table), dip_(dip_str),
    is_vxlan_nh_(true), nh_(NULL) {
}

PhysicalLocatorEntry::PhysicalLocatorEntry(PhysicalLocatorTable *table,
        const PhysicalLocatorEntry *key) : OvsdbDBEntry(table), dip_(key->dip_),
    is_vxlan_nh_(key->is_vxlan_nh_), nh_(key->nh_) {
}

PhysicalLocatorEntry::PhysicalLocatorEntry(PhysicalLocatorTable *table,
        const NextHop *entry) : OvsdbDBEntry(table), dip_(""),
    is_vxlan_nh_(false), nh_(entry) {
    if (entry->GetType() == NextHop::TUNNEL) {
        const TunnelNH *tunnel_nh = static_cast<const TunnelNH *>(entry);
        if (tunnel_nh->GetTunnelType().GetType() == TunnelType::VXLAN) {
            dip_ = tunnel_nh->GetDip()->to_string();
            is_vxlan_nh_ = true;
        }
    }
}

PhysicalLocatorEntry::PhysicalLocatorEntry(PhysicalLocatorTable *table,
        struct ovsdb_idl_row *entry) : OvsdbDBEntry(table, entry), dip_(""),
    is_vxlan_nh_(true), nh_(NULL) {
}

PhysicalLocatorEntry::~PhysicalLocatorEntry() {
}

void PhysicalLocatorEntry::AddMsg(struct ovsdb_idl_txn *txn) {
    if (!is_vxlan_nh_)
        return;
    ovsdb_wrapper_add_physical_locator(txn, ovs_entry_, dip_.c_str());
}

void PhysicalLocatorEntry::ChangeMsg(struct ovsdb_idl_txn *txn) {
    AddMsg(txn);
}

void PhysicalLocatorEntry::DeleteMsg(struct ovsdb_idl_txn *txn) {
    if (!is_vxlan_nh_)
        return;
    ovsdb_wrapper_delete_physical_locator(ovs_entry_);
}

bool PhysicalLocatorEntry::Sync(DBEntry *db_entry) {
    NextHop *entry = static_cast<NextHop *>(db_entry);
    if (entry->GetType() == NextHop::TUNNEL) {
        TunnelNH *tunnel_nh = static_cast<TunnelNH *>(entry);
        if (tunnel_nh->GetTunnelType().GetType() == TunnelType::VXLAN) {
            if (dip_ != tunnel_nh->GetDip()->to_string()) {
                // We don't handle dest ip change;
                assert(0);
            }
        }
    }
    return false;
}

bool PhysicalLocatorEntry::IsLess(const KSyncEntry &entry) const {
    const PhysicalLocatorEntry &pl_entry =
        static_cast<const PhysicalLocatorEntry&>(entry);
    if (is_vxlan_nh_ != pl_entry.is_vxlan_nh_) {
        return (is_vxlan_nh_ < pl_entry.is_vxlan_nh_);
    } else if (!is_vxlan_nh_) {
        return nh_ < pl_entry.nh_;
    }
    return (dip_ < pl_entry.dip_);
}

KSyncEntry *PhysicalLocatorEntry::UnresolvedReference() {
#if 0
    PhysicalSwitchTable *p_table = table_->client_idl()->physical_switch_table();
    PhysicalSwitchEntry key(p_table, device_name_.c_str());
    PhysicalSwitchEntry *p_switch =
        static_cast<PhysicalSwitchEntry *>(p_table->GetReference(&key));
    if (!p_switch->IsResolved()) {
        return p_switch;
    }
#endif
    return NULL;
}

PhysicalLocatorTable::PhysicalLocatorTable(OvsdbClientIdl *idl,
        DBTable *table) : OvsdbDBObject(idl, table) {
    idl->Register(OvsdbClientIdl::OVSDB_PHYSICAL_LOCATOR,
                  boost::bind(&PhysicalLocatorTable::OvsdbNotify, this, _1, _2));
}

PhysicalLocatorTable::~PhysicalLocatorTable() {
    client_idl_->UnRegister(OvsdbClientIdl::OVSDB_PHYSICAL_LOCATOR);
}

void PhysicalLocatorTable::OvsdbNotify(OvsdbClientIdl::Op op,
        struct ovsdb_idl_row *row) {
    if (op == OvsdbClientIdl::OVSDB_DEL) {
        printf("Delete of Physical Locator %s\n",
                ovsdb_wrapper_physical_locator_dst_ip(row));
        //PhysicalLocatorEntry key(this, ovsdb_wrapper_physical_locator_dst_ip(row));
        //NotifyDelete((OvsdbDBEntry*)&key);
    } else if (op == OvsdbClientIdl::OVSDB_ADD) {
        printf("Add/Change of Physical Locator %s\n",
                ovsdb_wrapper_physical_locator_dst_ip(row));
        //PhysicalLocatorEntry key(this, ovsdb_wrapper_physical_locator_dst_ip(row));
        //NotifyAdd((OvsdbDBEntry*)&key, row);
    } else {
        assert(0);
    }
}

KSyncEntry *PhysicalLocatorTable::Alloc(const KSyncEntry *key, uint32_t index) {
    const PhysicalLocatorEntry *k_entry =
        static_cast<const PhysicalLocatorEntry *>(key);
    PhysicalLocatorEntry *entry = new PhysicalLocatorEntry(this, k_entry);
    return entry;
}

KSyncEntry *PhysicalLocatorTable::DBToKSyncEntry(const DBEntry* db_entry) {
    const NextHop *entry =
        static_cast<const NextHop *>(db_entry);
    PhysicalLocatorEntry *key = new PhysicalLocatorEntry(this, entry);
    return static_cast<KSyncEntry *>(key);
}

OvsdbDBEntry *PhysicalLocatorTable::AllocOvsEntry(struct ovsdb_idl_row *row) {
    PhysicalLocatorEntry *entry = new PhysicalLocatorEntry(this, row);
    return static_cast<OvsdbDBEntry *>(entry);
}

