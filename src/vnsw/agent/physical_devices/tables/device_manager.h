/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */
#ifndef SRC_VNSW_AGENT_PHYSICAL_DEVICES_TABLES_DEVICE_MANAGER_H_
#define SRC_VNSW_AGENT_PHYSICAL_DEVICES_TABLES_DEVICE_MANAGER_H_

#include <cmn/agent_cmn.h>
#include <cmn/agent.h>
#include <agent_types.h>

namespace AGENT {
class PhysicalPortTable;
class LogicalPortTable;
}
class TsnVrfListener;

class PhysicalDeviceManager {
 public:
    explicit PhysicalDeviceManager(Agent *agent) : agent_(agent) { }
    virtual ~PhysicalDeviceManager() { }

    void CreateDBTables(DB* db);
    void RegisterDBClients();
    void Init();
    void Shutdown();

    Agent *agent() const { return agent_; }

    AGENT::PhysicalPortTable *physical_port_table() const {
        return physical_port_table_;
    }
    AGENT::LogicalPortTable *logical_port_table() const {
        return logical_port_table_;
    }

 private:
    Agent *agent_;
    AGENT::PhysicalPortTable *physical_port_table_;
    AGENT::LogicalPortTable *logical_port_table_;
    std::auto_ptr<TsnVrfListener> tsn_vrf_listener_;

    DISALLOW_COPY_AND_ASSIGN(PhysicalDeviceManager);
};

#endif  // SRC_VNSW_AGENT_PHYSICAL_DEVICES_TABLES_DEVICE_MANAGER_H_
