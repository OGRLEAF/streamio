SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

echo $SCRIPT_DIR
echo $1

ssh -F $SCRIPT_DIR/.ssh/config target $1

