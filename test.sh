#!/bin/bash

output="client-dump"
port=1234
ip="127.0.0.1"
filesize=10000
#filesize=10MB

file=$(mktemp)
base64 /dev/urandom | head --bytes=$filesize > $file
./server $port $file &
./client $ip $port
diff --brief $file $output
if [ $? -eq 0 ]; then
	echo "File received successfully"
fi
rm $file
