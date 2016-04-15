#!/bin/bash
#set -vx

WIDTH=$1;
DEPTH=$2;

write_tasks_to_parent () {
	local filename="tasks"
	while read -r line
	do
		echo $line >> /sys/fs/cgroup/systemd/tasks
	done < "$filename"
	let DEPTH=$DEPTH-1
}

write_from_all_childs() {
	local j;
	cd $1;
	write_tasks_to_parent
	if [ $DEPTH -eq 0 ]; then
		cd ..
		rmdir $1
	else
		for (( j=1; j<=$WIDTH; j++ ))
		do
			write_from_all_childs $j
		done
		cd ..
		rmdir $1
	fi
	DEPTH=$DEPTH+1
}

cd /sys/fs/cgroup/systemd/sai/
let DEPTH=$DEPTH+1
write_tasks_to_parent
for (( k=1; k<=$WIDTH; k++ ))
do
	write_from_all_childs $k
done
cd ..
rmdir sai
