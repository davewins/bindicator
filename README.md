# bindicator

I created this version of the bindicator after seeing this video: https://www.youtube.com/watch?v=zLU1cOYf1dY, but I don't like using NodeRed and I wanted to use the normal Arduino IDE

I live in Gloucestershire, UK and so my local council website publish their refuse days on their website (https://www.tewkesbury.gov.uk/waste-and-recycling#bin-collection-calendar), and so I looked at how they were creating the calendar.

Luckily for me - they are using a REST call (Inspect the Web Pages to find out how they do it for your locale), which I was able to utilise, once I'd worked out how to make a REST call over HTTPS in the Arduino code.

The model bin itself was just 3D printed from here: https://www.thingiverse.com/thing:2247931

I used an ESP8266 and mounted it inside the 3D Printed wheelie bin.

I wired up a Blue LED to pin D3, and the Green LED to pin D2.

Not to sure about my code when it flips between months, but it currently appears to work for the last 2 months (including 2020 leap year)
