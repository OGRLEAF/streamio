#/bin/bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

HOST=tq15egbase


login_string=root@$HOST

ssh_id_file=$SCRIPT_DIR/.ssh/id_ed25519


echo "Generating ssh-key"

mkdir -p $SCRIPT_DIR/.ssh

ssh-keygen -t ed25519 -f $ssh_id_file -N ''

# echo 'SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )' > envs.sh

# echo "export TARGET_DEVICE_HOST=$HOST" >> envs.sh
# echo "export TARGET_DEVICE_SSH_KEY="'$SCRIPT_DIR/'"$ssh_id_file" >> envs.sh

echo "
Host target
        HostName $HOST
        User root
        IdentityFile $ssh_id_file
        " > $SCRIPT_DIR/.ssh/config

pubkey=$(cat $ssh_id_file.pub)


ssh-keygen -f ~/.ssh/known_hosts -R $HOST


echo "Run following command on dev board
"
echo "mkdir -p .ssh && echo \"$pubkey\" > .ssh/authorized_keys"

echo ""

