# Template for table configuration

cfetables: staging/cpu1/cf/to_lab_sub.tbl

staging/cpu1/cf/to_lab_sub.tbl: CFE_TABLE_SCID      := 0x42
staging/cpu1/cf/to_lab_sub.tbl: CFE_TABLE_PRID      := 1
staging/cpu1/cf/to_lab_sub.tbl: CFE_TABLE_CPUNAME   := cpu1
staging/cpu1/cf/to_lab_sub.tbl: CFE_TABLE_APPNAME   := to_lab
staging/cpu1/cf/to_lab_sub.tbl: CFE_TABLE_BASENAME  := to_lab_sub

# Rules to build staging/cpu1/cf/to_lab_sub.tbl
elf/cpu1/to_lab_sub.c.o: /home/sdh2983/cfs/cFS/build/aarch64-linux-gnu/default_cpu1/apps/to_lab/libtblobj_cpu1_to_lab.tbl59e902b3737646cc7bd29d44da6b1ea7dd8d05d5.a
staging/cpu1/cf/to_lab_sub.tbl: elf/cpu1/to_lab_sub.c.o


