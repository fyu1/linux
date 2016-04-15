#!/bin/bash
#set -vx

CURRENT_PATH="$pwd"
filename="tasks"
while read -r line
do
	task_id="$line"
	cd /sys/fs/cgroup/systemd/
	echo $task_id >> tasks
	cd "$CURRENT_PATH"
done < "$filename"
