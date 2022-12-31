# <span style="color:blue">Introduction</span>

Each flow that originates on the device will be detected by netifyd. The flow data is formatted into JSON strings and placed on a stream unix socket. This application will consume those strings format the data and send it to a new socket where other application can consume it. The target consumer is the socket read plugin provided by collectd. The write plugin target is RRD. Therefore, the data must be provided in a format that RRD can consume, process and build databases with. The same flow data will also be pushed into MQTT topics and shipped to the MQTT broker.


# <span style="color:blue">Source Data</span>

Data providers are both the dictionaries constructed using the JSON flow data and the JSON application files. Each flow has three components coming in from the socket stream provided by netifyd.

* 	**A flow object** - Provides the digest, the application number and the interface the flow originated on.
* 	**A flow update object** - Provides the digest that matches the flow object, tx bytes and rx bytes.
* 	**A flow purge object** - Provides the digest that matches the flow object, tx bytes and rx bytes of the total when the flow has ended.

# <span style="color:blue">Build Dictionaries</span>

The flow object application number will be referenced in both the application JSON file and the application to application category mapping file. Once the application category is determined an application category number to application category name will be referenced. The application name and interface will be concatenated in a string that makes up the graph name in RRD. This is to show which flows went over which interfaces. It is important that the application names and application categories use exactly the same names provided in the JSON mapping file. It is also important that the same interface name as found in the inet table be used. This is for future display formatting functions.

# <span style="color:blue">Send to socket</span>

Once the application data and application category data is created, they will be placed on a socket for the collectd socket reader to consume. Collectd will then use the RRD output plugin to build the RRD graphs.

![Screen Shot 2022-02-10 at 11 05 57 AM](https://user-images.githubusercontent.com/8184748/153481445-8ccf0099-5fbf-46a1-a046-8c4075304205.png)
