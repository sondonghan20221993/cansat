# Template for table configuration

cfetables: staging/cpu1/cf/sample_app_tbl.tbl

staging/cpu1/cf/sample_app_tbl.tbl: CFE_TABLE_SCID      := 0x42
staging/cpu1/cf/sample_app_tbl.tbl: CFE_TABLE_PRID      := 1
staging/cpu1/cf/sample_app_tbl.tbl: CFE_TABLE_CPUNAME   := cpu1
staging/cpu1/cf/sample_app_tbl.tbl: CFE_TABLE_APPNAME   := sample_app
staging/cpu1/cf/sample_app_tbl.tbl: CFE_TABLE_BASENAME  := sample_app_tbl

# Rules to build staging/cpu1/cf/sample_app_tbl.tbl
elf/cpu1/sample_app_tbl.c.o: /home/sdh2983/cfs/cFS/build/aarch64-linux-gnu/default_cpu1/apps/sample_app/libtblobj_cpu1_sample_app.tbl4eb86b3e2b12d49e861764bf3c6608253ec41369.a
staging/cpu1/cf/sample_app_tbl.tbl: elf/cpu1/sample_app_tbl.c.o


