SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
# source $SCRIPT_DIR/envs.sh

# echo "Transferring files to local" 
# echo "$1 -> root@$TARGET_DEVICE_HOST/home/root/$2"

scp -F $SCRIPT_DIR/.ssh/config  target:~/$2/$1  ./

