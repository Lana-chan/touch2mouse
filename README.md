# touch2mouse

i shouldn't have to have made this.

this fixes pico-8 not taking mouse inputs on pocketchips running kernels newer than 4.4

the problem this solves is that the resistive touch panel (rtp) now sends touch events instead of mouse button events

you need to run this program in the background while you play pico8. you probably want to close it once you're done

`sudo make install` as usual