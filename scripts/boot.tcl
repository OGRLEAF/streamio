
connect -url tcp:100.64.0.14:3122
puts stderr "INFO: Configuring the FPGA..."
puts stderr "INFO: Downloading bitstream to the target."
fpga "/group/bcapps/seemas/cr_verify/933245/Xilinx-ZCU102-2016.1/pre-built/linux/implementation/download.bit"
after 2000
targets -set 9
mwr 0xffff0000 0x14000000
mask_write 0xFD1A0104 0x501 0x0
targets -set -nocase -filter {name =~ "*A53*#0"}
source /group/bcapps/seemas/cr_verify/933245/Xilinx-ZCU102-2016.1/subsystems/linux/hw-description/psinit/psu_init.tcl
puts stderr "INFO: Downloading ELF file to the target."
dow "/group/bcapps/seemas/cr_verify/933245/Xilinx-ZCU102-2016.1/pre-built/linux/images/zynqmp_fsbl.elf"
after 2000
con
after 4000; stop; catch {stop}; psu_ps_pl_isolation_removal; psu_ps_pl_reset_config
targets -set -nocase -filter {name =~ "*A53*#0"}
dow -data "/group/bcapps/seemas/cr_verify/933245/Xilinx-ZCU102-2016.1/pre-built/linux/images/Image" 0x00080000
after 2000
targets -set -nocase -filter {name =~ "*A53*#0"}
dow -data "/group/bcapps/seemas/cr_verify/933245/Xilinx-ZCU102-2016.1/pre-built/linux/images/system.dtb" 0x04080000
after 2000
targets -set -nocase -filter {name =~ "*A53*#0"}
puts stderr "INFO: Downloading ELF file to the target."
dow "/group/bcapps/seemas/cr_verify/933245/Xilinx-ZCU102-2016.1/build/linux/misc/linux-boot/linux-boot.elf"
after 2000
targets -set -nocase -filter {name =~ "*A53*#0"}
puts stderr "INFO: Downloading ELF file to the target."
dow "/group/bcapps/seemas/cr_verify/933245/Xilinx-ZCU102-2016.1/images/linux/bl31.elf"
after 2000
con
exit
puts stderr "INFO: Saving XSDB commands to zcu105_tcl. You can run 'xsdb zcu105_tcl' to execute"
