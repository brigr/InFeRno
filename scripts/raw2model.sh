#!/bin/sh

awk -F',' '{
	printf "%s",$NF;
	for (i=1; i<NF; i++)
		printf " %d:%s",i,$i;
	print "";
}'
