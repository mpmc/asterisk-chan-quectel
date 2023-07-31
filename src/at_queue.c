/*
   Copyright (C) 2009 - 2010

   Artem Makhutov <artem@makhutov.org>
   http://www.makhutov.org

   Dmitry Vagin <dmitry2004@yandex.ru>

   Copyright (C) 2010 - 2011
   bg <bg_one@mail.ru>
*/
#include "ast_config.h"

#include <asterisk/utils.h>		/* ast_free() */

#include "at_queue.h"
#include "chan_quectel.h"		/* struct pvt */
#include "helpers.h"

/*!
 * \brief Free an item data
 * \param cmd - struct at_queue_cmd
 */
#/* */
static void at_queue_free_data(at_queue_cmd_t* const cmd)
{
	if (cmd->data) {
		if ((cmd->flags & ATQ_CMD_FLAG_STATIC) == 0) {
			ast_free (cmd->data);
			cmd->data = NULL;
		}
		/* right because work with copy of static data */
	}
	cmd->length = 0;
}

/*!
 * \brief Free an item
 * \param e -- struct at_queue_task structure
 */
#/* */
static void at_queue_free(at_queue_task_t* const task)
{
	for(unsigned i = 0; i < task->cmdsno; ++i) {
		at_queue_free_data(&task->cmds[i]);
	}
	ast_free(task);
}


/*!
 * \brief Remove an job item from the front of the queue, and free it
 * \param pvt -- pvt structure
 */
#/* */
static void at_queue_remove(struct pvt* const pvt)
{
	// U+2327 : X In A Rectangle Box : 0xE2 0x8C 0xA7
	at_queue_task_t* const task = AST_LIST_REMOVE_HEAD(&pvt->at_queue, entry);

	if (!task) {
		return;
	}

	PVT_STATE(pvt, at_tasks)--;
	PVT_STATE(pvt, at_cmds) -= task->cmdsno - task->cindex;

	if (task->cindex >= task->cmdsno) {
		ast_debug(4, "[%s][%s] \xE2\x8C\xA7 cmds:%u response:[%s]\n",
			PVT_ID(pvt), at_cmd2str(task->cmds[0].cmd), task->cmdsno,
			at_res2str(task->cmds[0].res));
	}
	else {
		ast_debug(3, "[%s][%s] \xE2\x8C\xA7 cmds:%u/%u response:[%s]\n",
			PVT_ID(pvt), at_cmd2str(task->cmds[0].cmd), task->cindex, task->cmdsno,
			at_res2str(task->cmds[0].res));
	}

	at_queue_free(task);
}

/*!
 * \brief Add an list of commands (task) to the back of the queue
 * \param cpvt -- cpvt structure
 * \param cmds -- the commands that was sent to generate the response
 * \param cmdsno -- number of commands
 * \param prio -- priority 0 mean put at tail
 * \param at_once -- execute commands at once
 * \return task on success, NULL on error
 */
#/* */
at_queue_task_t* at_queue_add(struct cpvt * cpvt, const at_queue_cmd_t * cmds, unsigned cmdsno, int prio, unsigned at_once)
{
	// U+229E : Squared Plus : 0xE2 0x8A 0x9E
	at_queue_task_t* e = NULL;
	if (cmdsno > 0) {
		e = ast_malloc(sizeof(*e) + cmdsno * sizeof(*cmds));
		if(e) {
			pvt_t* const pvt = cpvt->pvt;
			at_queue_task_t * first;

			e->entry.next = 0;
			e->cmdsno = cmdsno;
			e->cindex = 0;
			e->cpvt = cpvt;
			e->at_once = at_once;

			memcpy(&e->cmds[0], cmds, cmdsno * sizeof(*cmds));

			if(prio && (first = AST_LIST_FIRST (&pvt->at_queue)))
				AST_LIST_INSERT_AFTER (&pvt->at_queue, first, e, entry);
			else
				AST_LIST_INSERT_TAIL (&pvt->at_queue, e, entry);

			PVT_STATE(pvt, at_tasks) ++;
			PVT_STATE(pvt, at_cmds) += cmdsno;

			PVT_STAT(pvt, at_tasks) ++;
			PVT_STAT(pvt, at_cmds) += cmdsno;

			if (e->cmdsno <= 1u) {
				ast_debug(4, "[%s][%s] \xE2\x8A\x9E response:[%s] position:[%s]\n",
						PVT_ID(pvt), at_cmd2str(e->cmds[0].cmd),
						at_res2str(e->cmds[0].res), prio ? "after head" : "at tail");
			}
			else {
				ast_debug(4, "[%s][%s] \xE2\x8A\x9E cmds:%u response:[%s] position:[%s]\n",
						PVT_ID(pvt), at_cmd2str(e->cmds[0].cmd), e->cmdsno,
						at_res2str(e->cmds[0].res), prio ? "after head" : "at tail");
			}
		}
	}
	return e;
}


/*!
 * \brief Write to fd
 * \param fd -- file descriptor
 * \param buf -- buffer to write
 * \param count -- number of bytes to write
 *
 * This function will write count characters from buf. It will always write
 * count chars unless it encounters an error.
 *
 * \retval number of bytes wrote
 */

#/* */
size_t write_all(int fd, const char* buf, size_t count)
{
	ssize_t out_count;
	size_t total = 0;
	unsigned errs = 10;

	while (count > 0) {
		out_count = write(fd, buf, count);
		if (out_count <= 0) {
			if(errno == EINTR || errno == EAGAIN) {
				errs--;
				if(errs != 0)
					continue;
			}
			break;
		}
		errs = 10;
		count -= out_count;
		buf += out_count;
		total += out_count;
	}
	return total;
}

/*!
 * \brief Write to fd
 * \param pvt -- pvt structure
 * \param buf -- buffer to write
 * \param count -- number of bytes to write
 *
 * This function will write count characters from buf. It will always write
 * count chars unless it encounters an error.
 *
 * \retval !0 on error
 * \retval  0 success
 */

#/* */
int at_write(struct pvt* pvt, const char* buf, size_t count)
{
	if (DEBUG_ATLEAST(5)) {
		struct ast_str* const ebuf = escape_nstr(buf, count);
		ast_debug(5, "[%s] [%s]\n", PVT_ID(pvt), ast_str_buffer(ebuf));
		ast_free(ebuf);
	}

	const size_t wrote = write_all(pvt->data_fd, buf, count);
	PVT_STAT(pvt, d_write_bytes) += wrote;
	if(wrote != count) {
		ast_debug(1, "[%s][DATA] Write: %s\n", PVT_ID(pvt), strerror(errno));
	}

	return wrote != count;
}

/*!
 * \brief Remove an cmd item from the front of the queue
 * \param pvt -- pvt structure
 */
#/* */
static void at_queue_remove_cmd(struct pvt* pvt, at_res_t res)
{
	at_queue_task_t* const task = AST_LIST_FIRST(&pvt->at_queue);
	if (!task) return;

	if (task->at_once) {
		task->cindex = task->cmdsno;
		PVT_STATE(pvt, at_cmds) -= task->cmdsno;
		if (task->cmds[0].res == res || (task->cmds[0].flags & ATQ_CMD_FLAG_IGNORE) != 0) {
			at_queue_remove(pvt);
		}
	}
	else {
		// U+229F : Squared Minus : 0xE2 0x8A 0x9F
		const unsigned index = task->cindex;

		task->cindex++;
		PVT_STATE(pvt, at_cmds)--;
		if (task->cmds[index].res == res) {
			ast_debug(6, "[%s][%s] \xE2\x8A\x9F result:[%s] cmd:%u/%u flags:%02x\n",
					PVT_ID(pvt), at_cmd2str(task->cmds[index].cmd),
					at_res2str(res),
					task->cindex, task->cmdsno, task->cmds[index].flags);
		}
		else {
			ast_debug(5, "[%s][%s] \xE2\x8A\x9F result:[%s/%s] cmd:%u/%u flags:%02x\n",
					PVT_ID(pvt), at_cmd2str(task->cmds[index].cmd),
					at_res2str(task->cmds[index].res), at_res2str(res),
					task->cindex, task->cmdsno, task->cmds[index].flags);
		}

		if ((task->cindex >= task->cmdsno) || (task->cmds[index].res != res && (task->cmds[index].flags & ATQ_CMD_FLAG_IGNORE) == 0)) {
			at_queue_remove(pvt);
		}
	}
}

static void at_queue_remove_task_at_once(struct pvt* const pvt)
{
	at_queue_task_t* const task = AST_LIST_FIRST(&pvt->at_queue);

	if (task && task->at_once) {
		task->cindex = task->cmdsno;
		PVT_STATE(pvt, at_cmds) -= task->cmdsno;
		if ((task->cmds[0].flags & ATQ_CMD_FLAG_IGNORE) == 0) {
			at_queue_remove(pvt);
		}
	}	
}

static size_t at_queue_get_total_cmd_len(const at_queue_task_t* const task)
{
	size_t res = 0;

	for(unsigned i=0; i<task->cmdsno; ++i) {
		res += task->cmds[i].length;
	}

	return res;
}

/*!
 * \brief Try real write first command on queue
 * \param pvt -- pvt structure
 * \return 0 on success, non-0 on error
 */
#/* */
int at_queue_run(struct pvt* pvt)
{
	int fail = 0;
	at_queue_task_t* const t = AST_LIST_FIRST(&pvt->at_queue);
	if (!t) return fail;

	if (t->at_once) {
		size_t buflen = at_queue_get_total_cmd_len(t);
		if (buflen == 0u) return fail; // do not send again

		buflen += 2u; // AT + (semicolon separated commands)
		buflen += t->cmdsno; // number of semicolons

		struct ast_str* buf = ast_str_create( buflen );
		ast_str_set_substr(&buf, buflen, "AT", 2);
		for(unsigned i=0; i<t->cmdsno; ++i) {
			ast_str_append_substr(&buf, buflen, t->cmds[i].data, t->cmds[i].length);
			ast_str_append_substr(&buf, buflen, 
				(i < (t->cmdsno - 1u)) ? ";" : "\r", 1);
		}

		if (DEBUG_ATLEAST(2)) {
			// U+2192 : Rightwards arrow : 0xE2 0x86 0x92
			struct ast_str* const ebuf = escape_str(buf);
			ast_debug(2, "[%s][%s] \xE2\x86\x92 [%s]\n", PVT_ID(pvt), at_cmd2str(t->cmds[0].cmd), ast_str_buffer(ebuf));
			ast_free(ebuf);
		}

		fail = at_write(pvt, ast_str_buffer(buf), ast_str_strlen(buf));
		if (fail) {
			// U+2947 : Rightwards Arrow Through X : 0xE2 0xA5 0x87 
			struct ast_str* const ebuf = escape_str(buf);
			ast_log(LOG_WARNING, "[%s][%s] \xE2\xA5\x87 [%s]\n", PVT_ID(pvt), at_cmd2str(t->cmds[0].cmd), ast_str_buffer(ebuf));
			ast_free(ebuf);

			at_queue_remove_task_at_once(pvt);
		}
		else {
			for(unsigned i=0; i<t->cmdsno; ++i) {
				at_queue_free_data(&t->cmds[i]);
			}
			at_queue_cmd_t* const cmd = &(t->cmds[0]);
			cmd->timeout = ast_tvadd(ast_tvnow(), cmd->timeout);
		}
		ast_free(buf);
	}
	else {
		at_queue_cmd_t* const cmd = &(t->cmds[t->cindex]);
		if (cmd->length == 0u) return fail;

		if (DEBUG_ATLEAST(2)) {
			struct ast_str* const ebuf = escape_nstr(cmd->data, cmd->length);
			ast_debug(2, "[%s][%s] \xE2\x86\x92 [%s]\n", PVT_ID(pvt), at_cmd2str(cmd->cmd), ast_str_buffer(ebuf));
			ast_free(ebuf);
		}

		fail = at_write(pvt, cmd->data, cmd->length);
		if(fail) {
			struct ast_str* const ebuf = escape_nstr(cmd->data, cmd->length);
			ast_log(LOG_ERROR, "[%s][%s] \xE2\xA5\x87 [%s]\n", PVT_ID(pvt), at_cmd2str(cmd->cmd), ast_str_buffer(ebuf));
			ast_free(ebuf);

			at_queue_remove_cmd(pvt, cmd->res + 1);
		}
		else {
			/* set expire time */
			cmd->timeout = ast_tvadd(ast_tvnow(), cmd->timeout);

			/* free data and mark as written */
			at_queue_free_data(cmd);
		}
	}

	return fail;
}

int at_queue_run_immediately(struct pvt* pvt)
{
	int fail = 0;
	size_t buflen = 0;
	size_t cmdsno = 0;
	size_t pos = 1u;
	at_queue_task_t* task;

	AST_LIST_TRAVERSE(&pvt->at_queue, task, entry) {
		if (!task->at_once) continue;
		buflen += at_queue_get_total_cmd_len( task );
		cmdsno += task->cmdsno;
	}

	if (!cmdsno) return fail;

	buflen += 2u;
	buflen += cmdsno;
	struct ast_str* buf = ast_str_create( buflen );
	ast_str_set_substr(&buf, buflen, "AT", 2);

	AST_LIST_TRAVERSE(&pvt->at_queue, task, entry) {
		if (!task->at_once) continue;
		for(unsigned i=0; i<task->cmdsno; ++i) {
			if (task->cmds[i].length) {
				ast_str_append_substr(&buf, buflen, task->cmds[i].data, task->cmds[i].length);
				ast_str_append_substr(&buf, buflen, (pos < cmdsno) ? ";" : "\r", 1);
			}
			pos += 1u;
		}
	}

	if (DEBUG_ATLEAST(2)) {
		// U+21D2 : Rightwards Double Arrow : 0xE2 0x87 0x92
		struct ast_str* const ebuf = escape_str(buf);
		ast_debug(2, "[%s] \xE2\x87\x92 [%s]\n", PVT_ID(pvt), ast_str_buffer(ebuf));
		ast_free(ebuf);
	}

	fail = at_write(pvt, ast_str_buffer(buf), ast_str_strlen(buf));
	if (fail) {
		// U+21CF : Rightwards Double Arrow with Stroke : 0xE2 0x87 0x8F
		struct ast_str* const ebuf = escape_str(buf);
		ast_log(LOG_WARNING, "[%s] \xE2\x87\x8F [%s]\n", PVT_ID(pvt), ast_str_buffer(ebuf));
		ast_free(ebuf);
	}

	ast_free(buf);
	return fail;
}

/*!
 * \brief Write commands with queue
 * \param pvt -- pvt structure
 * \return 0 on success non-0 on error
 */
#/* */
int at_queue_insert_const (struct cpvt * cpvt, const at_queue_cmd_t * cmds, unsigned cmdsno, int athead)
{
	return at_queue_add(cpvt, cmds, cmdsno, athead, 0u) == NULL || at_queue_run(cpvt->pvt);
}

int at_queue_insert_const_at_once(struct cpvt* cpvt, const at_queue_cmd_t* cmds, unsigned cmdsno, int athead)
{
	return at_queue_add(cpvt, cmds, cmdsno, athead, 1u) == NULL || at_queue_run(cpvt->pvt);
}

#/* */
int at_queue_insert_uid(struct cpvt * cpvt, at_queue_cmd_t * cmds, unsigned cmdsno, int athead, int uid)
{
	unsigned idx;
	at_queue_task_t *task = at_queue_add(cpvt, cmds, cmdsno, athead, 0u);
	task->uid = uid;

	if(!task)
	{
		for(idx = 0; idx < cmdsno; idx++)
		{
			at_queue_free_data(&cmds[idx]);
		}
	}

	if (at_queue_run(cpvt->pvt))
		task = NULL;

	return task == NULL;
}

#/* */
int at_queue_insert(struct cpvt * cpvt, at_queue_cmd_t * cmds, unsigned cmdsno, int athead)
{
	return at_queue_insert_uid(cpvt, cmds, cmdsno, athead, 0);
}



#/* */
void at_queue_handle_result (struct pvt* pvt, at_res_t res)
{
	/* move queue */
	at_queue_remove_cmd(pvt, res);
}

/*!
 * \brief Remove all itmes from the queue and free them
 * \param pvt -- pvt structure
 */

#/* */
void at_queue_flush (struct pvt* pvt)
{
	struct at_queue_task* task;

	while ((task = AST_LIST_FIRST (&pvt->at_queue)))
	{
		at_queue_remove(pvt);
	}
}

/*!
 * \brief Get the first task on queue
 * \param pvt -- pvt structure
 * \return a pointer to the first command of the given queue
 */
#/* */
const struct at_queue_task* at_queue_head_task (const struct pvt * pvt)
{
	return AST_LIST_FIRST (&pvt->at_queue);
}


/*!
 * \brief Get the first command of a queue
 * \param pvt -- pvt structure
 * \return a pointer to the first command of the given queue
 */
#/* */
const at_queue_cmd_t * at_queue_head_cmd(const struct pvt * pvt)
{
	return at_queue_task_cmd(at_queue_head_task(pvt));
}

#/* */
int at_queue_timeout(const struct pvt * pvt)
{
	int ms_timeout = -1;
	const at_queue_cmd_t * cmd = at_queue_head_cmd(pvt);

	if(cmd)
	{
		if(cmd->length == 0)
		{
			ms_timeout = ast_tvdiff_ms(cmd->timeout, ast_tvnow());
		}
	}

	return ms_timeout;
}
