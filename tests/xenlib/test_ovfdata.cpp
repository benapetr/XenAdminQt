#include "test_ovfdata.h"

const char* const kOvfXml_ovfXenConfig_qtFormat_parsesHvmBootPolicy = R"(<?xml version="1.0" encoding="UTF-8"?>
<Envelope xmlns="http://schemas.dmtf.org/ovf/envelope/1"
          xmlns:xen="http://www.citrix.com/xencenter/2009/xen"
          xmlns:ovf="http://schemas.dmtf.org/ovf/envelope/1">
  <References/>
  <VirtualSystem ovf:id="myvm">
    <Name>myvm</Name>
    <VirtualHardwareSection>
      <xen:Data ovf:required="false">
        <xen:Name>HVM_boot_policy</xen:Name>
        <xen:Value>BIOS order</xen:Value>
      </xen:Data>
      <xen:Data ovf:required="false">
        <xen:Name>platform</xen:Name>
        <xen:Value>nx=true;acpi=1</xen:Value>
      </xen:Data>
    </VirtualHardwareSection>
  </VirtualSystem>
</Envelope>)";

const char* const kOvfXml_ovfXenConfig_qtFormat_pvBootloader = R"(<?xml version="1.0" encoding="UTF-8"?>
<Envelope xmlns="http://schemas.dmtf.org/ovf/envelope/1"
          xmlns:xen="http://www.citrix.com/xencenter/2009/xen"
          xmlns:ovf="http://schemas.dmtf.org/ovf/envelope/1">
  <References/>
  <VirtualSystem ovf:id="pvvm">
    <Name>pvvm</Name>
    <VirtualHardwareSection>
      <xen:Data ovf:required="false">
        <xen:Name>PV_bootloader</xen:Name>
        <xen:Value>pygrub</xen:Value>
      </xen:Data>
      <xen:Data ovf:required="false">
        <xen:Name>PV_args</xen:Name>
        <xen:Value>quiet splash</xen:Value>
      </xen:Data>
    </VirtualHardwareSection>
  </VirtualSystem>
</Envelope>)";

const char* const kOvfXml_ovfXenConfig_csFormat_parsesBootPolicy = R"(<?xml version="1.0" encoding="UTF-8"?>
<Envelope xmlns="http://schemas.dmtf.org/ovf/envelope/1"
          xmlns:citrix="http://schemas.citrix.com/ovf/envelope/1"
          xmlns:ovf="http://schemas.dmtf.org/ovf/envelope/1">
  <References/>
  <VirtualSystem ovf:id="csvm">
    <Name>csvm</Name>
    <VirtualHardwareSection>
      <citrix:VirtualSystemOtherConfigurationData Name="HVM_boot_policy">
        <Value>BIOS order</Value>
      </citrix:VirtualSystemOtherConfigurationData>
      <citrix:VirtualSystemOtherConfigurationData Name="HVM_boot_params">
        <Value>order=dc</Value>
      </citrix:VirtualSystemOtherConfigurationData>
    </VirtualHardwareSection>
  </VirtualSystem>
</Envelope>)";

const char* const kOvfXml_ovfXenConfig_csFormat_recommendationsKeyExcluded = R"(<?xml version="1.0" encoding="UTF-8"?>
<Envelope xmlns="http://schemas.dmtf.org/ovf/envelope/1"
          xmlns:citrix="http://schemas.citrix.com/ovf/envelope/1"
          xmlns:ovf="http://schemas.dmtf.org/ovf/envelope/1">
  <References/>
  <VirtualSystem ovf:id="vm1">
    <Name>vm1</Name>
    <VirtualHardwareSection>
      <citrix:VirtualSystemOtherConfigurationData Name="recommendations">
        <Value><restrictions><restriction field="vcpus-max" value="8"/></restrictions></Value>
      </citrix:VirtualSystemOtherConfigurationData>
      <citrix:VirtualSystemOtherConfigurationData Name="HVM_boot_policy">
        <Value>BIOS order</Value>
      </citrix:VirtualSystemOtherConfigurationData>
    </VirtualHardwareSection>
  </VirtualSystem>
</Envelope>)";

const char* const kOvfXml_ovfXenConfig_noXenSection_returnsEmptyMap = R"(<?xml version="1.0" encoding="UTF-8"?>
<Envelope xmlns="http://schemas.dmtf.org/ovf/envelope/1"
          xmlns:ovf="http://schemas.dmtf.org/ovf/envelope/1">
  <References/>
  <VirtualSystem ovf:id="plainvm">
    <Name>plainvm</Name>
    <VirtualHardwareSection/>
  </VirtualSystem>
</Envelope>)";

const char* const kOvfXml_ovfXenConfig_multipleVms_configsAreSeparate = R"(<?xml version="1.0" encoding="UTF-8"?>
<Envelope xmlns="http://schemas.dmtf.org/ovf/envelope/1"
          xmlns:xen="http://www.citrix.com/xencenter/2009/xen"
          xmlns:ovf="http://schemas.dmtf.org/ovf/envelope/1">
  <References/>
  <VirtualSystemCollection ovf:id="myvapp">
    <VirtualSystem ovf:id="vm-hvm">
      <Name>vm-hvm</Name>
      <VirtualHardwareSection>
        <xen:Data ovf:required="false">
          <xen:Name>HVM_boot_policy</xen:Name>
          <xen:Value>BIOS order</xen:Value>
        </xen:Data>
      </VirtualHardwareSection>
    </VirtualSystem>
    <VirtualSystem ovf:id="vm-pv">
      <Name>vm-pv</Name>
      <VirtualHardwareSection>
        <xen:Data ovf:required="false">
          <xen:Name>PV_bootloader</xen:Name>
          <xen:Value>pygrub</xen:Value>
        </xen:Data>
      </VirtualHardwareSection>
    </VirtualSystem>
  </VirtualSystemCollection>
</Envelope>)";

const char* const kOvfXml_ovfXenConfig_emptyValueAllowed = R"(<?xml version="1.0" encoding="UTF-8"?>
<Envelope xmlns="http://schemas.dmtf.org/ovf/envelope/1"
          xmlns:xen="http://www.citrix.com/xencenter/2009/xen"
          xmlns:ovf="http://schemas.dmtf.org/ovf/envelope/1">
  <References/>
  <VirtualSystem ovf:id="uefivm">
    <Name>uefivm</Name>
    <VirtualHardwareSection>
      <xen:Data ovf:required="false">
        <xen:Name>HVM_boot_policy</xen:Name>
        <xen:Value></xen:Value>
      </xen:Data>
    </VirtualHardwareSection>
  </VirtualSystem>
</Envelope>)";
