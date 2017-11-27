pg_simula
==========

A database system failure simulation tool for PostgreSQL.

Usage
-----

1. Set up **simula_event** table

```
=# SELECT add_simula_event('INSERT', 'WAIT', 10);
INSERT 1
-- Simulate that a insertion takes at least 10 sec for whatever reason.
=# SELECT add_simula_evnet('TRUNCATE TABLE', 'ERROR', 0);
-- Simulate that a truncation of table failed for whatever reason.
```

2. Do operations
```
=# SET pg_simula.enable TO on;
SET
=# CREATE TABLE a (c int);
CREATE TABLE
=# INSERT INTO a VALUES (1);
-- wait for 10 sec
INSERT
=# TRUNCATE a;
ERROR:  simulation of ERROR by pg_simula
STATEMENT:  TRUNCATE a;
```

Simulation events management
--------------------------------
You can manage the simulation events by using the following management functions.

* clear_all_events()
  * Clear all simulation events.
* add_simula_event(text operation, text action, sec int)
  * Add a simulation event.

Note that you can also manage the simulation events by modifing **simula_events** table directory but it's possible that a simulation action is executed as unexpected due to  recursively execution of failure action.

GUC parameter
--------------
* pg_simula.enable (false by default)
  * Enable the functionality of pg_simula.
* pg_simula.connection_refuse (false by default)
  * Refuse all all new connections. The returned error code is `ERRCODE_INVALID_AUTHORIZATION_SPECIFICATION`.

Simulation Event Table
------------
|Column|Type|Description|
|:-----|:---|:----------|
|operation|text|A command tag of target operation|
|action|text|The action that you want to simulate: **ERROR**, **FATAL**, **PANIC** and **WAIT**|
|sec|int|Wait time in second (used only if the type of action is **WAIT**)|

Installation
-------------
Since pg_simula is an extension module for PostgreSQL, it can be installed to your system by the same way as other contribution module.

```bash
# Build and installation
git clone https://github.com/MasahikoSawada/pg_simula.git
cd pg_simula
make USE_PGXS=1 PG_CONFIG=/path/to/pg_config
su
make USE_PGXS=1 PG_CONFIG=/path/to/pg_config insta
# Configuration
vi $PGDATA/postgresql.conf
shared_preload_libraries = 'pg_simula'

# Registeration
psql -d postgres
=# CREATE EXTENSION pg_simula;
CREATE EXTENSION
=# TABLE simula_events;
 operation | action | sec
-----------+--------+-----
(0 rows)
```

Tested platform
---------------
pg_simula has been built and tested on the following platforms(*):

|Category|Module Name|
|:-------|:----------|
|OS|Linux|
|DBMS|PostgreSQL 10|

(*) Tested only PostgreSQL 10. If you have tested on other platforms please let me know your result on Issues :-)

Note
-----
pg_simula uses two hooks: planner_hook and ProcessUtility_hook, in order to do the particular action. So each action is executed either at planning in case of DML or at execution in case of DDL and other utility commands.
