/*
 * Copyright (c) 2014 Juniper Networks, Inc. All rights reserved.
 */

#ifndef tsn_vrf_listener_h_
#define tsn_vrf_listener_h_

class TsnVrfListener {
public:
    TsnVrfListener(Agent *agent);
    virtual ~TsnVrfListener();
    void VrfNotify(DBEntryBase *entry);

private:
    Agent *agent_;
    DBTableBase::ListenerId vrf_table_listener_id_;
    DISALLOW_COPY_AND_ASSIGN(TsnVrfListener);
};

#endif // tsn_vrf_listener_h_
