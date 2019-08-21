![CardSpotter](https://raw.githubusercontent.com/relgin/cardspotter/master/Assets/CardSpotterHeader.png)=======
CardSpotter is a browser extension that show automatic Magic:The Gathering card image tooltips for <video> streams. Tooltip shows the card image with a scryfall link.

It works by grabbing data from HTML5 video, extracting the clicked card using OpenCV and then finding the best match from a database using image hashing.

[50s demo recorded for 2.0](https://www.youtube.com/watch?v=-vKsLunV8Kg)

How to modify CardSpotter
=======
If you wish to modify anything **but** the WebAssembly
1. Clone the repository
2. Made the desired changes in the extension folder (MagicCardSpotter)
3. Load the MagicCardSpotter folder as an extension (Chrome on Windows)
	1. Go to chrome://extensions
	2. Enable Developer mode. (checkbox in the upper-right corner)
	3. Click the "Load unpacked extension..." button.
	4. Select the MagicCardSpotter directory.

To modify the CardSpotter WebAssembly
1. Clone the repository
2. Install Windows Subsystem for Linux - Ubuntu using the instructions here: https://docs.microsoft.com/en-us/windows/wsl/install-win10
3. From the Linux command line install python 2.7.
	1. sudo apt update
	2. sudo apt upgrade
	3. sudo apt install python2.7
4. Download [Emsdk 1.38.21 and OpenCV 4.0.1 with prebuilt dependecies](https://drive.google.com/open?id=1yX-rDAqLdsOB1eRgUq45K8qBhxwGgWOo) and extract them to the Code directory.
5. From the Linux command line in the Code directory run ./buildwasm.sh this will replace the cardspotter.wasm and cardspotter.js in the MagicCardSpotter directory.


