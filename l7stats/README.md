This is the initial version of the l7stat program. 

The basis for the development is to send layer 7 protocol statistics unix socket collectd plugin.
From there the data can be sent to any of the collectd write plugins; MQTT for example.
The L7 protocol data is derived from a UNIX socket provided by the netifyd engine.


