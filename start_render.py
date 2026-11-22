import http.server, os, shutil, stat, subprocess, sys, threading, urllib.request, zipfile

# 1. Tiny keep-alive HTTP server for Render's required web port
port = int(os.environ.get("PORT", 10000))
class PingHandler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.end_headers()
        self.wfile.write(b"VeloCT is alive!")
    def log_message(self, *args):
        pass

threading.Thread(
    target=lambda: http.server.HTTPServer(("0.0.0.0", port), PingHandler).serve_forever(),
    daemon=True
).start()

# 2. Download lichess-bot if missing
if not os.path.exists("lichess-bot.py"):
    urllib.request.urlretrieve(
        "https://github.com/lichess-bot-devs/lichess-bot/archive/refs/heads/master.zip",
        "lb.zip"
    )
    with zipfile.ZipFile("lb.zip") as z:
        z.extractall(".")
    os.remove("lb.zip")
    for item in os.listdir("lichess-bot-master"):
        if not os.path.exists(item):
            shutil.move(os.path.join("lichess-bot-master", item), item)
    shutil.rmtree("lichess-bot-master", ignore_errors=True)

# 3. Download v8_linux from your GitHub Release
os.makedirs("VeloCT", exist_ok=True)
bin_path = os.path.join("VeloCT", "v8_linux")
if not os.path.exists(bin_path):
    urllib.request.urlretrieve(
        "https://github.com/VibeDevGPT/VeloCT/releases/download/linux/v8_linux.zip",
        "v8.zip"
    )
    with zipfile.ZipFile("v8.zip") as z:
        z.extractall("VeloCT")
    os.remove("v8.zip")
    os.chmod(bin_path, os.stat(bin_path).st_mode | stat.S_IEXEC)

# 4. Generate config.yml using your Render Environment Variable
token = os.environ["LICHESS_TOKEN"]
with open("config.yml", "w") as f:
    f.write(f"""token: "{token}"
url: "https://lichess.org/"
engine:
  dir: "./VeloCT/"
  name: "v8_linux"
  working_dir: "./VeloCT/"
  protocol: "uci"
  ponder: false
  online_moves:
    online_egtb:
      enabled: true
      min_time: 20
      max_pieces: 7
      source: "lichess"
  uci_options:
    Move Overhead: 500
    Threads: 1
    Hash: 64
abort_time: 30
move_overhead: 2000
challenge:
  concurrency: 1
  sort_by: "best"
  accept_bot: true
  only_bot: false
  max_increment: 20
  min_increment: 0
  max_base: 1800
  min_base: 30
  variants: [standard]
  time_controls: [bullet, blitz, rapid]
  modes: [casual, rated]
greeting:
  hello: "Hi {{opponent}}! I'm VeloCT (V8). Good luck!"
  goodbye: "Good game!"
pgn_directory: ""
""")

# 5. Install dependencies & launch lichess-bot
subprocess.check_call([sys.executable, "-m", "pip", "install", "-r", "requirements.txt"])
subprocess.check_call([sys.executable, "lichess-bot.py"])
      
