Hangman Plotter
```
hangman_plotter/
  app.py                              # Python: game, drawing, web page
  firmware/hangman_plotter/
    hangman_plotter.ino               # Arduino (MKS Base v1.6)
```
Setup
Open `firmware/hangman_plotter/hangman_plotter.ino` in the Arduino IDE
(the sketch must stay inside a folder with the same name). Select
Arduino Mega 2560, pick the port, upload.
`pip install flask pyserial`
Edit `PORT` at the top of `app.py` (e.g. `COM3`, `/dev/ttyACM0`).
Close the Arduino Serial Monitor (it blocks the port), then run `python app.py`.
Open http://localhost:5000, or `http://<computer-ip>:5000` from a phone on the same Wi-Fi.
First-time tuning (use the Serial Monitor, 115200 baud, newline ending)
Verify Z pins. Firmware assumes the E0 slot: step 26, dir 28, enable 24.
Check against your board's pin map.
Pen travel. Send `Z 100` / `Z -100` to jog. Adjust `Z_TRAVEL_STEPS` so
pen-down touches paper with a little spring and pen-up clears it.
Flip `Z_DOWN_DIR` if the direction is backwards. A spring-loaded pen holder helps.
X/Y direction. Send `G 20 0`, then `G 20 20`, then `G 0 0`. If an axis moves the
wrong way, swap HIGH/LOW for that axis in `lineTo()`.
Work area. The drawing spans about 150 x 100 mm. Adjust `WORD_X`, `HANG_X`,
`LETTER_SCALE`, and the `HANG` coordinates in `app.py` if your travel is smaller.
Playing
Put the pen at the bottom-left of the paper, then press New game. The pen
position at that moment becomes (0,0). The gallows and blank slots are drawn, then
each guess either writes the letter in its slots or adds a hangman part.
After each guess the pen returns to (0,0).
