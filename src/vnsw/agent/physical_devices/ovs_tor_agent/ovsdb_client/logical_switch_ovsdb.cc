/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */

extern "C" {
#include <ovsdb_wrapper.h>
};
#include <physical_switch_ovsdb.h>
#include <logical_switch_ovsdb.h>

#include <oper/vn.h>
#include <physical_devices/tables/physical_device.h>
#include <physical_devices/tables/physical_device_vn.h>

using namespace AGENT;
using OVSDB::LogicalSwitchEntry;
using OVSDB::LogicalSwitchTable;
using OVSDB::OvsdbDBEntry;
using OVSDB::OvsdbDBObject;

LogicalSwitchEntry::LogicalSwitchEntry(OvsdbDBObject *table,
        const AGENT::PhysicalDeviceVnEntry *entry) : OvsdbDBEntry(table),
        name_(UuidToString(entry->vn()->GetUuid())), mcast_local_row_(NULL),
        mcast_remote_row_(NULL), old_mcast_remote_row_(NULL) {
    vxlan_id_ = entry->vn()->GetVxLanId();
    device_name_ = entry->device()->name();
}

LogicalSwitchEntry::LogicalSwitchEntry(OvsdbDBObject *table,
        const LogicalSwitchEntry *entry) : OvsdbDBEntry(table),
        mcast_local_row_(NULL), mcast_remote_row_(NULL),
        old_mcast_remote_row_(NULL) {
    name_ = entry->name_;
    vxlan_id_ = entry->vxlan_id_;;
    device_name_ = entry->device_name_;
}

LogicalSwitchEntry::LogicalSwitchEntry(OvsdbDBObject *table,
        struct ovsdb_idl_row *entry) : OvsdbDBEntry(table, entry),
        name_(ovsdb_wrapper_logical_switch_name(entry)), device_name_(""),
        vxlan_id_(ovsdb_wrapper_logical_switch_tunnel_key(entry)),
        mcast_local_row_(NULL), mcast_remote_row_(NULL),
        old_mcast_remote_row_(NULL) {
};

void LogicalSwitchEntry::AddMsg(struct ovsdb_idl_txn *txn) {
    struct ovsdb_idl_row *row =
        ovsdb_wrapper_add_logical_switch(txn, ovs_entry_, name_.c_str(),
                vxlan_id_);
    /* Add remote multicast entry if not already present */
    if (mcast_remote_row_ == NULL) {
        ovsdb_wrapper_add_mcast_mac_remote(txn, NULL, "unknown-dst", row,
                table_->client_idl()->tsn_ip().to_string().c_str());
    }
    if (old_mcast_remote_row_ != NULL) {
        ovsdb_wrapper_delete_mcast_mac_remote(old_mcast_remote_row_);
    }
}

void LogicalSwitchEntry::ChangeMsg(struct ovsdb_idl_txn *txn) {
    AddMsg(txn);
}

void LogicalSwitchEntry::DeleteMsg(struct ovsdb_idl_txn *txn) {
    if (mcast_local_row_ != NULL) {
        ovsdb_wrapper_delete_mcast_mac_local(mcast_local_row_);
    }
    if (mcast_remote_row_ != NULL) {
        ovsdb_wrapper_delete_mcast_mac_remote(mcast_remote_row_);
    }
    if (old_mcast_remote_row_ != NULL) {
        ovsdb_wrapper_delete_mcast_mac_remote(old_mcast_remote_row_);
    }
    ovsdb_wrapper_delete_logical_switch(ovs_entry_);
}

void LogicalSwitchEntry::OvsdbChange() {
    if (!IsResolved())
        table_->NotifyEvent(this, KSyncEntry::ADD_CHANGE_REQ);
}

bool LogicalSwitchEntry::Sync(DBEntry *db_entry) {
    PhysicalDeviceVnEntry *entry =
        static_cast<PhysicalDeviceVnEntry *>(db_entry);
    if (vxlan_id_ != entry->vn()->GetVxLanId()) {
        vxlan_id_ = entry->vn()->GetVxLanId();
        return true;
    }
    if (device_name_ != entry->device()->name()) {
        device_name_ = entry->device()->name();
        return true;
    }
    return false;
}

bool LogicalSwitchEntry::IsLess(const KSyncEntry &entry) const {
    const LogicalSwitchEntry &ps_entry =
        static_cast<const LogicalSwitchEntry&>(entry);
    return (name_.compare(ps_entry.name_) < 0);
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
                  boost::bind(&LogicalSwitchTable::OvsdbNotify, this, _1, _2));
    idl->Register(OvsdbClientIdl::OVSDB_MCAST_MAC_LOCAL,
                  boost::bind(&LogicalSwitchTable::OvsdbMcastLocalMacNotify,
                      this, _1, _2));
    idl->Register(OvsdbClientIdl::OVSDB_MCAST_MAC_REMOTE,
                  boost::bind(&LogicalSwitchTable::OvsdbMcastRemoteMacNotify,
                      this, _1, _2));
}

LogicalSwitchTable::~LogicalSwitchTable() {
    client_idl_->UnRegister(OvsdbClientIdl::OVSDB_LOGICAL_SWITCH);
    client_idl_->UnRegister(OvsdbClientIdl::OVSDB_MCAST_MAC_LOCAL);
    client_idl_->UnRegister(OvsdbClientIdl::OVSDB_MCAST_MAC_REMOTE);
}

void LogicalSwitchTable::OvsdbNotify(OvsdbClientIdl::Op op,
        struct ovsdb_idl_row *row) {
    if (op == OvsdbClientIdl::OVSDB_DEL) {
        printf("Delete of Logical Switch %s, VxLAN %ld\n",
                ovsdb_wrapper_logical_switch_name(row),
                ovsdb_wrapper_logical_switch_tunnel_key(row));
        LogicalSwitchEntry key(this, ovsdb_wrapper_logical_switch_name(row));
        NotifyDeleteOvsdb((OvsdbDBEntry*)&key);
    } else if (op == OvsdbClientIdl::OVSDB_ADD) {
        printf("Add/Change of Logical Switch %s, Vxlan %ld\n",
                ovsdb_wrapper_logical_switch_name(row),
                ovsdb_wrapper_logical_switch_tunnel_key(row));
        LogicalSwitchEntry key(this, ovsdb_wrapper_logical_switch_name(row));
        NotifyAddOvsdb((OvsdbDBEntry*)&key, row);
    } else {
        assert(0);
    }
}

void LogicalSwitchTable::OvsdbMcastLocalMacNotify(OvsdbClientIdl::Op op,
        struct ovsdb_idl_row *row) {
    const char *mac = ovsdb_wrapper_mcast_mac_local_mac(row);
    const char *ls = ovsdb_wrapper_mcast_mac_local_logical_switch(row);
    LogicalSwitchEntry *entry;
    if (ls) {
        LogicalSwitchEntry key(this, ls);
        entry = static_cast<LogicalSwitchEntry *>(Find(&key));
    }
    if (op == OvsdbClientIdl::OVSDB_DEL) {
        printf("Delete of local mcast mac %s, logical switch %s\n",
                mac, ls ? ls : "");
        if (entry) {
            entry->mcast_local_row_ = NULL;
        }
    } else if (op == OvsdbClientIdl::OVSDB_ADD) {
        printf("Add of local mcast mac %s, logical switch %s\n",
                mac, ls ? ls : "");
        if (entry) {
            entry->mcast_local_row_ = row;
        }
    } else {
        assert(0);
    }
}

void LogicalSwitchTable::OvsdbMcastRemoteMacNotify(OvsdbClientIdl::Op op,
        struct ovsdb_idl_row *row) {
    const char *mac = ovsdb_wrapper_mcast_mac_remote_mac(row);
    const char *ls = ovsdb_wrapper_mcast_mac_remote_logical_switch(row);
    LogicalSwitchEntry *entry = NULL;
    if (ls) {
        LogicalSwitchEntry key(this, ls);
        entry = static_cast<LogicalSwitchEntry *>(Find(&key));
    }
    if (op == OvsdbClientIdl::OVSDB_DEL) {
        printf("Delete of remote mcast mac %s, logical switch %s\n",
                mac, ls ? ls : "");
        if (entry) {
            if (entry->old_mcast_remote_row_ == row)
                entry->old_mcast_remote_row_ = NULL;
            if (entry->mcast_remote_row_ == row)
                entry->mcast_remote_row_ = NULL;
        }
    } else if (op == OvsdbClientIdl::OVSDB_ADD) {
        printf("Add of remote mcast mac %s, logical switch %s\n",
                mac, ls ? ls : "");
        if (entry) {
            entry->mcast_local_row_ = row;
            if (entry->mcast_remote_row_ != row) {
                entry->old_mcast_remote_row_ = entry->mcast_remote_row_;
                entry->mcast_remote_row_ = row;
                entry->OvsdbChange();
            }
        }
    } else {
        assert(0);
    }
    if (op == OvsdbClientIdl::OVSDB_DEL) {
        printf("Delete of Logical Switch %s, VxLAN %ld\n",
                ovsdb_wrapper_logical_switch_name(row),
                ovsdb_wrapper_logical_switch_tunnel_key(row));
    } else if (op == OvsdbClientIdl::OVSDB_ADD) {
        printf("Add/Change of Logical Switch %s, Vxlan %ld\n",
                ovsdb_wrapper_logical_switch_name(row),
                ovsdb_wrapper_logical_switch_tunnel_key(row));
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

OvsdbDBEntry *LogicalSwitchTable::AllocOvsEntry(struct ovsdb_idl_row *row) {
    LogicalSwitchEntry key(this, row);
    return static_cast<OvsdbDBEntry *>(Create(&key));
}

