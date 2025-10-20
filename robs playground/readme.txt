README

The code has implemented the following additions.  It is not based on Matilda's code, but should be easily integrated with it.

SOME THINGS WILL NEED TO BE CHANGED IN THE CODE BEFORE THIS WILL WORK IN YOUR SETUP.

1. a MAC-IP lookup table, to which other MAC addresses and IP addresses should be added. To do that, upload the code and run it while monitoring the serial port (note, for serial you'll need to be plugged in; it doesn't work OTA).  The first message will tell you the board's MAC address.  Add it in to the table, add the next IP address, then re-upload the code and reboot.

2. using the local wifi network: search for "LAN settings" and duplicate that block to add your local parameters.  Note that your laptop will need to have a static IP.  I'm using 192.168.0.200 for the laptop and 192.168.0.201-216 for the D1s (I only have 2 though).

3. once your D1 can get on the same network as your laptop, you can use OTA programming.  The board should show up under Ports/Network Ports as "D1mini_xxx" where xxx is the last digits of its IP address. Just select that port before uploading and you won't need to be tethered.  Note the serial monitor will not work in this case, so having a Max patch open with a [udpreceive 8887] object connected to a [print] object will give you debug info in the Max console.

4. the Max patch shows you how you can set it up to use the same [udpsend] object by selecting the IP address or 255 for broadcast (all D1s receive the packet). Simply click the "201" "202" or "255" to set the destination for the udpsend, and then click the messages connected to the input port.  There is probably a cleaner "selector" way to do this which I can work on when I have time later.

5. the udpsend input port constructs a message 'Fx' where F is for "flashes" and 0<=x<=20 is the number of flashes at 10Hz in a given 2s period.  x=0 means off and x=20 means on.  This is just test code that I could run without having connected the strips, but similar ideas will work for other messages.

6. also, I'm using ticker.h library to set up timer interrupts.  This is useful for the LED flashing because you don't use any delay commands anywhere so the code is not stalled.  Essentially, an interrupt works by setting a timer, then calling a predetermined function (see <x>Ticker.attach() in the code) every time that timer expires.  We may also want to set up other tickers because these are much more precise in terms of timing than using milli-based delays.

7. I've introduced communication the other way from D1 to Max via UDP as well; see the examples at the bottom of setup(). the sendUDPMessage() function is designed to accept a variable number of parameters and build an OSC message out of that, which it then sends over UDP. The defined OSC message types are /status and /debug.  You can see how to use a [route] object in Max to filter those to different destinations.  I like keeping debug separate from status because it makes it easier later to just put in a DEBUG_TRUE flag in the firmware so we can turn on and off debug messages without having to comment and uncomment them all the time.

