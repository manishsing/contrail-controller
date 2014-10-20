/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */

#ifndef SRC_VNSW_AGENT_PHYSICAL_DEVICES_OVS_TOR_AGENT_OVSDB_CLIENT_OVSDB_CLIENT_IDL_H_
#define SRC_VNSW_AGENT_PHYSICAL_DEVICES_OVS_TOR_AGENT_OVSDB_CLIENT_OVSDB_CLIENT_IDL_H_

#include <assert.h>

#include <cmn/agent_cmn.h>
#include <cmn/agent.h>
#include <agent_types.h>

extern SandeshTraceBufferPtr OvsdbTraceBuf;
#define OVSDB_TRACE(obj, ...)\
do {\
    Ovsdb##obj::TraceMsg(OvsdbTraceBuf, __FILE__, __LINE__, __VA_ARGS__);\
} while(false);

namespace OVSDB {
class OvsdbClientSession;
class PhysicalSwitchTable;
class LogicalSwitchTable;
class PhysicalPortTable;
class VlanPortBindingTable;
class PhysicalLocatorTable;
class UnicastMacLocalTable;
class OvsdbEntryBase;

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
        OVSDB_PHYSICAL_PORT,
        OVSDB_PHYSICAL_LOCATOR,
        OVSDB_UCAST_MAC_LOCAL,
        OVSDB_UCAST_MAC_REMOTE,
        OVSDB_PHYSICAL_LOCATOR_SET,
        OVSDB_MCAST_MAC_LOCAL,
        OVSDB_MCAST_MAC_REMOTE,
        OVSDB_TYPE_COUNT
    };
    typedef boost::function<void(OvsdbClientIdl::Op, struct ovsdb_idl_row *)> NotifyCB;
    typedef std::map<struct ovsdb_idl_txn *, OvsdbEntryBase *> PendingTxnMap;

    OvsdbClientIdl(OvsdbClientSession *session, Agent *agent);
    virtual ~OvsdbClientIdl();

    // Send request to start monitoring OVSDB server
    void SendMointorReq();
    // Send encode json rpc messgage to OVSDB server
    void SendJsonRpc(struct jsonrpc_msg *msg);
    // Process the recevied message and trigger update to ovsdb client
    void MessageProcess(const u_int8_t *buf, std::size_t len);
    // Create a OVSDB transaction to start encoding an update
    struct ovsdb_idl_txn *CreateTxn(OvsdbEntryBase *entry);
    // Delete the OVSDB transaction
    void DeleteTxn(struct ovsdb_idl_txn *txn);
    void Register(EntryType type, NotifyCB cb) {callback_[type] = cb;}
    void UnRegister(EntryType type) {callback_[type] = NULL;}
    // Get TOR Service Node IP
    Ip4Address tsn_ip();

    PhysicalSwitchTable *physical_switch_table() {return physical_switch_table_.get();}
    LogicalSwitchTable *logical_switch_table() {return logical_switch_table_.get();}
    PhysicalPortTable *physical_port_table() {return physical_port_table_.get();}
#if 0 //TODO
    PhysicalLocatorTable *physical_locator_table() {return physical_locator_table_.get();}
#endif
private:
    friend void ovsdb_wrapper_idl_callback(void *, int, struct ovsdb_idl_row *);
    friend void ovsdb_wrapper_idl_txn_ack(void *, struct ovsdb_idl_txn *);

    struct ovsdb_idl *idl_;
    struct json_parser * parser_;
    const struct vteprec_global *vtep_global_;
    OvsdbClientSession *session_;
    NotifyCB callback_[OVSDB_TYPE_COUNT];
    PendingTxnMap pending_txn_;
    std::auto_ptr<PhysicalSwitchTable> physical_switch_table_;
    std::auto_ptr<LogicalSwitchTable> logical_switch_table_;
    std::auto_ptr<PhysicalPortTable> physical_port_table_;
    std::auto_ptr<VlanPortBindingTable> vlan_port_table_;
#if 0 //TODO
    std::auto_ptr<PhysicalLocatorTable> physical_locator_table_;
    std::auto_ptr<UnicastMacLocalTable> unicast_mac_local_table_;
#endif
    DISALLOW_COPY_AND_ASSIGN(OvsdbClientIdl);
};

class OvsdbClientSession {
public:
    OvsdbClientSession(Agent *agent) : client_idl_(this, agent), agent_(agent) {}
    virtual ~OvsdbClientSession() {}

    virtual Ip4Address tsn_ip() = 0;
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
    DISALLOW_COPY_AND_ASSIGN(OvsdbClientSession);
};
};

#endif // SRC_VNSW_AGENT_PHYSICAL_DEVICES_OVS_TOR_AGENT_OVSDB_CLIENT_OVSDB_CLIENT_IDL_H_

