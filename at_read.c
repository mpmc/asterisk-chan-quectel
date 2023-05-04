/*
   Copyright (C) 2009 - 2010

   Artem Makhutov <artem@makhutov.org>
   http://www.makhutov.org

   Dmitry Vagin <dmitry2004@yandex.ru>

   Copyright (C) 2010 - 2011
   bg <bg_one@mail.ru>
*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE			/* vasprintf() in asterisk/utils.h */
#endif /* #ifndef _GNU_SOURCE */

#include "ast_config.h"

#include <sys/types.h>
#include <errno.h>

#include <asterisk/channel.h>		/* ast_waitfor_n_fd() */
#include <asterisk/logger.h>		/* ast_debug() */

#include "chan_quectel.h"
#include "at_read.h"
#include "ringbuffer.h"
#include "helpers.h"


/*!
 * \brief Wait for activity on an socket
 * \param fd -- file descriptor
 * \param ms  -- pointer to an int containing a timeout in ms
 * \return 0 on timeout and the socket fd (non-zero) otherwise
 * \retval 0 timeout
 */

int at_wait (int fd, int* ms)
{
	int exception, outfd;

	outfd = ast_waitfor_n_fd (&fd, 1, ms, &exception);

	if (outfd < 0)
	{
		outfd = 0;
	}

	return outfd;
}

#/* return number of bytes read */
ssize_t at_read (int fd, const char * dev, struct ringbuffer* rb)
{
	struct iovec	iov[2];
	int		iovcnt;
	ssize_t		n = -1;

	/* TODO: read until major error */
	iovcnt = rb_write_iov (rb, iov);

	if (iovcnt > 0)
	{
		n = readv (fd, iov, iovcnt);

		if (n < 0)
		{
			if (errno != EINTR && errno != EAGAIN)
			{
				ast_debug (1, "[%s] readv() error: %d\n", dev, errno);
				return n;
			}

			return 0;
		}
		else if (n > 0)
		{
			rb_write_upd (rb, n);

			ast_debug (6, "[%s] receive %zu byte, used %zu, free %zu, read %zu, write %zu\n",
				dev, n, rb_used (rb), rb_free (rb), rb->read, rb->write);

			iovcnt = rb_read_all_iov (rb, iov);

			if (iovcnt > 0 && DEBUG_ATLEAST(5)) {
				struct ast_str* const e0 = escape_nstr((const char*)iov[0].iov_base, iov[0].iov_len);
				if (iovcnt == 2) {
					struct ast_str* const e1 = escape_nstr((const char*)iov[1].iov_base, iov[1].iov_len);
					ast_debug(5, "[%s] [%d][%s%s]\n", dev, iovcnt, ast_str_buffer(e0), ast_str_buffer(e1));
					ast_free(e1);
				}
				else {
					ast_debug(5, "[%s] [%d][%s]\n", dev, iovcnt, ast_str_buffer(e0));
				}
				ast_free(e0);
			}
		}
	}
	else
		ast_log (LOG_ERROR, "[%s] at cmd receive buffer overflow\n", dev);
	return n;
}

size_t at_get_iov_size(const struct iovec* iov)
{
	return iov[0].iov_len + iov[1].iov_len;
}

size_t at_get_iov_size_n(const struct iovec* iov, int iovcnt)
{
	size_t res = 0u;
	for(int i=0; i<iovcnt; ++i) res += iov[i].iov_len;
	return res;
}

size_t prepare_result(struct ast_str* const result, const struct iovec* const iov, int iovcnt)
{
	const size_t len = at_get_iov_size_n(iov, iovcnt);

	ast_str_reset(result);
	if (iovcnt>0) {
		char* const buf = ast_str_buffer(result);		
		memcpy(buf, iov[0].iov_base, iov[0].iov_len);
		if (iovcnt>1) {
			memcpy(buf + iov[0].iov_len, iov[1].iov_base, iov[1].iov_len);
		}
		buf[len] = '\000';
		ast_str_update(result);	
	}

	return len;
}

int at_read_result_iov(
	const char* dev,
	int* read_result,
	size_t* skip,
	struct ringbuffer* rb,
	struct iovec *iov)
{
	static const char M_CSSI[] =		"+CSSI:";
	static const char M_CSSU[] =		"\r\n+CSSU:";
	static const char M_CMS_ERROR[] =	"\r\n+CMS ERROR:";
	static const char M_CMGS[] =		"\r\n+CMGS:";
	static const char M_CMGR[] =		"+CMGR:";
	static const char M_CNUM[] =		"+CNUM:";
	static const char M_ERROR_CNUM[] =	"ERROR+CNUM:";
	static const char M_CLCC[] =		"+CLCC:";
	static const char M_CMGL[] =		"+CMGL:";
	static const char M_EOL[] =			"\r\n";
	static const char M_SMS_PROMPT[] =	"> ";

	static const char T_OK[] =			"\r\n\r\nOK\r\n";
	static const char T_CMGL[] =		"\r\n+CMGL:";

	size_t s = rb_used(rb);

	if (s > 0) {
/*		ast_debug (5, "[%s] d_read_result %d len %d input [%.*s]\n", dev, *read_result, s, MIN(s, rb->size - rb->read), (char*)rb->buffer + rb->read); */

		if (*read_result == 0) {
			const int res = rb_memcmp(rb, M_EOL, STRLEN(M_EOL));
			if (res == 0) {
				rb_read_upd(rb, STRLEN(M_EOL));
				*read_result = 1;

				return at_read_result_iov(dev, read_result, skip, rb, iov);
			}
			else if (res > 0) {
				if (rb_memcmp(rb, "\n", 1) == 0) {
					rb_read_upd(rb, 1);

					return at_read_result_iov(dev, read_result, skip, rb, iov);
				}

				if (rb_read_until_char_iov(rb, iov, '\r') > 0) {
					s = at_get_iov_size(iov) + 1u;
				}

				rb_read_upd(rb, s);
				return at_read_result_iov(dev, read_result, skip, rb, iov);
			}

			return 0;
		}
		else {
			if (rb_memcmp (rb, M_CSSI, STRLEN(M_CSSI)) == 0) {
				const int iovcnt = rb_read_n_iov(rb, iov, STRLEN(M_CSSI));
				if (iovcnt) {
					*read_result = 0;
				}

				return iovcnt;
			}
			else if (rb_memcmp(rb, M_CSSU, STRLEN(M_CSSU)) == 0 || rb_memcmp(rb, M_CMS_ERROR, STRLEN(M_CMS_ERROR)) == 0 ||  rb_memcmp(rb, M_CMGS, STRLEN(M_CMGS)) == 0) {
				rb_read_upd(rb, 2);
				return at_read_result_iov(dev, read_result, skip, rb, iov);
			}
			else if (rb_memcmp(rb, M_SMS_PROMPT, STRLEN(M_SMS_PROMPT)) == 0) {
				*read_result = 0;
				return rb_read_n_iov(rb, iov, STRLEN(M_SMS_PROMPT));
			}
			else if (rb_memcmp(rb, M_CMGR, STRLEN(M_CMGR)) == 0 || rb_memcmp(rb, M_CNUM, STRLEN(M_CNUM)) == 0 || rb_memcmp(rb, M_ERROR_CNUM, STRLEN(M_ERROR_CNUM)) == 0 || rb_memcmp(rb, M_CLCC, STRLEN(M_CLCC)) == 0) {
				const int iovcnt = rb_read_until_mem_iov(rb, iov, T_OK, STRLEN(T_OK));
				if (iovcnt) {
					*skip = 4;
				}

				return iovcnt;
			}
			else if (rb_memcmp(rb, M_CMGL, STRLEN(M_CMGL)) == 0) {
				int iovcnt = rb_read_until_mem_iov(rb, iov, T_CMGL, STRLEN(T_CMGL));
				if (iovcnt) {
					*skip = 2;
				}
				else {
					iovcnt = rb_read_until_mem_iov(rb, iov, T_OK, STRLEN(T_OK));
					if (iovcnt) {
						*skip = 4;
					}
				}

				return iovcnt;
			}			
			else {
				const int iovcnt = rb_read_until_mem_iov(rb, iov, M_EOL, STRLEN(M_EOL));
				if (iovcnt) {
					*read_result = 0;
					s = at_get_iov_size_n(iov, iovcnt) + 1u;
					return rb_read_n_iov(rb, iov, s);
				}

				return iovcnt;
			}
		}
	}

	return 0;
}

at_res_t at_str2res(const struct ast_str* const result)
{
	at_res_t at_res = RES_UNKNOWN;
	const char* const buf = ast_str_buffer(result);

	for(unsigned idx = at_responses.ids_first; idx < at_responses.ids; ++idx) {
		if (!memcmp(buf,at_responses.responses[idx].id, at_responses.responses[idx].idlen)) {
			at_res = at_responses.responses[idx].res;
			break;
		}
	}

	return at_res;
}
