pg_simula
==========

A database system failure simulation tool for PostgreSQL.

Usage
-----

1. Set up **simula_event** table

```sql
=# INSERT INTO simula_events VALUES ('INSERT', 'WAIT', '10');
INSERT 1 
```

2. Do operations
```sql
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

Event Table
------------
|Column|Description|
|:-----|:----------|
|operation|A command tag of target operation|
|action|The action that you want to simulate: **ERROR**, **PANIC**, **WAIT**|
|sec|Wait time in second|
