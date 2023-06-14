#!/bin/bash

sudo apt-get install libevhtp-dev libevent-dev libjansson-dev
git clone https://github.com/bowors/GlitchGalore.git
cd /content/GlitchGalore
gcc -o server server.c -levhtp -levent -ljansson
chmod +x server

# Check if the keys directory is present
if [[ ! -d /content/drive/MyDrive/keys ]]; then
    echo "Error: keys directory not found"
    exit 1
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
    ./server
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
        case $line in
            "request received from /hook/1")
                wget -qO- https://example.com >&2 || \
                echo "Error: wget command failed" &
                pids+=($!)
                ;;
            "request received from /hook/2")
                wget -qO- https://google.com >&2 || \
                echo "Error: wget command failed" &
                pids+=($!)
                ;;
            "request received from /hook/3")
                wget -qO- https://bing.com >&2 || \
                echo "Error: wget command failed" &
                pids+=($!)
                ;;
        esac
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