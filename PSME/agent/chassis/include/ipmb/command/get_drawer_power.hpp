/*!
 * @section LICENSE
 *
 * @copyright
 * Copyright (c) 2015-2017 Intel Corporation
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
 * @section DESCRIPTION
 *
 * @file ipmb/command/get_drawer_power.hpp
 * @brief GetDrawerPower IPMI command handler for IPMB.
 * */

#pragma once
#include "ipmb/command/command.hpp"
#include "ipmb/command/drawer_power_response.hpp"

namespace agent {
namespace chassis {
namespace ipmb {
namespace command {

/*!
 * @brief GetDrawerPower class definition
 */
class GetDrawerPower : public Command {
public:
    /*!
     * Unpacks IPMI Message
     * @param msg IPMI Message
     * */
    void unpack(IpmiMessage& msg);

    /*!
     * Packs IPMI Message
     * @param msg IPMI Message
     * */
    void pack(IpmiMessage& msg);

    ~GetDrawerPower();
private:
    static const constexpr uint8_t OFFSET_CC = 0;
    static const constexpr uint8_t OFFSET_DATA = 1;

    void make_response(DrawerPowerIpmbResponse& response);
};

}
}
}
}

