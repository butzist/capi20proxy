#!/bin/bash 

echo -en "Checking dependencies for capifwd...\n"

echo -en "

#include <stdio.h>
#include <capi20.h>

int main() {
	printf(\":LibCAPI20 is installed, status: %d\n\n\",capi20_isinstalled());
	return 0;
}\n\n" >tmp.c

gcc -o tmp tmp.c -lcapi20 
./tmp

rm -f tmp tmp.c
