/* drivers/input/sec_input/synaptics/synaptics_fn.c
 *
 * Copyright (C) 2011 Samsung Electronics Co., Ltd.
 *
 * Core file for Samsung TSC driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "synaptics_dev.h"
#include "synaptics_reg.h"

static void synaptics_ts_coord_parsing(struct synaptics_ts_data *ts, struct sec_touch_event_data *p_event_coord, u8 t_id)
{
	ts->plat_data->coord[t_id].id = t_id;
	ts->plat_data->coord[t_id].action = p_event_coord->tchsta;
	ts->plat_data->coord[t_id].x = (p_event_coord->x_11_4 << 4) | (p_event_coord->x_3_0);
	ts->plat_data->coord[t_id].y = (p_event_coord->y_11_4 << 4) | (p_event_coord->y_3_0);
	ts->plat_data->coord[t_id].z = p_event_coord->z & 0x3F;
	ts->plat_data->coord[t_id].ttype = p_event_coord->ttype_3_2 << 2 | p_event_coord->ttype_1_0 << 0;
	ts->plat_data->coord[t_id].major = p_event_coord->major;
	ts->plat_data->coord[t_id].minor = p_event_coord->minor;

	if (!ts->plat_data->coord[t_id].palm && (ts->plat_data->coord[t_id].ttype == SYNAPTICS_TS_TOUCHTYPE_PALM))
		ts->plat_data->coord[t_id].palm_count++;

	ts->plat_data->coord[t_id].palm = (ts->plat_data->coord[t_id].ttype == SYNAPTICS_TS_TOUCHTYPE_PALM);
	if (ts->plat_data->coord[t_id].palm)
		ts->plat_data->palm_flag |= (1 << t_id);
	else
		ts->plat_data->palm_flag &= ~(1 << t_id);

	ts->plat_data->coord[t_id].left_event = p_event_coord->left_event;

	ts->plat_data->coord[t_id].noise_level = max(ts->plat_data->coord[t_id].noise_level,
							p_event_coord->noise_level);
	ts->plat_data->coord[t_id].max_strength = max(ts->plat_data->coord[t_id].max_strength,
							p_event_coord->max_strength);
	ts->plat_data->coord[t_id].hover_id_num = max(ts->plat_data->coord[t_id].hover_id_num,
							(u8)p_event_coord->hover_id_num);

	if (ts->plat_data->coord[t_id].z <= 0)
		ts->plat_data->coord[t_id].z = 1;
}

static void synaptics_ts_fod_vi_event(struct synaptics_ts_data *ts)
{
	int ret = 0;
	unsigned short offset;

	offset = SEC_TS_CMD_SPONGE_FOD_POSITION;

	ret = ts->synaptics_ts_read_sponge(ts, ts->plat_data->fod_data.vi_data, ts->plat_data->fod_data.vi_size,
		offset, ts->plat_data->fod_data.vi_size);
	if (ret < 0)
		input_info(true, &ts->client->dev, "%s: failed read fod vi\n", __func__);
}

static void synaptics_ts_gesture_event(struct synaptics_ts_data *ts, u8 *event_buff)
{
	struct sec_gesture_event_data *p_gesture_status;
	int x, y;

	p_gesture_status = (struct sec_gesture_event_data *)event_buff;

	x = (p_gesture_status->gesture_data_1 << 4) | (p_gesture_status->gesture_data_3 >> 4);
	y = (p_gesture_status->gesture_data_2 << 4) | (p_gesture_status->gesture_data_3 & 0x0F);

	if (p_gesture_status->stype == SYNAPTICS_TS_GESTURE_CODE_SWIPE) {
		sec_input_gesture_report(&ts->client->dev, SPONGE_EVENT_TYPE_SPAY, 0, 0);
	} else if (p_gesture_status->stype == SYNAPTICS_TS_GESTURE_CODE_DOUBLE_TAP) {
		if (p_gesture_status->gesture_id == SYNAPTICS_TS_GESTURE_ID_AOD) {
			sec_input_gesture_report(&ts->client->dev, SPONGE_EVENT_TYPE_AOD_DOUBLETAB, x, y);
		} else if (p_gesture_status->gesture_id == SYNAPTICS_TS_GESTURE_ID_DOUBLETAP_TO_WAKEUP) {
			input_info(true, &ts->client->dev, "%s: AOT\n", __func__);
			input_report_key(ts->plat_data->input_dev, KEY_WAKEUP, 1);
			input_sync(ts->plat_data->input_dev);
			input_report_key(ts->plat_data->input_dev, KEY_WAKEUP, 0);
			input_sync(ts->plat_data->input_dev);
		}
	} else if (p_gesture_status->stype  == SYNAPTICS_TS_GESTURE_CODE_SINGLE_TAP) {
		sec_input_gesture_report(&ts->client->dev, SPONGE_EVENT_TYPE_SINGLE_TAP, x, y);
	} else if (p_gesture_status->stype  == SYNAPTICS_TS_GESTURE_CODE_PRESS) {
		if (p_gesture_status->gesture_id == SYNAPTICS_TS_GESTURE_ID_FOD_LONG ||
			p_gesture_status->gesture_id == SYNAPTICS_TS_GESTURE_ID_FOD_NORMAL) {
			sec_input_gesture_report(&ts->client->dev, SPONGE_EVENT_TYPE_FOD_PRESS, x, y);
			input_info(true, &ts->client->dev, "%s: FOD %sPRESS\n",
					__func__, p_gesture_status->gesture_id ? "" : "LONG");
		} else if (p_gesture_status->gesture_id == SYNAPTICS_TS_GESTURE_ID_FOD_RELEASE) {
			sec_input_gesture_report(&ts->client->dev, SPONGE_EVENT_TYPE_FOD_RELEASE, x, y);
			input_info(true, &ts->client->dev, "%s: FOD RELEASE\n", __func__);
			memset(ts->plat_data->fod_data.vi_data, 0x0, ts->plat_data->fod_data.vi_size);
		} else if (p_gesture_status->gesture_id == SYNAPTICS_TS_GESTURE_ID_FOD_OUT) {
			sec_input_gesture_report(&ts->client->dev, SPONGE_EVENT_TYPE_FOD_OUT, x, y);
			input_info(true, &ts->client->dev, "%s: FOD OUT\n", __func__);
		} else if (p_gesture_status->gesture_id == SYNAPTICS_TS_GESTURE_ID_FOD_VI) {
			if ((ts->plat_data->lowpower_mode & SEC_TS_MODE_SPONGE_PRESS) && ts->plat_data->support_fod_lp_mode)
				synaptics_ts_fod_vi_event(ts);
		} else {
			input_info(true, &ts->client->dev, "%s: invalid id %d\n",
					__func__, p_gesture_status->gesture_id);
		}
	} else if (p_gesture_status->stype  == SYNAPTICS_TS_GESTURE_CODE_DUMPFLUSH) {
#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUMP_MODE)
		if (ts->sponge_inf_dump) {
			if (ts->plat_data->power_state == SEC_INPUT_STATE_LPM) {
				if (p_gesture_status->gesture_id == SYNAPTICS_TS_SPONGE_DUMP_0)
					synaptics_ts_sponge_dump_flush(ts, SYNAPTICS_TS_SPONGE_DUMP_0);
				if (p_gesture_status->gesture_id == SYNAPTICS_TS_SPONGE_DUMP_1)
					synaptics_ts_sponge_dump_flush(ts, SYNAPTICS_TS_SPONGE_DUMP_1);
			} else {
				ts->sponge_dump_delayed_flag = true;
				ts->sponge_dump_delayed_area = p_gesture_status->gesture_id;
			}
		}
#endif
	}
}

static void synaptics_ts_coordinate_event(struct synaptics_ts_data *ts, u8 *event_buff)
{
	struct sec_touch_event_data *p_event_coord;
	u8 t_id = 0;

	if (ts->plat_data->power_state != SEC_INPUT_STATE_POWER_ON) {
		input_err(true, &ts->client->dev,
				"%s: device is closed %x %x %x %x %x %x %x %x\n", __func__,
				event_buff[0], event_buff[1], event_buff[2],
				event_buff[3], event_buff[4], event_buff[5],
				event_buff[6], event_buff[7]);
		return;
	}

	p_event_coord = (struct sec_touch_event_data *)event_buff;

	t_id = p_event_coord->tid;

	if (t_id < SEC_TS_SUPPORT_TOUCH_COUNT) {
		ts->plat_data->prev_coord[t_id] = ts->plat_data->coord[t_id];
		synaptics_ts_coord_parsing(ts, p_event_coord, t_id);

		if ((ts->plat_data->coord[t_id].ttype == SYNAPTICS_TS_TOUCHTYPE_NORMAL)
				|| (ts->plat_data->coord[t_id].ttype == SYNAPTICS_TS_TOUCHTYPE_PALM)
				|| (ts->plat_data->coord[t_id].ttype == SYNAPTICS_TS_TOUCHTYPE_WET)
				|| (ts->plat_data->coord[t_id].ttype == SYNAPTICS_TS_TOUCHTYPE_GLOVE)) {
			sec_input_coord_event(&ts->client->dev, t_id);
		} else {
			input_err(true, &ts->client->dev,
					"%s: do not support coordinate type(%d)\n",
					__func__, ts->plat_data->coord[t_id].ttype);
		}
	} else {
		input_err(true, &ts->client->dev, "%s: tid(%d) is out of range\n", __func__, t_id);
	}
}

static void synaptics_ts_status_event(struct synaptics_ts_data *ts, u8 *event_buff)
{
	struct sec_status_event_data *p_event_status;

	p_event_status = (struct sec_status_event_data *)event_buff;

	if (p_event_status->stype > 0)
		input_info(true, &ts->client->dev, "%s: STATUS %x %x %x %x %x %x %x %x\n", __func__,
				event_buff[0], event_buff[1], event_buff[2],
				event_buff[3], event_buff[4], event_buff[5],
				event_buff[6], event_buff[7]);

	if (p_event_status->stype == SYNAPTICS_TS_TOUCHTYPE_PROXIMITY) {
		if (p_event_status->status_id == SYNAPTICS_TS_STATUS_EVENT_VENDOR_PROXIMITY) {
			ts->hover_event = p_event_status->status_data_1;
			sec_input_proximity_report(&ts->client->dev, p_event_status->status_data_1);
		}
	}

#if 0
	if (p_event_status->stype == SYNAPTICS_TS_EVENT_STATUSTYPE_ERROR) {
		if (p_event_status->status_id == SYNAPTICS_TS_ERR_EVENT_QUEUE_FULL) {
			input_err(true, &ts->client->dev, "%s: IC Event Queue is full\n", __func__);
			synaptics_ts_release_all_finger(ts);
		} else if (p_event_status->status_id == SYNAPTICS_TS_ERR_EVENT_ESD) {
			input_err(true, &ts->client->dev, "%s: ESD detected\n", __func__);
			if (!ts->reset_is_on_going)
				schedule_delayed_work(&ts->reset_work, msecs_to_jiffies(10));
		}
	} else if (p_event_status->stype == SYNAPTICS_TS_EVENT_STATUSTYPE_INFO) {
		if (p_event_status->status_id == SYNAPTICS_TS_INFO_READY_STATUS) {
			if (p_event_status->status_data_1 == 0x10) {
				input_err(true, &ts->client->dev, "%s: IC Reset\n", __func__);
				if (!ts->reset_is_on_going)
					schedule_delayed_work(&ts->reset_work, msecs_to_jiffies(10));
			}
		} else if (p_event_status->status_id == SYNAPTICS_TS_INFO_WET_MODE) {
			ts->plat_data->wet_mode = p_event_status->status_data_1;
			input_info(true, &ts->client->dev, "%s: water wet mode %d\n",
				__func__, ts->plat_data->wet_mode);
			if (ts->plat_data->wet_mode)
				ts->plat_data->hw_param.wet_count++;
		} else if (p_event_status->status_id == SYNAPTICS_TS_INFO_NOISE_MODE) {
			ts->plat_data->touch_noise_status = (p_event_status->status_data_1 >> 4);

			input_info(true, &ts->client->dev, "%s: NOISE MODE %s[%02X]\n",
					__func__, ts->plat_data->touch_noise_status == 0 ? "OFF" : "ON",
					p_event_status->status_data_1);

			if (ts->plat_data->touch_noise_status)
				ts->plat_data->hw_param.noise_count++;
		}
	} else if (p_event_status->stype == SYNAPTICS_TS_EVENT_STATUSTYPE_VENDORINFO) {
		if (ts->plat_data->support_ear_detect) {
			if (p_event_status->status_id == 0x6A) {
				ts->hover_event = p_event_status->status_data_1;
				input_report_abs(ts->plat_data->input_dev_proximity, ABS_MT_CUSTOM, p_event_status->status_data_1);
				input_sync(ts->plat_data->input_dev_proximity);
				input_info(true, &ts->client->dev, "%s: proximity: %d\n", __func__, p_event_status->status_data_1);
			}
		}
	}
#endif
}

/**
 * syna_tcm_get_touch_data()
 *
 * Get data entity from the received report according to bit offset and bit
 * length defined in the touch report configuration.
 *
 * @param
 *    [ in] report:      touch report generated by TouchComm device
 *    [ in] report_size: size of given report
 *    [ in] offset:      bit offset in the report
 *    [ in] bits:        number of bits representing the data
 *    [out] data:        data parsed
 *
 * @return
 *    on success, 0 or positive value; otherwise, negative value on error.
 */
int synaptics_ts_get_touch_data(struct synaptics_ts_data *ts, const unsigned char *report,
		unsigned int report_size, unsigned int offset,
		unsigned int bits, unsigned int *data)
{
	unsigned char mask;
	unsigned char byte_data;
	unsigned int output_data;
	unsigned int bit_offset;
	unsigned int byte_offset;
	unsigned int data_bits;
	unsigned int available_bits;
	unsigned int remaining_bits;

	if (bits == 0 || bits > 32) {
		input_err(true, &ts->client->dev, "%s: Invalid number of bits %d\n", __func__, bits);
		return -EINVAL;
	}

	if (!report) {
		input_err(true, &ts->client->dev,"Invalid report data\n");
		return -EINVAL;
	}

	if (offset + bits > report_size * 8) {
		*data = 0;
		return 0;
	}

	output_data = 0;
	remaining_bits = bits;

	bit_offset = offset % 8;
	byte_offset = offset / 8;

	while (remaining_bits) {
		byte_data = report[byte_offset];
		byte_data >>= bit_offset;

		available_bits = 8 - bit_offset;
		data_bits = MIN(available_bits, remaining_bits);
		mask = 0xff >> (8 - data_bits);

		byte_data &= mask;

		output_data |= byte_data << (bits - remaining_bits);

		bit_offset = 0;
		byte_offset += 1;
		remaining_bits -= data_bits;
	}

	*data = output_data;

	return 0;
}

/**
 * syna_tcm_get_gesture_data()
 *
 * The contents of the gesture data entity depend on which gesture
 * is detected. The default size of data is defined in 16-64 bits natively.
 *
 * @param
 *    [ in] report:       touch report generated by TouchComm device
 *    [ in] report_size:  size of given report
 *    [ in] offset:       bit offset in the report
 *    [ in] bits:         total bits representing the gesture data
 *    [out] gesture_data: gesture data parsed
 *    [ in] gesture_id:   gesture id retrieved
 *
 * @return
 *    on success, 0 or positive value; otherwise, negative value on error.
 */
static int synaptics_ts_get_gesture_data(struct synaptics_ts_data *ts, const unsigned char *report,
		unsigned int report_size, unsigned int offset,
		unsigned int bits, struct synaptics_ts_gesture_data_blob *gesture_data,
		unsigned int gesture_id)
{
	int retval;
	unsigned int idx;
	unsigned int data;
	unsigned int size;
	unsigned int data_end;

	if (offset + bits > report_size * 8)
		return 0;

	data_end = offset + bits;

	size = (sizeof(gesture_data->data) / sizeof(unsigned char));

	idx = 0;
	while ((offset < data_end) && (idx < size)) {
		retval = synaptics_ts_get_touch_data(ts, report, report_size,
				offset, 16, &data);
		if (retval < 0) {
			input_err(true, &ts->client->dev, "%s: Fail to get object index\n", __func__);
			return retval;
		}
		gesture_data->data[idx++] = (unsigned char)(data & 0xff);
		gesture_data->data[idx++] = (unsigned char)((data >> 8) & 0xff);
		offset += 16;
	}

	switch (gesture_id) {
	case GESTURE_ID_DOUBLE_TAP:
	case GESTURE_ID_ACTIVE_TAP_AND_HOLD:
		input_info(true, &ts->client->dev, "%s: Tap info: (%d, %d)\n", __func__,
		synaptics_ts_pal_le2_to_uint(gesture_data->tap_x),
		synaptics_ts_pal_le2_to_uint(gesture_data->tap_y));
		break;
	case GESTURE_ID_SWIPE:
		input_info(true, &ts->client->dev, "%s: Swipe info: direction:%x (%d, %d)\n", __func__,
		synaptics_ts_pal_le2_to_uint(gesture_data->swipe_direction),
		synaptics_ts_pal_le2_to_uint(gesture_data->tap_x),
		synaptics_ts_pal_le2_to_uint(gesture_data->tap_y));
		break;
	default:
		input_err(true, &ts->client->dev, "%s: Unknown gesture_id:%d\n", __func__, gesture_id);
		break;
	}

	return 0;
}

/**
 * syna_tcm_parse_touch_report()
 *
 * Traverse through touch report configuration and parse the contents of
 * report packet to get the exactly touched data entity from touch reports.
 *
 * At the end of function, the touched data will be parsed and stored at the
 * associated position in structure touch_data_blob.
 *
 * @param
 *    [ in] tcm_dev:     the device handle
 *    [ in] report:      touch report generated by TouchComm device
 *    [ in] report_size: size of given report
 *    [out] touch_data:  touch data generated
 *
 * @return
 *    on success, 0 or positive value; otherwise, negative value on error.
 */
int synaptics_ts_parse_touch_report(struct synaptics_ts_data *ts,
		unsigned char *report, unsigned int report_size,
		struct synaptics_ts_touch_data_blob *touch_data)
{
	int retval;
	bool active_only;
	bool num_of_active_objects;
	unsigned char code;
	unsigned int size;
	unsigned int idx;
	unsigned int obj;
	unsigned int next;
	unsigned int data;
	unsigned int bits;
	unsigned int offset;
	unsigned int objects;
	unsigned int active_objects;
	unsigned int config_size;
	unsigned char *config_data;
	struct synaptics_ts_objects_data_blob *object_data;
	static unsigned int end_of_foreach;

	if (!ts) {
		pr_err("%s%s:Invalid tcm device handle\n", SECLOG, __func__);
		return -EINVAL;
	}

	if (!report) {
		input_err(true, &ts->client->dev,"Invalid report data\n");
		return -EINVAL;
	}

	if (!touch_data) {
		input_err(true, &ts->client->dev,"Invalid touch data structure\n");
		return -EINVAL;
	}
	if (ts->max_objects == 0) {
		input_err(true, &ts->client->dev, "%s: Invalid max_objects supported\n", __func__);
		return -EINVAL;
	}

	object_data = touch_data->object_data;

	if (!object_data) {
		input_err(true, &ts->client->dev, "%s: Invalid object_data\n", __func__);
		return -EINVAL;
	}

	config_data = ts->touch_config.buf;
	config_size = ts->touch_config.data_length;

	if ((!config_data) || (config_size == 0)) {
		input_err(true, &ts->client->dev, "%s: Invalid config_data\n", __func__);
		return -EINVAL;
	}

	size = sizeof(touch_data->object_data);
	synaptics_ts_pal_mem_set(touch_data->object_data, 0x00, size);

	num_of_active_objects = false;

	idx = 0;
	offset = 0;
	objects = 0;
	active_objects = 0;
	active_only = false;
	obj = 0;
	next = 0;

	while (idx < config_size) {
		code = config_data[idx++];
		switch (code) {
		case TOUCH_REPORT_END:
			goto exit;
		case TOUCH_REPORT_FOREACH_ACTIVE_OBJECT:
			obj = 0;
			next = idx;
			active_only = true;
			break;
		case TOUCH_REPORT_FOREACH_OBJECT:
			obj = 0;
			next = idx;
			active_only = false;
			break;
		case TOUCH_REPORT_FOREACH_END:
			end_of_foreach = idx;
			if (active_only) {
				if (num_of_active_objects) {
					objects++;
					obj++;
					if (objects < active_objects)
						idx = next;
				} else if (offset < report_size * 8) {
					idx = next;
				}
			} else {
				obj++;
				if (obj < ts->max_objects)
					idx = next;
			}
			break;
		case TOUCH_REPORT_PAD_TO_NEXT_BYTE:
			offset = (((offset + 8) - 1)/ 8)  * 8;
			break;
		case TOUCH_REPORT_TIMESTAMP:
			bits = config_data[idx++];
			retval = synaptics_ts_get_touch_data(ts, report, report_size,
					offset, bits, &data);
			if (retval < 0) {
				input_err(true, &ts->client->dev, "%s: Fail to get time-stamp\n", __func__);
				return retval;
			}
			touch_data->timestamp = data;
			offset += bits;
			break;
		case TOUCH_REPORT_OBJECT_N_INDEX:
			bits = config_data[idx++];
			retval = synaptics_ts_get_touch_data(ts, report, report_size,
					offset, bits, &data);
			if (retval < 0) {
				input_err(true, &ts->client->dev, "%s: Fail to get object index\n", __func__);
				return retval;
			}
			obj = data;
			offset += bits;
			break;
		case TOUCH_REPORT_OBJECT_N_CLASSIFICATION:
			bits = config_data[idx++];
			retval = synaptics_ts_get_touch_data(ts, report, report_size,
					offset, bits, &data);
			if (retval < 0) {
				input_err(true, &ts->client->dev, "%s: Fail to get object classification\n", __func__);
				return retval;
			}
			object_data[obj].status = (unsigned char)data;
			offset += bits;
			break;
		case TOUCH_REPORT_OBJECT_N_X_POSITION:
			bits = config_data[idx++];
			retval = synaptics_ts_get_touch_data(ts, report, report_size,
					offset, bits, &data);
			if (retval < 0) {
				input_err(true, &ts->client->dev, "%s: Fail to get object x position\n", __func__);
				return retval;
			}
			object_data[obj].x_pos = data;
			offset += bits;
			break;
		case TOUCH_REPORT_OBJECT_N_Y_POSITION:
			bits = config_data[idx++];
			retval = synaptics_ts_get_touch_data(ts, report, report_size,
					offset, bits, &data);
			if (retval < 0) {
				input_err(true, &ts->client->dev, "%s: Fail to get object y position\n", __func__);
				return retval;
			}
			object_data[obj].y_pos = data;
			offset += bits;
			break;
		case TOUCH_REPORT_OBJECT_N_Z:
			bits = config_data[idx++];
			retval = synaptics_ts_get_touch_data(ts, report, report_size,
					offset, bits, &data);
			if (retval < 0) {
				input_err(true, &ts->client->dev, "%s: Fail to get object z\n", __func__);
				return retval;
			}
			object_data[obj].z = data;
			offset += bits;
			break;
		case TOUCH_REPORT_OBJECT_N_X_WIDTH:
			bits = config_data[idx++];
			retval = synaptics_ts_get_touch_data(ts, report, report_size,
					offset, bits, &data);
			if (retval < 0) {
				input_err(true, &ts->client->dev, "%s: Fail to get object x width\n", __func__);
				return retval;
			}
			object_data[obj].x_width = data;
			offset += bits;
			break;
		case TOUCH_REPORT_OBJECT_N_Y_WIDTH:
			bits = config_data[idx++];
			retval = synaptics_ts_get_touch_data(ts, report, report_size,
					offset, bits, &data);
			if (retval < 0) {
				input_err(true, &ts->client->dev, "%s: Fail to get object y width\n", __func__);
				return retval;
			}
			object_data[obj].y_width = data;
			offset += bits;
			break;
		case TOUCH_REPORT_OBJECT_N_TX_POSITION_TIXELS:
			bits = config_data[idx++];
			retval = synaptics_ts_get_touch_data(ts, report, report_size,
					offset, bits, &data);
			if (retval < 0) {
				input_err(true, &ts->client->dev, "%s: Fail to get object tx position\n", __func__);
				return retval;
			}
			object_data[obj].tx_pos = data;
			offset += bits;
			break;
		case TOUCH_REPORT_OBJECT_N_RX_POSITION_TIXELS:
			bits = config_data[idx++];
			retval = synaptics_ts_get_touch_data(ts, report, report_size,
					offset, bits, &data);
			if (retval < 0) {
				input_err(true, &ts->client->dev, "%s: Fail to get object rx position\n", __func__);
				return retval;
			}
			object_data[obj].rx_pos = data;
			offset += bits;
			break;
		case TOUCH_REPORT_NUM_OF_ACTIVE_OBJECTS:
			bits = config_data[idx++];
			retval = synaptics_ts_get_touch_data(ts, report, report_size,
					offset, bits, &data);
			if (retval < 0) {
				input_err(true, &ts->client->dev, "%s: Fail to get number of active objects\n", __func__);
				return retval;
			}
			active_objects = data;
			num_of_active_objects = true;
			touch_data->num_of_active_objects = data;
			offset += bits;
			if (touch_data->num_of_active_objects == 0) {
				if (end_of_foreach == 0) {
					input_err(true, &ts->client->dev, "%s: Invalid end_foreach\n", __func__);
					return 0;
				}
				idx = end_of_foreach;
			}
			break;
		case TOUCH_REPORT_0D_BUTTONS_STATE:
			bits = config_data[idx++];
			retval = synaptics_ts_get_touch_data(ts, report, report_size,
					offset, bits, &data);
			if (retval < 0) {
				input_err(true, &ts->client->dev, "%s: Fail to get 0D buttons state\n", __func__);
				return retval;
			}
			touch_data->buttons_state = data;
			offset += bits;
			break;
		case TOUCH_REPORT_GESTURE_ID:
			bits = config_data[idx++];
			retval = synaptics_ts_get_touch_data(ts, report,
				report_size, offset, bits, &data);
	
			if (retval < 0) {
				input_err(true, &ts->client->dev, "%s: Fail to get gesture id\n", __func__);
				return retval;
			}

			touch_data->gesture_id = data;
			offset += bits;
			break;
		case TOUCH_REPORT_GESTURE_DATA:
			bits = config_data[idx++];
			retval = synaptics_ts_get_gesture_data(ts, report,
						report_size,
						offset, bits,
						&touch_data->gesture_data,
						touch_data->gesture_id);
			offset += bits;
			if (retval < 0) {
				input_err(true, &ts->client->dev, "%s: Fail to get gesture data\n", __func__);
				return retval;
			}
			break;
		case TOUCH_REPORT_FRAME_RATE:
			bits = config_data[idx++];
			retval = synaptics_ts_get_touch_data(ts, report, report_size,
					offset, bits, &data);
			if (retval < 0) {
				input_err(true, &ts->client->dev, "%s: Fail to get frame rate\n", __func__);
				return retval;
			}
			touch_data->frame_rate = data;
			offset += bits;
			break;
		case TOUCH_REPORT_FORCE_MEASUREMENT:
			bits = config_data[idx++];
			retval = synaptics_ts_get_touch_data(ts, report, report_size,
					offset, bits, &data);
			if (retval < 0) {
				input_err(true, &ts->client->dev, "%s: Fail to get force measurement data\n", __func__);
				return retval;
			}
			touch_data->force_data = data;
			offset += bits;
			break;
		case TOUCH_REPORT_FINGERPRINT_AREA_MEET:
			bits = config_data[idx++];
			retval = synaptics_ts_get_touch_data(ts, report, report_size,
					offset, bits, &data);
			if (retval < 0) {
				input_err(true, &ts->client->dev, "%s: Fail to get data for fingerprint area\n", __func__);
				return retval;
			}
			touch_data->fingerprint_area_meet = data;
			offset += bits;
			break;
		case TOUCH_REPORT_POWER_IM:
			bits = config_data[idx++];
			retval = synaptics_ts_get_touch_data(ts, report, report_size,
					offset, bits, &data);
			if (retval < 0) {
				input_err(true, &ts->client->dev, "%s: Fail to get power IM\n", __func__);
				return retval;
			}
			touch_data->power_im = data;
			offset += bits;
			break;
		case TOUCH_REPORT_CID_IM:
			bits = config_data[idx++];
			retval = synaptics_ts_get_touch_data(ts, report, report_size,
					offset, bits, &data);
			if (retval < 0) {
				input_err(true, &ts->client->dev, "%s: Fail to get CID IM\n", __func__);
				return retval;
			}
			touch_data->cid_im = data;
			offset += bits;
			break;
		case TOUCH_REPORT_RAIL_IM:
			bits = config_data[idx++];
			retval = synaptics_ts_get_touch_data(ts, report, report_size,
					offset, bits, &data);
			if (retval < 0) {
				input_err(true, &ts->client->dev, "%s: Fail to get rail IM\n", __func__);
				return retval;
			}
			touch_data->rail_im = data;
			offset += bits;
			break;
		case TOUCH_REPORT_CID_VARIANCE_IM:
			bits = config_data[idx++];
			retval = synaptics_ts_get_touch_data(ts, report, report_size,
					offset, bits, &data);
			if (retval < 0) {
				input_err(true, &ts->client->dev, "%s: Fail to get CID variance IM\n", __func__);
				return retval;
			}
			touch_data->cid_variance_im = data;
			offset += bits;
			break;
		case TOUCH_REPORT_NSM_FREQUENCY_INDEX:
			bits = config_data[idx++];
			retval = synaptics_ts_get_touch_data(ts, report, report_size,
					offset, bits, &data);
			if (retval < 0) {
				input_err(true, &ts->client->dev, "%s: Fail to get NSM frequency\n", __func__);
				return retval;
			}
			touch_data->nsm_frequency = data;
			offset += bits;
			break;
		case TOUCH_REPORT_NSM_STATE:
			bits = config_data[idx++];
			retval = synaptics_ts_get_touch_data(ts, report, report_size,
					offset, bits, &data);
			if (retval < 0) {
				input_err(true, &ts->client->dev, "%s: Fail to get NSM state\n", __func__);
				return retval;
			}
			touch_data->nsm_state = data;
			offset += bits;
			break;
		case TOUCH_REPORT_CPU_CYCLES_USED_SINCE_LAST_FRAME:
			bits = config_data[idx++];
			retval = synaptics_ts_get_touch_data(ts, report, report_size,
					offset, bits, &data);
			if (retval < 0) {
				input_err(true, &ts->client->dev, "%s: Fail to get cpu cycles info\n", __func__);
				return retval;
			}
			touch_data->num_of_cpu_cycles = data;
			offset += bits;
			break;
		case TOUCH_REPORT_FACE_DETECT:
			bits = config_data[idx++];
			retval = synaptics_ts_get_touch_data(ts, report, report_size,
					offset, bits, &data);
			if (retval < 0) {
				input_err(true, &ts->client->dev, "%s: Fail to detect face\n", __func__);
				return retval;
			}
			touch_data->fd_data = data;
			offset += bits;
			break;
		case TOUCH_REPORT_SENSING_MODE:
			bits = config_data[idx++];
			retval = synaptics_ts_get_touch_data(ts, report, report_size,
					offset, bits, &data);
			if (retval < 0) {
				input_err(true, &ts->client->dev, "%s: Fail to get sensing mode\n", __func__);
				return retval;
			}
			touch_data->sensing_mode = data;
			offset += bits;
			break;
		default:

			input_err(true, &ts->client->dev, "%s: Unknown touch config code:0x%02x (length:%d)\n", __func__,
				code, config_data[idx]);
			bits = config_data[idx++];
			offset += bits;
			break;
		}
	}

exit:
	return 0;
}

/**
 * syna_tcm_get_event_data()
 *
 * Helper to read TouchComm messages when ATTN signal is asserted.
 * After returning, the ATTN signal should be no longer asserted.
 *
 * The 'code' returned will guide the caller on the next action.
 * For example, do touch reporting once returned code is equal to REPORT_TOUCH.
 *
 * @param
 *    [ in] tcm_dev: the device handle
 *    [out] code:    received report code
 *    [out] report:  report data returned
 *    [out] tp_data: touched data returned, if 'code' is REPORT_TOUCH
 *
 * @return
 *    on success, 0 or positive value; otherwise, negative value on error.
 */
int synaptics_ts_get_event(struct synaptics_ts_data *ts,
		unsigned char *code, struct synaptics_ts_buffer *report,
		struct synaptics_ts_touch_data_blob *tp_data)
{
	int retval = 0;

	if (!ts) {
		pr_err("%s%s:Invalid tcm device handle\n", SECLOG, __func__);
		return -EINVAL;
	}

	if (!code) {
		input_err(true, &ts->client->dev,"Invalid parameter\n");
		return -EINVAL;
	}

	/* retrieve the event data */
	retval = ts->read_message(ts,
			code);
	if (retval < 0) {
		input_err(true, &ts->client->dev,"Fail to read messages\n");
		return retval;
	}

	synaptics_ts_buf_lock(&ts->report_buf);

	/* no perform data parsing if no tp_data provided */
	if (!tp_data)
		goto report_cpy;

	/* parse touch report once received */
	if (*code == REPORT_TOUCH) {
		retval = synaptics_ts_parse_touch_report(ts,
				ts->report_buf.buf,
				ts->report_buf.data_length,
				tp_data);
		if (retval < 0)
			input_err(true, &ts->client->dev, "%s: Fail to parse touch report\n", __func__);
	}

report_cpy:
	/* exit if no buffer provided or no action needed */
	if ((!report) || (ts->report_buf.data_length == 0))
		goto exit;

	/* return the received report */
	if ((*code >= REPORT_IDENTIFY) && (*code != STATUS_INVALID)) {
		retval = synaptics_ts_buf_copy(report, &ts->report_buf);
		if (retval < 0) {
			input_err(true, &ts->client->dev,"Fail to copy report data\n");
			goto exit;
		}
	}

exit:
	synaptics_ts_buf_unlock(&ts->report_buf);	

	return retval;
}

irqreturn_t synaptics_ts_irq_thread(int irq, void *ptr)
{
	struct synaptics_ts_data *ts = (struct synaptics_ts_data *)ptr;
	unsigned char event_id;
	int ret;
	int curr_pos;
	u8 *event_buff;
	int remain_event_count;
	ret = event_id = curr_pos = remain_event_count = 0;

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (synaptics_secure_filter_interrupt(ts) == IRQ_HANDLED) {
		wait_for_completion_interruptible_timeout(&ts->secure_interrupt,
				msecs_to_jiffies(5 * MSEC_PER_SEC));

		input_info(true, &ts->client->dev,
				"%s: secure interrupt handled\n", __func__);

		return IRQ_HANDLED;
	}
#endif
#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return IRQ_HANDLED;
#endif

#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
	/* for vendor */
	if (ts->vendor_data->is_attn_redirecting) {
		syna_cdev_redirect_attn(ts);
	}
#endif

	ret = sec_input_handler_start(&ts->client->dev);
	if (ret < 0)
		return IRQ_HANDLED;

	ret = synaptics_ts_get_event(ts, &event_id, &ts->event_data, &ts->tp_data);
	if (ret < 0)
		return IRQ_HANDLED;

	if (ts->event_data.data_length == 0) {
		input_err(true, &ts->client->dev, "%s:data_length %d\n", __func__, ts->event_data.data_length);
		return IRQ_HANDLED;
	}

	mutex_lock(&ts->eventlock);
	
	remain_event_count = ts->event_data.data_length/sizeof(struct sec_touch_event_data);
	do {
		event_buff = &ts->event_data.buf[curr_pos * sizeof(struct sec_touch_event_data)];
//		event_id = event_buff[0] & 0x3;
		if (ts->debug_flag & SEC_TS_DEBUG_PRINT_ALLEVENT)
			input_info(true, &ts->client->dev, "%s: ALL event_id(%02X) %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
						__func__, event_id, event_buff[0], event_buff[1], event_buff[2], event_buff[3], event_buff[4], event_buff[5],
						event_buff[6], event_buff[7], event_buff[8], event_buff[9], event_buff[10], event_buff[11],
						event_buff[12], event_buff[13], event_buff[14], event_buff[15]);

		if (event_id == REPORT_SEC_STATUS_EVENT)
			synaptics_ts_status_event(ts, event_buff);
		else if (event_id == REPORT_SEC_COORDINATE_EVENT)
			synaptics_ts_coordinate_event(ts, event_buff);
		else if (event_id == REPORT_SEC_SPONGE_GESTURE)
			synaptics_ts_gesture_event(ts, event_buff);
		else if (event_id == STATUS_OK)
			input_info(true, &ts->client->dev, "%s: response event event_id(%02X)\n", __func__, event_id);
		else
			input_info(true, &ts->client->dev,
					"%s: unknown event event_id(%02X) %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
						__func__, event_id, event_buff[0], event_buff[1], event_buff[2], event_buff[3], event_buff[4], event_buff[5],
						event_buff[6], event_buff[7], event_buff[8], event_buff[9], event_buff[10], event_buff[11],
						event_buff[12], event_buff[13], event_buff[14], event_buff[15]);
		curr_pos++;
		remain_event_count--;
	} while (remain_event_count > 0);

	synaptics_ts_external_func(ts);

	mutex_unlock(&ts->eventlock);

	return IRQ_HANDLED;
}


MODULE_LICENSE("GPL");

