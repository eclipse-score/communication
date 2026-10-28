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

#include "Configuration.h"
#include "ConfigConstant.h"
#include <algorithm>
#include <cstdlib>
#include <exception/Exception.h>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <stdexcept>

#include <utils/log/formatLog.h>
#include <utils/log/logger.h>

namespace lgsomeip {

namespace {

const rapidjson::Value& require_member(const rapidjson::Value& object, const char* member, const char* context) {
    if (!object.IsObject() || !object.HasMember(member)) {
        throw LSAR_CONFIGURATION_ERROR(std::string("missing '") + member + "' in " + context);
    }
    return object[member];
}

void require_string(const rapidjson::Value& value, const char* member, const char* context) {
    if (!value.IsString()) {
        throw LSAR_CONFIGURATION_ERROR(std::string("'") + member + "' in " + context + " must be a string");
    }
}

void require_string_or_uint(const rapidjson::Value& value, const char* member, const char* context) {
    if (!value.IsString() && !value.IsUint()) {
        throw LSAR_CONFIGURATION_ERROR(std::string("'") + member + "' in " + context + " must be numeric");
    }
}

void require_optional_string(const rapidjson::Value& object, const char* member, const char* context) {
    if (object.HasMember(member)) {
        require_string(object[member], member, context);
    }
}

void require_optional_string_or_uint(const rapidjson::Value& object, const char* member, const char* context) {
    if (object.HasMember(member)) {
        require_string_or_uint(object[member], member, context);
    }
}

void require_optional_bool_or_string(const rapidjson::Value& object, const char* member, const char* context) {
    if (object.HasMember(member) && !object[member].IsBool() && !object[member].IsString()) {
        throw LSAR_CONFIGURATION_ERROR(std::string("'") + member + "' in " + context + " must be a boolean or string");
    }
}

void validate_service_shape(const rapidjson::Value& service) {
    if (!service.IsObject()) {
        throw LSAR_CONFIGURATION_ERROR("each service entry must be an object");
    }

    require_string(require_member(service, kConfigService, "service"), kConfigService, "service");
    require_string(require_member(service, kConfigInstance, "service"), kConfigInstance, "service");
    require_optional_string(service, kConfigName, "service");
    for (const char* member : {kConfigMajorVersion, kConfigMinorVersion, kConfigMinimumMinorVersion, kConfigUnreliable,
                               kConfigVlanPriority}) {
        require_optional_string(service, member, "service");
    }
    require_optional_bool_or_string(service, kConfigIsProvider, "service");
    require_optional_bool_or_string(service, kConfigSecureConnection, "service");

    for (const char* member : {kConfigReliable, kConfigMulticast}) {
        if (service.HasMember(member) && !service[member].IsObject()) {
            throw LSAR_CONFIGURATION_ERROR(std::string("'") + member + "' in service must be an object");
        }
    }

    for (const char* member : {kConfigSomeIpTp, kConfigEvents, kConfigEventGroups}) {
        if (service.HasMember(member) && !service[member].IsArray()) {
            throw LSAR_CONFIGURATION_ERROR(std::string("'") + member + "' in service must be an array");
        }
    }

    if (service.HasMember(kConfigSomeIpTp)) {
        for (const auto& method_or_event : service[kConfigSomeIpTp].GetArray()) {
            require_string(method_or_event, kConfigSomeIpTp, "service");
        }
    }

    if (service.HasMember(kConfigReliable)) {
        const auto& reliable = service[kConfigReliable];
        require_optional_string(reliable, kConfigPort, "reliable");
        require_optional_string(service, kConfigVlanPriority, "service");
        require_optional_bool_or_string(reliable, kConfigMagicCookies, "reliable");
    }

    if (service.HasMember(kConfigMulticast)) {
        const auto& multicast = service[kConfigMulticast];
        require_optional_string(multicast, kConfigMulticastAddress, "multicast");
        require_optional_string(multicast, kConfigPort, "multicast");
    }

    if (service.HasMember(kConfigEvents)) {
        for (const auto& event : service[kConfigEvents].GetArray()) {
            if (!event.IsObject()) {
                throw LSAR_CONFIGURATION_ERROR("each event entry must be an object");
            }
            require_string(require_member(event, kConfigEvent, "event"), kConfigEvent, "event");
            require_optional_bool_or_string(event, kConfigIsField, "event");
            require_optional_string_or_uint(event, kConfigUpdateCycle, "event");
        }
    }

    if (service.HasMember(kConfigEventGroups)) {
        for (const auto& event_group : service[kConfigEventGroups].GetArray()) {
            if (!event_group.IsObject()) {
                throw LSAR_CONFIGURATION_ERROR("each eventgroup entry must be an object");
            }
            require_string(require_member(event_group, kConfigEventGroup, "eventgroup"), kConfigEventGroup,
                           "eventgroup");
            require_optional_bool_or_string(event_group, kConfigIsMulticast, "eventgroup");
            require_optional_string_or_uint(event_group, kConfigThreshold, "eventgroup");
            if (event_group.HasMember(kConfigMulticast)) {
                const auto& multicast = event_group[kConfigMulticast];
                if (!multicast.IsObject()) {
                    throw LSAR_CONFIGURATION_ERROR("'multicast' in eventgroup must be an object");
                }
                require_optional_string(multicast, kConfigMulticastAddress, "eventgroup multicast");
                require_optional_string(multicast, kConfigPort, "eventgroup multicast");
            }
            require_member(event_group, kConfigEvents, "eventgroup");
            if (!event_group[kConfigEvents].IsArray()) {
                throw LSAR_CONFIGURATION_ERROR("'events' in eventgroup must be an array");
            }
            for (const auto& event : event_group[kConfigEvents].GetArray()) {
                require_string(event, kConfigEvent, "eventgroup events");
            }
        }
    }
}

void validate_configuration_document(const rapidjson::Document& document) {
    if (!document.IsObject()) {
        throw LSAR_CONFIGURATION_ERROR("configuration root must be a JSON object");
    }

    require_string(require_member(document, kConfigAddress, "configuration"), kConfigAddress, "configuration");
    if (document.HasMember(kConfigIpType)) {
        require_string(document[kConfigIpType], kConfigIpType, "configuration");
    }

    const auto& applications = require_member(document, kConfigApplications, "configuration");
    if (!applications.IsArray()) {
        throw LSAR_CONFIGURATION_ERROR("'applications' must be an array");
    }
    for (const auto& application : applications.GetArray()) {
        if (!application.IsObject()) {
            throw LSAR_CONFIGURATION_ERROR("each application entry must be an object");
        }
        require_string(require_member(application, kConfigName, "application"), kConfigName, "application");
        require_string_or_uint(require_member(application, kConfigId, "application"), kConfigId, "application");
    }

    if (document.HasMember(kConfigServices)) {
        const auto& services = document[kConfigServices];
        if (!services.IsArray()) {
            throw LSAR_CONFIGURATION_ERROR("'services' must be an array");
        }
        for (const auto& service : services.GetArray()) {
            validate_service_shape(service);
        }
    }

    if (document.HasMember(kConfigE2E)) {
        const auto& e2e = document[kConfigE2E];
        if (!e2e.IsObject()) {
            throw LSAR_CONFIGURATION_ERROR("'e2e' must be an object");
        }
        const auto& e2e_enabled = require_member(e2e, kConfigE2EEnabled, "e2e");
        if (!e2e_enabled.IsBool() && !e2e_enabled.IsString()) {
            throw LSAR_CONFIGURATION_ERROR("'e2e_enabled' must be a boolean or string");
        }
        const auto& protected_entries = require_member(e2e, kConfigProtected, "e2e");
        if (!protected_entries.IsArray()) {
            throw LSAR_CONFIGURATION_ERROR("'protected' in e2e must be an array");
        }
        for (const auto& entry : protected_entries.GetArray()) {
            if (!entry.IsObject()) {
                throw LSAR_CONFIGURATION_ERROR("each e2e entry must be an object");
            }
            require_string_or_uint(require_member(entry, kConfigServiceId, "e2e entry"), kConfigServiceId, "e2e entry");
            require_string_or_uint(require_member(entry, kConfigEventId, "e2e entry"), kConfigEventId, "e2e entry");
            require_optional_string_or_uint(entry, kConfigDataId, "e2e entry");
            require_optional_string_or_uint(entry, kConfigCrcOffset, "e2e entry");
            require_optional_string_or_uint(entry, kConfigCounterOffset, "e2e entry");
            require_optional_string_or_uint(entry, kConfigDataIdMode, "e2e entry");
            require_optional_string_or_uint(entry, kConfigDataIdNibbleOffset, "e2e entry");
            require_optional_string_or_uint(entry, kConfigDataLength, "e2e entry");
            require_string(require_member(entry, kConfigVariant, "e2e entry"), kConfigVariant, "e2e entry");
            require_string(require_member(entry, kConfigProfile, "e2e entry"), kConfigProfile, "e2e entry");
        }
    }

    if (document.HasMember(kConfigServiceDiscovery)) {
        const auto& service_discovery = document[kConfigServiceDiscovery];
        if (!service_discovery.IsObject()) {
            throw LSAR_CONFIGURATION_ERROR("'service-discovery' must be an object");
        }
        const auto& enabled = require_member(service_discovery, kConfigEnable, "service-discovery");
        if (!enabled.IsBool() && !enabled.IsString()) {
            throw LSAR_CONFIGURATION_ERROR("'enable' in service-discovery must be a boolean or string");
        }
        for (const char* member : {kConfigMulticast, kConfigProtocol}) {
            if (service_discovery.HasMember(member)) {
                require_string(service_discovery[member], member, "service-discovery");
            }
        }
        for (const char* member : {kConfigPort, kConfigVlanPriority, kConfigInitialDelayMin, kConfigInitialDelayMax,
                                   kConfigRepetitionsBaseDelay, kConfigRepetitionsMax, kConfigTtl,
                                   kConfigCyclicOfferDelay, kConfigRequestResponseDelay}) {
            if (service_discovery.HasMember(member)) {
                require_string_or_uint(service_discovery[member], member, "service-discovery");
            }
        }
    }
}

template <typename Type> Type parse_unsigned(const rapidjson::Value& value, const char* member, int base) {
    unsigned long parsed = 0;
    if (value.IsUint()) {
        parsed = value.GetUint();
    } else if (value.IsString()) {
        std::size_t processed = 0;
        try {
            parsed = std::stoul(value.GetString(), &processed, base);
        } catch (const std::exception&) {
            throw LSAR_CONFIGURATION_ERROR(std::string("invalid numeric value for '") + member + "'");
        }
        if (processed != std::strlen(value.GetString())) {
            throw LSAR_CONFIGURATION_ERROR(std::string("invalid numeric value for '") + member + "'");
        }
    } else {
        throw LSAR_CONFIGURATION_ERROR(std::string("'") + member + "' must be numeric");
    }

    if (parsed > std::numeric_limits<Type>::max()) {
        throw LSAR_CONFIGURATION_ERROR(std::string("numeric value for '") + member + "' is out of range");
    }
    return static_cast<Type>(parsed);
}

} // namespace

Configuration::Configuration(std::string path) : log_level_("info") {
    initialize(path);
}

Configuration::~Configuration() {}

void Configuration::initialize(std::string path) {
    bool address_valid = false;
    bool multicast_valid = false;
    std::string config_path{path};

    if (path.empty()) {
        const char* environment_path = std::getenv("LGSOMEIP_CONFIGURATION");
        if (environment_path != nullptr && *environment_path != '\0') {
            config_path = environment_path;
        } else {
            config_path = kConfigDefaultPath;
        }
    }

    // read a json file
    std::ifstream config_file(config_path);
    if (config_file.fail()) {
        lgsomeip::Logger::instance().init(log_level_, log_console_enabled_, log_file_enabled_, log_file_path_);
        LGSOMEIP_LOG_WARN << "Configuration file does not exist. Using default-configuration.";
        ip_type_ = 4;
        address_ = "127.0.0.1";
        service_discovery_.initialize();
        has_config_ = false;

        return;
    }
    rapidjson::IStreamWrapper input_stream(config_file);
    rapidjson::Document document;
    document.ParseStream(input_stream);
    if (document.HasParseError()) {
        throw LSAR_CONFIGURATION_ERROR("configuration file contains invalid JSON");
    }
    validate_configuration_document(document);

    rapidjson::Value::ConstMemberIterator member_iterator;

    if ((member_iterator = document.FindMember(kConfigLogging)) != document.MemberEnd()) {
        const rapidjson::Value& logging = member_iterator->value;
        if (logging.IsObject()) {
            auto parse_bool = [](const rapidjson::Value& value, bool fallback) {
                if (value.IsBool()) {
                    return value.GetBool();
                }
                if (value.IsString()) {
                    std::string text = value.GetString();
                    std::for_each(text.begin(), text.end(), [](char& character) {
                        if (character >= 'A' && character <= 'Z') {
                            character = static_cast<char>(character - 'A' + 'a');
                        }
                    });
                    if (text == "true") {
                        return true;
                    }
                    if (text == "false") {
                        return false;
                    }
                }
                return fallback;
            };

            if (logging.HasMember(kConfigLogLevel) && logging[kConfigLogLevel].IsString()) {
                log_level_ = logging[kConfigLogLevel].GetString();
            }
            if (logging.HasMember(kConfigLogConsole)) {
                log_console_enabled_ = parse_bool(logging[kConfigLogConsole], log_console_enabled_);
            }
            if (logging.HasMember(kConfigLogFile) && logging[kConfigLogFile].IsObject()) {
                const rapidjson::Value& file = logging[kConfigLogFile];
                if (file.HasMember(kConfigEnable)) {
                    log_file_enabled_ = parse_bool(file[kConfigEnable], log_file_enabled_);
                }
                if (file.HasMember(kConfigLogPath) && file[kConfigLogPath].IsString()) {
                    log_file_path_ = file[kConfigLogPath].GetString();
                }
            }
        }
    }

    // Initialize the logger with the configured level and output destinations.
    lgsomeip::Logger::instance().init(log_level_, log_console_enabled_, log_file_enabled_, log_file_path_);

    // Load the unicast address.
    if ((member_iterator = document.FindMember(kConfigAddress)) == document.MemberEnd()) {
        throw LSAR_CONFIGURATION_ERROR("Configuration must include 'unicast' item");
    }

    address_ = member_iterator->value.GetString();
    // Use IPv4 by default when the configuration does not specify an IP version.
    if ((member_iterator = document.FindMember(kConfigIpType)) != document.MemberEnd()) {
        std::string ip_type_name = (member_iterator->value).GetString();
        std::for_each(ip_type_name.begin(), ip_type_name.end(), [](char& c) { c = tolower(c); });
        ip_type_ = (ip_type_name == "ipv6") ? 6 : 4;
    } else if (address_.find(":") != std::string::npos) {
        ip_type_ = 6;
    }

    // Load the maximum payload size.
    if ((member_iterator = document.FindMember(kConfigMaxPayloadSize)) != document.MemberEnd()) {
        max_payload_size_ = parse_unsigned<int>(member_iterator->value, kConfigMaxPayloadSize, 10);
    }

    // Load application names and IDs.
    const rapidjson::Value& applications = document[kConfigApplications];
    std::set<std::uint16_t> application_ids;
    for (rapidjson::SizeType i = 0; i < applications.Size(); i++) { // Uses SizeType instead of size_t
        const std::string application_name = applications[i][kConfigName].GetString();
        const auto application_id = parse_unsigned<std::uint16_t>(applications[i][kConfigId], kConfigId, 16);
        if (applications_.find(application_name) != applications_.end()) {
            throw LSAR_CONFIGURATION_ERROR("Duplicate application name in configuration: " + application_name);
        }
        if (!application_ids.insert(application_id).second) {
            throw LSAR_CONFIGURATION_ERROR("Duplicate application ID in configuration: " +
                                           message_id_to_string(application_id, 4));
        }
        applications_.emplace(application_name, application_id);
    }

    // loading information of service
    if ((member_iterator = document.FindMember(kConfigServices)) != document.MemberEnd()) {
        std::uint16_t service_id;
        std::uint16_t instance_id;
        const rapidjson::Value& services = member_iterator->value;

        for (rapidjson::SizeType i = 0; i < services.Size(); i++) {
            service_id = parse_unsigned<std::uint16_t>(services[i][kConfigService], kConfigService, 16);
            instance_id = parse_unsigned<std::uint16_t>(services[i][kConfigInstance], kConfigInstance, 16);
            auto& service = services_[service_id][instance_id];
            try {
                service.initialize(services[i], address_, ip_type_);
            } catch (const ConfigurationErrorException&) {
                throw;
            } catch (const std::exception& exception) {
                throw LSAR_CONFIGURATION_ERROR(std::string("invalid service configuration: ") + exception.what());
            }
        }
    }

    // loading E2E
    if ((member_iterator = document.FindMember(kConfigE2E)) != document.MemberEnd()) {
        auto& e2e_entries = member_iterator->value[kConfigProtected];
        std::uint16_t service_id;
        std::uint16_t event_id;

        if (document[kConfigE2E][kConfigE2EEnabled].IsBool()) {
            e2e_enabled_ = document[kConfigE2E][kConfigE2EEnabled].GetBool();
        } else {
            e2e_enabled_ = (strcmp(document[kConfigE2E][kConfigE2EEnabled].GetString(), "true") == 0) ? true : false;
        }

        for (rapidjson::SizeType i = 0; i < e2e_entries.Size(); i++) {
            service_id = parse_unsigned<std::uint16_t>(e2e_entries[i][kConfigServiceId], kConfigServiceId, 16);
            event_id = parse_unsigned<std::uint16_t>(e2e_entries[i][kConfigEventId], kConfigEventId, 16);
            vsomeip::e2exf::data_identifier identifier = {service_id, event_id};
            try {
                e2e_configs_[identifier] = std::make_shared<ConfigE2E>(e2e_entries[i], service_id, event_id);
            } catch (...) {
                throw LSAR_CONFIGURATION_ERROR("e2e.initialize error. please check the json file.");
            }
        }
    }

    // loading information of ServiceDiscovery
    if ((member_iterator = document.FindMember(kConfigServiceDiscovery)) != document.MemberEnd()) {
        try {
            service_discovery_.initialize(member_iterator->value);
        } catch (const ConfigurationErrorException&) {
            throw;
        } catch (const std::exception& exception) {
            throw LSAR_CONFIGURATION_ERROR(std::string("invalid service-discovery configuration: ") + exception.what());
        }
    }

    if (ip_type_ == 6) {
        char ipv6_address[16];
        address_valid = inet_pton(AF_INET6, address_.c_str(), (void*)&ipv6_address);
        multicast_valid = inet_pton(AF_INET6, service_discovery_.get_multicast().c_str(), (void*)&ipv6_address);
    } else {
        std::uint32_t ipv4_address;
        address_valid = inet_pton(AF_INET, address_.c_str(), (void*)&ipv4_address);
        multicast_valid = inet_pton(AF_INET, service_discovery_.get_multicast().c_str(), (void*)&ipv4_address);
    }

    if (address_valid == false || multicast_valid == false) {
        throw LSAR_CONFIGURATION_ERROR(
            "Invalid IP address in configuration file. Please check the unicast and multicast addresses.");
    }
}

#if defined(ENABLE_SOMEIP_PACKET_FILTERING)
void Configuration::configure_someip_packet_filter(void) {
    // read a json file
    std::ifstream config_file(kConfigSomeIpPacketFilterPath);
    if (config_file.fail()) {
        LGSOMEIP_LOG_INFO << "Configuration file doesn't exist for SOME/IP Packet Filtering ("
                          << kConfigSomeIpPacketFilterPath << ").";

        return;
    } else {
        LGSOMEIP_LOG_DEBUG << "Configuration file exists for SOME/IP Packet Filtering ("
                           << kConfigSomeIpPacketFilterPath << ").";
    }

    rapidjson::IStreamWrapper input_stream(config_file);
    rapidjson::Document document;
    document.ParseStream(input_stream);
    if (document.HasParseError() || !document.IsObject()) {
        throw LSAR_CONFIGURATION_ERROR("packet-filter configuration must be a JSON object");
    }

    rapidjson::Value::ConstMemberIterator member_iterator;

    if ((member_iterator = document.FindMember(kConfigServices)) != document.MemberEnd()) {
        const rapidjson::Value& services = member_iterator->value;
        if (!services.IsArray()) {
            throw LSAR_CONFIGURATION_ERROR("'services' in packet-filter configuration must be an array");
        }
        std::uint32_t message_id;
        std::uint32_t service_id;

        LGSOMEIP_LOG_DEBUG << "Configuration::configure_someip_packet_filter / services are found, # of services = "
                           << services.Size();

        for (rapidjson::SizeType i = 0; i < services.Size(); i++) {
            if (!services[i].IsObject()) {
                throw LSAR_CONFIGURATION_ERROR("each packet-filter service entry must be an object");
            }
            service_id = parse_unsigned<std::uint32_t>(
                require_member(services[i], kConfigService, "packet-filter service"), kConfigService, 0);

            if ((member_iterator = services[i].FindMember(kConfigEvents)) != services[i].MemberEnd()) {
                const rapidjson::Value& events = member_iterator->value;
                if (!events.IsArray()) {
                    throw LSAR_CONFIGURATION_ERROR("'events' in packet-filter service must be an array");
                }
                std::uint32_t event_id;
                std::uint16_t protected_interval;

                LGSOMEIP_LOG_DEBUG << "Configuration::configure_someip_packet_filter / events are found, # of events = "
                                   << events.Size();

                for (rapidjson::SizeType j = 0; j < events.Size(); j++) {
                    if (!events[j].IsObject()) {
                        throw LSAR_CONFIGURATION_ERROR("each packet-filter event entry must be an object");
                    }
                    event_id = parse_unsigned<std::uint32_t>(
                        require_member(events[j], kConfigId, "packet-filter event"), kConfigId, 0);
                    protected_interval = parse_unsigned<std::uint16_t>(
                        require_member(events[j], kConfigProtectedIntervalMs, "packet-filter event"),
                        kConfigProtectedIntervalMs, 10);
                    message_id = service_id << 16 | event_id;
                    LGSOMEIP_LOG_DEBUG << "Configuration::configure_someip_packet_filter / service_id = "
                                       << MSGID_FORMAT4(service_id) << ", event_id = " << MSGID_FORMAT4(event_id)
                                       << ", message_id = " << MSGID_FORMAT8(message_id);

                    packet_filter_list_[message_id] = protected_interval;

                    LGSOMEIP_LOG_DEBUG << "Configuration::configure_someip_packet_filter / protected_interval = "
                                       << this->packet_filter_list_[message_id];
                }
            }
        }
    }
}
#endif // ENABLE_SOMEIP_PACKET_FILTERING

std::string Configuration::get_address() const {
    return address_;
}

std::uint16_t Configuration::get_ip_type() const {
    return ip_type_;
}

int Configuration::get_max_payload_size() const {
    return max_payload_size_;
}

std::uint16_t Configuration::get_application_id(std::string name) {
    return applications_[name];
}

ServiceInfo* Configuration::get_service_info(std::uint16_t service_id, std::uint16_t instance_id) {
    const auto& service = services_.find(service_id);
    if (service == services_.end())
        return nullptr;

    if (instance_id == 0xFFFF) {
        if (service->second.size() > 0)
            return &std::begin(service->second)->second;
    } else {
        const auto& instance = service->second.find(instance_id);
        if (instance != std::end(service->second))
            return &instance->second;
    }

    return nullptr;
}

ConfigurationSD* Configuration::get_service_discovery_info() {
    return &service_discovery_;
}

bool Configuration::is_e2e_enabled() const {
    return e2e_enabled_;
}

bool Configuration::has_config() const {
    return has_config_;
}

std::map<vsomeip::e2exf::data_identifier, std::shared_ptr<ConfigE2E>> Configuration::get_e2e_configs() {
    return e2e_configs_;
}

} // namespace lgsomeip
