/*!
 * @brief Thermal zone builder class interface
 *
 * @copyright Copyright (c) 2017 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @header{Files}
 * @file thermal_zone_builder.hpp
 */

#pragma once



#include "agent-framework/module/model/thermal_zone.hpp"
#include "smbios/smbios_parser.hpp"



namespace agent {
namespace compute {
namespace discovery {

class ThermalZoneBuilder {
public:
    /*!
     * @brief Build default thermal zone object.
     * @param parent_uuid Parent UUID.
     * @return Default built thermal zone object.
     */
    static agent_framework::model::ThermalZone build_default(const std::string& parent_uuid);
};

}
}
}
