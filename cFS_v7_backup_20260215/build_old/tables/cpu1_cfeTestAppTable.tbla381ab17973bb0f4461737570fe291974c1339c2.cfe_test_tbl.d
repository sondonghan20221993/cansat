# Template for table configuration

cfetables: staging/cpu1/cf/cfe_test_tbl.tbl

staging/cpu1/cf/cfe_test_tbl.tbl: CFE_TABLE_SCID      := 0x42
staging/cpu1/cf/cfe_test_tbl.tbl: CFE_TABLE_PRID      := 1
staging/cpu1/cf/cfe_test_tbl.tbl: CFE_TABLE_CPUNAME   := cpu1
staging/cpu1/cf/cfe_test_tbl.tbl: CFE_TABLE_APPNAME   := cfeTestAppTable
staging/cpu1/cf/cfe_test_tbl.tbl: CFE_TABLE_BASENAME  := cfe_test_tbl

# Rules to build staging/cpu1/cf/cfe_test_tbl.tbl
elf/cpu1/cfe_test_tbl.c.o: /home/sdh2983/cfs/cFS/build/aarch64-linux-gnu/default_cpu1/apps/cfe_testcase/libtblobj_cpu1_cfeTestAppTable.tbla381ab17973bb0f4461737570fe291974c1339c2.a
staging/cpu1/cf/cfe_test_tbl.tbl: elf/cpu1/cfe_test_tbl.c.o


