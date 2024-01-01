# Stargate_LSD
A Stargate Atlantis lifesigns detector prop design files and software. This is not a 1:1 prop replica, because I couldn't find screen which would match, but it is close enough. It was originally made as a reaction to one of my friends suggestions that we need a dork particle detector or as in our czech language kokoton detektor.

## Case printing and painting:
Case is printed out on a Elegoo Mars 2 Pro resin printer with Phrozen aqua clear resin. Painting is done with diluted transparent blue enamel paint and sanded over.

## Electronics:
Mainboard: [[https://github.com/LaskaKit/ESPD-35/tree/main]] but anything should fit with 3d model change. It is based on ESP32 because I wanted simplicity of arduino and wireless capabilities for future functions.<br>
Buttons use my custom PCB made with KiCAD.<br>
For sensors: SHT40 and VL53L0X are used.<br>
Optional other sensors and stuff can be programmed in and connected via pin header in the bottom of the device.

## Software:
It is a mess made on long nights over a 4 month period. It contains 3 modes in which it can operate:<br>
Dork particle detector, which uses the ToF sensor.<br
A temperature and humidity monitor.<br>
And as a life signs detector which is WIP and I don't know when it will be finished with all features. Right now it is just a simple blinking and beeping dot in the middle of the screen, but I have an idea to make it into a "real" life signs detector with an array of stationary beacons scattered around an area where it should detect stuff which will triangulate positions of the LSD and other mobile beacons and send that data into the LSD over MQTT or something like that. I don't know when I will have the time and mood to do that. Probably when the need arises for some LARP or something.
