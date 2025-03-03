#!/usr/bin/python3

import argparse
import os

FIRMWARE_PATH="/lib/firmware/xilinx"

DRV_PROBE_PATH = []

FPGA_COMMAND = "fpgautil"
"""
fpgautil -h

fpgautil: FPGA Utility for Loading/reading PL Configuration in zynqMP

Usage:  fpgautil -b <bin file path> -o <dtbo file path>

Options: -b <binfile>           (Bin file path)
         -o <dtbofile>          (DTBO file path)
         -f <flags>             Optional: <Bitstream type flags>
                                   f := <Full | Partial >
                                Default: <Full>
          -s <secure flags>     Optional: <Secure flags>
                                   s := <AuthDDR | AuthOCM | EnUsrKey | EnDevKey | AuthEnUsrKeyDDR | AuthEnUsrK>
          -k <AesKey>           Optional: <AES User Key>
          -r <Readback>         Optional: <file name>
                                Default: By default Read back contents will be stored in readback.bin file
          -t                    Optional: <Readback Type>
                                   0 - Configuration Register readback
                                   1 - Configuration Data Frames readback
                                Default: 0 (Configuration register readback)
          -R                    Optional: Remove overlay from a live tree

Examples:
(Load Full Bitstream using Overlay)
fpgautil -b top.bin -o can.dtbo
(Load Partial Bitstream through the sysfs interface)
fpgautil -b top.bin -f Partial
(Load Authenticated Bitstream through the sysfs interface)
fpgautil -b top.bin -f Full -s AuthDDR
(Load Parital Encrypted Userkey Bitstream using Overlay)
fpgautil -b top.bin -o pl.dtbo -f Partial -s EnUsrKey -k <32byte key value>
(Read PL Configuration Registers)
fpgautil -b top.bin -r
"""

STREAMIO_MODULES = ["streamio-drv", "streamio-ctrl-drv"]

try:
    firmware_choices = os.listdir(FIRMWARE_PATH)
except FileNotFoundError as e:
    print(f"! No firmware found under {FIRMWARE_PATH}")
    firmware_choices = ['streamio', 'adrv9001']

parser = argparse.ArgumentParser(
    prog="StreamIO",
    description="StreamIO driver and PL management",
)

parser.add_argument('-b', '--bitstream')
parser.add_argument('-o', '--dtbo')
parser.add_argument('-m', '--module')

parser.add_argument('--firmware', help=f"firmware (combined with PL and driver), default 'streamio', {len(firmware_choices)} available: {', '.join(firmware_choices)}", default='streamio')

parser.add_argument('--drv-probe', action='store_true', default=True)
parser.add_argument('--skip-pl', action='store_true', default=False)
parser.add_argument('--skip-drv', action='store_true', default=False)

parser.add_argument('--list-firmware', help="list all firmware build in this image", action="store_true", default=False)

parser.add_argument('--dry-run', action='store_true', default=False)

if __name__ == '__main__':
    commands_to_exec = []

    args = parser.parse_args()

    if args.list_firmware:
        print("All available firmware:")
        for firmware_name in firmware_choices:
            print(f" \t {firmware_name}: {FIRMWARE_PATH}/{firmware_name}")
        exit()

    if not args.skip_pl:
        fpgautil_args = []
        fpga_bin_path = None
        dtbo_path = None
        if args.firmware:
            select_firmware_name = args.firmware
            fpga_bin_path = f"{FIRMWARE_PATH}/{select_firmware_name}/{select_firmware_name}.bin"
            dtbo_path = f"{FIRMWARE_PATH}/{select_firmware_name}/{select_firmware_name}.dtbo"
        if args.bitstream:
            fpga_bin_path = args.bitstream
        if args.dtbo:
            dtbo_path = args.dtbo

        if fpga_bin_path:
            fpgautil_args += ["-b", fpga_bin_path]
        if dtbo_path:
            fpgautil_args += ["-o", dtbo_path]
            commands_to_exec.append([FPGA_COMMAND, "-R"])
        commands_to_exec.append([FPGA_COMMAND, *fpgautil_args])


    if args.drv_probe:
        for mod in STREAMIO_MODULES:
            commands_to_exec.insert(0, ["rmmod", mod])
            commands_to_exec.append(["modprobe", mod])

    for command in commands_to_exec:
        exec_command = ' '.join(command)
        print("# " + exec_command, end="")
        if not args.dry_run:
            print(f" > {os.system(exec_command)}")
        else:
            print("")

