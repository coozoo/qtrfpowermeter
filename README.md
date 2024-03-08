# QT RF Power Meter
This application few hours project intended to improve usage of chinese RF power meter RF8000. Chinese default application is terrible unreliable with no functionality and lot of crashes.

Device shown on image below.

![image](https://github.com/coozoo/qtrfpowermeter/assets/25594311/392a07ee-0ab3-42d0-b2da-7b5d18c994e4)


There is versions of device with lower frequency range I'm not aware if they're compatible, but I suspect the only limitation will be the ability to set higher frequencies.

Simple serial protocol that reports dbm and Vpp values, with some bugs of broken charachters can be fixed with setting frequency and offset (basically most of the times program will do that for you, but if you see no captures and broken symbols on device screen simply try to set frequency using program).

There is no way to identify device for sure so you should do that by your own if you have more than one serial devices connected.

To initiate connection press connect. After that raw data captured from device will be in log tab. 

On data tab there is parsed and calculated data exactly in the same way it's written to csv if such option selected on status tab.

Status tab contains on-fly monitors, box with possibility to set device offset and frequency, there is chart with posibility to save images.

![image](https://github.com/coozoo/qtrfpowermeter/assets/25594311/b71a20ff-38fb-4361-9710-ce44f0b54d50)



