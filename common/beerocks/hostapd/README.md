
# Directory Content
This diectory contains prplMesh utilities to work with hostapd
It has few features as listed below

## Configuration
Manipulates hostapd configuration.
* load
* save
* add
* remove
* edit
* disable
* find (if required)

## Process
Manages hostapd process
* start
* stop
* restart

## Commands
Manages sending and receiving commands to hostapd

## Events
Manages events originated by hostapd itself

# Code

## namespace
The entire code is under prplmesh::hostapd
* Configuration: prplmesh::hostapd::config
* Process:       prplmesh::hostapd::process
* Commands:      prplmesh::hostapd::commands
* Events:        prplmesh::hostapd::events

