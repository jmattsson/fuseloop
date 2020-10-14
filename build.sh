#!/usr/bin/env bash

TAG=$(uuidgen | tr '[:upper:]' '[:lower:]')
CONTAINER=$(uuidgen | tr '[:upper:]' '[:lower:]')
docker build -t $TAG .
docker create -ti --name $CONTAINER $TAG bash
docker cp $CONTAINER:/fuseloop fuseloop
docker rm -f $CONTAINER
docker image rm -f $TAG
