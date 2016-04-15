#!/bin/bash
#set -vx

WIDTH=$1;
DEPTH=$2;
NUM_OF_TASKS_PER_FILE=$3;

NUM_OF_TASKS=0;
INDEX=0;

calculate_num_of_tasks () {
	local depth=$DEPTH;
	local pow=0;
	local total=0;
	
	while [ $depth -gt 0 ]
	do
		let pow=$(echo "$WIDTH^$depth" | bc);
		let total=$pow+$total;
		let depth=$depth-1;
	done
	
	let total=$total+1
	let NUM_OF_TASKS=$total*$NUM_OF_TASKS_PER_FILE
}

write_tasks () {
	local i;
	for (( i=1; i<=$NUM_OF_TASKS_PER_FILE; i++ ))
	do
		echo ${TASKS_ARRAY[$INDEX]} >> tasks
		let INDEX=$INDEX+1
	done
	let DEPTH=$DEPTH-1
}

write_in_childs() {
	local j;
	cd $1;
	write_tasks
	if [ $DEPTH -eq 0 ]; then
		cd ..
	else
		for (( j=1; j<=$WIDTH; j++ ))
		do
			write_in_childs $j
		done
		cd ..
	fi
	DEPTH=$DEPTH+1
}

calculate_num_of_tasks

filename="tasks"
TASKS_ARRAY[NUM_OF_TASKS]=0
TASK=0
while read -r line
do
	task_id="$line"
	TASKS_ARRAY[$TASK]=$task_id
	let TASK=$TASK+1
done < "$filename"

cd /sys/fs/cgroup/systemd/sai/
let DEPTH=$DEPTH+1
write_tasks
for (( k=1; k<=$WIDTH; k++ ))
do
	write_in_childs $k
done
cd ..
