echo "Transferring files to server..."
echo "$1->"
# rsync -Ptrs $1 root@100.64.0.16:/home/orgleaf/workspace/rootfs/home/root
# rsync -Ptrs $1 root@100.64.0.18:/srv/peta_rootfs/home/root
# sudo rsync -Ptrs $1 /home/orgleaf/works/xilinx/mount_root_home

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
source $SCRIPT_DIR/../envs.sh
scp -i $TARGET_DEVICE_SSH_KEY $1 root@$TARGET_DEVICE_IP:/home/root/kernel_modules