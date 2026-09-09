#include <ell/ell.h>
#include "util_dbg.h"

DBG_LOCAL_LOG_DEBUG


#define SZ_ALLOC (0x100000)
#define SZ_MUNIT (0x10)
void main(void)
{
	uint8_t *pt_u8 = NULL;
	uint32_t idx = 0;
	while(1)
	{
		pt_u8 = (uint8_t*)l_malloc(SZ_ALLOC);
		if(NULL == pt_u8)
		{
			DBG_LOG_ERR("malloc fail");
			break;
		}
		DBG_LOG_INFO("idx = %d pointer sz = %d ,the memory pt_u8@%p, %lx SZ_ALLOC=0x%x", idx, sizeof(pt_u8), pt_u8, pt_u8, SZ_ALLOC);
		idx ++;
		memset( pt_u8, 0, SZ_ALLOC);
		for(uint32_t i = 0; i < SZ_ALLOC; i++)
		{
			pt_u8[i] = i;
		}
		//l_free( pt_u8); FK

		pt_u8 = (uint8_t*)l_malloc(SZ_MUNIT);
		if(NULL == pt_u8)
		{
			DBG_LOG_ERR("malloc fail");
			break;
		}
		DBG_LOG_INFO("idx = %d pointer sz = %d ,the memory pt_u8@%p, %lx SZ_ALLOC=0x%x", idx, sizeof(pt_u8), pt_u8, pt_u8, SZ_MUNIT);
		idx ++;
		memset( pt_u8, 0, SZ_MUNIT);
		sleep(1);
	}
}



