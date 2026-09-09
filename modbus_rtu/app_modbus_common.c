/*
*/
#include "app_modbus_common.h"
#include "errno.h"
#include "util_dbg.h"
#include "app_mslave.h"

DBG_LOCAL_LOG_DEBUG


#if(1)

/* Sends a request/response
*/
int app_modbus_com_send_msg(modbus_t *ctx, uint8_t *msg, int msg_length)
{
    int rc;
    int i;

	if((NULL == ctx)||(NULL == msg)||(msg_length <= 0)||(msg_length > SZ_RTU_MSGBUF))
	{
		DBG_LOG_ERR("invalid parameter");
		return -1;
	}

    msg_length = ctx->backend->send_msg_pre(msg, msg_length);

    if (ctx->debug) {
        for (i = 0; i < msg_length; i++)
            printf("[%.2X]", msg[i]);
        	printf("\n");
    }

    /* In recovery mode, the write command will be issued until to be
       successful! Disabled by default. */
    do {
        rc = ctx->backend->send(ctx, msg, msg_length);
        if (rc == -1) {
            _error_print(ctx, NULL);
            if (ctx->error_recovery & MODBUS_ERROR_RECOVERY_LINK) 
			{
				#if(0)
				#ifdef _WIN32
	                const int wsa_err = WSAGetLastError();
	                if (wsa_err == WSAENETRESET || wsa_err == WSAENOTCONN ||
	                    wsa_err == WSAENOTSOCK || wsa_err == WSAESHUTDOWN ||
	                    wsa_err == WSAEHOSTUNREACH || wsa_err == WSAECONNABORTED ||
	                    wsa_err == WSAECONNRESET || wsa_err == WSAETIMEDOUT) {
	                    modbus_close(ctx);
	                    _sleep_response_timeout(ctx);
	                    modbus_connect(ctx);
	                } else {
	                    _sleep_response_timeout(ctx);
	                    modbus_flush(ctx);
	                }
				#else
	                int saved_errno = errno;

	                if ((errno == EBADF || errno == ECONNRESET || errno == EPIPE)) {
	                    modbus_close(ctx);
	                    _sleep_response_timeout(ctx);
	                    modbus_connect(ctx);
	                } else {
	                    _sleep_response_timeout(ctx);
	                    modbus_flush(ctx);
	                }
	                errno = saved_errno;
				#endif
				#else
					DBG_LOG_ERR("Todo============to complete error recovery");
				#endif
	        }
        }
    } while ((ctx->error_recovery & MODBUS_ERROR_RECOVERY_LINK) && rc == -1);

    if (rc > 0 && rc != msg_length) {
        errno = EMBBADDATA;
        return -1;
    }

    return rc;
}


#endif




