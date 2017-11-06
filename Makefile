# pg_simula/Makefile

MODULE_big = pg_simula
OBJS = pg_simula.o
DATA = pg_simula--1.0.sql

EXTENSION = pg_simula

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/pg_simula
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
