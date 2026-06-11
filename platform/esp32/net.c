/* Wired Ethernet for the JC1060P470C Ethernet variant.
 *
 * IP101 PHY at address 1, reset/power on GPIO51, 50 MHz RMII REF_CLK
 * input on GPIO50, MDC/MDIO on GPIO31/52 — which is exactly the ESP32-P4
 * default EMAC pinout (also used on Guition's ESP32-P4-M3-Dev). */

#include "net.h"

#include "esp_eth.h"
#include "esp_eth_mac_esp.h"
#include "esp_eth_phy.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#define PHY_ADDR     1
#define PHY_RST_GPIO 51

/* How long net_init() blocks waiting for DHCP before letting the UI
 * start anyway (the 60 s refresh timer retries forever). */
#define GOT_IP_TIMEOUT_MS 15000

static const char *TAG = "net";

static EventGroupHandle_t s_net_events;
#define GOT_IP_BIT BIT0

static esp_eth_handle_t s_eth_handle;
static esp_netif_t     *s_eth_netif;

static void eth_event_handler(void *arg, esp_event_base_t base,
                              int32_t event_id, void *event_data)
{
    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "ethernet link up");
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "ethernet link down");
        xEventGroupClearBits(s_net_events, GOT_IP_BIT);
        break;
    default:
        break;
    }
}

static void got_ip_handler(void *arg, esp_event_base_t base,
                           int32_t event_id, void *event_data)
{
    const ip_event_got_ip_t *event = event_data;
    ESP_LOGI(TAG, "got IP " IPSTR " gw " IPSTR,
             IP2STR(&event->ip_info.ip), IP2STR(&event->ip_info.gw));
    xEventGroupSetBits(s_net_events, GOT_IP_BIT);
}

void net_init(void)
{
    s_net_events = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    /* P4 defaults: MDC 31, MDIO 52, REF_CLK EXT_IN on 50, standard RMII
     * data pins — all match this board. */
    eth_esp32_emac_config_t emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&emac_config, &mac_config);

    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = PHY_ADDR;
    phy_config.reset_gpio_num = PHY_RST_GPIO;
    esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_config);

    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_err_t err = esp_eth_driver_install(&eth_config, &s_eth_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ethernet driver install failed (%s); running offline",
                 esp_err_to_name(err));
        return;
    }

    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    s_eth_netif = esp_netif_new(&netif_cfg);
    ESP_ERROR_CHECK(esp_netif_attach(s_eth_netif,
                                     esp_eth_new_netif_glue(s_eth_handle)));

    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID,
                                               eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP,
                                               got_ip_handler, NULL));

    ESP_ERROR_CHECK(esp_eth_start(s_eth_handle));

    ESP_LOGI(TAG, "waiting up to %d ms for DHCP...", GOT_IP_TIMEOUT_MS);
    EventBits_t bits = xEventGroupWaitBits(s_net_events, GOT_IP_BIT,
                                           pdFALSE, pdTRUE,
                                           pdMS_TO_TICKS(GOT_IP_TIMEOUT_MS));
    if (!(bits & GOT_IP_BIT)) {
        ESP_LOGW(TAG, "no IP yet; UI starts offline, refresh will retry");
    }
}

void net_cleanup(void)
{
    if (s_eth_handle) {
        esp_eth_stop(s_eth_handle);
    }
}
