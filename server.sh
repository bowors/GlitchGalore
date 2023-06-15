#!/bin/bash

# Check if the keys directory is present
if [[ ! -d /content/drive/MyDrive/keys ]]; then
    echo "Error: keys directory not found"
    exit 1
fi

# Check if Flask is already installed and install it if not
if ! python3 -c "import flask" &> /dev/null; then
    pip install flask
fi

# Create .ssh directory and copy keys into it
mkdir ~/.ssh
cp /content/drive/MyDrive/keys/* ~/.ssh

# Create a named pipe for communication between processes
if [[ -e mypipe ]]; then
    rm mypipe
fi
mkfifo mypipe

# Define process 1: Run the Flask web server
process1() {
    python3 app.py 
}

# Define process 2: Keep track of subprocesses and download file when notified by process 1
process2() {
    pids=()
    while true; do
        read line < mypipe
        if [[ $line == "request received from /hook/"* ]]; then
            for pid in "${pids[@]}"; do
                kill $pid 2> /dev/null
            done
            pids=()
        fi
        hook_id=$(echo $line | grep -oP '(?<=/hook/)[0-9]+')
        if [[ -n $hook_id ]]; then
            url=${urls[$((hook_id-1))]}
            wget -qO- $url >&2 || \
            echo "Error: wget command failed" &
            pids+=($!)
        fi
    done
}

# Define process 3: Expose the web server to the internet using serveo
process3() {
    ssh -T -o StrictHostKeyChecking=no \
        -R pingin-ngepot:80:localhost:5150 serveo.net || \
        echo "Error: ssh command failed"
}

# Start all three processes in the background in numerical order
process1 &
process2 &
process3 &
