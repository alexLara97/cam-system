import threading
from flask import Flask


app = Flask(__name__)
app.config["SECRET_KEY"] = "ANDROMEDA_WORLD"
socketio = SocketIO(app, cors_allowed_origins="*")

state_lock = threading.Lock()
state = {"pan": 90, "tilt": 90}

last_move_time = 0.0


def start_mqtt():
    pass


if __name__ == "__main__":
    start_mqtt()