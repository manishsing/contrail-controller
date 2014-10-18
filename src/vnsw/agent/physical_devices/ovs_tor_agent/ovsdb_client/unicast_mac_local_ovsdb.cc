/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */

extern "C" {
#include <ovsdb_api.h>
};
#include <unicast_mac_local_ovsdb.h>

UnicastMacLocalEntry::UnicastMacLocalEntry(UnicastMacLocalTable *table,
        struct ovsdb_idl_row *ovsdb_row, std::string mac, std::string ip_addr,
        std::string ls_name) : OvsdbEntry(table),
    ovsdb_row_(ovsdb_row), mac_(mac), ip_addr_(ip_addr), ls_name_(ls_name) {
}

bool UnicastMacLocalEntry::IsLess(const KSyncEntry &entry) const {
    const UnicastMacLocalEntry &uc_entry =
        static_cast<const UnicastMacLocalEntry&>(entry);
    return (ovsdb_row_ < uc_entry.ovsdb_row_);
}

KSyncEntry *UnicastMacLocalEntry::UnresolvedReference() {
    return NULL;
}

UnicastMacLocalTable::UnicastMacLocalTable(OvsdbClientIdl *idl) :
    OvsdbObject(idl) {
    idl->Register(OvsdbClientIdl::OVSDB_UCAST_MAC_LOCAL,
                  boost::bind(&UnicastMacLocalTable::Notify, this, _1, _2));
}

UnicastMacLocalTable::~UnicastMacLocalTable() {
    client_idl_->UnRegister(OvsdbClientIdl::OVSDB_UCAST_MAC_LOCAL);
}

void UnicastMacLocalTable::Notify(OvsdbClientIdl::Op op,
        struct ovsdb_idl_row *row) {
    /* if logical switch is not avialable, trigger entry delete */
    bool delete_entry =
        (ovsdb_wrapper_ucast_mac_local_logical_switch(row) == NULL);
    const char *ls_name = ovsdb_wrapper_ucast_mac_local_logical_switch(row);
    if (ls_name == NULL)
        ls_name = "";
    UnicastMacLocalEntry key(this, row, ovsdb_wrapper_ucast_mac_local_mac(row),
            ovsdb_wrapper_ucast_mac_local_ip(row), ls_name);
    UnicastMacLocalEntry *entry =
        static_cast<UnicastMacLocalEntry *>(Find(&key));
    if (op == OvsdbClientIdl::OVSDB_DEL || delete_entry) {
        printf("Delete of unicast local mac %s, IP %s, Logical Switch %s\n",
                ovsdb_wrapper_ucast_mac_local_mac(row),
                ovsdb_wrapper_ucast_mac_local_ip(row), ls_name);
        if (entry != NULL)
            Delete(entry);
    } else if (op == OvsdbClientIdl::OVSDB_ADD) {
        printf("Add/Change of unicast local mac %s, IP %s, Logical Switch %s\n",
                ovsdb_wrapper_ucast_mac_local_mac(row),
                ovsdb_wrapper_ucast_mac_local_ip(row), ls_name);
        if (entry == NULL)
            Create(&key);
    } else {
        assert(0);
    }
}

KSyncEntry *UnicastMacLocalTable::Alloc(const KSyncEntry *key, uint32_t index) {
    const UnicastMacLocalEntry *k_entry =
        static_cast<const UnicastMacLocalEntry *>(key);
    UnicastMacLocalEntry *entry = new UnicastMacLocalEntry(this,
            k_entry->ovsdb_row_, k_entry->mac_, k_entry->ip_addr_,
            k_entry->ls_name_);
    return entry;
}

