/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */

#ifndef __OVSDDB_CLIENT__
#define __OVSDDB_CLIENT__

#include <cmn/agent_cmn.h>
#include <cmn/agent.h>
#include <agent_types.h>

class TorAgentParam;

class OvsdbClient {
public:
    OvsdbClient() {}
    virtual ~OvsdbClient() {}
    void Init();
    static OvsdbClient* Allocate(Agent *agent, TorAgentParam *params);
};

#endif //__OVSDDB_CLIENT__
