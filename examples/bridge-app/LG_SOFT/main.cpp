/*
 *
 *    Copyright (c) 2021 Project CHIP Authors
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include <AppMain.h>
#include <platform/CHIPDeviceLayer.h>
#include <platform/PlatformManager.h>

#include <app-common/zap-generated/ids/Attributes.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/AttributeAccessInterfaceRegistry.h>
#include <app/ConcreteAttributePath.h>
#include <app/EventLogging.h>
#include <app/reporting/reporting.h>
#include <app/util/af-types.h>
#include <app/util/attribute-storage.h>
#include <app/util/endpoint-config-api.h>
#include <app/util/util.h>
#include <credentials/DeviceAttestationCredsProvider.h>
#include <credentials/examples/DeviceAttestationCredsExample.h>
#include <lib/core/CHIPError.h>
#include <lib/support/CHIPMem.h>
#include <lib/support/ZclString.h>
#include <platform/CommissionableDataProvider.h>
#include <setup_payload/QRCodeSetupPayloadGenerator.h>
#include <setup_payload/SetupPayload.h>

#include <pthread.h>
#include <sys/ioctl.h>

#include "CommissionableInit.h"
#include "Device.h"
#include "main.h"
#include <app/server/Server.h>

#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include <mosquitto.h>
#include <json/json.h>
#include <getopt.h> // For command-line argument parsing
#include <nlohmann/json.hpp>
#include <exception>

using json = nlohmann::json;

using namespace chip;
using namespace chip::app;
using namespace chip::Credentials;
using namespace chip::Inet;
using namespace chip::Transport;
using namespace chip::DeviceLayer;
using namespace chip::app::Clusters;

struct mosquitto *mqtt_client = nullptr;

namespace {

const int kNodeLabelSize = 32;
const int kUniqueIdSize  = 32;
// Current ZCL implementation of Struct uses a max-size array of 254 bytes
const int kDescriptorAttributeArraySize = 254;

EndpointId gCurrentEndpointId;
EndpointId gFirstDynamicEndpointId;
// Power source is on the same endpoint as the composed device
Device * gDevices[CHIP_DEVICE_CONFIG_DYNAMIC_ENDPOINT_COUNT + 1];
std::vector<Room *> gRooms;
std::vector<Action *> gActions;

const int16_t minMeasuredValue     = -27315;
const int16_t maxMeasuredValue     = 32766;
const int16_t initialMeasuredValue = 100;

// ENDPOINT DEFINITIONS:
// =================================================================================
//
// Endpoint definitions will be reused across multiple endpoints for every instance of the
// endpoint type.
// There will be no intrinsic storage for the endpoint attributes declared here.
// Instead, all attributes will be treated as EXTERNAL, and therefore all reads
// or writes to the attributes must be handled within the emberAfExternalAttributeWriteCallback
// and emberAfExternalAttributeReadCallback functions declared herein. This fits
// the typical model of a bridge, since a bridge typically maintains its own
// state database representing the devices connected to it.

// Device types for dynamic endpoints: TODO Need a generated file from ZAP to define these!
// (taken from matter-devices.xml)
#define DEVICE_TYPE_BRIDGED_NODE 0x0013
// (taken from lo-devices.xml)
#define DEVICE_TYPE_LO_ON_OFF_LIGHT 0x0100
// (taken from matter-devices.xml)
#define DEVICE_TYPE_POWER_SOURCE 0x0011
// (taken from matter-devices.xml)
#define DEVICE_TYPE_TEMP_SENSOR 0x0302

// Device Version for dynamic endpoints:
#define DEVICE_VERSION_DEFAULT 1

// ---------------------------------------------------------------------------
//
// LIGHT ENDPOINT: contains the following clusters:
//   - On/Off
//   - Descriptor
//   - Bridged Device Basic Information

// Declare On/Off cluster attributes
DECLARE_DYNAMIC_ATTRIBUTE_LIST_BEGIN(onOffAttrs)
DECLARE_DYNAMIC_ATTRIBUTE(OnOff::Attributes::OnOff::Id, BOOLEAN, 1, 0), /* on/off */
    DECLARE_DYNAMIC_ATTRIBUTE_LIST_END();

// Declare Descriptor cluster attributes
DECLARE_DYNAMIC_ATTRIBUTE_LIST_BEGIN(descriptorAttrs)
DECLARE_DYNAMIC_ATTRIBUTE(Descriptor::Attributes::DeviceTypeList::Id, ARRAY, kDescriptorAttributeArraySize, 0), /* device list */
    DECLARE_DYNAMIC_ATTRIBUTE(Descriptor::Attributes::ServerList::Id, ARRAY, kDescriptorAttributeArraySize, 0), /* server list */
    DECLARE_DYNAMIC_ATTRIBUTE(Descriptor::Attributes::ClientList::Id, ARRAY, kDescriptorAttributeArraySize, 0), /* client list */
    DECLARE_DYNAMIC_ATTRIBUTE(Descriptor::Attributes::PartsList::Id, ARRAY, kDescriptorAttributeArraySize, 0),  /* parts list */
    DECLARE_DYNAMIC_ATTRIBUTE_LIST_END();

// Declare Bridged Device Basic Information cluster attributes
DECLARE_DYNAMIC_ATTRIBUTE_LIST_BEGIN(bridgedDeviceBasicAttrs)
DECLARE_DYNAMIC_ATTRIBUTE(BridgedDeviceBasicInformation::Attributes::NodeLabel::Id, CHAR_STRING, kNodeLabelSize, 0), /* NodeLabel */
    DECLARE_DYNAMIC_ATTRIBUTE(BridgedDeviceBasicInformation::Attributes::Reachable::Id, BOOLEAN, 1, 0),              /* Reachable */
    DECLARE_DYNAMIC_ATTRIBUTE(BridgedDeviceBasicInformation::Attributes::UniqueID::Id, CHAR_STRING, kUniqueIdSize, 0),
    DECLARE_DYNAMIC_ATTRIBUTE(BridgedDeviceBasicInformation::Attributes::FeatureMap::Id, BITMAP32, 4, 0), /* feature map */
    DECLARE_DYNAMIC_ATTRIBUTE_LIST_END();

// Declare Cluster List for Bridged Light endpoint
// TODO: It's not clear whether it would be better to get the command lists from
// the ZAP config on our last fixed endpoint instead.
constexpr CommandId onOffIncomingCommands[] = {
    app::Clusters::OnOff::Commands::Off::Id,
    app::Clusters::OnOff::Commands::On::Id,
    app::Clusters::OnOff::Commands::Toggle::Id,
    app::Clusters::OnOff::Commands::OffWithEffect::Id,
    app::Clusters::OnOff::Commands::OnWithRecallGlobalScene::Id,
    app::Clusters::OnOff::Commands::OnWithTimedOff::Id,
    kInvalidCommandId,
};

DECLARE_DYNAMIC_CLUSTER_LIST_BEGIN(bridgedLightClusters)
DECLARE_DYNAMIC_CLUSTER(OnOff::Id, onOffAttrs, ZAP_CLUSTER_MASK(SERVER), onOffIncomingCommands, nullptr),
    DECLARE_DYNAMIC_CLUSTER(Descriptor::Id, descriptorAttrs, ZAP_CLUSTER_MASK(SERVER), nullptr, nullptr),
    DECLARE_DYNAMIC_CLUSTER(BridgedDeviceBasicInformation::Id, bridgedDeviceBasicAttrs, ZAP_CLUSTER_MASK(SERVER), nullptr,
                            nullptr) DECLARE_DYNAMIC_CLUSTER_LIST_END;

// Declare Bridged Light endpoint

DECLARE_DYNAMIC_ENDPOINT(bridgedLightEndpoint, bridgedLightClusters);
DataVersion gLightDataVersions[ArraySize(bridgedLightClusters)];

// Setup composed device with two temperature sensors and a power source
ComposedDevice gComposedDevice("Composed Device", "Bedroom");
DeviceTempSensor ComposedTempSensor1("Composed TempSensor 1", "Bedroom", minMeasuredValue, maxMeasuredValue, initialMeasuredValue);
DeviceTempSensor ComposedTempSensor2("Composed TempSensor 2", "Bedroom", minMeasuredValue, maxMeasuredValue, initialMeasuredValue);
DevicePowerSource ComposedPowerSource("Composed Power Source", "Bedroom", PowerSource::Feature::kBattery);

Room room1("Room 1", 0xE001, Actions::EndpointListTypeEnum::kRoom, true);
Room room2("Room 2", 0xE002, Actions::EndpointListTypeEnum::kRoom, true);
Room room3("Zone 3", 0xE003, Actions::EndpointListTypeEnum::kZone, false);

Action action1(0x1001, "Room 1 On", Actions::ActionTypeEnum::kAutomation, 0xE001, 0x1, Actions::ActionStateEnum::kInactive, true);
Action action2(0x1002, "Turn On Room 2", Actions::ActionTypeEnum::kAutomation, 0xE002, 0x01, Actions::ActionStateEnum::kInactive,
               true);
Action action3(0x1003, "Turn Off Room 1", Actions::ActionTypeEnum::kAutomation, 0xE003, 0x01, Actions::ActionStateEnum::kInactive,
               false);

} // namespace

// REVISION DEFINITIONS:
// =================================================================================

#define ZCL_DESCRIPTOR_CLUSTER_REVISION (1u)
#define ZCL_BRIDGED_DEVICE_BASIC_INFORMATION_CLUSTER_REVISION (2u)
#define ZCL_BRIDGED_DEVICE_BASIC_INFORMATION_FEATURE_MAP (0u)
#define ZCL_FIXED_LABEL_CLUSTER_REVISION (1u)
#define ZCL_ON_OFF_CLUSTER_REVISION (4u)
#define ZCL_TEMPERATURE_SENSOR_CLUSTER_REVISION (1u)
#define ZCL_TEMPERATURE_SENSOR_FEATURE_MAP (0u)
#define ZCL_POWER_SOURCE_CLUSTER_REVISION (2u)

// ---------------------------------------------------------------------------

int AddDeviceEndpoint(Device * dev, EmberAfEndpointType * ep, const Span<const EmberAfDeviceType> & deviceTypeList,
                      const Span<DataVersion> & dataVersionStorage, chip::EndpointId parentEndpointId = chip::kInvalidEndpointId)
{
    uint8_t index = 0;
    while (index < CHIP_DEVICE_CONFIG_DYNAMIC_ENDPOINT_COUNT)
    {
        if (nullptr == gDevices[index])
        {
            gDevices[index] = dev;
            CHIP_ERROR err;
            while (true)
            {
                // Todo: Update this to schedule the work rather than use this lock
                DeviceLayer::StackLock lock;
                dev->SetEndpointId(gCurrentEndpointId);
                dev->SetParentEndpointId(parentEndpointId);
                err =
                    emberAfSetDynamicEndpoint(index, gCurrentEndpointId, ep, dataVersionStorage, deviceTypeList, parentEndpointId);
                if (err == CHIP_NO_ERROR)
                {
                    ChipLogProgress(DeviceLayer, "Added device %s to dynamic endpoint %d (index=%d)", dev->GetName(),
                                    gCurrentEndpointId, index);

                    if (dev->GetUniqueId()[0] == '\0')
                    {
                        dev->GenerateUniqueId();
                    }

                    return index;
                }
                if (err != CHIP_ERROR_ENDPOINT_EXISTS)
                {
                    gDevices[index] = nullptr;
                    return -1;
                }
                // Handle wrap condition
                if (++gCurrentEndpointId < gFirstDynamicEndpointId)
                {
                    gCurrentEndpointId = gFirstDynamicEndpointId;
                }
            }
        }
        index++;
    }
    ChipLogProgress(DeviceLayer, "Failed to add dynamic endpoint: No endpoints available!");
    return -1;
}

int RemoveDeviceEndpoint(Device * dev)
{
    uint8_t index = 0;
    while (index < CHIP_DEVICE_CONFIG_DYNAMIC_ENDPOINT_COUNT)
    {
        if (gDevices[index] == dev)
        {
            // Todo: Update this to schedule the work rather than use this lock
            DeviceLayer::StackLock lock;
            // Silence complaints about unused ep when progress logging
            // disabled.
            [[maybe_unused]] EndpointId ep = emberAfClearDynamicEndpoint(index);
            gDevices[index]                = nullptr;
            ChipLogProgress(DeviceLayer, "Removed device %s from dynamic endpoint %d (index=%d)", dev->GetName(), ep, index);
            return index;
        }
        index++;
    }
    return -1;
}

std::vector<EndpointListInfo> GetEndpointListInfo(chip::EndpointId parentId)
{
    std::vector<EndpointListInfo> infoList;

    for (auto room : gRooms)
    {
        if (room->getIsVisible())
        {
            EndpointListInfo info(room->getEndpointListId(), room->getName(), room->getType());
            int index = 0;
            while (index < CHIP_DEVICE_CONFIG_DYNAMIC_ENDPOINT_COUNT)
            {
                if ((gDevices[index] != nullptr) && (gDevices[index]->GetParentEndpointId() == parentId))
                {
                    std::string location;
                    if (room->getType() == Actions::EndpointListTypeEnum::kZone)
                    {
                        location = gDevices[index]->GetZone();
                    }
                    else
                    {
                        location = gDevices[index]->GetLocation();
                    }
                    if (room->getName().compare(location) == 0)
                    {
                        info.AddEndpointId(gDevices[index]->GetEndpointId());
                    }
                }
                index++;
            }
            if (info.GetEndpointListSize() > 0)
            {
                infoList.push_back(info);
            }
        }
    }

    return infoList;
}

std::vector<Action *> GetActionListInfo(chip::EndpointId parentId)
{
    return gActions;
}

namespace {
void CallReportingCallback(intptr_t closure)
{
    auto path = reinterpret_cast<app::ConcreteAttributePath *>(closure);
    MatterReportingAttributeChangeCallback(*path);
    Platform::Delete(path);
}

void ScheduleReportingCallback(Device * dev, ClusterId cluster, AttributeId attribute)
{
    auto * path = Platform::New<app::ConcreteAttributePath>(dev->GetEndpointId(), cluster, attribute);
    PlatformMgr().ScheduleWork(CallReportingCallback, reinterpret_cast<intptr_t>(path));
}
} // anonymous namespace

void HandleDeviceStatusChanged(Device * dev, Device::Changed_t itemChangedMask)
{
    if (itemChangedMask & Device::kChanged_Reachable)
    {
        ScheduleReportingCallback(dev, BridgedDeviceBasicInformation::Id, BridgedDeviceBasicInformation::Attributes::Reachable::Id);
    }

    if (itemChangedMask & Device::kChanged_Name)
    {
        ScheduleReportingCallback(dev, BridgedDeviceBasicInformation::Id, BridgedDeviceBasicInformation::Attributes::NodeLabel::Id);
    }
}


const EmberAfDeviceType gBridgedOnOffDeviceTypes[] = { { DEVICE_TYPE_LO_ON_OFF_LIGHT, DEVICE_VERSION_DEFAULT },
                                                       { DEVICE_TYPE_BRIDGED_NODE, DEVICE_VERSION_DEFAULT } };


void HandleMatterCommand(const std::string &matterCommand, const std::string &deviceId) {
    std::string deviceTopic = "zigbee2mqtt/" + deviceId + "/set";
    int ret = mosquitto_publish(mqtt_client, nullptr, deviceTopic.c_str(), 
                                static_cast<int>(matterCommand.length()), matterCommand.c_str(), 0, false);

    if (ret != MOSQ_ERR_SUCCESS) {
        ChipLogError(DeviceLayer, "Failed to publish Zigbee command to %s: %s", deviceTopic.c_str(), mosquitto_strerror(ret));
    } else {
        ChipLogProgress(DeviceLayer, "Published Zigbee command to %s: %s", deviceTopic.c_str(), matterCommand.c_str());
    }
}

void HandleDeviceOnOffStatusChanged(DeviceOnOff * dev, DeviceOnOff::Changed_t itemChangedMask)
{
    if (itemChangedMask & (DeviceOnOff::kChanged_Reachable | DeviceOnOff::kChanged_Name | DeviceOnOff::kChanged_Location))
    {
        HandleDeviceStatusChanged(static_cast<Device *>(dev), (Device::Changed_t) itemChangedMask);
    }

    if (itemChangedMask & DeviceOnOff::kChanged_OnOff) {
        ScheduleReportingCallback(dev, OnOff::Id, OnOff::Attributes::OnOff::Id);
    }
}


Protocols::InteractionModel::Status HandleReadBridgedDeviceBasicAttribute(Device * dev, chip::AttributeId attributeId,
                                                                          uint8_t * buffer, uint16_t maxReadLength)
{
    using namespace BridgedDeviceBasicInformation::Attributes;

    ChipLogProgress(DeviceLayer, "HandleReadBridgedDeviceBasicAttribute: attrId=%d, maxReadLength=%d", attributeId, maxReadLength);

    if ((attributeId == Reachable::Id) && (maxReadLength == 1))
    {
        *buffer = dev->IsReachable() ? 1 : 0;
    }
    else if ((attributeId == NodeLabel::Id) && (maxReadLength == 32))
    {
        MutableByteSpan zclNameSpan(buffer, maxReadLength);
        MakeZclCharString(zclNameSpan, dev->GetName());
    }
    else if ((attributeId == UniqueID::Id) && (maxReadLength == 32))
    {
        MutableByteSpan zclUniqueIdSpan(buffer, maxReadLength);
        MakeZclCharString(zclUniqueIdSpan, dev->GetUniqueId());
    }
    else if ((attributeId == ClusterRevision::Id) && (maxReadLength == 2))
    {
        uint16_t rev = ZCL_BRIDGED_DEVICE_BASIC_INFORMATION_CLUSTER_REVISION;
        memcpy(buffer, &rev, sizeof(rev));
    }
    else if ((attributeId == FeatureMap::Id) && (maxReadLength == 4))
    {
        uint32_t featureMap = ZCL_BRIDGED_DEVICE_BASIC_INFORMATION_FEATURE_MAP;
        memcpy(buffer, &featureMap, sizeof(featureMap));
    }
    else
    {
        return Protocols::InteractionModel::Status::Failure;
    }

    return Protocols::InteractionModel::Status::Success;
}

Protocols::InteractionModel::Status HandleReadOnOffAttribute(DeviceOnOff * dev, chip::AttributeId attributeId, uint8_t * buffer,
                                                             uint16_t maxReadLength)
{
    ChipLogProgress(DeviceLayer, "HandleReadOnOffAttribute: attrId=%d, maxReadLength=%d", attributeId, maxReadLength);

    if ((attributeId == OnOff::Attributes::OnOff::Id) && (maxReadLength == 1))
    {
        *buffer = dev->IsOn() ? 1 : 0;
    }
    else if ((attributeId == OnOff::Attributes::ClusterRevision::Id) && (maxReadLength == 2))
    {
        uint16_t rev = ZCL_ON_OFF_CLUSTER_REVISION;
        memcpy(buffer, &rev, sizeof(rev));
    }
    else
    {
        return Protocols::InteractionModel::Status::Failure;
    }

    return Protocols::InteractionModel::Status::Success;
}

Protocols::InteractionModel::Status HandleWriteOnOffAttribute(DeviceOnOff * dev, chip::AttributeId attributeId, uint8_t * buffer)
{
    if ((attributeId == OnOff::Attributes::OnOff::Id) && (dev->IsReachable()))
    {
        bool newState = *buffer;
        ChipLogProgress(DeviceLayer, "Setting OnOff state to %d for device %s", newState, dev->GetName());

        // Update the device state
        dev->SetOnOff(newState);

        // Send MQTT command
        std::string matterCommand = newState ? "on" : "off";
        std::string deviceId = dev->GetUniqueId();
        ChipLogProgress(DeviceLayer, "Sending MQTT command: %s to device %s", matterCommand.c_str(), deviceId.c_str());
        HandleMatterCommand(matterCommand, deviceId);

        return Protocols::InteractionModel::Status::Success;
    }
    else
    {
        ChipLogError(DeviceLayer, "Failed to write OnOff attribute: Device not reachable or invalid attribute");
        return Protocols::InteractionModel::Status::Failure;
    }
}

Protocols::InteractionModel::Status HandleReadTempMeasurementAttribute(DeviceTempSensor * dev, chip::AttributeId attributeId,
                                                                       uint8_t * buffer, uint16_t maxReadLength)
{
    using namespace TemperatureMeasurement::Attributes;

    if ((attributeId == MeasuredValue::Id) && (maxReadLength == 2))
    {
        int16_t measuredValue = dev->GetMeasuredValue();
        memcpy(buffer, &measuredValue, sizeof(measuredValue));
    }
    else if ((attributeId == MinMeasuredValue::Id) && (maxReadLength == 2))
    {
        int16_t minValue = dev->mMin;
        memcpy(buffer, &minValue, sizeof(minValue));
    }
    else if ((attributeId == MaxMeasuredValue::Id) && (maxReadLength == 2))
    {
        int16_t maxValue = dev->mMax;
        memcpy(buffer, &maxValue, sizeof(maxValue));
    }
    else if ((attributeId == FeatureMap::Id) && (maxReadLength == 4))
    {
        uint32_t featureMap = ZCL_TEMPERATURE_SENSOR_FEATURE_MAP;
        memcpy(buffer, &featureMap, sizeof(featureMap));
    }
    else if ((attributeId == ClusterRevision::Id) && (maxReadLength == 2))
    {
        uint16_t clusterRevision = ZCL_TEMPERATURE_SENSOR_CLUSTER_REVISION;
        memcpy(buffer, &clusterRevision, sizeof(clusterRevision));
    }
    else
    {
        return Protocols::InteractionModel::Status::Failure;
    }

    return Protocols::InteractionModel::Status::Success;
}

Protocols::InteractionModel::Status emberAfExternalAttributeReadCallback(EndpointId endpoint, ClusterId clusterId,
                                                                         const EmberAfAttributeMetadata * attributeMetadata,
                                                                         uint8_t * buffer, uint16_t maxReadLength)
{
    uint16_t endpointIndex = emberAfGetDynamicIndexFromEndpoint(endpoint);

    Protocols::InteractionModel::Status ret = Protocols::InteractionModel::Status::Failure;

    if ((endpointIndex < CHIP_DEVICE_CONFIG_DYNAMIC_ENDPOINT_COUNT) && (gDevices[endpointIndex] != nullptr))
    {
        Device * dev = gDevices[endpointIndex];

        if (clusterId == BridgedDeviceBasicInformation::Id)
        {
            ret = HandleReadBridgedDeviceBasicAttribute(dev, attributeMetadata->attributeId, buffer, maxReadLength);
        }
        else if (clusterId == OnOff::Id)
        {
            ret = HandleReadOnOffAttribute(static_cast<DeviceOnOff *>(dev), attributeMetadata->attributeId, buffer, maxReadLength);
        }
        else if (clusterId == TemperatureMeasurement::Id)
        {
            ret = HandleReadTempMeasurementAttribute(static_cast<DeviceTempSensor *>(dev), attributeMetadata->attributeId, buffer,
                                                     maxReadLength);
        }
    }

    return ret;
}

std::string mqttBrokerAddress = "localhost";
int mqttBrokerPort = 1883;

void InitMQTT()
{
    if (mosquitto_lib_init() != MOSQ_ERR_SUCCESS) {
        printf("Failed to initialize Mosquitto library\n");
        exit(1);
    }

    mqtt_client = mosquitto_new("matter_bridge", true, nullptr);
    if (!mqtt_client)
    {
        printf("Failed to create MQTT client\n");
        mosquitto_lib_cleanup();
        exit(1);
    }

    // Enable logging
    mosquitto_log_callback_set(mqtt_client, [](mosquitto *, void *, int level, const char *str) {
        printf("MQTT Log: Level=%d, Message=%s\n", level, str);
    });

    // Enable automatic reconnection with exponential backoff
    mosquitto_reconnect_delay_set(mqtt_client, 1, 30, true);

    int retry_count = 0;
    const int max_retries = 5;
    while (retry_count < max_retries)
    {
        if (mosquitto_connect(mqtt_client, mqttBrokerAddress.c_str(), mqttBrokerPort, 120) == MOSQ_ERR_SUCCESS)
        {
            printf("Connected to Mosquitto broker at %s:%d\n", mqttBrokerAddress.c_str(), mqttBrokerPort);
            break;
        }

        printf("Failed to connect to Mosquitto broker at %s:%d (Retry %d/%d)\n",
               mqttBrokerAddress.c_str(), mqttBrokerPort, retry_count + 1, max_retries);
        sleep(2); // Wait before retrying
        retry_count++;
    }

    if (retry_count == max_retries)
    {
        printf("Could not connect to MQTT broker after %d retries. Exiting...\n", max_retries);
        mosquitto_destroy(mqtt_client);
        mosquitto_lib_cleanup();
        exit(1);
    }

    // Start the Mosquitto loop in a background thread
    if (mosquitto_loop_start(mqtt_client) != MOSQ_ERR_SUCCESS)
    {
        printf("Failed to start Mosquitto loop\n");
        mosquitto_destroy(mqtt_client);
        mosquitto_lib_cleanup();
        exit(1);
    }
}


void CleanupMQTT()
{
    if (mqtt_client) {
        if (mosquitto_disconnect(mqtt_client) != MOSQ_ERR_SUCCESS) {
            printf("Warning: Failed to disconnect MQTT client\n");
        }
        mosquitto_destroy(mqtt_client);
        mqtt_client = nullptr;
    }

    mosquitto_lib_cleanup();
    printf("MQTT client and library cleaned up\n");
}


class BridgedPowerSourceAttrAccess : public AttributeAccessInterface
{
public:
    // Register on all endpoints.
    BridgedPowerSourceAttrAccess() : AttributeAccessInterface(Optional<EndpointId>::Missing(), PowerSource::Id) {}

    CHIP_ERROR
    Read(const ConcreteReadAttributePath & aPath, AttributeValueEncoder & aEncoder) override
    {
        uint16_t powerSourceDeviceIndex = CHIP_DEVICE_CONFIG_DYNAMIC_ENDPOINT_COUNT;

        if ((gDevices[powerSourceDeviceIndex] != nullptr))
        {
            DevicePowerSource * dev = static_cast<DevicePowerSource *>(gDevices[powerSourceDeviceIndex]);
            if (aPath.mEndpointId != dev->GetEndpointId())
            {
                return CHIP_IM_GLOBAL_STATUS(UnsupportedEndpoint);
            }
            switch (aPath.mAttributeId)
            {
            case PowerSource::Attributes::BatChargeLevel::Id:
                aEncoder.Encode(dev->GetBatChargeLevel());
                break;
            case PowerSource::Attributes::Order::Id:
                aEncoder.Encode(dev->GetOrder());
                break;
            case PowerSource::Attributes::Status::Id:
                aEncoder.Encode(dev->GetStatus());
                break;
            case PowerSource::Attributes::Description::Id:
                aEncoder.Encode(chip::CharSpan(dev->GetDescription().c_str(), dev->GetDescription().size()));
                break;
            case PowerSource::Attributes::EndpointList::Id: {
                std::vector<chip::EndpointId> & list = dev->GetEndpointList();
                DataModel::List<EndpointId> dm_list(chip::Span<chip::EndpointId>(list.data(), list.size()));
                aEncoder.Encode(dm_list);
                break;
            }
            case PowerSource::Attributes::ClusterRevision::Id:
                aEncoder.Encode(ZCL_POWER_SOURCE_CLUSTER_REVISION);
                break;
            case PowerSource::Attributes::FeatureMap::Id:
                aEncoder.Encode(dev->GetFeatureMap());
                break;

            case PowerSource::Attributes::BatReplacementNeeded::Id:
                aEncoder.Encode(false);
                break;
            case PowerSource::Attributes::BatReplaceability::Id:
                aEncoder.Encode(PowerSource::BatReplaceabilityEnum::kNotReplaceable);
                break;
            default:
                return CHIP_IM_GLOBAL_STATUS(UnsupportedAttribute);
            }
        }
        return CHIP_NO_ERROR;
    }
};

BridgedPowerSourceAttrAccess gPowerAttrAccess;

Protocols::InteractionModel::Status emberAfExternalAttributeWriteCallback(EndpointId endpoint, ClusterId clusterId,
                                                                          const EmberAfAttributeMetadata * attributeMetadata,
                                                                          uint8_t * buffer)
{
    uint16_t endpointIndex = emberAfGetDynamicIndexFromEndpoint(endpoint);

    Protocols::InteractionModel::Status ret = Protocols::InteractionModel::Status::Failure;

    // ChipLogProgress(DeviceLayer, "emberAfExternalAttributeWriteCallback: ep=%d", endpoint);

    if (endpointIndex < CHIP_DEVICE_CONFIG_DYNAMIC_ENDPOINT_COUNT)
    {
        Device * dev = gDevices[endpointIndex];

        if ((dev->IsReachable()) && (clusterId == OnOff::Id))
        {
            ret = HandleWriteOnOffAttribute(static_cast<DeviceOnOff *>(dev), attributeMetadata->attributeId, buffer);
        }
    }

    return ret;
}

void runOnOffRoomAction(Room * room, bool actionOn, EndpointId endpointId, uint16_t actionID, uint32_t invokeID, bool hasInvokeID)
{
    if (hasInvokeID)
    {
        Actions::Events::StateChanged::Type event{ actionID, invokeID, Actions::ActionStateEnum::kActive };
        EventNumber eventNumber;
        chip::app::LogEvent(event, endpointId, eventNumber);
    }

    if (hasInvokeID)
    {
        Actions::Events::StateChanged::Type event{ actionID, invokeID, Actions::ActionStateEnum::kInactive };
        EventNumber eventNumber;
        chip::app::LogEvent(event, endpointId, eventNumber);
    }
}

bool emberAfActionsClusterInstantActionCallback(app::CommandHandler * commandObj, const app::ConcreteCommandPath & commandPath,
                                                const Actions::Commands::InstantAction::DecodableType & commandData)
{
    bool hasInvokeID      = false;
    uint32_t invokeID     = 0;
    EndpointId endpointID = commandPath.mEndpointId;
    auto & actionID       = commandData.actionID;

    if (commandData.invokeID.HasValue())
    {
        hasInvokeID = true;
        invokeID    = commandData.invokeID.Value();
    }

    if (actionID == action1.getActionId() && action1.getIsVisible())
    {
        // Turn On Lights in Room 1
        runOnOffRoomAction(&room1, true, endpointID, actionID, invokeID, hasInvokeID);
        commandObj->AddStatus(commandPath, Protocols::InteractionModel::Status::Success);
        return true;
    }
    if (actionID == action2.getActionId() && action2.getIsVisible())
    {
        // Turn On Lights in Room 2
        runOnOffRoomAction(&room2, true, endpointID, actionID, invokeID, hasInvokeID);
        commandObj->AddStatus(commandPath, Protocols::InteractionModel::Status::Success);
        return true;
    }
    if (actionID == action3.getActionId() && action3.getIsVisible())
    {
        // Turn Off Lights in Room 1
        runOnOffRoomAction(&room1, false, endpointID, actionID, invokeID, hasInvokeID);
        commandObj->AddStatus(commandPath, Protocols::InteractionModel::Status::Success);
        return true;
    }

    commandObj->AddStatus(commandPath, Protocols::InteractionModel::Status::NotFound);
    return true;
}

void StartMQTTDiscovery()
{
    InitMQTT();

    // Subscribe to the Zigbee2MQTT bridge event topic
    mosquitto_subscribe(mqtt_client, nullptr, "zigbee2mqtt/bridge/event", 0);
    mosquitto_subscribe(mqtt_client, nullptr, "zigbee2mqtt/bridge/response/device/remove", 0);

    // Register a callback for incoming MQTT messages
    mosquitto_message_callback_set(mqtt_client, [](struct mosquitto *, void *, const struct mosquitto_message *message) 
    {
        if (strcmp(message->topic, "zigbee2mqtt/bridge/event") == 0)
        {
            const char *payload = static_cast<const char *>(message->payload);
            printf("Received event: %s\n", payload);

            try {
                json event = json::parse(payload);
                std::string eventType = event["type"];

                if (eventType == "device_joined" || eventType == "device_interview") {
                    std::string ieeeAddress = event["data"]["ieee_address"];
                    std::string friendlyName = event["data"]["friendly_name"];

                    printf("Discovered device: %s (IEEE: %s)\n", friendlyName.c_str(), ieeeAddress.c_str());

                    // Add the device to the Matter bridge
                    Device *newDevice = nullptr;
                    if (event.contains("data") && event["data"].contains("definition")) {
                        std::string deviceType;
                        for (const auto& expose : event["data"]["definition"]["exposes"]) {
                            if (expose.contains("type") && expose["type"] == "light") {
                                deviceType = "light";
                                    break; // No need to continue checking once we find a light
                            }
                        }

                        if (deviceType == "light") {
                            newDevice = new DeviceOnOff(friendlyName.c_str(), "UnknownLocation");
                        }

                        if (newDevice) {
                            newDevice->SetUniqueId(ieeeAddress.c_str());
                            newDevice->SetReachable(true);
                            int result = AddDeviceEndpoint(newDevice, &bridgedLightEndpoint, 
                                                           Span<const EmberAfDeviceType>(gBridgedOnOffDeviceTypes), 
                                                           Span<DataVersion>(gLightDataVersions), 1);

                            if (result >= 0) {
                                printf("Device '%s' successfully added with endpoint index %d\n", friendlyName.c_str(), result);
                                printf("Adding device '%s' with IEEE: %s\n", friendlyName.c_str(), ieeeAddress.c_str());
                                for (int i = 0; i < CHIP_DEVICE_CONFIG_DYNAMIC_ENDPOINT_COUNT; i++) {
                                    if (gDevices[i] == newDevice) {
                                        printf("Device '%s' successfully added at index %d\n", friendlyName.c_str(), i);
                                        break;
                                    }
                                }
                            } else {
                                printf("Failed to add device '%s'\n", friendlyName.c_str());
                                delete newDevice;
                            }
                        } else {
                            printf("Unsupported device type: %s\n", deviceType.c_str());
                        }
                    }
                }
            } catch (const std::exception &e) {
                printf("Error parsing MQTT message: %s\n", e.what());
            }
        }

        if (strcmp(message->topic, "zigbee2mqtt/bridge/response/device/remove") == 0) {
            const char *payload = static_cast<const char *>(message->payload);
            printf("Received event: %s\n", payload);

            try {
                json event = json::parse(payload);

                if (!event.contains("status") || event["status"] != "ok") {
                    printf("Warning: 'status' field is missing or not 'ok'.\n");
                    return;
                }

                if (!event.contains("data") || !event["data"].is_object()) {
                    printf("Warning: 'data' field is missing or not an object.\n");
                    return;
                }

                json data = event["data"];

                if (!data.contains("id") || !data["id"].is_string()) {
                    printf("Warning: Missing or invalid 'id' in 'data'.\n");
                    return;
                }

                std::string ieeeAddress = data["id"];
                printf("Device removed (IEEE: %s)\n", ieeeAddress.c_str());

                // Find and remove the device from the Matter bridge
                for (int i = 0; i < CHIP_DEVICE_CONFIG_DYNAMIC_ENDPOINT_COUNT; i++) {
                    if (gDevices[i] != nullptr && strcmp(gDevices[i]->GetUniqueId(), ieeeAddress.c_str()) == 0) {
                        RemoveDeviceEndpoint(gDevices[i]);
                        printf("Device with IEEE %s removed from Matter bridge\n", ieeeAddress.c_str());
                        gDevices[i] = nullptr;  // Clear the slot
                        break;
                    }
                }
            } catch (const std::exception &e) {
                printf("Error parsing MQTT message: %s\n", e.what());
            }
        }
    });
}

#define POLL_INTERVAL_MS (100)

bool kbhit()
{
    int byteswaiting;
    ioctl(0, FIONREAD, &byteswaiting);
    return byteswaiting > 0;
}


void * bridge_polling_thread(void * context)
{
    // bool light1_added = true;
    while (true)
    {
        if (kbhit())
        {
            int ch = getchar();
            if (ch == 'm')
            {
                // TC-ACT-2.2 step 3c, rename "Turn on Room 1 lights"
                action1.setName("Turn On Room 1");
            }
            if (ch == 'n')
            {
                // TC-ACT-2.2 step 3f, remove "Turn on Room 2 lights"
                action2.setIsVisible(false);
            }
            if (ch == 'o')
            {
                // TC-ACT-2.2 step 3i, add "Turn off Room 1 renamed lights"
                action3.setIsVisible(true);
            }
            continue;
        }

        // Sleep to avoid tight loop reading commands
        usleep(POLL_INTERVAL_MS * 1000);
    }

    return nullptr;
}

void ApplicationInit()
{
    // Clear device database
    memset(gDevices, 0, sizeof(gDevices));

    // Initialize first dynamic endpoint ID
    gFirstDynamicEndpointId = static_cast<chip::EndpointId>(
        static_cast<int>(emberAfEndpointFromIndex(static_cast<uint16_t>(emberAfFixedEndpointCount() - 1))) + 1);
    gCurrentEndpointId = gFirstDynamicEndpointId;

    // Disable the last fixed endpoint (placeholder for ZAP-generated code)
    emberAfEndpointEnableDisable(
        emberAfEndpointFromIndex(static_cast<uint16_t>(emberAfFixedEndpointCount() - 1)), false);

    // Setup background polling thread for Matter
    pthread_t poll_thread;
    int res = pthread_create(&poll_thread, nullptr, bridge_polling_thread, nullptr);
    if (res)
    {
        printf("Error creating polling thread: %d\n", res);
        exit(1);
    }

    // Start MQTT-based Zigbee device discovery
    StartMQTTDiscovery();
}

void ApplicationShutdown() {}


int main(int argc, char *argv[]) {

    if (ChipLinuxAppInit(argc, argv) != 0) {
        return -1;
    }

    ChipLinuxAppMainLoop();
    return 0;
}
