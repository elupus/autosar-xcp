XCP_PATH := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))

VPATH += $(XCP_PATH)
inc-y += $(XCP_PATH)

obj-y += Xcp_Cfg.o
obj-y += Xcp.o
obj-y += XcpOnCan.o
obj-y += Xcp_Program.o
obj-y += Xcp_Memory.o

