/*
 * Copyright (c) 2013 Juniper Networks, Inc. All rights reserved.
 */
#ifndef vnsw_agent_physical_interface_hpp
#define vnsw_agent_physical_interface_hpp
struct PhysicalInterfaceBaseData;

/////////////////////////////////////////////////////////////////////////////
// Implementation of Physical Ports
// Can be Ethernet Ports or LAG Ports
// Name of port is used as key
/////////////////////////////////////////////////////////////////////////////
class PhysicalInterface : public Interface {
public:
    enum SubType {
        FABRIC,     // Physical port connecting to fabric network
        VMWARE,     // For vmware, port connecting to contrail-vm-portgroup
        CONFIG,     // Interface created from config
        EXTERNAL,   // Interface created from config on external device
        INVALID
    };

    enum EncapType {
        ETHERNET,       // Ethernet with ARP
        RAW_IP          // No L2 header. Packets sent as raw-ip
    };

    PhysicalInterface(const boost::uuids::uuid &uuid, const std::string &name,
                      VrfEntry *vrf);
    virtual ~PhysicalInterface();
    virtual bool CmpInterface(const DBEntry &rhs) const;
    virtual std::string ToString() const;
    virtual KeyPtr GetDBRequestKey() const;
    virtual void Add();
    virtual bool Delete(const DBRequest *req);
    virtual bool OnChange(const InterfaceTable *table,
                          const PhysicalInterfaceBaseData *data);
    void PostAdd();
    virtual void ConfigEventHandler(IFMapNode *node);

    SubType subtype() const { return subtype_; }
    // Lets kernel know if physical interface is to be kept after agent exits or
    // dies. If its true keep the interface, else remove it.
    // Currently only vnware physical interface is persistent.
    // By default every physical interface is non-persistent.
    bool persistent() const {return persistent_;}
    EncapType encap_type() const { return encap_type_; }
    bool no_arp() const { return no_arp_; }
    PhysicalDeviceEntry *physical_device() const {
        return physical_device_.get();
    }

    // Helper functions
    static void CreateReq(InterfaceTable *table, const std::string &ifname,
                          const std::string &vrf_name, SubType subtype,
                          EncapType encap, bool no_arp);
    static void Create(InterfaceTable *table, const std::string &ifname,
                       const std::string &vrf_name, SubType sub_type,
                       EncapType encap, bool no_arp);
    static void DeleteReq(InterfaceTable *table, const std::string &ifname);
    static void Delete(InterfaceTable *table, const std::string &ifname);

    friend class PhysicalInterfaceKey;
private:
    std::string fq_name_;
    std::string display_name_;
    PhysicalDeviceEntryRef physical_device_;
    bool persistent_;
    SubType subtype_;
    EncapType encap_type_;
    bool no_arp_;
    DISALLOW_COPY_AND_ASSIGN(PhysicalInterface);
};

struct PhysicalInterfaceKey : public InterfaceKey {
    PhysicalInterfaceKey(const boost::uuids::uuid &uuid,
                         const std::string &name);
    ~PhysicalInterfaceKey();

    Interface *AllocEntry(const InterfaceTable *table) const;
    Interface *AllocEntry(const InterfaceTable *table,
                          const InterfaceData *data) const;
    InterfaceKey *Clone() const;
};

struct PhysicalInterfaceBaseData : public InterfaceData {
    PhysicalInterfaceBaseData(const std::string &vrf_name,
                              PhysicalInterface::SubType subtype,
                              PhysicalInterface::EncapType encap,
                              bool no_arp);
    virtual ~PhysicalInterfaceBaseData() { }

    PhysicalInterface::SubType subtype_;
    PhysicalInterface::EncapType encap_;
    bool no_arp_;
};

struct PhysicalInterfaceData : public PhysicalInterfaceBaseData {
    PhysicalInterfaceData(const std::string &vrf_name,
                          PhysicalInterface::SubType subtype,
                          PhysicalInterface::EncapType encaptype,
                          bool no_arp);
    virtual ~PhysicalInterfaceData() {}
};

struct ConfigPhysicalInterfaceData : public PhysicalInterfaceBaseData {
    ConfigPhysicalInterfaceData(const std::string &vrf_name,
                                const std::string &fq_name,
                                const boost::uuids::uuid &device_uuid,
                                PhysicalInterface::SubType subtype,
                                PhysicalInterface::EncapType encaptype,
                                bool no_arp);
    virtual ~ConfigPhysicalInterfaceData() {}

    std::string fq_name_;
    boost::uuids::uuid device_uuid_;
};

#endif // vnsw_agent_physical_interface_hpp
