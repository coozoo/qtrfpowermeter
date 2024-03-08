# QT RF Power Meter
This application few hours project intended to improve usage of chinese RF power meter RF8000. Chinese default application is terrible unreliablr with no functionality and lot of crashes.

Device shown on image below.

<img src="https://private-user-images.githubusercontent.com/25594311/310401908-3579a887-5e65-40f3-8108-0039601ab364.png" width="60%"></img> 

There is versions of device with lower frequency range I'm not aware if they're compatible, but I suspect the only limitation will be the ability to set higher frequencies.

Simple serial protocol that reports dbm and Vpp values, with some bugs of broken charachters can be fixed with setting frequency and offset (basically most of the times program will do that for you, but if you see no captures and broken symbols on device screen simply try to set frequency using program).

There is no way to identify device for sure so you should do that by your own if you have more than one serial devices connected.

To initiate connection press connect. After that raw data captured from device will be in log tab. 

On data tab there is parsed and calculated data exactly in the same way it's written to csv if such option selected on stat tab.

Stat tab contains on fly monitors, box with possibility to set device offcet and frequency, there is chart with posibility to save images.

<img src="https://private-user-images.githubusercontent.com/25594311/311221844-ffeae88f-2a85-4209-a16a-127aba5ff7e8.png" width="60%"></img> 


