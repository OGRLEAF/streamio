SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
source $SCRIPT_DIR/envs.sh

echo "Transferring files to server..."
echo "$1-> root@$TARGET_DEVICE_IP/home/root/$2"

ssh -i $TARGET_DEVICE_SSH_KEY root@$TARGET_DEVICE_IP mkdir -p /home/root/$2
scp -i $TARGET_DEVICE_SSH_KEY $1 root@$TARGET_DEVICE_IP:/home/root/$2