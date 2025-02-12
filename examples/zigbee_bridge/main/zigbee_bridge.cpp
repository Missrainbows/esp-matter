/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <app_bridged_device.h>
#include <esp_check.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_matter.h>
#include <esp_matter_bridge.h>
#include <zigbee_bridge.h>

static const char *TAG = "zigbee_bridge";

using namespace chip::app::Clusters;
using namespace esp_matter;
using namespace esp_matter::cluster;

extern uint16_t aggregator_endpoint_id;

static esp_err_t zigbee_bridge_init_bridged_onoff_light(esp_matter_bridge_device_t *dev)
{
    if (!dev) {
        ESP_LOGE(TAG, "Invalid bridge device to be initialized");
        return ESP_ERR_INVALID_ARG;
    }

    on_off::config_t config;
    on_off::create(dev->endpoint, &config, CLUSTER_MASK_SERVER, ESP_MATTER_NONE_FEATURE_ID);
    endpoint::add_device_type(dev->endpoint, endpoint::on_off_light::get_device_type_id(),
                              endpoint::on_off_light::get_device_type_version());
    if (endpoint::enable(dev->endpoint, dev->parent_endpoint_id) != ESP_OK) {
        ESP_LOGE(TAG, "ESP Matter enable dynamic endpoint failed");
        endpoint::destroy(dev->node, dev->endpoint);
        return ESP_FAIL;
    }
    return ESP_OK;
}
void zigbee_bridge_find_bridged_on_off_light_cb(zb_uint8_t zdo_status, zb_uint16_t addr, zb_uint8_t endpoint)
{
    ESP_LOGI(TAG, "on_off_light found: address:0x%x, endpoint:%d, response_status:%d", addr, endpoint, zdo_status);
    if (zdo_status == ZB_ZDP_STATUS_SUCCESS) {
        node_t *node = node::get();
        if (!node) {
            ESP_LOGE(TAG, "Could not find esp_matter node");
            return;
        }
        if (app_bridge_get_device_by_zigbee_shortaddr(addr)) {
            ESP_LOGI(TAG, "Bridged node for 0x%04x zigbee device on endpoint %d has been created", addr,
                     app_bridge_get_matter_endpointid_by_zigbee_shortaddr(addr));
        } else {
            app_bridged_device_t *bridged_device =
                app_bridge_create_bridged_device(node, aggregator_endpoint_id, ESP_MATTER_BRIDGED_DEVICE_TYPE_ZIGBEE,
                                                 app_bridge_zigbee_address(endpoint, addr));
            if (!bridged_device) {
                ESP_LOGE(TAG, "Failed to create zigbee bridged device (on_off light)");
                return;
            }
            if (zigbee_bridge_init_bridged_onoff_light(bridged_device->dev) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to initialize the bridged node");
                return;
            }
            ESP_LOGI(TAG, "Create/Update bridged node for 0x%04x zigbee device on endpoint %d", addr,
                     app_bridge_get_matter_endpointid_by_zigbee_shortaddr(addr));
        }
    }
}

esp_err_t zigbee_bridge_attribute_update(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id,
                                         esp_matter_attr_val_t *val)
{
    app_bridged_device_t *zigbee_device = app_bridge_get_device_by_matter_endpointid(endpoint_id);
    if (zigbee_device && zigbee_device->dev && zigbee_device->dev->endpoint) {
        if (cluster_id == OnOff::Id) {
            if (attribute_id == OnOff::Attributes::OnOff::Id) {
                ESP_LOGD(TAG, "Update Bridged Device, ep: %d, cluster: %d, att: %d", endpoint_id, cluster_id,
                         attribute_id);
                esp_zb_zcl_on_off_cmd_t cmd_req;
                cmd_req.zcl_basic_cmd.dst_addr_u.addr_short = zigbee_device->dev_addr.zigbee_shortaddr;
                cmd_req.zcl_basic_cmd.dst_endpoint = zigbee_device->dev_addr.zigbee_endpointid;
                cmd_req.zcl_basic_cmd.src_endpoint = esp_matter::endpoint::get_id(zigbee_device->dev->endpoint);
                cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
                cmd_req.on_off_cmd_id = val->val.b ? ZB_ZCL_CMD_ON_OFF_ON_ID : ZB_ZCL_CMD_ON_OFF_OFF_ID;
                esp_zb_zcl_on_off_cmd_req(&cmd_req);
            }
        }
    }
    return ESP_OK;
}
