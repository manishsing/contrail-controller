/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */

#ifndef __OVSDB_CLIENT_IDL_H__
#define __OVSDB_CLIENT_IDL_H__

#include <assert.h>

#include <cmn/agent_cmn.h>
#include <cmn/agent.h>
#include <agent_types.h>


class OvsdbClientSession;
class PhysicalSwitchTable;
class LogicalSwitchTable;

class OvsdbClientIdl {
public:
    enum Op {
        OVSDB_DEL = 0,
        OVSDB_ADD,
        OVSDB_INVALID_OP
    };

    enum EntryType {
        OVSDB_PHYSICAL_SWITCH = 0,
        OVSDB_LOGICAL_SWITCH,
        OVSDB_TYPE_COUNT
    };
    typedef boost::function<void(OvsdbClientIdl::Op, struct ovsdb_idl_row *)> NotifyCB;

    OvsdbClientIdl(OvsdbClientSession *session, Agent *agent);
    virtual ~OvsdbClientIdl();

    void MessageProcess(const u_int8_t *buf, std::size_t len);
    void SendMointorReq();
    void SendJsonRpc(struct jsonrpc_msg *msg);
    struct ovsdb_idl_txn *CreateTxn();
    void Register(EntryType type, NotifyCB cb) {callback_[type] = cb;}
    void UnRegister(EntryType type) {callback_[type] = NULL;}
    PhysicalSwitchTable *physical_switch_table() {return physical_switch_table_.get();}
private:
    //virtual void EncodeData(struct ovsdb_idl_txn *txn);
    friend void ovsdb_api_idl_callback(void *, int, struct ovsdb_idl_row *);

    struct ovsdb_idl *idl_;
    struct json_parser * parser_;
    const struct vteprec_global *vtep_global_;
    OvsdbClientSession *session_;
    NotifyCB callback_[OVSDB_TYPE_COUNT];
    std::auto_ptr<PhysicalSwitchTable> physical_switch_table_;
    std::auto_ptr<LogicalSwitchTable> logical_switch_table_;
};

class OvsdbClientSession {
public:
    OvsdbClientSession(Agent *agent) : client_idl_(this, agent), agent_(agent) {}
    virtual ~OvsdbClientSession() {}

    virtual void SendMsg(u_int8_t *buf, std::size_t len) = 0;
    void MessageProcess(const u_int8_t *buf, std::size_t len) {
        client_idl_.MessageProcess(buf, len);
    }

    void OnEstablish() {
        client_idl_.SendMointorReq();
    }

    void OnClose() {
        assert(0);
    }
private:
    friend class OvsdbClientIdl;
    OvsdbClientIdl client_idl_;
    Agent *agent_;
};

#endif // __OVSDB_CLIENT_IDL_H__
