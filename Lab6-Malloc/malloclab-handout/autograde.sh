#!/bin/bash

echo -e "\c" > result.txt
for file in traces/* ; do
	echo -e "$file" >> result.txt
	./mdriver -f $file >> result.txt
	echo "" >> result.txt
done
