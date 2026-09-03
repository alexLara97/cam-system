import json
import time
import threading

import paho.mqtt.client as mqtt
from flask import Flask, render_template
from flask_socketio import SocketIO

# ============================
# Configuración
# ============================
MQTT_BROKER = "localhost"       # Mosquitto corre en la misma Pi
MQTT_PORT = 1883
MQTT_CLIENT_ID = "hmi-backend"

TOPIC_PAN = "casa/camara/servo/pan"
TOPIC_TILT = "casa/camara/servo/tilt"
TOPIC_ESTADO = "casa/camara/servo/estado"

ESP32_CAM_IP = "192.168.10.27"   # la IP fija que le pusiste a la ESP32-CAM
STREAM_URL = f"http://{ESP32_CAM_IP}:81/stream"

# Límites (deben coincidir con los del firmware de servos, es solo una
# protección extra en el lado del HMI, la autoridad real está en el ESP32)
PAN_MIN, PAN_MAX = 0, 180
TILT_MIN, TILT_MAX = 30, 150

# Cuánto se mueve el servo por "tick" del joystick (grados)
STEP_DEGREES = 3

# Cada cuánto se permite enviar un nuevo comando MQTT mientras el joystick
# está siendo arrastrado, para no saturar el broker/servo (segundos)
MOVE_THROTTLE = 0.1

app = Flask(__name__)
app.config["SECRET_KEY"] = "cambia-esto-por-algo-tuyo"
socketio = SocketIO(app, cors_allowed_origins="*")

# Estado en memoria del servidor (se sincroniza con lo que publica el ESP32)
state_lock = threading.Lock()
state = {"pan": 90, "tilt": 90}

last_move_time = 0.0


# ============================
# MQTT
# ============================
def on_mqtt_connect(client, userdata, flags, rc, properties=None):
    print(f"[MQTT] Conectado con código {rc}")
    client.subscribe(TOPIC_ESTADO)


def on_mqtt_message(client, userdata, msg):
    global state
    try:
        payload = json.loads(msg.payload.decode())
    except (json.JSONDecodeError, UnicodeDecodeError):
        return

    if msg.topic == TOPIC_ESTADO:
        with state_lock:
            state["pan"] = payload.get("pan", state["pan"])
            state["tilt"] = payload.get("tilt", state["tilt"])
        # Reenvía el estado real (confirmado por el ESP32) a todos los navegadores conectados
        socketio.emit("estado_servos", state)


mqtt_client = mqtt.Client(client_id=MQTT_CLIENT_ID, callback_api_version=mqtt.CallbackAPIVersion.VERSION2)
mqtt_client.on_connect = on_mqtt_connect
mqtt_client.on_message = on_mqtt_message


def start_mqtt():
    mqtt_client.connect(MQTT_BROKER, MQTT_PORT, keepalive=60)
    mqtt_client.loop_start()  # hilo propio de paho, no bloquea Flask-SocketIO


def clamp(value, min_v, max_v):
    return max(min_v, min(max_v, value))


def publish_position():
    """Publica en MQTT la posición actual calculada en el servidor.
    El ESP32 hace su propio clamp también, esto es solo primera línea de defensa."""
    mqtt_client.publish(TOPIC_PAN, str(state["pan"]))
    mqtt_client.publish(TOPIC_TILT, str(state["tilt"]))


# ============================
# Rutas HTTP
# ============================
@app.route("/")
def index():
    return render_template("index.html", stream_url=STREAM_URL)


# ============================
# Eventos Socket.IO
# ============================
@socketio.on("connect")
def handle_connect():
    # Al conectar un cliente nuevo, le mandamos el último estado conocido
    with state_lock:
        socketio.emit("estado_servos", state)


@socketio.on("mover_joystick")
def handle_joystick(data):
    """
    data esperado: {"x": -1..1, "y": -1..1}
    x controla pan, y controla tilt. Viene de nipplejs en el frontend.
    """
    global last_move_time

    now = time.time()
    if now - last_move_time < MOVE_THROTTLE:
        return  # throttle: ignoramos eventos demasiado seguidos
    last_move_time = now

    x = data.get("x", 0)
    y = data.get("y", 0)

    with state_lock:
        state["pan"] = clamp(state["pan"] + int(x * STEP_DEGREES), PAN_MIN, PAN_MAX)
        state["tilt"] = clamp(state["tilt"] + int(y * STEP_DEGREES), TILT_MIN, TILT_MAX)
        publish_position()
        # Emitimos optimistamente al instante (sin esperar confirmación del
        # ESP32) para que el joystick se sienta responsivo; luego el evento
        # 'estado_servos' de MQTT confirmará/corregirá el valor real.
        socketio.emit("estado_servos", state)


@socketio.on("centrar")
def handle_centrar():
    with state_lock:
        state["pan"] = 90
        state["tilt"] = 90
        publish_position()
        socketio.emit("estado_servos", state)


if __name__ == "__main__":
    start_mqtt()
    socketio.run(app, host="0.0.0.0", port=5000, allow_unsafe_werkzeug=True)
