/*
 * Copyright (c) 2013 Crocodile RCS Ltd
 * Copyright (C) 2014-2015 Sipwise GmbH, http://www.sipwise.com
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "../../lib/srdb1/db.h"
#include "../../lib/srdb1/db_res.h"

#include "../../core/parser/msg_parser.h"
#include "rtpengine.h"

#define RTPP_TABLE_VERSION 1

static db_func_t rtpp_dbf;
static db1_con_t *rtpp_db_handle = NULL;

str rtpp_db_url = {NULL, 0};
str rtpp_table_name = str_init("rtpengine");
str rtpp_setid_col = str_init("setid");
str rtpp_url_col = str_init("url");
str rtpp_weight_col = str_init("weight");
str rtpp_disabled_col = str_init("disabled");

static int rtpp_connect_db(void)
{
	if((rtpp_db_url.s == NULL) || (rtpp_db_url.len == 0))
		return -1;
	if((rtpp_db_handle = rtpp_dbf.init(&rtpp_db_url)) == NULL) {
		LM_ERR("Cannot initialize db connection\n");
		return -1;
	}
	return 0;
}

static void rtpp_disconnect_db(void)
{
	if(rtpp_db_handle) {
		rtpp_dbf.close(rtpp_db_handle);
		rtpp_db_handle = NULL;
	}
}

static int rtpp_load_db(void)
{
	int i;
	struct rtpp_set *rtpp_list = NULL;
	db1_res_t *res = NULL;
	db_val_t *values = NULL;
	db_row_t *rows = NULL;
	db_key_t query_cols[] = {&rtpp_setid_col, &rtpp_url_col, &rtpp_weight_col,
			&rtpp_disabled_col};

	str url;
	int disabled;
	unsigned int setid, weight, ticks;

	/* int weight, flags; */
	int n_rows = 0;
	int n_cols = 4;

	if(rtpp_db_handle == NULL) {
		LM_ERR("invalid db handle\n");
		return -1;
	}
	if(rtpp_dbf.use_table(rtpp_db_handle, &rtpp_table_name) < 0) {
		LM_ERR("unable to use table '%.*s'\n", rtpp_table_name.len,
				rtpp_table_name.s);
		return -1;
	}
	if(rtpp_dbf.query(rtpp_db_handle, 0, 0, 0, query_cols, 0, n_cols, 0, &res)
			< 0) {
		LM_ERR("error while running db query\n");
		return -1;
	}

	rtpengine_delete_node_all();

	n_rows = RES_ROW_N(res);
	rows = RES_ROWS(res);
	if(n_rows == 0) {
		LM_WARN("No rtpengine instances in database\n");
		rtpp_dbf.free_result(rtpp_db_handle, res);
		return 0;
	}

	for(i = 0; i < n_rows; i++) {
		values = ROW_VALUES(rows + i);

		setid = VAL_INT(values);
		url.s = VAL_STR(values + 1).s;
		url.len = strlen(url.s);
		weight = VAL_INT(values + 2);
		disabled = VAL_INT(values + 3);
		if(disabled) {
			ticks = RTPENGINE_MAX_RECHECK_TICKS;
		} else {
			ticks = 0;
		}
		/*
		weight = VAL_INT(values+2);
		flags = VAL_INT(values+3);
		*/
		if((rtpp_list = get_rtpp_set(setid, 1)) == NULL) {
			LM_ERR("error getting rtpp_list for set %d\n", setid);
			continue;
		}

		if(add_rtpengine_socks(rtpp_list, url.s, weight, disabled, ticks, 1)
				!= 0) {
			LM_ERR("error inserting '%.*s' into set %d disabled=%d\n", url.len,
					url.s, setid, disabled);
		}
	}

	rtpp_dbf.free_result(rtpp_db_handle, res);
	return 0;
}

int init_rtpengine_db(void)
{
	int ret;
	if(rtpp_db_url.s == NULL)
		/* Database not configured */
		return 0;

	if(db_bind_mod(&rtpp_db_url, &rtpp_dbf) < 0) {
		LM_ERR("Unable to bind to db driver - %.*s\n", rtpp_db_url.len,
				rtpp_db_url.s);
		return -1;
	}
	if(rtpp_connect_db() != 0) {
		LM_ERR("Unable to connect to db\n");
		return -1;
	}

	if(db_check_table_version(
			   &rtpp_dbf, rtpp_db_handle, &rtpp_table_name, RTPP_TABLE_VERSION)
			< 0) {
		DB_TABLE_VERSION_ERROR(rtpp_table_name);
		ret = -1;
		goto done;
	}
	ret = rtpp_load_db();

done:
	rtpp_disconnect_db();

	return ret;
}
