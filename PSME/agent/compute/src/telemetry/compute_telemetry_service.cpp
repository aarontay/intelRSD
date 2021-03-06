/*!
 * @brief Implementation of TelemetryService class
 *
 * @copyright
 * Copyright (c) 2017 Intel Corporation
 *
 * @copyright
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * @copyright
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * @copyright
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @file telemetry/telemetry_service.cpp
 */

#include "telemetry/compute_telemetry_service.hpp"
#include "ipmi/command/generic/get_device_id.hpp"
#include "telemetry/metric_definition_builder.hpp"
#include "telemetry/grantley_cpu_dimm_temperature_telemetry_reader.hpp"
#include "telemetry/purley_cpu_dimm_temperature_telemetry_reader.hpp"
#include "telemetry/bandwidth_telemetry_reader.hpp"
#include "telemetry/health_telemetry_reader.hpp"
#include "telemetry/sensor_telemetry_reader.hpp"
#include "ipmi/purley_sel_record.hpp"
#include "telemetry/telemetry_hub_telemetry_reader.hpp"
#include "telemetry/memory_throttling_telemetry_reader.hpp"
#include "logger/logger_factory.hpp"

namespace {

using namespace telemetry;
using namespace agent_framework::model;

MetricDefinition CPU_CONSUMED_POWER =
        MetricDefinitionBuilder("/ConsumedPowerWatt")
            .set_name("processorConsumedPower")
            .set_is_linear(true)
            .set_units("Watts")
            .set_metric_type(enums::MetricType::Numeric)
            .set_physical_context(enums::PhysicalContext::CPU)
            .build();

MetricDefinition SYSTEM_CPUS_CONSUMED_POWER =
        MetricDefinitionBuilder("/ProcessorPowerWatt")
            .set_name("systemConsumedPower")
            .set_is_linear(true)
            .set_units("Watts")
            .set_metric_type(enums::MetricType::Numeric)
            .set_physical_context(enums::PhysicalContext::SystemBoard)
            .build();

MetricDefinition CPU_TEMPERATURE =
        MetricDefinitionBuilder("/TemperatureCelsius")
            .set_name("processorTemperature")
            .set_is_linear(true)
            .set_metric_type(enums::MetricType::Numeric)
            .set_sensor_type(enums::SensorType::Temperature)
            .set_units("Celsius")
            .set_physical_context(enums::PhysicalContext::CPU)
            .build();

MetricDefinition DIMM_TEMPERATURE =
        MetricDefinitionBuilder("/Oem/Intel_RackScale/TemperatureCelsius")
            .set_name("memoryTemperature")
            .set_is_linear(true)
            .set_metric_type(enums::MetricType::Numeric)
            .set_sensor_type(enums::SensorType::Temperature)
            .set_units("Celsius")
            .set_physical_context(enums::PhysicalContext::SystemBoard)
            .build();

MetricDefinition SYSTEM_DIMMS_CONSUMED_POWER =
        MetricDefinitionBuilder("/MemoryPowerWatt")
            .set_name("memoryConsumedPower")
            .set_is_linear(true)
            .set_units("Watts")
            .set_metric_type(enums::MetricType::Numeric)
            .set_physical_context(enums::PhysicalContext::SystemBoard)
            .build();

MetricDefinition CPU_THROTTLING =
        MetricDefinitionBuilder("/ThrottlingCelsius")
            .set_name("processorMarginToThrottle")
            .set_is_linear(true)
            .set_metric_type(enums::MetricType::Numeric)
            .set_sensor_type(enums::SensorType::Temperature)
            .set_units("Celsius")
            .set_physical_context(enums::PhysicalContext::CPU)
            .build();

MetricDefinition CPU_BANDWIDTH =
        MetricDefinitionBuilder("/ProcessorBandwidthPercent")
            .set_name("systemProcessorBandwidth")
            .set_is_linear(true)
            .set_metric_type(enums::MetricType::Numeric)
            .set_physical_context(enums::PhysicalContext::CPU)
            .build();

MetricDefinition MEMORY_BANDWIDTH =
        MetricDefinitionBuilder("/MemoryBandwidthPercent")
            .set_name("systemMemoryBandwidth")
            .set_is_linear(true)
            .set_metric_type(enums::MetricType::Numeric)
            .set_physical_context(enums::PhysicalContext::SystemBoard)
            .build();

MetricDefinition IO_BANDWIDTH =
        MetricDefinitionBuilder("/IOBandwidthGBps")
            .set_name("systemIOBandwidth")
            .set_is_linear(true)
            .set_metric_type(enums::MetricType::Numeric)
            .set_physical_context(enums::PhysicalContext::SystemBoard)
            .set_units("GBps")
            .build();

MetricDefinition INLET_TEMPERATURE =
        MetricDefinitionBuilder("/ReadingCelsius")
            .set_name("sledInletTemperature")
            .set_is_linear(true)
            .set_metric_type(enums::MetricType::Numeric)
            .set_sensor_type(enums::SensorType::Temperature)
            .set_units("Celsius")
            .set_physical_context(enums::PhysicalContext::SystemBoard)
            .build();

MetricDefinition OUTLET_TEMPERATURE =
        MetricDefinitionBuilder("/ReadingCelsius")
            .set_name("sledOutletTemperature")
            .set_is_linear(true)
            .set_metric_type(enums::MetricType::Numeric)
            .set_sensor_type(enums::SensorType::Temperature)
            .set_units("Celsius")
            .set_physical_context(enums::PhysicalContext::SystemBoard)
            .build();

MetricDefinition HSC_INPUT_POWER =
        MetricDefinitionBuilder("/PowerConsumedWatts")
            .set_name("sledInputACPower")
            .set_is_linear(true)
            .set_metric_type(enums::MetricType::Numeric)
            .set_sensor_type(enums::SensorType::PowerUnit)
            .set_units("Watts")
            .set_physical_context(enums::PhysicalContext::PowerSupply)
            .build();

MetricDefinition CPU_AVG_FREQ =
        MetricDefinitionBuilder("/AverageFrequencyMHz")
            .set_name("processorAverageFrequency")
            .set_metric_type(enums::MetricType::Numeric)
            .set_physical_context(enums::PhysicalContext::CPU)
            .set_units("MHz")
            .build();

MetricDefinition CPU_HEALTH =
    MetricDefinitionBuilder("/Health")
        .set_name("processorHealth")
        .set_metric_type(enums::MetricType::Discrete)
        .set_discrete_metric_type(enums::DiscreteMetricType::Multiple)
        .set_sensor_type(enums::SensorType::Processor)
        .set_physical_context(enums::PhysicalContext::CPU)
        .build();

MetricDefinition MEMORY_HEALTH =
    MetricDefinitionBuilder("/Oem/Intel_RackScale/Health")
        .set_name("memoryHealth")
        .set_metric_type(enums::MetricType::Discrete)
        .set_discrete_metric_type(enums::DiscreteMetricType::Multiple)
        .set_sensor_type(enums::SensorType::Memory)
        .set_physical_context(enums::PhysicalContext::SystemBoard)
        .build();

MetricDefinition SYSTEM_HEALTH =
    MetricDefinitionBuilder("/Health")
        .set_name("systemHealth")
        .set_metric_type(enums::MetricType::Discrete)
        .set_discrete_metric_type(enums::DiscreteMetricType::Multiple)
        .set_physical_context(enums::PhysicalContext::SystemBoard)
        .build();

MetricDefinition SYSTEM_MEMORY_THROTTLING_PERCENT =
        MetricDefinitionBuilder("/MemoryThrottledCyclesPercent")
            .set_name("systemMemoryThrottlingPercent")
            .set_is_linear(true)
            .set_units("Percent")
            .set_metric_type(enums::MetricType::Numeric)
            .set_physical_context(enums::PhysicalContext::SystemBoard)
            .build();

constexpr const uint8_t GRANTLEY_INLET_TEMP_SENSOR_NO = 0x07;
constexpr const uint8_t GRANTLEY_OUTLET_TEMP_SENSOR_NO = 0x01;
constexpr const uint8_t PURLEY_INLET_TEMP_SENSOR_NO = 0xA0;
constexpr const uint8_t PURLEY_OUTLET_TEMP_SENSOR_NO = 0xA1;
constexpr const uint8_t HSC_INPUT_POWER_SENSOR_NO = 0x29;


TelemetryReader::PtrVector create_grantley_readers() {
    return
    {
        std::make_shared<MemoryBandwidthTelemetryReader>(ResourceInstance{ResourceInstance::Component::System}, MEMORY_BANDWIDTH),
        std::make_shared<CpuBandwidthTelemetryReader>(ResourceInstance{ResourceInstance::Component::System}, CPU_BANDWIDTH),
        std::make_shared<IoBandwidthTelemetryReader>(ResourceInstance{ResourceInstance::Component::System}, IO_BANDWIDTH),
        std::make_shared<GrantleyCpuTemperatureTelemetryReader>(ResourceInstance{ResourceInstance::Component::Processor, 0}, CPU_TEMPERATURE),
        std::make_shared<GrantleyCpuMarginToThrottleTelemetryReader>(ResourceInstance{ResourceInstance::Component::Processor, 0}, CPU_THROTTLING),
        std::make_shared<GrantleyCpuTemperatureTelemetryReader>(ResourceInstance{ResourceInstance::Component::Processor, 1}, CPU_TEMPERATURE),
        std::make_shared<GrantleyCpuMarginToThrottleTelemetryReader>(ResourceInstance{ResourceInstance::Component::Processor, 1}, CPU_THROTTLING),
        std::make_shared<GrantleyDimmTemperatureTelemetryReader>(ResourceInstance{ResourceInstance::Component::Memory, 0}, DIMM_TEMPERATURE),
        std::make_shared<GrantleyDimmTemperatureTelemetryReader>(ResourceInstance{ResourceInstance::Component::Memory, 1}, DIMM_TEMPERATURE),
        std::make_shared<GrantleyDimmTemperatureTelemetryReader>(ResourceInstance{ResourceInstance::Component::Memory, 2}, DIMM_TEMPERATURE),
        std::make_shared<GrantleyDimmTemperatureTelemetryReader>(ResourceInstance{ResourceInstance::Component::Memory, 3}, DIMM_TEMPERATURE),
        std::make_shared<GrantleyDimmTemperatureTelemetryReader>(ResourceInstance{ResourceInstance::Component::Memory, 4}, DIMM_TEMPERATURE),
        std::make_shared<GrantleyDimmTemperatureTelemetryReader>(ResourceInstance{ResourceInstance::Component::Memory, 5}, DIMM_TEMPERATURE),
        std::make_shared<GrantleyDimmTemperatureTelemetryReader>(ResourceInstance{ResourceInstance::Component::Memory, 6}, DIMM_TEMPERATURE),
        std::make_shared<GrantleyDimmTemperatureTelemetryReader>(ResourceInstance{ResourceInstance::Component::Memory, 7}, DIMM_TEMPERATURE),
        std::make_shared<GrantleyDimmTemperatureTelemetryReader>(ResourceInstance{ResourceInstance::Component::Memory, 8}, DIMM_TEMPERATURE),
        std::make_shared<GrantleyDimmTemperatureTelemetryReader>(ResourceInstance{ResourceInstance::Component::Memory, 9}, DIMM_TEMPERATURE),
        std::make_shared<GrantleyDimmTemperatureTelemetryReader>(ResourceInstance{ResourceInstance::Component::Memory, 10}, DIMM_TEMPERATURE),
        std::make_shared<GrantleyDimmTemperatureTelemetryReader>(ResourceInstance{ResourceInstance::Component::Memory, 11}, DIMM_TEMPERATURE),
        std::make_shared<GrantleyDimmTemperatureTelemetryReader>(ResourceInstance{ResourceInstance::Component::Memory, 12}, DIMM_TEMPERATURE),
        std::make_shared<GrantleyDimmTemperatureTelemetryReader>(ResourceInstance{ResourceInstance::Component::Memory, 13}, DIMM_TEMPERATURE),
        std::make_shared<GrantleyDimmTemperatureTelemetryReader>(ResourceInstance{ResourceInstance::Component::Memory, 14}, DIMM_TEMPERATURE),
        std::make_shared<GrantleyDimmTemperatureTelemetryReader>(ResourceInstance{ResourceInstance::Component::Memory, 15}, DIMM_TEMPERATURE),
        std::make_shared<SensorTelemetryReader>(ResourceInstance{ResourceInstance::Component::ThermalZone}, INLET_TEMPERATURE, GRANTLEY_INLET_TEMP_SENSOR_NO, 0x40, 0x01),
        std::make_shared<SensorTelemetryReader>(ResourceInstance{ResourceInstance::Component::ThermalZone}, OUTLET_TEMPERATURE, GRANTLEY_OUTLET_TEMP_SENSOR_NO, 0x07, 0x00),
        std::make_shared<SensorTelemetryReader>(ResourceInstance{ResourceInstance::Component::PowerZone}, HSC_INPUT_POWER, HSC_INPUT_POWER_SENSOR_NO, 0x14, 0x02),

        std::make_shared<HealthTelemetryReader>(ResourceInstance{ResourceInstance::Component::Processor, 0},
            HealthTelemetryReader::Warnings{
                {ipmi::SelRecordSensor::PROCESSOR_AUTOMATIC_THROTTLED, "Throttled"},
                {ipmi::SelRecordSensor::MACHINE_CHECK_ERROR, "Machine Check Err"},
                {ipmi::SelRecordSensor::PROCESSOR_DISABLED, "Disabled"}
            },
            HealthTelemetryReader::Critical{
                {ipmi::SelRecordSensor::IERR, "IERR"},
                {ipmi::SelRecordSensor::THERMAL_TRIP, "Thermal Trip"},
                {ipmi::SelRecordSensor::FRB1_BIST_FAILURE, "FRB1 BIST Fail"},
                {ipmi::SelRecordSensor::FRB2_HANG_IN_POST, "FRB2 Hang in post"},
                {ipmi::SelRecordSensor::FRB3_INITIALIZATION_FAILURE, "FRB3 Init Fail"},
                {ipmi::SelRecordSensor::CONFIGURATION_ERROR, "Config Err"},
                {ipmi::SelRecordSensor::SMBIOS_UNCORRECTABLE_ERROR, "SMBIOS Err"},
                {ipmi::SelRecordSensor::MACHINE_CHECK_EXCEPTION, "Machine Check Except"}
            }),
        std::make_shared<HealthTelemetryReader>(ResourceInstance{ResourceInstance::Component::Processor, 1},
            HealthTelemetryReader::Warnings{
                {ipmi::SelRecordSensor::PROCESSOR_AUTOMATIC_THROTTLED, "Throttled"},
                {ipmi::SelRecordSensor::MACHINE_CHECK_ERROR, "Machine Check Err"},
                {ipmi::SelRecordSensor::PROCESSOR_DISABLED, "Disabled"}
            },
            HealthTelemetryReader::Critical{
                {ipmi::SelRecordSensor::IERR, "IERR"},
                {ipmi::SelRecordSensor::THERMAL_TRIP, "Thermal Trip"},
                {ipmi::SelRecordSensor::FRB1_BIST_FAILURE, "FRB1 BIST Fail"},
                {ipmi::SelRecordSensor::FRB2_HANG_IN_POST, "FRB2 Hang in post"},
                {ipmi::SelRecordSensor::FRB3_INITIALIZATION_FAILURE, "FRB3 Init Fail"},
                {ipmi::SelRecordSensor::CONFIGURATION_ERROR, "Config Err"},
                {ipmi::SelRecordSensor::SMBIOS_UNCORRECTABLE_ERROR, "SMBIOS Err"},
                {ipmi::SelRecordSensor::MACHINE_CHECK_EXCEPTION, "Machine Check Except"}
            })
    };
}

TelemetryReader::Ptr memory_health_reader(unsigned instance, ipmi::SelRecordGeneric::EventSensorType dimm_hot) {
    return std::make_shared<HealthTelemetryReader>(
        ResourceInstance{ResourceInstance::Component::Memory, static_cast<int>(instance)},
        MEMORY_HEALTH,
        HealthTelemetryReader::Warnings{
            {ipmi::PurleySelRecord::for_memory(ipmi::PurleySelRecord::MEMORY_ECC_ERROR_PARITY, instance), "ECC Parity Err"},
            {dimm_hot, "DIMM Hot"}
        },
        HealthTelemetryReader::Critical{
            {ipmi::PurleySelRecord::for_memory(ipmi::PurleySelRecord::MEMORY_ECC_ERROR_UNCORRECTABLE, instance), "ECC Uncorrectable"},
            {ipmi::PurleySelRecord::for_memory(ipmi::PurleySelRecord::MEMORY_ECC_ERROR_LOG_LIMIT, instance), "ECC Log Limit"}
        });
}

TelemetryReader::PtrVector create_purley_readers() {
    return
    {
        std::make_shared<MemoryBandwidthTelemetryReader>(ResourceInstance{ResourceInstance::Component::System}, MEMORY_BANDWIDTH),
        std::make_shared<CpuBandwidthTelemetryReader>(ResourceInstance{ResourceInstance::Component::System}, CPU_BANDWIDTH),
        std::make_shared<IoBandwidthTelemetryReader>(ResourceInstance{ResourceInstance::Component::System}, IO_BANDWIDTH),
        std::make_shared<PurleyCpuTemperatureTelemetryReader>(ResourceInstance{ResourceInstance::Component::Processor, 0}, CPU_TEMPERATURE),
        std::make_shared<PurleyCpuMarginToThrottleTelemetryReader>(ResourceInstance{ResourceInstance::Component::Processor, 0}, CPU_THROTTLING),
        std::make_shared<PurleyCpuTemperatureTelemetryReader>(ResourceInstance{ResourceInstance::Component::Processor, 1}, CPU_TEMPERATURE),
        std::make_shared<PurleyCpuMarginToThrottleTelemetryReader>(ResourceInstance{ResourceInstance::Component::Processor, 1}, CPU_THROTTLING),
        std::make_shared<PurleyDimmTemperatureTelemetryReader>(ResourceInstance{ResourceInstance::Component::Memory, 0}, DIMM_TEMPERATURE),
        std::make_shared<PurleyDimmTemperatureTelemetryReader>(ResourceInstance{ResourceInstance::Component::Memory, 1}, DIMM_TEMPERATURE),
        std::make_shared<PurleyDimmTemperatureTelemetryReader>(ResourceInstance{ResourceInstance::Component::Memory, 2}, DIMM_TEMPERATURE),
        std::make_shared<PurleyDimmTemperatureTelemetryReader>(ResourceInstance{ResourceInstance::Component::Memory, 3}, DIMM_TEMPERATURE),
        std::make_shared<PurleyDimmTemperatureTelemetryReader>(ResourceInstance{ResourceInstance::Component::Memory, 4}, DIMM_TEMPERATURE),
        std::make_shared<PurleyDimmTemperatureTelemetryReader>(ResourceInstance{ResourceInstance::Component::Memory, 5}, DIMM_TEMPERATURE),
        std::make_shared<PurleyDimmTemperatureTelemetryReader>(ResourceInstance{ResourceInstance::Component::Memory, 6}, DIMM_TEMPERATURE),
        std::make_shared<PurleyDimmTemperatureTelemetryReader>(ResourceInstance{ResourceInstance::Component::Memory, 7}, DIMM_TEMPERATURE),
        std::make_shared<PurleyDimmTemperatureTelemetryReader>(ResourceInstance{ResourceInstance::Component::Memory, 8}, DIMM_TEMPERATURE),
        std::make_shared<PurleyDimmTemperatureTelemetryReader>(ResourceInstance{ResourceInstance::Component::Memory, 9}, DIMM_TEMPERATURE),
        std::make_shared<PurleyDimmTemperatureTelemetryReader>(ResourceInstance{ResourceInstance::Component::Memory, 10}, DIMM_TEMPERATURE),
        std::make_shared<PurleyDimmTemperatureTelemetryReader>(ResourceInstance{ResourceInstance::Component::Memory, 11}, DIMM_TEMPERATURE),
        std::make_shared<SensorTelemetryReader>(ResourceInstance{ResourceInstance::Component::ThermalZone}, INLET_TEMPERATURE, PURLEY_INLET_TEMP_SENSOR_NO, 0x03, 0x00),
        std::make_shared<SensorTelemetryReader>(ResourceInstance{ResourceInstance::Component::ThermalZone}, OUTLET_TEMPERATURE, PURLEY_OUTLET_TEMP_SENSOR_NO, 0x03, 0x00),
        std::make_shared<SensorTelemetryReader>(ResourceInstance{ResourceInstance::Component::PowerZone}, HSC_INPUT_POWER, HSC_INPUT_POWER_SENSOR_NO, 0x14, 0x01),

        // system health
        std::make_shared<HealthTelemetryReader>(ResourceInstance{ResourceInstance::Component::System},
            SYSTEM_HEALTH,
            HealthTelemetryReader::Warnings{
                {ipmi::PurleySelRecord::CPU_DIMM_HOT, "DIMM Hot"},
                {ipmi::PurleySelRecord::SYSTEM_THROTTLE, "Sys Throttled"}
            },
            HealthTelemetryReader::Critical{
                {ipmi::PurleySelRecord::VR_HOT, "VR Hot"},
                {ipmi::PurleySelRecord::PROCESSOR_FAIL, "FRB3"},
                {ipmi::PurleySelRecord::WATCHDOG_BIOS_FRB2, "FRB2"},
                {ipmi::PurleySelRecord::CATERR, "CAT Err"},
                {ipmi::PurleySelRecord::MSMI, "MSMI"},
                {ipmi::PurleySelRecord::SYSTEM_STS_CPU_CATERR, "Sys CAT Err"},
                {ipmi::PurleySelRecord::PCH_THERMAL_TRIP, "PCH TT"},
                {ipmi::PurleySelRecord::POST_ERROR, "Post Err"},
                {ipmi::PurleySelRecord::POWER_ERROR, "Power Err"},
                {ipmi::PurleySelRecord::NM_EXCEPTION, "NM Except"}
            }),

        // processor health
        std::make_shared<HealthTelemetryReader>(ResourceInstance{ResourceInstance::Component::Processor, 0},
            CPU_HEALTH,
            HealthTelemetryReader::Warnings{
                {ipmi::PurleySelRecord::CPU0_THERM_STATUS_TCC_ACTIVE, "TCC Active"},
                {ipmi::PurleySelRecord::CPU0_DIMM_HOT, "DIMM Hot"},
                {ipmi::PurleySelRecord::CPU0_CORRECTABLE_MACHINE_CHECK_ERROR, "Correctable Check Err"}
            },
            HealthTelemetryReader::Critical{
                {ipmi::PurleySelRecord::CPU0_UNCORRECTABLE_MACHINE_CHECK_ERROR, "Uncorrectable Check Err"},
                {ipmi::PurleySelRecord::CPU0_FIVR_FAULT, "FIVR Fault"},
                {ipmi::PurleySelRecord::CPU0_THERMAL_TRIP, "Thermal Trip"},
                {ipmi::PurleySelRecord::SYSTEM_STS_CPU0_THERMAL_TRIP, "STS Thermal Trip"},
                {ipmi::PurleySelRecord::SYSTEM_STS_CPU0_FIVR_FAULT, "STS FIVR Fault"},
                {ipmi::PurleySelRecord::VR_HOT_CPU0_VCCIN, "VCCIN VR Hot"},
                {ipmi::PurleySelRecord::VR_HOT_CPU0_DIMM_ABC, "DIMM ABC VR Hot"},
                {ipmi::PurleySelRecord::VR_HOT_CPU0_DIMM_DEF, "DIMM DEF VR Hot"},
                {ipmi::PurleySelRecord::CPU0_THERM_STATUS_CRITICAL, "Therm Status Crit"},
                {ipmi::PurleySelRecord::CPU0_THERM_STATUS_PROCHOT, "Proc Hot"}
            }),
        std::make_shared<HealthTelemetryReader>(ResourceInstance{ResourceInstance::Component::Processor, 1},
            CPU_HEALTH,
            HealthTelemetryReader::Warnings{
                {ipmi::PurleySelRecord::CPU1_THERM_STATUS_TCC_ACTIVE, "TCC Active"},
                {ipmi::PurleySelRecord::CPU1_DIMM_HOT, "DIMM Hot"},
                {ipmi::PurleySelRecord::CPU1_CORRECTABLE_MACHINE_CHECK_ERROR, "Correctable Check Err"}
            },
            HealthTelemetryReader::Critical{
                {ipmi::PurleySelRecord::CPU1_UNCORRECTABLE_MACHINE_CHECK_ERROR, "Uncorrectable Check Err"},
                {ipmi::PurleySelRecord::CPU1_FIVR_FAULT, "FIVR Fault"},
                {ipmi::PurleySelRecord::CPU1_THERMAL_TRIP, "Thermal Trip"},
                {ipmi::PurleySelRecord::SYSTEM_STS_CPU1_THERMAL_TRIP, "STS Thermal Trip"},
                {ipmi::PurleySelRecord::SYSTEM_STS_CPU1_FIVR_FAULT, "STS FIVR Fault"},
                {ipmi::PurleySelRecord::VR_HOT_CPU1_VCCIN, "VCCIN VR Hot"},
                {ipmi::PurleySelRecord::VR_HOT_CPU1_DIMM_GHJ, "DIMM ABC VR Hot"},
                {ipmi::PurleySelRecord::VR_HOT_CPU1_DIMM_KLM, "DIMM DEF VR Hot"},
                {ipmi::PurleySelRecord::CPU1_THERM_STATUS_CRITICAL, "Therm Status Crit"},
                {ipmi::PurleySelRecord::CPU1_THERM_STATUS_PROCHOT, "Proc Hot"}
            }),

        // memory health
        memory_health_reader(0, ipmi::PurleySelRecord::CPU0_DIMM_ABC_HOT),
        memory_health_reader(1, ipmi::PurleySelRecord::CPU0_DIMM_ABC_HOT),
        memory_health_reader(2, ipmi::PurleySelRecord::CPU0_DIMM_ABC_HOT),
        memory_health_reader(3, ipmi::PurleySelRecord::CPU0_DIMM_DEF_HOT),
        memory_health_reader(4, ipmi::PurleySelRecord::CPU0_DIMM_DEF_HOT),
        memory_health_reader(5, ipmi::PurleySelRecord::CPU0_DIMM_DEF_HOT),
        memory_health_reader(6, ipmi::PurleySelRecord::CPU1_DIMM_GHJ_HOT),
        memory_health_reader(7, ipmi::PurleySelRecord::CPU1_DIMM_GHJ_HOT),
        memory_health_reader(8, ipmi::PurleySelRecord::CPU1_DIMM_GHJ_HOT),
        memory_health_reader(9, ipmi::PurleySelRecord::CPU1_DIMM_KLM_HOT),
        memory_health_reader(10, ipmi::PurleySelRecord::CPU1_DIMM_KLM_HOT),
        memory_health_reader(11, ipmi::PurleySelRecord::CPU1_DIMM_KLM_HOT),

        std::make_shared<TelemetryHubTelemetryReader>(CPU_AVG_FREQ, MetricUid::CPU0_AVG_FREQ),
        std::make_shared<TelemetryHubTelemetryReader>(CPU_AVG_FREQ, MetricUid::CPU1_AVG_FREQ),
        std::make_shared<TelemetryHubTelemetryReader>(CPU_CONSUMED_POWER, MetricUid::CPU0_CONSUMED_POWER),
        std::make_shared<TelemetryHubTelemetryReader>(CPU_CONSUMED_POWER, MetricUid::CPU1_CONSUMED_POWER),
        std::make_shared<TelemetryHubAggregatedTelemetryReader>(ResourceInstance{ResourceInstance::Component::System},
                    SYSTEM_CPUS_CONSUMED_POWER, MetricUidVec{MetricUid::CPU0_CONSUMED_POWER, MetricUid::CPU1_CONSUMED_POWER}),
        std::make_shared<TelemetryHubAggregatedTelemetryReader>(ResourceInstance{ResourceInstance::Component::System},
                    SYSTEM_DIMMS_CONSUMED_POWER, MetricUidVec{MetricUid::CPU0_DRAM_POWER, MetricUid::CPU1_DRAM_POWER}),
        std::make_shared<MemoryThrottlingTelemetryReader>(ResourceInstance{ResourceInstance::Component::System},
                                                          SYSTEM_MEMORY_THROTTLING_PERCENT)
    };
}

TelemetryReader::PtrVector create_readers(OptionalField<std::uint16_t> platform_id) {
    if (platform_id.has_value()) {
        using ipmi::command::generic::ProductId;
        if (ProductId::PRODUCT_ID_INTEL_XEON_BDC_R == platform_id) {
            return create_grantley_readers();
        }
        else if (ProductId::PRODUCT_ID_INTEL_XEON_PURLEY == platform_id) {
            return create_purley_readers();
        }
    }
    /* platform is not handled */
    return {};
}

} // namespace


namespace agent {
namespace compute {

ComputeTelemetryService::~ComputeTelemetryService() {
}

ComputeTelemetryService::ComputeTelemetryService(ipmi::IpmiController& ipmi, OptionalField<std::uint16_t> platform_id) :
    telemetry::TelemetryService(ipmi, create_readers(platform_id)) { }

}
}
