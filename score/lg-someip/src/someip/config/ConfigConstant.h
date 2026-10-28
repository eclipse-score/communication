/********************************************************************************
 * Copyright (C) 2017-2026 LG Electronics Inc.
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ********************************************************************************/

#ifndef LG_SOMEIP_CONFIG_CONFIGCONSTANT_H
#define LG_SOMEIP_CONFIG_CONFIGCONSTANT_H

namespace lgsomeip {

constexpr char kConfigDefaultPath[] = "/etc/lgsomeip_config.json";

#if defined(ENABLE_SOMEIP_PACKET_FILTERING)
constexpr char kConfigSomeIpPacketFilterPath[] = "/etc/someip_packet_filter_config.json";
constexpr char kConfigProtectedIntervalMs[] = "protected_interval_ms";
#endif // ENABLE_SOMEIP_PACKET_FILTERING

constexpr char kConfigLogging[] = "logging";
constexpr char kConfigLogLevel[] = "level";
constexpr char kConfigLogConsole[] = "console";
constexpr char kConfigLogFile[] = "file";
constexpr char kConfigLogPath[] = "path";
constexpr char kConfigAddress[] = "unicast";
constexpr char kConfigMaxPayloadSize[] = "someiptpmaxpayloadsize";
constexpr char kConfigIpType[] = "iptype";
constexpr char kConfigReliable[] = "reliable";
constexpr char kConfigMagicCookies[] = "enable-magic-cookies";
constexpr char kConfigService[] = "service";
constexpr char kConfigServices[] = "services";
constexpr char kConfigInstance[] = "instance";
constexpr char kConfigMajorVersion[] = "major_version";
constexpr char kConfigMinorVersion[] = "minor_version";
constexpr char kConfigMinimumMinorVersion[] = "minimum_minor_version";
constexpr char kConfigMinimumMinorPolicy[] = "minimum_minor_policy";
constexpr char kConfigPort[] = "port";
constexpr char kConfigVlanPriority[] = "vlan_qos";
constexpr char kConfigUnreliable[] = "unreliable";
constexpr char kConfigSomeIpTp[] = "someiptp";
constexpr char kConfigIsProvider[] = "is-provider";
constexpr char kConfigSecureConnection[] = "secure-connection";
constexpr char kConfigEvents[] = "events";
constexpr char kConfigEvent[] = "event";
constexpr char kConfigIsField[] = "is_field";
constexpr char kConfigUpdateCycle[] = "update-cycle";
constexpr char kConfigEventGroups[] = "eventgroups";
constexpr char kConfigEventGroup[] = "eventgroup";
constexpr char kConfigApplications[] = "applications";
constexpr char kConfigName[] = "name";
constexpr char kConfigId[] = "id";
constexpr char kConfigServiceDiscovery[] = "service-discovery";
constexpr char kConfigEnable[] = "enable";
constexpr char kConfigMulticast[] = "multicast";
constexpr char kConfigMulticastAddress[] = "address";
constexpr char kConfigProtocol[] = "protocol";
constexpr char kConfigInitialDelayMin[] = "initial_delay_min";
constexpr char kConfigInitialDelayMax[] = "initial_delay_max";
constexpr char kConfigRepetitionsBaseDelay[] = "repetitions_base_delay";
constexpr char kConfigRepetitionsMax[] = "repetitions_max";
constexpr char kConfigTtl[] = "ttl";
constexpr char kConfigCyclicOfferDelay[] = "cyclic_offer_delay";
constexpr char kConfigRequestResponseDelay[] = "request_response_delay";
constexpr char kConfigThreshold[] = "threshold";
constexpr char kConfigIsMulticast[] = "is_multicast";

// E2E
constexpr char kConfigE2E[] = "e2e";
constexpr char kConfigE2EEnabled[] = "e2e_enabled";
constexpr char kConfigProtected[] = "protected";
constexpr char kConfigDataId[] = "data_id";
constexpr char kConfigServiceId[] = "service_id";
constexpr char kConfigEventId[] = "event_id";
constexpr char kConfigVariant[] = "variant";
constexpr char kConfigProfile[] = "profile";
constexpr char kConfigCrcOffset[] = "crc_offset";
constexpr char kConfigCounterOffset[] = "counter_offset";
constexpr char kConfigDataIdMode[] = "data_id_mode";
constexpr char kConfigDataIdNibbleOffset[] = "data_id_nibble_offset";
constexpr char kConfigDataLength[] = "data_length";
} // namespace lgsomeip

#endif // LG_SOMEIP_CONFIG_CONFIGCONSTANT_H
