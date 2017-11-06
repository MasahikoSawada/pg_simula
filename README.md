pg_simula
==========

A database system failure simulation tool for PostgreSQL.

Usage
-----

1. Set up **simula_event** table

```
=# INSERT INTO simula_events VALUES ('INSERT', 'WAIT', 10);
INSERT 1 
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

Event Table
------------
|Column|Type|Description|
|:-----|:---|:----------|
|operation|text|A command tag of target operation|
|action|text|The action that you want to simulate: **ERROR**, **PANIC**, **WAIT**|
|sec|int|Wait time in second|
