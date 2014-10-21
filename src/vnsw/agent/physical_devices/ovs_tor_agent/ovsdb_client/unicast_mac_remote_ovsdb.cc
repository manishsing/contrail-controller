/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */

extern "C" {
#include <ovsdb_wrapper.h>
};
#include <logical_switch_ovsdb.h>
#include <unicast_mac_remote_ovsdb.h>

#include <oper/vn.h>
#include <oper/vrf.h>
#include <oper/nexthop.h>
#include <oper/tunnel_nh.h>
#include <oper/agent_path.h>
#include <oper/layer2_route.h>
#include <ovsdb_types.h>

using namespace AGENT;
using OVSDB::UnicastMacRemoteEntry;
using OVSDB::UnicastMacRemoteTable;
using OVSDB::VrfOvsdbObject;
using OVSDB::OvsdbDBEntry;
using OVSDB::OvsdbDBObject;

UnicastMacRemoteEntry::UnicastMacRemoteEntry(OvsdbDBObject *table,
        const std::string mac, const std::string logical_switch) :
    OvsdbDBEntry(table), mac_(mac), logical_switch_name_(logical_switch),
    self_exported_route_(false) {
}

UnicastMacRemoteEntry::UnicastMacRemoteEntry(OvsdbDBObject *table,
        const Layer2RouteEntry *entry) : OvsdbDBEntry(table),
        mac_(entry->GetAddress().ToString()),
        logical_switch_name_(UuidToString(entry->vrf()->vn()->GetUuid())) {
}

UnicastMacRemoteEntry::UnicastMacRemoteEntry(OvsdbDBObject *table,
        const UnicastMacRemoteEntry *entry) : OvsdbDBEntry(table),
        mac_(entry->mac_), logical_switch_name_(entry->logical_switch_name_),
        dest_ip_(entry->dest_ip_) {
}

UnicastMacRemoteEntry::UnicastMacRemoteEntry(OvsdbDBObject *table,
        struct ovsdb_idl_row *entry) : OvsdbDBEntry(table, entry),
    mac_(ovsdb_wrapper_ucast_mac_remote_mac(entry)),
    logical_switch_name_(ovsdb_wrapper_ucast_mac_remote_logical_switch(entry)),
    dest_ip_() {
    const char *dest_ip = ovsdb_wrapper_ucast_mac_remote_dst_ip(entry);
    if (dest_ip) {
        dest_ip_ = std::string(dest_ip);
    }
};

void UnicastMacRemoteEntry::AddMsg(struct ovsdb_idl_txn *txn) {
    if (self_exported_route_) {
        return;
    }
    LogicalSwitchTable *l_table = table_->client_idl()->logical_switch_table();
    LogicalSwitchEntry key(l_table, logical_switch_name_.c_str());
    LogicalSwitchEntry *logical_switch =
        static_cast<LogicalSwitchEntry *>(l_table->GetReference(&key));
    logical_switch_ = logical_switch;
    if (ovs_entry_ == NULL && !dest_ip_.empty()) {
        obvsdb_wrapper_add_ucast_mac_remote(txn, mac_.c_str(),
                logical_switch->ovs_entry(), dest_ip_.c_str());
        SendTrace(UnicastMacRemoteEntry::ADD_REQ);
    }
}

void UnicastMacRemoteEntry::ChangeMsg(struct ovsdb_idl_txn *txn) {
    AddMsg(txn);
}

void UnicastMacRemoteEntry::DeleteMsg(struct ovsdb_idl_txn *txn) {
    logical_switch_ = NULL;
    ovsdb_wrapper_delete_ucast_mac_remote(ovs_entry_);
    SendTrace(UnicastMacRemoteEntry::DEL_REQ);
}

void UnicastMacRemoteEntry::OvsdbChange() {
    if (!IsResolved())
        table_->NotifyEvent(this, KSyncEntry::ADD_CHANGE_REQ);
}

bool UnicastMacRemoteEntry::Sync(DBEntry *db_entry) {
    const Layer2RouteEntry *entry =
        static_cast<const Layer2RouteEntry *>(db_entry);
    std::string dest_ip;
    const NextHop *nh = entry->GetActiveNextHop();
    /* 
     * TOR Agent will not have any local VM so only tunnel nexthops
     * are to be looked into
     */
    if (nh && nh->GetType() == NextHop::TUNNEL) {
        /*
         * we don't care the about the tunnel type in nh and always program
         * the entry to ovsdb expecting vrouter to always handle
         * VxLAN encapsulation.
         */
        const TunnelNH *tunnel = static_cast<const TunnelNH *>(nh);
        dest_ip = tunnel->GetDip()->to_string();
    }
    bool change = false;
    if (dest_ip_ != dest_ip) {
        dest_ip_ = dest_ip;
        change = true;
    }
    bool self_exported_route =
        (entry->FindPath((Peer *)table_->client_idl()->route_peer()) != NULL);
    if (self_exported_route_ != self_exported_route) {
        self_exported_route_ = self_exported_route;
        change = true;
    }
    return change;
}

bool UnicastMacRemoteEntry::IsLess(const KSyncEntry &entry) const {
    const UnicastMacRemoteEntry &ucast =
        static_cast<const UnicastMacRemoteEntry&>(entry);
    if (mac_ != ucast.mac_)
        return mac_ < ucast.mac_;
    return logical_switch_name_ < ucast.logical_switch_name_;
}

KSyncEntry *UnicastMacRemoteEntry::UnresolvedReference() {
    LogicalSwitchTable *l_table = table_->client_idl()->logical_switch_table();
    LogicalSwitchEntry key(l_table, logical_switch_name_.c_str());
    LogicalSwitchEntry *l_switch =
        static_cast<LogicalSwitchEntry *>(l_table->GetReference(&key));
    if (!l_switch->IsResolved()) {
        return l_switch;
    }
    return NULL;
}

void UnicastMacRemoteEntry::SendTrace(Trace event) const {
    SandeshUnicastMacRemoteInfo info;
    switch (event) {
    case ADD_REQ:
        info.set_op("Add Requested");
        break;
    case DEL_REQ:
        info.set_op("Delete Requested");
        break;
    case ADD_ACK:
        info.set_op("Add Received");
        break;
    case DEL_ACK:
        info.set_op("Delete Received");
        break;
    default:
        info.set_op("unknown");
    }
    info.set_mac(mac_);
    info.set_logical_switch(logical_switch_name_);
    info.set_dest_ip(dest_ip_);
    OVSDB_TRACE(UnicastMacRemote, info);
}

UnicastMacRemoteTable::UnicastMacRemoteTable(OvsdbClientIdl *idl, DBTable *table) :
    OvsdbDBObject(idl, table) {
    //idl->Register(OvsdbClientIdl::OVSDB_UCAST_MAC_REMOTE,
    //              boost::bind(&UnicastMacRemoteTable::OvsdbNotify, this, _1, _2));
}

UnicastMacRemoteTable::~UnicastMacRemoteTable() {
    //client_idl_->UnRegister(OvsdbClientIdl::OVSDB_UCAST_MAC_REMOTE);
}

void UnicastMacRemoteTable::OvsdbNotify(OvsdbClientIdl::Op op,
        struct ovsdb_idl_row *row) {
    const char *mac = ovsdb_wrapper_ucast_mac_remote_mac(row);
    const char *logical_switch =
        ovsdb_wrapper_ucast_mac_remote_logical_switch(row);
    /* if logical switch is not available ignore nodtification */
    if (logical_switch == NULL)
        return;
    const char *dest_ip = ovsdb_wrapper_ucast_mac_remote_dst_ip(row);
    UnicastMacRemoteEntry key(this, mac, logical_switch);
    if (op == OvsdbClientIdl::OVSDB_DEL) {
        NotifyDeleteOvsdb((OvsdbDBEntry*)&key);
        if (dest_ip)
            key.dest_ip_ = std::string(dest_ip);
        key.SendTrace(UnicastMacRemoteEntry::DEL_ACK);
    } else if (op == OvsdbClientIdl::OVSDB_ADD) {
        NotifyAddOvsdb((OvsdbDBEntry*)&key, row);
        if (dest_ip)
            key.dest_ip_ = std::string(dest_ip);
        key.SendTrace(UnicastMacRemoteEntry::ADD_ACK);
    } else {
        assert(0);
    }
}

KSyncEntry *UnicastMacRemoteTable::Alloc(const KSyncEntry *key, uint32_t index) {
    const UnicastMacRemoteEntry *k_entry =
        static_cast<const UnicastMacRemoteEntry *>(key);
    UnicastMacRemoteEntry *entry = new UnicastMacRemoteEntry(this, k_entry);
    return entry;
}

KSyncEntry *UnicastMacRemoteTable::DBToKSyncEntry(const DBEntry* db_entry) {
    const Layer2RouteEntry *entry =
        static_cast<const Layer2RouteEntry *>(db_entry);
    UnicastMacRemoteEntry *key = new UnicastMacRemoteEntry(this, entry);
    return static_cast<KSyncEntry *>(key);
}

OvsdbDBEntry *UnicastMacRemoteTable::AllocOvsEntry(struct ovsdb_idl_row *row) {
    UnicastMacRemoteEntry key(this, row);
    return static_cast<OvsdbDBEntry *>(Create(&key));
}

VrfOvsdbObject::VrfOvsdbObject(OvsdbClientIdl *idl, DBTable *table) :
    client_idl_(idl), table_(table) {
    vrf_listener_id_ = table->Register(boost::bind(&VrfOvsdbObject::VrfNotify,
                this, _1, _2));
    client_idl_->Register(OvsdbClientIdl::OVSDB_UCAST_MAC_REMOTE,
            boost::bind(&VrfOvsdbObject::OvsdbRouteNotify, this, _1, _2));
}

VrfOvsdbObject::~VrfOvsdbObject() {
    table_->Unregister(vrf_listener_id_);
    client_idl_->UnRegister(OvsdbClientIdl::OVSDB_UCAST_MAC_REMOTE);
}

void VrfOvsdbObject::OvsdbRouteNotify(OvsdbClientIdl::Op op,
        struct ovsdb_idl_row *row) {
    const char *mac = ovsdb_wrapper_ucast_mac_remote_mac(row);
    const char *logical_switch =
        ovsdb_wrapper_ucast_mac_remote_logical_switch(row);
    /* if logical switch is not available ignore notification */
    if (logical_switch == NULL)
        return;
    LogicalSwitchMap::iterator it = logical_switch_map_.find(logical_switch);
    if (it == logical_switch_map_.end()) {
        // TODO encode the row delete and send
        return;
    }
    const char *dest_ip = ovsdb_wrapper_ucast_mac_remote_dst_ip(row);
    UnicastMacRemoteTable *table= it->second->l2_table.get();
    UnicastMacRemoteEntry key(table, mac, logical_switch);
    if (op == OvsdbClientIdl::OVSDB_DEL) {
        table->NotifyDeleteOvsdb((OvsdbDBEntry*)&key);
        if (dest_ip)
            key.dest_ip_ = std::string(dest_ip);
        key.SendTrace(UnicastMacRemoteEntry::DEL_ACK);
    } else if (op == OvsdbClientIdl::OVSDB_ADD) {
        table->NotifyAddOvsdb((OvsdbDBEntry*)&key, row);
        if (dest_ip)
            key.dest_ip_ = std::string(dest_ip);
        key.SendTrace(UnicastMacRemoteEntry::ADD_ACK);
    } else {
        assert(0);
    }
}

void VrfOvsdbObject::VrfNotify(DBTablePartBase *partition, DBEntryBase *e) {
    VrfEntry *vrf = static_cast<VrfEntry *>(e);
    VrfState *state = static_cast<VrfState *>
        (vrf->GetState(partition->parent(), vrf_listener_id_));
    if (vrf->IsDeleted()) {
        if (state) {
            logical_switch_map_.erase(state->logical_switch_name_);
            vrf->ClearState(partition->parent(), vrf_listener_id_);
            delete state;
        }
        return;
    }

    if (vrf->vn() == NULL) {
        return;
    }

    if (state == NULL) {
        state = new VrfState();
        state->logical_switch_name_ = UuidToString(vrf->vn()->GetUuid());
        // Assumption one vn maps only to one vrf
        logical_switch_map_[state->logical_switch_name_] = state;
        vrf->SetState(partition->parent(), vrf_listener_id_, state);

        /* We are interested only in L2 Routes */
        state->l2_table.reset(new UnicastMacRemoteTable(client_idl_,
                    vrf->GetLayer2RouteTable()));
    }
}

