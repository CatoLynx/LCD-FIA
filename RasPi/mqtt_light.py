import argparse
import json
import traceback
import time

import paho.mqtt.client as mqtt

from fia_control import FIA
from local_settings import *


def on_message(fia, client, userdata, message):
    if message.topic == COMMAND_TOPIC:
        state = message.payload.decode("utf-8")
        if state == "ON":
            print("Turning on")
            try:
                fia.set_backlight_state(True)
                client.publish(STATE_TOPIC, "ON")
            except:
                traceback.print_exc()
        elif state == "OFF":
            print("Turning off")
            try:
                fia.set_backlight_state(False)
                client.publish(STATE_TOPIC, "OFF")
            except:
                traceback.print_exc()


def loop(client):
    update_interval = 10
    discovery_interval = 300
    last_update = 0
    last_discovery = 0
    while True:
        now = time.time()
        
        if now - last_discovery >= discovery_interval:
            last_discovery = now
            print("Sending discovery message")
            payload = {
                "name": "LCD-FIA Licht",
                "icon": "mdi:train",
                "object_id": "lcd_fia_light",
                "unique_id": "lcd_fia_light",
                "command_topic": COMMAND_TOPIC,
                "state_topic": STATE_TOPIC
            }
            client.publish(DISCOVERY_TOPIC, json.dumps(payload), retain=True)
        else:
            time.sleep(1)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-d", "--delete", action='store_true', required=False, help="Delete sensor in Home Assistant")
    args = parser.parse_args()
    
    fia = FIA("/dev/ttyAMA1", (3, 0), width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT)
    
    while True:
        try:
            client = mqtt.Client()
            client.on_message = lambda *args, **kwargs: on_message(fia, *args, **kwargs)
            client.username_pw_set(username=MQTT_USER, password=MQTT_PASSWORD)
            client.connect(MQTT_BROKER, 1883, 60)
            
            client.publish(STATE_TOPIC, "ON" if fia.get_backlight_state() else "OFF")
            
            client.subscribe(COMMAND_TOPIC)
            client.loop_start()
        except KeyboardInterrupt:
            return
        except:
            try:
                client.loop_stop()
            except:
                traceback.print_exc()
            traceback.print_exc()
            time.sleep(5)
        else:
            break
    
    if args.delete:
        print("Deleting sensor")
        client.publish(DISCOVERY_TOPIC, "")
        return
    
    while True:
        try:
            loop(client)
        except KeyboardInterrupt:
            return
        except:
            traceback.print_exc()
            time.sleep(5)
            client.connect(MQTT_BROKER, 1883, 60)


if __name__ == "__main__":
    main()
