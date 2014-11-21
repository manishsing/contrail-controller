/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */

#include <db/db.h>
#include <cmn/agent_factory.h>
#include <cmn/agent_cmn.h>
#include <init/agent_param.h>
#include <oper/operdb_init.h>
#include <oper/ifmap_dependency_manager.h>
#include <oper/physical_device.h>
#include <oper/physical_device_vn.h>

#include <physical_devices/tables/device_manager.h>
#include <physical_devices/tsn/tsn_vrf_listener.h>

using boost::assign::map_list_of;
using boost::assign::list_of;

SandeshTraceBufferPtr
PhysicalDeviceManagerTraceBuf(SandeshTraceBufferCreate("Device Manager", 500));

void PhysicalDeviceManager::CreateDBTables(DB *db) {
}

void PhysicalDeviceManager::RegisterDBClients() {
    if (agent_->tsn_enabled()) {
        tsn_vrf_listener_.reset(new TsnVrfListener(agent()));
    }
}

void PhysicalDeviceManager::Init() {
}

void PhysicalDeviceManager::Shutdown() {
}
