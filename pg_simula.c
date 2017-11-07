/* -------------------------------------------------------------------------
 *
 * pg_simula.c
 *
 * Simulation tool for database system failure
 *
 * -------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/transam.h"
#include "access/xact.h"
#include "executor/spi.h"
#include "commands/extension.h"
#include "fmgr.h"
#include "funcapi.h"
#include "lib/stringinfo.h"
#include "miscadmin.h"
#include "optimizer/planner.h"
#include "optimizer/prep.h"
#include "pgstat.h"
#include "replication/syncrep.h"
#include "storage/spin.h"
#include "tcop/utility.h"
#include "utils/builtins.h"
#include "utils/snapmgr.h"
#include "utils/ps_status.h"
#include "utils/memutils.h"

#define EVENT_TABLE_NAME	"simula_events"
#define MAX_LENGTH	100

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(clear_all_events);

void	_PG_init(void);
void	_PG_fini(void);

typedef struct SimualEvent
{
	char operation[MAX_LENGTH];
	char action[MAX_LENGTH];
	int	sec;
} SimulaEvent;
List *SimulaEvents = NIL;

PG_FUNCTION_INFO_V1(add_simula_event);

/* pg_simula hook functions */
static PlannedStmt *pg_simula_planner(Query *parse, int cursorOptions,
										 ParamListInfo boundParams);
static void pg_simula_ProcessUtility(PlannedStmt *pstmt,
									  const char *queryString,
									  ProcessUtilityContext context,
									  ParamListInfo params,
									  QueryEnvironment *queryEnv,
									  DestReceiver *dest,
									  char *completionTag);

static void reloadEventTableData(void);
static void doEventIfAny(const char *commandTag);
static bool isPgSimulaLoaded(void);

/* Simualtion functions */
static void error_func(int sec);
static void panic_func(int sec);
static void wait_func(int sec);

typedef void (*act_func) (int sec);
typedef struct Action
{
	char *action;
	act_func func;
} Action;

Action ActionTable[] =
{
	{"error", error_func},
	{"panic", panic_func},
	{"wait", wait_func},
	{NULL, NULL}
};

static planner_hook_type prev_planner = NULL;
static ProcessUtility_hook_type prev_ProcessUtility = NULL;

static bool in_simula_event_progress = false;
static bool registered_to_callback = false;

/* GUC parameter */
static bool enable_simulation = false;

void
_PG_init(void)
{
	DefineCustomBoolVariable("pg_simula.enabled",
							 "Enable simulation mode",
							 NULL,
							 &enable_simulation,
							 false,
							 PGC_USERSET,
							 0,
							 NULL,
							 NULL,
							 NULL);

	prev_planner = planner_hook;
	planner_hook = pg_simula_planner;
	prev_ProcessUtility = ProcessUtility_hook;
	ProcessUtility_hook = pg_simula_ProcessUtility;
}

/* Uninstall hook functions */
void _PG_fini(void)
{
	planner_hook = prev_planner;
	ProcessUtility_hook = prev_ProcessUtility;
}

static void
reloadEventTableData(void)
{
	StringInfoData buf;
	int	ret;
	int	ntup;
	int i;

	list_free_deep(SimulaEvents);
	SimulaEvents = NIL;

	if (!isPgSimulaLoaded())
		return;

	SetCurrentStatementStartTimestamp();
	SPI_connect();
	PushActiveSnapshot(GetTransactionSnapshot());

	initStringInfo(&buf);
	appendStringInfo(&buf, "SELECT * FROM %s", EVENT_TABLE_NAME);

	ret = SPI_execute(buf.data, true, 0);
	if (ret != SPI_OK_SELECT)
		elog(ERROR, "SPI_execute falied: error code %d", ret);

	ntup = SPI_processed;

	if (ret > 0 && SPI_tuptable != NULL)
    {
        TupleDesc tupdesc = SPI_tuptable->tupdesc;
        SPITupleTable *tuptable = SPI_tuptable;

		for (i = 0; i < ntup; i++)
		{
            HeapTuple tuple = tuptable->vals[i];
			SimulaEvent *event;
			MemoryContext old;
			char *operation = SPI_getvalue(tuple, tupdesc, 1);
			char *action = SPI_getvalue(tuple, tupdesc, 2);
			char *sec = SPI_getvalue(tuple, tupdesc, 3);

			old = MemoryContextSwitchTo(TopMemoryContext);
			event = palloc(sizeof(SimulaEvent));
			strncpy(event->operation, operation, MAX_LENGTH);
			strncpy(event->action, action, MAX_LENGTH);
			event->sec = atoi(sec);
			SimulaEvents = lappend(SimulaEvents, event);
			MemoryContextSwitchTo(old);
		}
	}

	SPI_finish();
	PopActiveSnapshot();
}

Datum
add_simula_event(PG_FUNCTION_ARGS)
{
	text	*operation = PG_GETARG_TEXT_P(0);
	text	*action = PG_GETARG_TEXT_P(1);
	int		sec = PG_GETARG_INT32(2);
	char	*ope_str = text_to_cstring(operation);
	char	*act_str = text_to_cstring(action);
	StringInfoData	buf;
	int		ret;

	in_simula_event_progress = true;

	initStringInfo(&buf);
	appendStringInfo(&buf,
					 "INSERT INTO %s VALUES('%s', '%s', %d) ON CONFLICT "
					 "ON CONSTRAINT simula_events_pkey "
					 "DO UPDATE SET (action, sec) = (excluded.action, excluded.sec)",
					 EVENT_TABLE_NAME,
					 ope_str, act_str, sec);

	SetCurrentStatementStartTimestamp();
	SPI_connect();
	PushActiveSnapshot(GetTransactionSnapshot());
	ret = SPI_execute(buf.data, false, 0);
	SPI_finish();
	PopActiveSnapshot();

	in_simula_event_progress = false;

	PG_RETURN_BOOL(ret);
}

/* Clear all simulation events */
Datum
clear_all_events(PG_FUNCTION_ARGS)
{
	int ret;
	StringInfoData buf;

	in_simula_event_progress = true;
	SetCurrentStatementStartTimestamp();
	SPI_connect();
	PushActiveSnapshot(GetTransactionSnapshot());

	initStringInfo(&buf);
	appendStringInfo(&buf, "TRUNCATE %s", EVENT_TABLE_NAME);

	ret = SPI_execute(buf.data, false, 0);

	SPI_finish();
	PopActiveSnapshot();

	in_simula_event_progress = false;

    PG_RETURN_BOOL(ret);
}

/* Check if pg_simula is already loaded (created) on the database */
static bool
isPgSimulaLoaded(void)
{
	return OidIsValid(get_extension_oid("pg_simula", true));
}

static void
pgsimula_xact_callback(XactEvent event, void *arg)
{
	in_simula_event_progress = false;
}

/*
 * Detect SQL command other than utility commands.
 */
static PlannedStmt *
pg_simula_planner(Query *parse, int cursorOptions, ParamListInfo boundParams)
{
	const char	*commandTag;

	commandTag = CreateCommandTag((Node *) &parse->type);

	/* Register callback function if not yet */
	if (!registered_to_callback)
	{
		RegisterXactCallback(pgsimula_xact_callback, NULL);
		registered_to_callback = true;
	}

	if (enable_simulation &&
		!in_simula_event_progress &&
		IsTransactionState())
	{
		/* in_simulat_event_progress is turned off at end of the transaction */
		in_simula_event_progress = true;
		reloadEventTableData();
		doEventIfAny(commandTag);
	}

	if (prev_planner)
        return (*prev_planner) (parse, cursorOptions, boundParams);
    else
		return standard_planner(parse, cursorOptions, boundParams);
}

static void
pg_simula_ProcessUtility(PlannedStmt *pstmt,
						 const char *queryString,
						 ProcessUtilityContext context,
						 ParamListInfo params,
						 QueryEnvironment *queryEnv,
						 DestReceiver *dest,
						 char *completionTag)
{
	Node	*parsetree = pstmt->utilityStmt;
	const char *commandTag;

	commandTag = CreateCommandTag(parsetree);

	/* Register callback function if not yet */
	if (!registered_to_callback)
	{
		RegisterXactCallback(pgsimula_xact_callback, NULL);
		registered_to_callback = true;
	}

	if (enable_simulation &&
		!in_simula_event_progress &&
		IsTransactionState())
	{
		/* in_simulat_event_progress is turned off at end of the transaction */
		in_simula_event_progress = true;
		reloadEventTableData();
		doEventIfAny(commandTag);
	}

    /* Call the standard process utility chain. */
    if (prev_ProcessUtility)
        (*prev_ProcessUtility) (pstmt, queryString, context, params,
                                     queryEnv, dest, completionTag);
    else
        standard_ProcessUtility(pstmt, queryString, context, params,
                                queryEnv, dest, completionTag);
}

static void
doEventIfAny(const char *commandTag)
{
	ListCell	*cell;

	foreach(cell, SimulaEvents)
	{
		SimulaEvent *event = lfirst(cell);

		if (pg_strcasecmp(event->operation, commandTag) == 0)
		{
			Action *act = ActionTable;

			for (act = ActionTable; act != NULL; act++)
			{
				if (pg_strcasecmp(act->action, event->action) == 0)
				{
					/* expected to be at most one action per command */
					act->func(event->sec);
					return;
				}
			}
		}
	}
}

static void
error_func(int sec)
{
	ereport(ERROR, (errmsg("simulation of ERROR by pg_simula")));
}

static void
panic_func(int sec)
{
	ereport(PANIC, (errmsg("simulation of PANIC by pg_simula")));
}

static void
wait_func(int sec)
{
	pg_usleep(sec * 1000 * 1000);
}
