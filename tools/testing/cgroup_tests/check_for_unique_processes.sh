#!/bin/bash
#set -vx

WIDTH=$1;
DEPTH=$2;
CURRENT_PATH="$pwd"
COUNT=0

search_in_pwd () {
	local filename="tasks"
	while read -r line
	do
		if [ $line -eq $1 ]; then
			let COUNT=$COUNT+1
		fi
	done < "$filename"
	let DEPTH=$DEPTH-1
}

search_in_childs() {
	local j;
	cd $1;
	search_in_pwd $2
	if [ $DEPTH -eq 0 ]; then
		cd ..
	else
		for (( j=1; j<=$WIDTH; j++ ))
		do
			search_in_childs $j $2
		done
		cd ..
	fi
	DEPTH=$DEPTH+1
}

search_in_tree () {
	let COUNT=0
	
	DEPTH=$DEPTH+1
	cd /sys/fs/cgroup/systemd/
	#cd /home/saipraneeth/cgroup_tests/systemd/
	search_in_pwd $1
	
	DEPTH=$DEPTH+1
	cd sai
	search_in_pwd $1
	
	for (( k=1; k<=$WIDTH; k++ ))
	do
		search_in_childs $k $1
	done
	cd ..
	if [ $COUNT -gt 1 ]; then
		echo "Search hierarchy for unique threads failed!!"
	fi
}

filename="tasks"
while read -r line
do
	search_in_tree $line
	cd "$CURRENT_PATH"
done < "$filename"
