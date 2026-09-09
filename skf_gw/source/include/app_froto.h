#ifndef __APP_FROTO_H
#define __APP_FROTO_H

#include "sys_def.h"
#include <ell/ell.h>
#include <ell/queue.h>
#include <ell/timeout.h>
//#include "queue.h"
#include "util_froto.h"
#include <time.h>
#include "SensingDataUpload.pb.h"
#include "Common.pb.h"
#include "app_gw_scheduler.h"

#include "ppGW.h"

// The version of Froto (application) is 1.0
#define BULLET_SENSOR_FROTO_APP_VERSION	(1)

#ifndef FPATH_CONF
	#define FPATH_CONF	"/var/lib/skf-gateway/config/fconf.json"
	//#define FPATH_CONF	"/home/root/fconf.json"
#endif
#ifndef FPATH_CONF_FROM_DB
	#define FPATH_CONF_FROM_DB "/var/lib/skf-gateway/config/fconf_from_db.json"
#endif

#ifndef FPATH_CONF_FROM_APP
	#define FPATH_CONF_FROM_APP "/var/lib/skf-gateway/config/fconf_from_app.json"
#endif

#ifndef FPATH_IMG
	#define FPATH_IMG "/var/lib/skf-gateway/update/sensor/fimg"
	//#define FPATH_IMG "/home/root/fimg"
#endif
#ifndef FPATH_SDATA_PREFIX 
	//#define FPATH_SDATA_PREFIX "/tmp/sdata_" 
	//#define FPATH_SDATA_PREFIX "/home/root/wave_" 
	#define FPATH_SDATA_PREFIX "/var/lib/skf-gateway/data/wave_"  // there is a reason !?
#endif

#ifndef FPATH_ALARM_PREFIX
	//#define FPATH_ALARM_PREFIX "/home/root/alarm_code_"
	#define FPATH_ALARM_PREFIX "/var/lib/skf-gateway/data/alarm_code_"
#endif

#ifndef DT_STORAGE_FORM_DIGIT	
	#define DT_STORAGE_FORM_DIGIT "Digit"
#endif

#ifndef DT_STORAGE_FORM_FILENAME
	#define DT_STORAGE_FORM_FILENAME "Filename"
#endif



#ifndef MAX_LENGTH_MAC_ADDR
	#define MAX_LENGTH_MAC_ADDR GW_PRO_MAX_STRING_LEN_BYTE//(32U)
#endif

#ifndef MAC_LENGTH_ICCID
	#define MAX_ICCID_LENGTH	(32U)
#endif

#ifndef MAX_IMEI_LENGTH
	#define MAX_IMEI_LENGTH	(32U)
#endif

#ifndef MAX_LENGTH_SENSOR_NAME
	#define MAX_LENGTH_SENSOR_NAME	 GW_PRO_MAX_STRING_LEN_BYTE //(32U)
#endif

#ifndef MAX_FTRANS_RETRY // we retry for 5 times at most when fail to transportate file 
	#define MAX_FTRANS_RETRY	(5U)
#endif

#ifndef MAX_SENSOR_TYPE_STR
	#define MAX_SENSOR_TYPE_STR (GW_PRO_MAX_STRING_LEN_BYTE)
#endif

#ifndef CONFIG_SENSOR_OP_FW_VERSION_RETRIEVE_TIMEOUT
  #define CONFIG_SENSOR_OP_FW_VERSION_RETRIEVE_TIMEOUT  (15)
#endif

#ifndef CONFIG_SENSOR_OP_IMG_DISSEM_TIMEOUT
  #define CONFIG_SENSOR_OP_IMG_DISSEM_TIMEOUT  (5 * 60)
#endif

enum file_type{
	FTYPE_UNKNOWN = 0,
	FTYPE_CONF = 1,
	FTYPE_IMG = 2,
};

enum file_trans_op{
	FOP_UNKNOWN = 0,
	FOP_RECV = 1,
	FOP_SEND = 2,
};

/*data structure definition for file transportation;  
*/ 
struct file_trans_control_block{
	uint32_t tskid; // we give each file transportation operation an ID
	enum file_type ftype; // the configuraiton or image	
	uint32_t total_blks; // the total number of the blocks
	uint32_t blk_idx; // to keep the index of the block received in the last transportation operation

	int fd; // the file descriptor
	uint32_t fsz; // the size of the file
	uint8_t des_fpath[MAX_FPATH] ; // the length of destination/source file path, where we keep the file received or we read data to send to the peer-dvice
};


/*data structure definition for file(configuration-file/image-file)transportation controlling

NOTE!! by this folloing controller, we make it possible to receive/send different file at the same time

*/ 
struct file_trans_controller{
	struct l_queue *pt_queue_ftcb ;// the node on the queue ref@struct file_trans_control_block
	pthread_mutex_t mtx; // mutex to protect the queue
	uint32_t ftrans_id_cnt; // we increase this variable and set it to the taskid for each file transportation operation
};

#if(0)
{ /* In the current design: field numbers 1 - 21 have been used. */
    /* The type of measurement: what the sensing data represents */
    SKFChina_Common_MeasurementType measure_type;
    /* The type of sensor: if the data is sampled by an individual sensor, this field can be filled.
 Otherwise, no need to fill this field */
    bool has_sensor;
    SKFChina_Common_SensorType sensor;
    /* The type of product */
    SKFChina_Common_ProductType product;
    /* The range of the sensor (if applicable) */
    bool has_range;
    SKFChina_Common_Range range;
    /* The unit of the data */
    bool has_unit;
    SKFChina_Common_Unit unit;
    /* The length (points) of a measurement: how many points are in this measurement
 For instance, given a measurement of @p TEMPERATURE_CURRENT,  @p measure_length is 1 because 
 there is only one temperature point.
 Given a measurement of @p VIBRATION_ENV3_RMS, @p measure_length is the length of the vibration
 measurement rather than the length of the rms (i.e., 1 byte). */
    bool has_measure_length_sample;
    uint32_t measure_length_sample;
    /* The length (points) of the data: how many points are in the uploaded data totally (not just the current packets)
 For instance, for a measurement of @p TEMPERATURE_HISTORY, @p measure_length is 1 but @p total_data_length
 is the total length of the history (e.g., 256 history temperature sample points) even the history requires packet
 split. */
    bool has_total_data_length_sample;
    uint32_t total_data_length_sample;
    /* The sequence number of a measurement: cyclic increasing 1 for each measurement.
 For instance, a sensor performs a vibration measurement and based on this measurement env 3 is calculated.
 In this case, @p VIBRATION_ACC_WAVE and @p VIBRATION_ENV3_WAVE have the same @p measure_seq_no. */
    uint32_t measure_seq_no;
    /* The data format: for instance, uint32_t or int16_t. */
    SKFChina_Common_Format data_format;
    /* check value of the whole data (if the data is sent with multiple packets). */
    bool has_crc32_value;
    uint32_t crc32_value;
    /* Encryption: indicate which encryption is adopted for @p Data.data. */
    bool has_encryp;
    SKFChina_Common_Encryption encryp;
    /* The sensor ID: considering future applications with a gateway */
    pb_callback_t sensor_id;
    /* The moment of measurement (unix timestamp, 0 time zone, unit is ms) */
    uint64_t sample_time;
    /* Memory ID */
    bool has_memory_id;
    int32_t memory_id;
    /* This field is required only if it is an alarm or notification */
    bool has_alarm_or_notify;
    SKFChina_SensingDataUpload_AlarmAndNotifyInfo alarm_or_notify;
    /* Sample rate (in Hz) of the data (e.g., vibration) */
    bool has_sample_rate_hz;
    float sample_rate_hz;
    /* Sample period (in S) of the data (e.g., temperature) */
    bool has_sample_period_s;
    uint32_t sample_period_s;
    /* Dimension mask to indicate which axis (or axes) used��SKFChina.Common.Dimension */
    bool has_dimension;
    uint32_t dimension;
    /* Communication stuffs: if measure_type is a communication-related metric, then the followings are expected
 For the devices with BLE or IEEE 802.15.4 transceivers */
    pb_callback_t mac_address;
    /* For the devices with cellular modules */
    pb_callback_t iccid;
    /* For the devices with cellular modules */
    pb_callback_t imei;
}

#endif


struct waveform_file{
	int fd; // fhe file descriptor	
	char fpath[MAX_FPATH]; // for waveform, the 
};

#ifndef MAX_DTITEM_VAL_ARRAY
	#define MAX_DTITEM_VAL_ARRAY (0x10)
#endif

#ifndef SZ_FROTO_ALARM_CODE
	#define SZ_FROTO_ALARM_CODE (9U)
#endif

union dtitem_value{
	struct waveform_file fpath; // for waveform, the 
	uint8_t byte_array[MAX_DTITEM_VAL_ARRAY];
	uint64_t valu64;
	uint32_t valu32; // 
	uint16_t valu16;
	uint8_t valu8;
};
/*data structurre definition for each item in data table
*/
#ifndef MAX_LENGTH_MANUFACTURER
	#define MAX_LENGTH_MANUFACTURER	GW_PRO_MAX_STRING_LEN_BYTE//(32U)
#endif

#ifndef MAX_DT_STORATE_FORM_BUF
	#define MAX_DT_STORATE_FORM_BUF 0x10
#endif
struct data_item{
	uint32_t idx; // the item index
	uint32_t name_id;
	uint8_t sensor_name[MAX_LENGTH_SENSOR_NAME];// the sensor name
	uint8_t sensor_mac[MAX_LENGTH_MAC_ADDR]; // the sensor MAC addr
	uint8_t sensor_type[MAX_SENSOR_TYPE_STR];
	uint8_t manufacturer[MAX_LENGTH_MANUFACTURER];
	uint32_t meas_id; // measure_seq_no
	uint64_t meas_ts;//sample_time
	uint64_t received_ts; // 
	SKFChina_Common_AlarmAndNotify alarm;
	SKFChina_Common_MeasurementType data_type;
	uint8_t format[MAX_DT_STORATE_FORM_BUF]; //  Digit / Filename	
    SKFChina_Common_Range range;
    SKFChina_Common_Unit unit;	
    uint32_t measure_length_sample;
    uint32_t total_data_length_sample;
	uint32_t dimension;			
    SKFChina_Common_Format data_format;	
    uint32_t sample_period_s;	
    SKFChina_Common_Encryption encryp;

	union dtitem_value value; //ref@data_type to decice the format of vlaue
	uint32_t crc32; // the CRC32 of value above

	float sample_rate_hz; //sample_rate
	SKFChina_Common_SensorType froto_sensor_type; //Sensor type
	SKFChina_Common_ProductType product_type; // Product type
};


app_state_t app_froto_incomming_msg_handler( const uint8_t *pt_dtbuf, const uint32_t kplen);


app_state_t app_froto_init_file_trans_ctr(struct file_trans_controller*pt_fctr);
app_state_t app_froto_definit_file_trans_ctr(struct file_trans_controller*pt_fctr);
struct file_trans_controller *app_froto_get_file_send_ctr(void);
struct file_trans_controller *app_froto_get_file_recv_ctr(void);

app_state_t app_froto_create_file_trans_task(struct file_trans_controller*pt_fctr, const uint32_t blk_num, const uint32_t tsk_id, enum file_type ftype, enum file_trans_op fop);


bool app_froto_get_node_queue(const struct msg_tree_node*pt_root, const struct node_path kp_nd_path, struct l_queue*pt_res_queue);
bool app_froto_get_node(const struct msg_tree_node*pt_root, const struct node_path kp_nd_path, struct msg_tree_node**pt_node);

uint32_t app_froto_allocate_file_send_id(struct file_trans_controller*pt_ftcr);

/**
 * Data structure for the context of short data transfer (A new short data
 * transfer only can be launched when the previous data transfer has been
 * responsed).
 */
struct shortdata_xfer_ctr
{
  pthread_mutex_t mtx;
  bool is_in_process;  // Set to true when data transfer is in progress
  struct l_queue *pt_dtrequest_queue;  // Sometimes, we might request for
                                       // several data, so we post a short
                                       // data transfer to the queue.
  uint32_t msg_seq_no;  // The message seq no
  timer_t timer_id;  // Timer for timeout operation
};

/**
 * Data structure for the context of long data (data cannot be put into one
 * individual packet) transfer.
 */
struct longdata_xfer_ctr
{
  pthread_mutex_t mtx;
  bool is_in_process;  // Set to true when data transfer is in progress
  // File information
  int fd;  // File descriptor of the file to be transferred (e.g., the
           // FUOTA image file)
  char img_fpath[MAX_FPATH];  // File path
  off_t fsize;  // File size
  int32_t dtlen;  // The length of data (in byte) in the following buffer
                  // (i.e., pkt_buf)
  uint8_t pkt_buf[FILE_PKT_SIZE];  // We put the last data into this
                                   // buffer, in case of re-transfer
                                   // when error happens
  uint32_t f_tsk_id;  // The task ID of the long data transfer
  uint32_t total_blk;
  uint32_t cur_blk_idx;
  uint32_t last_seq_no;
  uint32_t retry_cnt;  // Retry counter for each packet
  // Only for FUOTA application
  uint32_t img_ver;  // The FUOTA image version info
  bool force_fuota;

  timer_t timer_id;  // Timer for timeout operation
};

struct shortdata_xfer_ctr *app_froto_get_shortdata_xfer_timer_ctr(void);
void app_froto_init_shortdata_xfer_timer_ctr(
  struct shortdata_xfer_ctr *shortdata_xfer_timer_ctr);
void app_froto_deinit_shortdata_xfer_timer_ctr(
  struct shortdata_xfer_ctr *shortdata_xfer_timer_ctr);
struct longdata_xfer_ctr *app_froto_get_longdata_xfer_timer_ctr(
  void);
void app_froto_init_longdata_xfer_timer_ctr(
  struct longdata_xfer_ctr *longdata_xfer_timer_ctr);
void app_froto_deinit_longdata_xfer_timer_ctr(
  struct longdata_xfer_ctr *longdata_xfer_timer_ctr);

//app_state_t app_froto_send_data_selection_request(SKFChina_Common_MeasurementType kp_mtype, const uint8_t * pt_gwid_str, uint32_t kp_seq_no);
app_state_t app_froto_send_data_selection_request(const SKFChina_Common_MeasurementType *pt_mtype_array, const uint32_t kp_arr_len,const uint8_t * pt_gwid_str, const uint32_t kp_seq_no);

app_state_t app_froto_data_uploading_handler(const struct msg_tree_node*pt_root);


bool is_mtype_to_file(const SKFChina_Common_MeasurementType kp_mtype);
uint32_t to_get_waiting_period(SKFChina_Common_MeasurementType kp_mtype);
app_state_t app_froto_dtselect_reset_timer(struct shortdata_xfer_ctr *pt_dtselect_ctr);

app_state_t app_froto_pick_conf_hash_value_and_edit_time( struct msg_tree_node *pt_root, struct sensor_conf_retrieve_resp *pt_resp_buf_array,const  uint32_t kp_arrsz );
app_state_t app_froto_pick_sensor_conf_info( struct msg_tree_node *pt_root, struct sensor_conf_retrieve_resp *pt_resp_buf_array,const  uint32_t kp_arrsz );

app_state_t app_froto_retrieve_sensor_fw_version_info(struct sensor_op_controller*pt_sensor_op_ctr, uint32_t *pt_ver_buf);

extern uint8_t app_froto_shortDataReveivedFg;
extern uint8_t app_froto_shortDataReveivedRetryCnt;

void app_froto_set_force_fuota(
  struct longdata_xfer_ctr *pt_img_dissem_ctr);
void app_froto_clear_force_fuota(
  struct longdata_xfer_ctr *pt_img_dissem_ctr);
app_state_t app_froto_img_dissemination_with_block(
  struct longdata_xfer_ctr *pt_file_dissem_ctr,
  struct sensor_op_controller *pt_sensor_op_ctr,
  const uint8_t *pt_file_fpath,
  const uint32_t kp_verinfo);
void app_froto_to_dump_dtitem(struct data_item *pt_dtitem);
#if 0 // Deprecated
app_state_t app_froto_trig_img_dissemination(
  struct longdata_xfer_ctr *pt_img_dissem_ctr,
  const uint8_t *pt_img_fpath,
  const uint32_t kp_verinfo);
#endif
#endif


