pg_simula
==========

A database system failure simulation tool for PostgreSQL.

Usage
-----

1. Set up **simula_event** table

```
=# INSERT INTO simula_events VALUES ('INSERT', 'WAIT', 10);
INSERT 1 
-- Simulate that a insertion takes at least 10 sec for whatever reason.
```

2. Do operations
```
=# BEGIN;
BEGIN
=# SET pg_simula.enable TO on;
SET
=# CREATE TABLE a (c int);
CREATE TABLE
=# INSERT INTO a VALUES (1);
-- wait for 10 sec
INSERT
=# COMMIT;
COMMIT
```

Simulation events management
--------------------------------
You can manage the simulation events by either updating **simula_events** table (see below) or using the following management functions.

* clear_all_events()
  * Clear all simulation events. (same as `TRUNCATE simula_events;`)
* add_simula_event(text operation, text action, sec int)
  * Add a simulation event.

GUC parameter
--------------
* pg_simula.enable (false by default)
 * Enable the functionality of pg_simula.

Simulation Event Table
------------
|Column|Type|Description|
|:-----|:---|:----------|
|operation|text|A command tag of target operation|
|action|text|The action that you want to simulate: **ERROR**, **PANIC**, **WAIT**|
|sec|int|Wait time in second|

Installation
-------------
Since pg_simula is an extension module for PostgreSQL, it can be installed to your system by the same way as other contribution module.

```bash
# Build and installation
git clone https://github.com/MasahikoSawada/pg_simula.git
cd pg_simula
make USE_PGXS=1 PG_CONFIG=/path/to/pg_config
su
make USE_PGXS=1 PG_CONFIG=/path/to/pg_config install

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

Note
-----
pg_simula uses two hooks: planner_hook and ProcessUtility_hook, in order to do the particular action. So each action is executed either at planning in case of DML or at execution in case of DDL and other utility commands.
