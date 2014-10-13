/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */
#include <ovsdb_object.h>
#include <ovsdb_entry.h>
#include <ovsdb_api.h>

bool OvsdbDBEntry::Add() {
    OvsdbDBObject *object = static_cast<OvsdbDBObject*>(GetObject());
    struct ovsdb_idl_txn *txn = object->client_idl_->CreateTxn();
    AddMsg(txn);
    struct jsonrpc_msg *msg = ovsdb_idl_txn_encode(txn);
    if (msg == NULL)
        return true;
    object->client_idl_->SendJsonRpc(msg);
    return false;
}

bool OvsdbDBEntry::Change() {
    OvsdbDBObject *object = static_cast<OvsdbDBObject*>(GetObject());
    struct ovsdb_idl_txn *txn = object->client_idl_->CreateTxn();
    ChangeMsg(txn);
    struct jsonrpc_msg *msg = ovsdb_idl_txn_encode(txn);
    if (msg == NULL)
        return true;
    object->client_idl_->SendJsonRpc(msg);
    return false;
}

bool OvsdbDBEntry::Delete() {
    OvsdbDBObject *object = static_cast<OvsdbDBObject*>(GetObject());
    struct ovsdb_idl_txn *txn = object->client_idl_->CreateTxn();
    DeleteMsg(txn);
    struct jsonrpc_msg *msg = ovsdb_idl_txn_encode(txn);
    if (msg == NULL)
        return true;
    object->client_idl_->SendJsonRpc(msg);
    return false;
}

