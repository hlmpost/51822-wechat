/* Copyright (c) 2014 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */

/** @file
 *
 * @defgroup ble_sdk_app_template_main main.c
 * @{
 * @ingroup ble_sdk_app_template
 * @brief Template project main file.
 *
 * This file contains a template for creating a new application. It has the code necessary to wakeup
 * from button, advertise, get a connection restart advertising on disconnect and if no new
 * connection created go back to system-off mode.
 * It can easily be used as a starting point for creating a new application, the comments identified
 * with 'YOUR_JOB' indicates where and how you can customize.
 */

#include <stdint.h>
#include <string.h>
#include "nordic_common.h"
#include "nrf.h"
#include "app_error.h"
#include "ble.h"
#include "ble_hci.h"
#include "ble_srv_common.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "boards.h"
#include "softdevice_handler.h"
#include "app_timer.h"
#include "device_manager.h"
#include "pstorage.h"
#include "app_trace.h"
#include "bsp.h"
#include "bsp_btn_ble.h"
#include "nrf_drv_gpiote.h"
#include "nrf_gpiote.h"
#include "nrf_drv_ppi.h"
#include "ble_bas.h"
#include "ble_dis.h"
#include "ble_hrs.h"
#include "ble_step.h"
#include "ble_nus.h"

#include "app_uart.h"





#define IS_SRVC_CHANGED_CHARACT_PRESENT  1                                          /**< Include or not the service_changed characteristic. if not enabled, the server's database cannot be changed for the lifetime of the device*/

#define DEVICE_NAME                      "eric_demo"                               /**< Name of device. Will be included in the advertising data. */
#define MANUFACTURER_NAME                "GWI"                      /**< Manufacturer. Will be passed to Device Information Service. */
#define APP_ADV_INTERVAL                 300                                        /**< The advertising interval (in units of 0.625 ms. This value corresponds to 25 ms). */
#define APP_ADV_TIMEOUT_IN_SECONDS       180                                        /**< The advertising timeout in units of seconds. */

#define APP_TIMER_PRESCALER              0                                          /**< Value of the RTC1 PRESCALER register. */
#define APP_TIMER_OP_QUEUE_SIZE          4                                          /**< Size of timer operation queues. */

#define MIN_CONN_INTERVAL                MSEC_TO_UNITS(100, UNIT_1_25_MS)           /**< Minimum acceptable connection interval (0.1 seconds). */
#define MAX_CONN_INTERVAL                MSEC_TO_UNITS(200, UNIT_1_25_MS)           /**< Maximum acceptable connection interval (0.2 second). */
#define SLAVE_LATENCY                    0                                          /**< Slave latency. */
#define CONN_SUP_TIMEOUT                 MSEC_TO_UNITS(4000, UNIT_10_MS)            /**< Connection supervisory timeout (4 seconds). */

#define FIRST_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(5000, APP_TIMER_PRESCALER) /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY    APP_TIMER_TICKS(30000, APP_TIMER_PRESCALER)/**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT     3                                          /**< Number of attempts before giving up the connection parameter negotiation. */

#define SEC_PARAM_BOND                   1                                          /**< Perform bonding. */
#define SEC_PARAM_MITM                   0                                          /**< Man In The Middle protection not required. */
#define SEC_PARAM_IO_CAPABILITIES        BLE_GAP_IO_CAPS_NONE                       /**< No I/O capabilities. */
#define SEC_PARAM_OOB                    0                                          /**< Out Of Band data not available. */
#define SEC_PARAM_MIN_KEY_SIZE           7                                          /**< Minimum encryption key size. */
#define SEC_PARAM_MAX_KEY_SIZE           16                                         /**< Maximum encryption key size. */

#define DEAD_BEEF                        0xDEADBEEF                                 /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

static dm_application_instance_t        m_app_handle;                               /**< Application identifier allocated by device manager */

static uint16_t                          m_conn_handle = BLE_CONN_HANDLE_INVALID;   /**< Handle of the current connection. */

//eric-han
static ble_bas_t                         m_bas;                                     /**< Structure used to identify the battery service. */
static ble_hrs_t                         m_hrs;                                     /**< Structure used to identify the heart rate service. */

static ble_step_t 						m_ble_wechat;

static ble_nus_t                        m_nus;                                      /**< Structure to identify the Nordic UART Service. */


static uint8_t m_addl_adv_manuf_data [BLE_GAP_ADDR_LEN];

#define	COMPANY_IDENTIFIER						0x0056

#define UART_TX_BUF_SIZE                128                                         /**< UART TX buffer size. */
#define UART_RX_BUF_SIZE                128                                        /**< UART RX buffer size. */

extern uint8_t rece_flag;//接收到命令的标志
extern uint8_t buffer[];

//核心数据
uint16_t hrs_rate=60;//心率
uint32_t step_count=100;//步数
uint8_t bat_percent=50;//电池

//eric-han:for timer test
APP_TIMER_DEF(m_blinky_timer_id);                /**< blinky timer. */
APP_TIMER_DEF(m_sensor_timer_id);                /**< all sensor timer. */
//----------------------------------------------------------
//获得的数据
static void nus_data_handler(ble_nus_t * p_nus, uint8_t * p_data, uint16_t length)
{
	/*
   for (uint32_t i = 0; i < length; i++)
   {
        while(app_uart_put(p_data[i]) != NRF_SUCCESS);
	}*/
	eric_uart_send(p_data,length);
    //while(app_uart_put('\n') != NRF_SUCCESS);
//ble_nus_string_send(&m_nus, p_data, length);
}

//-----------------------------------------------------------
static void get_mac_addr(uint8_t *p_mac_addr)
{
		uint32_t error_code;
		ble_gap_addr_t *p_mac_addr_t = (ble_gap_addr_t*)malloc(sizeof(ble_gap_addr_t));
		error_code = sd_ble_gap_address_get(p_mac_addr_t);
		APP_ERROR_CHECK(error_code);
		uint8_t *d = p_mac_addr_t->addr;
		for ( uint8_t i = 6; i >0;)
		{	
			i--;
			p_mac_addr[5-i]= d[i];
		}
		free(p_mac_addr_t);
		p_mac_addr_t = NULL;
}
//--------------------------------------------------------------------------------
static void sensor_timeout_handler(void * p_context)
{

	//static uint16_t heart_rate=60;
	//uint8_t batt_rate=bat_percent;
	static uint8_t step_wechat[7]={0};
	static uint8_t target[4]={0};
	static uint32_t temp=0;
	uint32_t	err_code;
	//battery
	ble_bas_battery_level_update(&m_bas, bat_percent);//有变化才通知
	//step
	step_wechat[0]=1;
	memcpy(&step_wechat[1],&step_count,3);
	//step_count[1]=batt_rate;
	//step_count[4]=batt_rate;
	ble_step_count_update(&m_ble_wechat, step_wechat );//有变化才通知
	target[0]=1;
	target[1]=0;
	target[2]=0;
	target[3]=0;
	ble_wechat_target_update(&m_ble_wechat, target );
	
	//update hrs
	//can contact
	ble_hrs_sensor_contact_detected_update(&m_hrs, true);

err_code = ble_hrs_heart_rate_measurement_send(&m_hrs, hrs_rate);

    if ((err_code != NRF_SUCCESS) &&
        (err_code != NRF_ERROR_INVALID_STATE) &&
        (err_code != BLE_ERROR_NO_TX_BUFFERS) &&
        (err_code != BLE_ERROR_GATTS_SYS_ATTR_MISSING)
        )
    {
        APP_ERROR_HANDLER(err_code);
    }
/*
hrs_rate+=2;
bat_percent++;
if(hrs_rate>200)
	hrs_rate=60;
if(bat_percent>98)
	bat_percent=10;
*/

}
//-------------------------------------------------------------------------------
#if 1
static void blinky_timeout_handler(void * p_context)
{
static uint8_t flag=0;

    UNUSED_PARAMETER(p_context);
	if(flag==0)
	{
		nrf_gpio_pin_set(7);
		flag=1;
		
	}
	else
		{
			flag=0;
			nrf_gpio_pin_clear(7);
		}
	
}
#endif


/* YOUR_JOB: Declare all services structure your application is using
static ble_xx_service_t                     m_xxs;
static ble_yy_service_t                     m_yys;
*/

// YOUR_JOB: Use UUIDs for service(s) used in your application.
static ble_uuid_t m_adv_uuids[] = {{BLE_UUID_HEART_RATE_SERVICE, BLE_UUID_TYPE_BLE},
								     {BLE_UUID_BATTERY_SERVICE,			  BLE_UUID_TYPE_BLE},
									 //{BLE_UUID_DEVICE_INFORMATION_SERVICE, BLE_UUID_TYPE_BLE},
									 {BLE_UUID_WECHAT_SERVICE, BLE_UUID_TYPE_BLE}
									// {BLE_UUID_NUS_SERVICE, BLE_UUID_TYPE_VENDOR_BEGIN}

}; /**< Universally unique service identifiers. */

//eric-han for nuc
static ble_uuid_t m_scanrsp_uuids[] = {{BLE_UUID_NUS_SERVICE, BLE_UUID_TYPE_VENDOR_BEGIN}}; 

/**@brief Callback function for asserts in the SoftDevice.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in] line_num   Line number of the failing ASSERT call.
 * @param[in] file_name  File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}


/**@brief Function for the Timer initialization.
 *
 * @details Initializes the timer module. This creates and starts application timers.
 */
static void timers_init(void)
{

    // Initialize timer module.
    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, false);

    // Create timers.

    /* YOUR_JOB: Create any timers to be used by the application.
                 Below is an example of how to create a timer.
                 For every new timer needed, increase the value of the macro APP_TIMER_MAX_TIMERS by
                 one.
    uint32_t err_code;
    err_code = app_timer_create(&m_app_timer_id, APP_TIMER_MODE_REPEATED, timer_timeout_handler);
    APP_ERROR_CHECK(err_code); */
	//led blinky
	uint32_t err_code;
    err_code = app_timer_create(&m_blinky_timer_id,APP_TIMER_MODE_REPEATED,blinky_timeout_handler);
	nrf_gpio_cfg_output(7);
	//sensor timer
	err_code = app_timer_create(&m_sensor_timer_id,APP_TIMER_MODE_REPEATED,sensor_timeout_handler);

	
}


/**@brief Function for the GAP initialization.
 *
 * @details This function sets up all the necessary GAP (Generic Access Profile) parameters of the
 *          device including the device name, appearance, and the preferred connection parameters.
 */
static void gap_params_init(void)
{
    uint32_t                err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *)DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);

    /* YOUR_JOB: Use an appearance value matching the application's use case.
    err_code = sd_ble_gap_appearance_set(BLE_APPEARANCE_);
    APP_ERROR_CHECK(err_code); */
    //eric-han
	//err_code = sd_ble_gap_appearance_set(BLE_APPEARANCE_HEART_RATE_SENSOR_HEART_RATE_BELT);
	//	APP_ERROR_CHECK(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling the YYY Service events. 
 * YOUR_JOB implement a service handler function depending on the event the service you are using can generate
 *
 * @details This function will be called for all YY Service events which are passed to
 *          the application.
 *
 * @param[in]   p_yy_service   YY Service structure.
 * @param[in]   p_evt          Event received from the YY Service.
 *
 *
static void on_yys_evt(ble_yy_service_t     * p_yy_service, 
                       ble_yy_service_evt_t * p_evt)
{
    switch (p_evt->evt_type)
    {
        case BLE_YY_NAME_EVT_WRITE:
            APPL_LOG("[APPL]: charact written with value %s. \r\n", p_evt->params.char_xx.value.p_str);
            break;
        
        default:
            // No implementation needed.
            break;
    }
}*/

/**@brief Function for initializing services that will be used by the application.
 */
static void services_init(void)
{
    /* YOUR_JOB: Add code to initialize the services used by the application.
    uint32_t                           err_code;
    ble_xxs_init_t                     xxs_init;
    ble_yys_init_t                     yys_init;

    // Initialize XXX Service.
    memset(&xxs_init, 0, sizeof(xxs_init));

    xxs_init.evt_handler                = NULL;
    xxs_init.is_xxx_notify_supported    = true;
    xxs_init.ble_xx_initial_value.level = 100; 
    
    err_code = ble_bas_init(&m_xxs, &xxs_init);
    APP_ERROR_CHECK(err_code);

    // Initialize YYY Service.
    memset(&yys_init, 0, sizeof(yys_init));
    yys_init.evt_handler                  = on_yys_evt;
    yys_init.ble_yy_initial_value.counter = 0;

    err_code = ble_yy_service_init(&yys_init, &yy_init);
    APP_ERROR_CHECK(err_code);
    */
    //eric
    uint32_t       err_code;
    ble_bas_init_t bas_init;
    ble_dis_init_t dis_init;
	ble_hrs_init_t hrs_init;
	ble_step_init_t step_init;
	ble_nus_init_t nus_init;
	uint8_t		body_sensor_location;
    #if 1
	 // Initialize Battery Service.
    memset(&bas_init, 0, sizeof(bas_init));

    // Here the sec level for the Battery Service can be changed/increased.
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&bas_init.battery_level_char_attr_md.cccd_write_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&bas_init.battery_level_char_attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&bas_init.battery_level_char_attr_md.write_perm);

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&bas_init.battery_level_report_read_perm);

    bas_init.evt_handler          = NULL;
    bas_init.support_notification = true;
    bas_init.p_report_ref         = NULL;
    bas_init.initial_batt_level   = 100;

    err_code = ble_bas_init(&m_bas, &bas_init);
    APP_ERROR_CHECK(err_code);

    // Initialize Device Information Service.
    #if 0
    memset(&dis_init, 0, sizeof(dis_init));

    ble_srv_ascii_to_utf8(&dis_init.manufact_name_str, (char *)MANUFACTURER_NAME);

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&dis_init.dis_attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&dis_init.dis_attr_md.write_perm);

    err_code = ble_dis_init(&dis_init);
    APP_ERROR_CHECK(err_code);
	#endif
	// Initialize Heart Rate Service.
	   body_sensor_location = BLE_HRS_BODY_SENSOR_LOCATION_WRIST;
	
	   memset(&hrs_init, 0, sizeof(hrs_init));
	
	   hrs_init.evt_handler 				= NULL;
	   hrs_init.is_sensor_contact_supported = true;
	   hrs_init.p_body_sensor_location		= &body_sensor_location;
	
	   // Here the sec level for the Heart Rate Service can be changed/increased.
	   BLE_GAP_CONN_SEC_MODE_SET_OPEN(&hrs_init.hrs_hrm_attr_md.cccd_write_perm);
	   BLE_GAP_CONN_SEC_MODE_SET_OPEN(&hrs_init.hrs_hrm_attr_md.read_perm);
	   BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&hrs_init.hrs_hrm_attr_md.write_perm);
	
	   BLE_GAP_CONN_SEC_MODE_SET_OPEN(&hrs_init.hrs_bsl_attr_md.read_perm);
	   BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&hrs_init.hrs_bsl_attr_md.write_perm);
	
	   err_code = ble_hrs_init(&m_hrs, &hrs_init);
	   APP_ERROR_CHECK(err_code);
//Initialize wechat service
#if 0
	err_code = ble_wechat_add_service(&m_ble_wechat);
	APP_ERROR_CHECK(err_code);
	err_code = ble_wechat_add_characteristics(&m_ble_wechat);
	APP_ERROR_CHECK(err_code);
#endif
memset(&step_init, 0, sizeof(step_init));

// Here the sec level for the Battery Service can be changed/increased.
BLE_GAP_CONN_SEC_MODE_SET_OPEN(&step_init.step_count_char_attr_md.cccd_write_perm);
BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&step_init.step_count_char_attr_md.read_perm);
BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&step_init.step_count_char_attr_md.write_perm);

BLE_GAP_CONN_SEC_MODE_SET_OPEN(&step_init.step_count_report_read_perm);

step_init.evt_handler		  = NULL;
step_init.support_notification = true;
step_init.p_report_ref		  = NULL;
step_init.initial_step_count   = 100;

err_code = ble_step_init(&m_ble_wechat, &step_init);
APP_ERROR_CHECK(err_code);
#endif
//nus init
#if 1
memset(&nus_init, 0, sizeof(nus_init));

nus_init.data_handler = nus_data_handler;

err_code = ble_nus_init(&m_nus, &nus_init);

APP_ERROR_CHECK(err_code);
#endif
}


/**@brief Function for handling the Connection Parameters Module.
 *
 * @details This function will be called for all events in the Connection Parameters Module which
 *          are passed to the application.
 *          @note All this function does is to disconnect. This could have been done by simply
 *                setting the disconnect_on_fail config parameter, but instead we use the event
 *                handler mechanism to demonstrate its use.
 *
 * @param[in] p_evt  Event received from the Connection Parameters Module.
 */
static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
    uint32_t err_code;

    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        APP_ERROR_CHECK(err_code);
    }
}


/**@brief Function for handling a Connection Parameters error.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for initializing the Connection Parameters module.
 */
static void conn_params_init(void)
{
    uint32_t               err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    //cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.start_on_notify_cccd_handle    = m_hrs.hrm_handles.cccd_handle;

    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = on_conn_params_evt;
    cp_init.error_handler                  = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for starting timers.
*/
static void application_timers_start(void)
{
    /* YOUR_JOB: Start your timers. below is an example of how to start a timer.
    uint32_t err_code;
    err_code = app_timer_start(m_app_timer_id, TIMER_INTERVAL, NULL);
    APP_ERROR_CHECK(err_code); */
    uint32_t err_code;
    err_code = app_timer_start(m_blinky_timer_id, APP_TIMER_TICKS(2000, APP_TIMER_PRESCALER), NULL);
    APP_ERROR_CHECK(err_code);
	 err_code = app_timer_start(m_sensor_timer_id, APP_TIMER_TICKS(1000, APP_TIMER_PRESCALER), NULL);
    APP_ERROR_CHECK(err_code);

}


/**@brief Function for putting the chip into sleep mode.
 *
 * @note This function will not return.
 */
static void sleep_mode_enter(void)
{
    uint32_t err_code = bsp_indication_set(BSP_INDICATE_IDLE);
    APP_ERROR_CHECK(err_code);

    // Prepare wakeup buttons.
    err_code = bsp_btn_ble_sleep_mode_prepare();
    APP_ERROR_CHECK(err_code);

    // Go to system-off mode (this function will not return; wakeup will cause a reset).
    err_code = sd_power_system_off();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling advertising events.
 *
 * @details This function will be called for advertising events which are passed to the application.
 *
 * @param[in] ble_adv_evt  Advertising event.
 */
static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    uint32_t err_code;

    switch (ble_adv_evt)
    {
        case BLE_ADV_EVT_FAST:
            err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
            APP_ERROR_CHECK(err_code);
            break;
        case BLE_ADV_EVT_IDLE:
            sleep_mode_enter();
            break;
        default:
            break;
    }
}


/**@brief Function for handling the Application's BLE Stack events.
 *
 * @param[in] p_ble_evt  Bluetooth stack event.
 */
static void on_ble_evt(ble_evt_t * p_ble_evt)
{
    uint32_t err_code;

    switch (p_ble_evt->header.evt_id)
            {
        case BLE_GAP_EVT_CONNECTED:
            err_code = bsp_indication_set(BSP_INDICATE_CONNECTED);
            APP_ERROR_CHECK(err_code);
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            m_conn_handle = BLE_CONN_HANDLE_INVALID;
            break;

        default:
            // No implementation needed.
            break;
    }
}


/**@brief Function for dispatching a BLE stack event to all modules with a BLE stack event handler.
 *
 * @details This function is called from the BLE Stack event interrupt handler after a BLE stack
 *          event has been received.
 *
 * @param[in] p_ble_evt  Bluetooth stack event.
 */
static void ble_evt_dispatch(ble_evt_t * p_ble_evt)
{
    dm_ble_evt_handler(p_ble_evt);
    ble_conn_params_on_ble_evt(p_ble_evt);
    //bsp_btn_ble_on_ble_evt(p_ble_evt);
    ble_hrs_on_ble_evt(&m_hrs, p_ble_evt);
    ble_bas_on_ble_evt(&m_bas, p_ble_evt);
    on_ble_evt(p_ble_evt);
    ble_advertising_on_ble_evt(p_ble_evt);
    ble_nus_on_ble_evt(&m_nus, p_ble_evt);
	ble_step_on_ble_evt(&m_ble_wechat, p_ble_evt);
    /*YOUR_JOB add calls to _on_ble_evt functions from each service your application is using
    ble_xxs_on_ble_evt(&m_xxs, p_ble_evt);
    ble_yys_on_ble_evt(&m_yys, p_ble_evt);
    */
}


/**@brief Function for dispatching a system event to interested modules.
 *
 * @details This function is called from the System event interrupt handler after a system
 *          event has been received.
 *
 * @param[in] sys_evt  System stack event.
 */
static void sys_evt_dispatch(uint32_t sys_evt)
{
    pstorage_sys_event_handler(sys_evt);
    ble_advertising_on_sys_evt(sys_evt);
}


/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
    uint32_t err_code;

    // Initialize the SoftDevice handler module.
    SOFTDEVICE_HANDLER_INIT(NRF_CLOCK_LFCLKSRC_XTAL_20_PPM, NULL);

#if defined(S110) || defined(S130) || defined(S132)
    // Enable BLE stack.
    ble_enable_params_t ble_enable_params;
    memset(&ble_enable_params, 0, sizeof(ble_enable_params));
#if (defined(S130) || defined(S132))
    ble_enable_params.gatts_enable_params.attr_tab_size   = BLE_GATTS_ATTR_TAB_SIZE_DEFAULT;
#endif
    ble_enable_params.gatts_enable_params.service_changed = IS_SRVC_CHANGED_CHARACT_PRESENT;
    err_code = sd_ble_enable(&ble_enable_params);
    APP_ERROR_CHECK(err_code);
#endif

    // Register with the SoftDevice handler module for BLE events.
    err_code = softdevice_ble_evt_handler_set(ble_evt_dispatch);
    APP_ERROR_CHECK(err_code);

    // Register with the SoftDevice handler module for BLE events.
    err_code = softdevice_sys_evt_handler_set(sys_evt_dispatch);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling events from the BSP module.
 *
 * @param[in]   event   Event generated by button press.
 */
void bsp_event_handler(bsp_event_t event)
{
    uint32_t err_code;
    switch (event)
    {
        case BSP_EVENT_SLEEP:
            sleep_mode_enter();
            break;

        case BSP_EVENT_DISCONNECT:
            err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            if (err_code != NRF_ERROR_INVALID_STATE)
            {
                APP_ERROR_CHECK(err_code);
            }
            break;

        case BSP_EVENT_WHITELIST_OFF:
            err_code = ble_advertising_restart_without_whitelist();
            if (err_code != NRF_ERROR_INVALID_STATE)
            {
                APP_ERROR_CHECK(err_code);
            }
            break;

        default:
			sleep_mode_enter();
            break;
    }
}


/**@brief Function for handling the Device Manager events.
 *
 * @param[in] p_evt  Data associated to the device manager event.
 */
static uint32_t device_manager_evt_handler(dm_handle_t const * p_handle,
                                           dm_event_t const  * p_event,
                                           ret_code_t        event_result)
{
    APP_ERROR_CHECK(event_result);

#ifdef BLE_DFU_APP_SUPPORT
    if (p_event->event_id == DM_EVT_LINK_SECURED)
    {
        app_context_load(p_handle);
    }
#endif // BLE_DFU_APP_SUPPORT

    return NRF_SUCCESS;
}


/**@brief Function for the Device Manager initialization.
 *
 * @param[in] erase_bonds  Indicates whether bonding information should be cleared from
 *                         persistent storage during initialization of the Device Manager.
 */
static void device_manager_init(bool erase_bonds)
{
    uint32_t               err_code;
    dm_init_param_t        init_param = {.clear_persistent_data = erase_bonds};
    dm_application_param_t register_param;

    // Initialize persistent storage module.
    err_code = pstorage_init();
    APP_ERROR_CHECK(err_code);

    err_code = dm_init(&init_param);
    APP_ERROR_CHECK(err_code);

    memset(&register_param.sec_param, 0, sizeof(ble_gap_sec_params_t));

    register_param.sec_param.bond         = SEC_PARAM_BOND;
    register_param.sec_param.mitm         = SEC_PARAM_MITM;
    register_param.sec_param.io_caps      = SEC_PARAM_IO_CAPABILITIES;
    register_param.sec_param.oob          = SEC_PARAM_OOB;
    register_param.sec_param.min_key_size = SEC_PARAM_MIN_KEY_SIZE;
    register_param.sec_param.max_key_size = SEC_PARAM_MAX_KEY_SIZE;
    register_param.evt_handler            = device_manager_evt_handler;
    register_param.service_type           = DM_PROTOCOL_CNTXT_GATT_SRVR_ID;

    err_code = dm_register(&m_app_handle, &register_param);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing the Advertising functionality.
 */
static void advertising_init(void)
{
    uint32_t      err_code;
    ble_advdata_t advdata;
    ble_advdata_t scanrsp;

	//eric-han:for wechat
	ble_advdata_manuf_data_t manuf_data;
	
    // Build advertising data struct to pass into @ref ble_advertising_init.
    memset(&advdata, 0, sizeof(advdata));

    advdata.name_type               = BLE_ADVDATA_FULL_NAME;
    advdata.include_appearance      = false;
    advdata.flags                   = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    advdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    advdata.uuids_complete.p_uuids  = m_adv_uuids;

    ble_adv_modes_config_t options = {0};
    options.ble_adv_fast_enabled  = BLE_ADV_FAST_ENABLED;
    options.ble_adv_fast_interval = APP_ADV_INTERVAL;
    options.ble_adv_fast_timeout  = APP_ADV_TIMEOUT_IN_SECONDS;
	//----------------------------------------------
	//eric-han:for wechat
	manuf_data.company_identifier = COMPANY_IDENTIFIER;
    manuf_data.data.size          = sizeof(m_addl_adv_manuf_data);
    manuf_data.data.p_data        = m_addl_adv_manuf_data;
    advdata.p_manuf_specific_data = &manuf_data;
	//-------------------------------------------------
	
    memset(&scanrsp, 0, sizeof(scanrsp));
    scanrsp.uuids_complete.uuid_cnt = sizeof(m_scanrsp_uuids) / sizeof(m_scanrsp_uuids[0]);
    scanrsp.uuids_complete.p_uuids  = m_scanrsp_uuids;

	
    err_code = ble_advertising_init(&advdata, &scanrsp, &options, on_adv_evt, NULL);
    APP_ERROR_CHECK(err_code);
}
//-----------------------------------------------
//串口通讯处理
void send_data_phone(uint8_t * buffer, uint16_t length)
{
	uint32_t       err_code;
	err_code=ble_nus_string_send(&m_nus, buffer, length);
	if (err_code != NRF_ERROR_INVALID_STATE)
	{
			APP_ERROR_CHECK(err_code);
	}
}
/*
void uart_event_handle(app_uart_evt_t * p_event)
{
    static uint8_t data_array[BLE_NUS_MAX_DATA_LEN];
    static uint8_t index = 0;
    uint32_t       err_code;

   switch (p_event->evt_type)
    {
        case APP_UART_DATA_READY:
            //UNUSED_VARIABLE(app_uart_get(&data_array[index]));
            index++;

            if ((data_array[index - 1] == '\n') || (index >= (BLE_NUS_MAX_DATA_LEN)))
            {
                err_code = ble_nus_string_send(&m_nus, data_array, index);
                if (err_code != NRF_ERROR_INVALID_STATE)
                {
                    APP_ERROR_CHECK(err_code);
                }
                
                index = 0;
            }
            break;

        case APP_UART_COMMUNICATION_ERROR:
            APP_ERROR_HANDLER(p_event->data.error_communication);
            break;

        case APP_UART_FIFO_ERROR:
            APP_ERROR_HANDLER(p_event->data.error_code);
            break;

        default:
            break;
    }
}
*/
//---------------------------------------------------
//串口初始化
static void uart_init(void)
{
/*
    uint32_t                     err_code;
    const app_uart_comm_params_t comm_params =
    {
        RX_PIN_NUMBER,
        TX_PIN_NUMBER,
        RTS_PIN_NUMBER,
        CTS_PIN_NUMBER,
        APP_UART_FLOW_CONTROL_DISABLED,
        false,
        UART_BAUDRATE_BAUDRATE_Baud115200
    };

    APP_UART_FIFO_INIT( &comm_params,
                       UART_RX_BUF_SIZE,
                       UART_TX_BUF_SIZE,
                       uart_event_handle,
                       APP_IRQ_PRIORITY_LOW,
                       err_code);
    APP_ERROR_CHECK(err_code);
    */
}
//-------------------------------------------------------

/**@brief Function for initializing buttons and leds.
 *
 * @param[out] p_erase_bonds  Will be true if the clear bonding button was pressed to wake the application up.
 */
static void buttons_leds_init(bool * p_erase_bonds)
{
    bsp_event_t startup_event;

    uint32_t err_code = bsp_init(BSP_INIT_BUTTONS,
                                 APP_TIMER_TICKS(100, APP_TIMER_PRESCALER), 
                                 bsp_event_handler);
    APP_ERROR_CHECK(err_code);

    err_code = bsp_btn_ble_init(NULL, &startup_event);
    APP_ERROR_CHECK(err_code);

    *p_erase_bonds = (startup_event == BSP_EVENT_CLEAR_BONDING_DATA);
}


/**@brief Function for the Power manager.
 */
static void power_manage(void)
{
    uint32_t err_code = sd_app_evt_wait();
    APP_ERROR_CHECK(err_code);
}

void in_pin_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    unsigned char temp;
	  
	  temp=0;
}

/**@brief Function for application main entry.
 */
int main(void)
{ 
    uint32_t err_code;
    bool erase_bonds;
	//-----------------------------------------------
	
	
	//eric-han:test gpioe ppi
		uint32_t in_evt_addr;
    uint32_t out_task_addr;
		nrf_ppi_channel_t ppi_channel;
		nrf_drv_gpiote_out_config_t out_config=GPIOTE_CONFIG_OUT_TASK_TOGGLE(true);
		nrf_drv_gpiote_in_config_t in_config=GPIOTE_CONFIG_IN_SENSE_HITOLO(true);
	  err_code = nrf_drv_ppi_init();
    APP_ERROR_CHECK(err_code);
	  err_code = nrf_drv_gpiote_init();
    APP_ERROR_CHECK(err_code);
	
	//out_config.init_set = true;
	err_code = nrf_drv_gpiote_out_init(5, &out_config);
	err_code = nrf_drv_ppi_channel_alloc(&ppi_channel);
    APP_ERROR_CHECK(err_code);
	out_task_addr = nrf_drv_gpiote_out_task_addr_get(5);
	//in_config = GPIOTE_CONFIG_IN_SENSE_HITOLO(true);
    in_config.pull = NRF_GPIO_PIN_PULLUP;
	err_code = nrf_drv_gpiote_in_init(01, &in_config, in_pin_handler);
    APP_ERROR_CHECK(err_code);
	in_evt_addr=nrf_drv_gpiote_in_event_addr_get(1);
	//ppi chanel alloc
	 err_code = nrf_drv_ppi_channel_assign(ppi_channel, in_evt_addr, out_task_addr);
    APP_ERROR_CHECK(err_code);
	//ppi enabled
    err_code = nrf_drv_ppi_channel_enable(ppi_channel);
    APP_ERROR_CHECK(err_code);
	nrf_drv_gpiote_out_task_enable(5);
	nrf_drv_gpiote_in_event_enable(01, false);
//------------------------------------------------------------------
	//uart_init();
	eric_uart_Init();

	timers_init();
		


    // Initialize.
    ble_stack_init();
	//eric-han for wechat
	get_mac_addr(m_addl_adv_manuf_data);
	
    device_manager_init(erase_bonds);
    gap_params_init();
    services_init();
	advertising_init();
    conn_params_init();
 
    // Start execution.
    application_timers_start();
    err_code = ble_advertising_start(BLE_ADV_MODE_FAST);
		APP_ERROR_CHECK(err_code);

    // Enter main loop.
    for (;;)
    {
			  if(rece_flag==1)
				{
					rece_dispatch(buffer);
					rece_flag=0;
				}
        power_manage();
    }
}

/**
 * @}
 */
