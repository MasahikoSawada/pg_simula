/* pg_simula */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_simula" to load this file. \quit

CREATE TABLE simula_events (
operation	TEXT primary key,
action		TEXT,
sec		int);

CREATE FUNCTION clear_all_events()
RETURNS bool
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT VOLATILE;

CREATE FUNCTION add_simula_event(text, text, int)
RETURNS bool
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT VOLATILE;
