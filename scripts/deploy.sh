SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
# source $SCRIPT_DIR/envs.sh

# echo "Transferring files to server..."
# echo "$1 -> root@$TARGET_DEVICE_HOST/home/root/$2"

ssh -F $SCRIPT_DIR/.ssh/config target mkdir -p /home/root/$2
scp -F $SCRIPT_DIR/.ssh/config $1 target:/home/root/$2

