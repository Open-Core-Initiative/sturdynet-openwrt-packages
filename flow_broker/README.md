# flow_broker
ulogd to uds flow and packet broker. It pulls NFCT and NFLOG data out of netfilter, hashes it and sends it out the uds for consumption. For now this application is specific to netifyd but soon will be expanded as a plugable architecture.

Install these on your cirrus.
![Screen Shot 2022-03-22 at 4 40 55 PM](https://user-images.githubusercontent.com/8184748/159594564-18a09913-c692-4f3c-81fc-3b99821c0ca8.png)


![Screen Shot 2022-03-22 at 4 42 15 PM](https://user-images.githubusercontent.com/8184748/159594654-60c89028-ce54-4ff6-a564-b422df2fb386.png)


![Screen Shot 2022-03-25 at 11 58 02 AM](https://user-images.githubusercontent.com/8184748/160184980-d6f59fcc-aa44-416a-aff2-ae087eb11a5d.png)


ulogd confoguration file in in the repo.

firewall.user file is in repo. Place that file in /etc/firewall.user and restart your firewall.




