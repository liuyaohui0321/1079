connect -url tcp:127.0.0.1:3121
targets -set -filter {jtag_cable_name =~ "Digilent JTAG-SMT2 SULEE2306662" && level==0} -index 0
fpga -file C:/Users/liuyaohui/Desktop/0115_1x/nhc_amba_core_V3_hw_platform_0/nhc_amba_core0321a.bit
configparams mdm-detect-bscan-mask 2
targets -set -nocase -filter {name =~ "microblaze*#0" && bscan=="USER2"  && jtag_cable_name =~ "Digilent JTAG-SMT2 SULEE2306662"} -index 0
rst -system
after 3000
targets -set -nocase -filter {name =~ "microblaze*#0" && bscan=="USER2"  && jtag_cable_name =~ "Digilent JTAG-SMT2 SULEE2306662"} -index 0
dow C:/Users/liuyaohui/Desktop/0115_1x/dmauart/Debug/dmauart.elf
bpadd -addr &main
