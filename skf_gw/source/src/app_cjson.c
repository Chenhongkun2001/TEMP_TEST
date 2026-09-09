#include "app_cjson.h"

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include "sys_def.h"

#include "util_dbg.h"
#include "jsonParse.h"
#include "jsonBuild.h"

DBG_LOCAL_LOG_DEBUG


//gwConfig_t gwConfig_1;
//gwConfig_t gwConfig_2;

//sensorConfig_t sensorConfig_1;
//sensorConfig_t sensorConfig_2;

/*to parase the JSON string from a file into a structure
pt_sensor_cft@the structure where the data will be stored
f_path@the path to the file  where the JSON string is
ret@true when the string is parsed successfully
*/
bool app_cjson_jsonstr_to_gwconfig( gwConfig_t *pt_gw_cfg, const uint8_t*f_path)
{
	
	bool tpret = true;
	int kp_fd = -1;
	uint8_t *pt_strbuf = NULL;
	off_t kp_fszie = 0;
	int8_t tpi8 = 0;
	
	if((NULL == pt_gw_cfg)||(NULL == f_path))
	{
		DBG_LOG_ERR("unexpected NULL");
		tpret = false;
		return tpret;
	}

	kp_fd = open(f_path, O_RDONLY);
	if(kp_fd < 0)
	{
		DBG_LOG_ERR("fail to open file %s", f_path);
		tpret = false;
		return tpret;
	}
	kp_fszie = lseek( kp_fd, 0, SEEK_END);
	if(kp_fszie < 0)
	{
		DBG_LOG_ERR("fail to get the file size");
		tpret = false;
		goto EXIT;
	}
	lseek( kp_fd, 0, SEEK_SET);

	pt_strbuf = l_malloc( kp_fszie);
	if(NULL == pt_strbuf)
	{
		DBG_LOG_ERR("l_malloc fail");
		tpret = false;
		goto EXIT;
	}
	memset( pt_strbuf, 0, kp_fszie);

	if(read(kp_fd, pt_strbuf, kp_fszie) < 0)
	{
		tpret = false;
		goto EXIT;
	}
	DBG_LOG_INFO("Going to parse json-str :\n %s \n", pt_strbuf);
	
	parseGwConfigJson( pt_strbuf, pt_gw_cfg, &tpi8);

	if(tpi8)
	{
		DBG_LOG_ERR("parseGwConfigJson fail");
		tpret = false;
		goto EXIT;
	}
	DBG_LOG_INFO("the json-str is parased successfully");
	
	EXIT:
		if(kp_fd >= 0)
		{
			close(kp_fd);
		}
		if(pt_strbuf)
		{
			l_free( pt_strbuf);
			pt_strbuf = NULL;
		}
		return tpret;
	
	
}



/*save gwconfig structure as JSON string to a file
pt_gwconf@GW configuration info
pt_fpath@ the file that the JSON string written to
ret@true
*/
#ifndef SZ_JSONSTR_BUF 
	#define SZ_JSONSTR_BUF	(8192U * 2)
#endif
bool app_cjson_gwconfig_to_jsonstr( const gwConfig_t *pt_gwconf, uint8_t *pt_fpath)
{
	bool tpret = true;
	uint8_t *pt_strbuf = NULL;
	int8_t tpi8 = 0;
	int kp_fd = -1;
	
	pt_strbuf = (uint8_t*)l_malloc( SZ_JSONSTR_BUF);
	if(NULL == pt_strbuf)
	{
		DBG_LOG_ERR("l_malloc fail");
		tpret = false;
		goto EXIT;
	}
	generateGwConfigJson( pt_gwconf, pt_strbuf, &tpi8);
	if(tpi8 < 0)
	{
		DBG_LOG_ERR("fail to format GW conf to JSON str");
		tpret = false;
		goto EXIT;
	}

	DBG_LOG_INFO("BKP SZ_JSONSTR_BUF = %d", SZ_JSONSTR_BUF);
	
	// to write JSON string to file
	kp_fd = open(pt_fpath, O_CREAT|O_RDWR|O_TRUNC, S_IRUSR | S_IWUSR); 
	if(kp_fd < 0)
	{
		DBG_LOG_ERR("fail to open %s, for %s", pt_fpath, strerror(errno));
		tpret = false;
		goto EXIT;
	}
	if(write( kp_fd, pt_strbuf, strlen(pt_strbuf)) < 0)
	{
		DBG_LOG_ERR("file write fail, for %s", strerror(errno));
		tpret = false;
		goto EXIT;
	}
	
	EXIT:
		if(kp_fd >= 0)
		{ // to close the file opened
			close(kp_fd);
		}
		
		if(pt_strbuf)
		{
			l_free( pt_strbuf);
		}
		return tpret;
}



/* to be called to release the children list when the list is applied
*/
app_state_t app_cjson_release_chilren_list(gwConfig_t *pt_gwconf)
{
	app_state_t tpret = ST_OK;
#if(GWCONFIG_CHILDREN_USING_LINKED_LIST == 1)
	gwConfig_gwConfig_children_t *pt_child_ndtemp = NULL, *pt_cur_child_nd = NULL; 


	if(list_empty( &pt_gwconf->gwConfig.childrenList))
	{
		DBG_LOG_INFO(" the children list is empty , nothing to do");
		return tpret;
	}
	// to release the memory
	list_for_each_entry_safe( pt_cur_child_nd,  pt_child_ndtemp, &pt_gwconf->gwConfig.childrenList, childrenNode)
	{
		list_del( &pt_cur_child_nd->childrenNode);
		free(pt_cur_child_nd);
	}
	//
	pt_gwconf->gwConfig.childrenNumber = 0;
	INIT_LIST_HEAD( &pt_gwconf->gwConfig.childrenList);
#else	
	tpret = ST_OK;
#endif
	return tpret;
}




