"""Hangman plotter: game logic + web interface + drawing over serial.

Run:  python app.py      then open http://localhost:5000
"""
import math
import random
import sys
import threading
import time

import serial
from flask import Flask, jsonify, render_template_string, request

# ---------------- configuration ----------------
PORT = "COM3"            # change to your Arduino's port (e.g. /dev/ttyACM0 on Linux/Mac)
BAUD = 115200
WORDS = ["gear", "torque", "matrix", "lever", "vector", "beam", "shaft"]
MAX_WRONG = 6

# Layout in mm. Origin (0,0) = where the pen sits when a game starts.
# The full drawing spans about 150 x 100 mm; shrink these if your travel is smaller.
LETTER_SCALE = 3.0       # font grid is 2 x 4 units -> 6 x 12 mm letters
SLOT_PITCH = 10.0        # spacing between letter slots
WORD_X, WORD_Y = 70.0, 10.0
HANG_X, HANG_Y = 10.0, 10.0

# ---------------- stroke font ----------------
# 2 (wide) x 4 (tall) grid. Each letter is a list of polylines.
FONT = {
    'a': [[(0, 0), (1, 4), (2, 0)], [(0.5, 1.6), (1.5, 1.6)]],
    'b': [[(0, 0), (0, 4), (1.6, 4), (1.6, 2.2), (0, 2)], [(0, 2), (2, 1.9), (2, 0), (0, 0)]],
    'c': [[(2, 4), (0, 4), (0, 0), (2, 0)]],
    'd': [[(0, 0), (0, 4), (1.5, 4), (2, 3), (2, 1), (1.5, 0), (0, 0)]],
    'e': [[(2, 4), (0, 4), (0, 0), (2, 0)], [(0, 2), (1.5, 2)]],
    'f': [[(2, 4), (0, 4), (0, 0)], [(0, 2), (1.5, 2)]],
    'g': [[(2, 4), (0, 4), (0, 0), (2, 0), (2, 2), (1, 2)]],
    'h': [[(0, 4), (0, 0)], [(2, 4), (2, 0)], [(0, 2), (2, 2)]],
    'i': [[(0, 4), (2, 4)], [(1, 4), (1, 0)], [(0, 0), (2, 0)]],
    'j': [[(2, 4), (2, 0), (0, 0), (0, 1)]],
    'k': [[(0, 4), (0, 0)], [(2, 4), (0, 2), (2, 0)]],
    'l': [[(0, 4), (0, 0), (2, 0)]],
    'm': [[(0, 0), (0, 4), (1, 2), (2, 4), (2, 0)]],
    'n': [[(0, 0), (0, 4), (2, 0), (2, 4)]],
    'o': [[(0, 0), (0, 4), (2, 4), (2, 0), (0, 0)]],
    'p': [[(0, 0), (0, 4), (2, 4), (2, 2), (0, 2)]],
    'q': [[(0, 0), (0, 4), (2, 4), (2, 0), (0, 0)], [(1, 1), (2, 0)]],
    'r': [[(0, 0), (0, 4), (2, 4), (2, 2), (0, 2)], [(0.8, 2), (2, 0)]],
    's': [[(2, 4), (0, 4), (0, 2), (2, 2), (2, 0), (0, 0)]],
    't': [[(0, 4), (2, 4)], [(1, 4), (1, 0)]],
    'u': [[(0, 4), (0, 0), (2, 0), (2, 4)]],
    'v': [[(0, 4), (1, 0), (2, 4)]],
    'w': [[(0, 4), (0.5, 0), (1, 2), (1.5, 0), (2, 4)]],
    'x': [[(0, 4), (2, 0)], [(2, 4), (0, 0)]],
    'y': [[(0, 4), (1, 2), (2, 4)], [(1, 2), (1, 0)]],
    'z': [[(0, 4), (2, 4), (0, 0), (2, 0)]],
}


def circle(cx, cy, r, n=24):
    return [(cx + r * math.cos(2 * math.pi * k / n),
             cy + r * math.sin(2 * math.pi * k / n)) for k in range(n + 1)]


# Hangman stages in mm, relative to (HANG_X, HANG_Y). Stage 0 = gallows.
HANG = {
    0: [[(0, 0), (40, 0)], [(10, 0), (10, 80)], [(10, 80), (35, 80)], [(35, 80), (35, 70)]],
    1: [circle(35, 64, 6)],        # head
    2: [[(35, 58), (35, 38)]],     # body
    3: [[(35, 54), (25, 46)]],     # left arm
    4: [[(35, 54), (45, 46)]],     # right arm
    5: [[(35, 38), (27, 22)]],     # left leg
    6: [[(35, 38), (43, 22)]],     # right leg
}

# ---------------- serial / drawing ----------------
try:
    ser = serial.Serial(PORT, BAUD, timeout=120)
except serial.SerialException as e:
    sys.exit(f"Could not open {PORT}: {e}\nEdit PORT at the top of app.py.")
time.sleep(2.5)          # Arduino resets when the port opens
ser.readline()           # swallow "ready"
lock = threading.Lock()


def send(cmd):
    ser.write((cmd + "\n").encode())
    ser.readline()       # block until the Arduino replies "ok"


def draw_polylines(lines, ox, oy, scale=1.0):
    for pl in lines:
        x0, y0 = pl[0]
        send("U")
        send(f"G {ox + x0 * scale:.2f} {oy + y0 * scale:.2f}")
        send("D")
        for x, y in pl[1:]:
            send(f"G {ox + x * scale:.2f} {oy + y * scale:.2f}")
    send("U")


def draw_letter(ch, slot):
    draw_polylines(FONT[ch], WORD_X + slot * SLOT_PITCH, WORD_Y, LETTER_SCALE)


def draw_hang_stage(n):
    draw_polylines(HANG[n], HANG_X, HANG_Y)


def draw_blanks(count):
    lines = [[(i * SLOT_PITCH, -2), (i * SLOT_PITCH + 6, -2)] for i in range(count)]
    draw_polylines(lines, WORD_X, WORD_Y)


# ---------------- game state ----------------
state = {}


def new_game():
    state.update(word=random.choice(WORDS), guessed=set(), wrong=0, over=False)
    with lock:
        send("H")                    # current pen position becomes (0,0)
        draw_hang_stage(0)
        draw_blanks(len(state["word"]))
        send("G 0 0")


def display():
    return " ".join(c if c in state["guessed"] else "_" for c in state["word"])


# ---------------- web interface ----------------
app = Flask(__name__)

PAGE = """
<!doctype html>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Hangman Plotter</title>
<body style="font-family:sans-serif;max-width:420px;margin:2em auto;text-align:center">
<h2>Hangman Plotter</h2>
<p id="status" style="font-size:1.6em;letter-spacing:.1em">Press New game</p>
<input id="l" maxlength="1" size="2" style="font-size:1.4em;text-align:center">
<button id="gb" onclick="g()">Guess</button>
<button onclick="n()">New game</button>
<p id="busy" style="color:#888"></p>
<script>
const el = id => document.getElementById(id);
async function call(url, body) {
  el('busy').innerText = 'Drawing...';
  el('gb').disabled = true;
  const r = await fetch(url, {method:'POST', headers:{'Content-Type':'application/json'},
                              body: JSON.stringify(body || {})});
  const d = await r.json();
  el('busy').innerText = ''; el('gb').disabled = false;
  show(d);
}
function g() { call('/guess', {letter: el('l').value}); el('l').value = ''; el('l').focus(); }
function n() { call('/new'); }
function show(d) {
  el('status').innerText = d.error ? d.error :
    d.display + '\\nwrong: ' + d.wrong + '/6' +
    (d.won ? '\\nYOU WON!' : '') + (d.lost ? '\\nYOU LOST: ' + d.word : '');
}
el('l').addEventListener('keydown', e => { if (e.key === 'Enter') g(); });
</script>
</body>
"""


@app.route("/")
def index():
    return render_template_string(PAGE)


@app.route("/new", methods=["POST"])
def new():
    new_game()
    return jsonify(display=display(), wrong=0, won=False, lost=False)


@app.route("/guess", methods=["POST"])
def guess():
    if not state or state["over"]:
        return jsonify(error="Start a new game")
    letter = (request.json.get("letter") or "").lower()
    if len(letter) != 1 or letter not in FONT or letter in state["guessed"]:
        return jsonify(error="Invalid or repeated letter")
    state["guessed"].add(letter)
    with lock:
        if letter in state["word"]:
            for i, c in enumerate(state["word"]):
                if c == letter:
                    draw_letter(letter, i)
        else:
            state["wrong"] += 1
            draw_hang_stage(state["wrong"])
        send("G 0 0")
    won = all(c in state["guessed"] for c in state["word"])
    lost = state["wrong"] >= MAX_WRONG
    state["over"] = won or lost
    return jsonify(display=display(), wrong=state["wrong"], won=won, lost=lost,
                   word=state["word"] if lost else None)


if __name__ == "__main__":
    # Not auto-starting a game: position the pen at the bottom-left of the paper
    # first, then press "New game" in the browser.
    app.run(host="0.0.0.0", port=5000, threaded=False)
