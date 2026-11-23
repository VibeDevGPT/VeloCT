import http.server, os, re, shutil, stat, subprocess, sys, threading, urllib.request, zipfile

# 1. Start keep-alive HTTP server FIRST so Render doesn't time out during compilation
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
    print("Downloading lichess-bot...", flush=True)
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

# 3. Locate v8.cpp and prepare VeloCT/ folder
os.makedirs("VeloCT", exist_ok=True)
if os.path.exists("v8.cpp"):
    shutil.copy2("v8.cpp", "VeloCT/v8.cpp")
cpp_path = os.path.join("VeloCT", "v8.cpp")
# 4. Scan v8.cpp for NNUE files and download from GitHub mirror first
with open(cpp_path, "r", errors="ignore") as f:
    nets = sorted(set(re.findall(r"nn-[0-9a-f]+\.nnue", f.read())))

for net in nets:
    net_path = os.path.join("VeloCT", net)
    if not os.path.exists(net_path) or os.path.getsize(net_path) < 100_000:
        urls = [
            f"https://github.com/official-stockfish/networks/raw/master/{net}",
            f"https://data.stockfishchess.org/nn/{net}",
            f"https://tests.stockfishchess.org/api/nn/{net}",
        ]
        for url in urls:
            print(f"Downloading {net} from {url}...", flush=True)
            res = subprocess.run(["curl", "-fL", "-A", "Mozilla/5.0", url, "-o", net_path])
            if res.returncode == 0 and os.path.exists(net_path) and os.path.getsize(net_path) >= 100_000:
                break
        else:
            raise RuntimeError(f"Failed to download {net} from all mirrors")
            

# 5. Compile v8.cpp every time
print("Compiling v8.cpp...", flush=True)
subprocess.check_call(
    [
        "g++", "-std=c++20", "-O3", "-march=x86-64-v2",
        "-pthread", "-DNDEBUG", "-DIS_64BIT",
        "v8.cpp", "-o", "v8_linux"
    ],
    cwd="VeloCT"
)
bin_path = os.path.join("VeloCT", "v8_linux")
os.chmod(bin_path, os.stat(bin_path).st_mode | stat.S_IEXEC)

# 6. Generate config.yml using Render Environment Variable
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

# 7. Install dependencies and launch lichess-bot
subprocess.check_call([sys.executable, "-m", "pip", "install", "-r", "requirements.txt"])
subprocess.check_call([sys.executable, "lichess-bot.py"])
