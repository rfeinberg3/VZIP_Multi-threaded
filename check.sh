#!/bin/bash

if [[ $(diff video.vzip reference.vzip) == "" ]]
then
	echo SUCCESS
else
	echo FAIL
fi
