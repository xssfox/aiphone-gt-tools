import socket
import struct
from dataclasses import dataclass
import logging
from enum import Enum
import types
from paho.mqtt import client as mqtt_client
import argparse
import random
import time
import json


class MqttClient():
    def __init__(self, broker, port, username, password, entrance_callback, lift_callback, remote_callback, unlock_callback, resident_address, entrance, lifts, remote_entrance):
        self.entrance_callback = entrance_callback
        self.lift_callback = lift_callback
        self.remote_callback = remote_callback
        self.unlock_callback = unlock_callback
        self.connect_mqtt(broker, port, username, password)
        self.client.loop_start()
        self.resident_address = resident_address
        self.entrance = entrance
        self.remote_entrance = remote_entrance
        self.lifts = lifts
    def connect_mqtt(self, broker, port, username, password):
        def on_connect(client, userdata, flags, rc):
            if rc == 0:
                logging.info("Connected to MQTT Broker!")
                self.send_discovery() # TODO send often
                client.subscribe(f"aiphone-{self.resident_address}/command")
            else:
                logging.error("Failed to connect, return code %d\n", rc)
        # Set Connecting Client ID
        client_id = f'aiphone-mqtt-{random.randint(0, 1000)}'
        self.client = mqtt_client.Client(mqtt_client.CallbackAPIVersion.VERSION1, client_id)


        self.client.username_pw_set(username, password)
        self.client.on_connect = on_connect
        self.client.on_disconnect = self.on_disconnect
        self.client.on_message = self.on_message

        self.client.connect(broker, port)
        
        logging.debug("MQTT connect called")

    def on_message(self, client, userdata, msg):
        print(f"Received `{msg.payload.decode()}` from `{msg.topic}` topic")
        if msg.topic == f"aiphone-{self.resident_address}/command":
            data = json.loads(msg.payload.decode())
            self.client.publish(f"aiphone-{self.resident_address}/availability","offline",2,True)
            if data['type'] == "entrance":
                self.entrance_callback(int(data['entrance']))
            if data['type'] == "lift":
                self.lift_callback(int(data['lift']))
            if data['type'] == "remote":
                self.remote_callback(int(data['section']),int(data['entrance']))
            if data['type'] == 'unlock':
                self.unlock_callback()
            self.client.publish(f"aiphone-{self.resident_address}/availability","online",2,True)

    def send_update(self, lineinuse):
        TOPIC = f"aiphone-{self.resident_address}/state"
        self.client.publish(TOPIC,json.dumps(
            {
                "line": "inuse" if lineinuse else "free"
            }
        ))
    def send_ring(self,from_type, from_address, to_type, to_address):
        TOPIC = f"aiphone-{self.resident_address}/event"
        self.client.publish(TOPIC,json.dumps(
            {
                "event_type": "ring",
                "to_type": to_type,
                "from_type": from_type,
                "from_address": from_address,
                "to_address": to_address
            }
        ))
    def send_message(self,message):
        TOPIC = f"aiphone-{self.resident_address}/message"
        self.client.publish(TOPIC,message)
    def send_discovery(self):
        TOPIC = f"homeassistant/device/aiphone-{self.resident_address}/config"
        config = {
                "dev": {
                    "ids": f"aiphone-{self.resident_address}",
                    "name": "Aiphone Intercom",
                    "mf": "Aiphone",
                    "mdl": "GT 1C7W",
                    "sw": "1.0",
                    "sn": "",
                    "hw": ""
                },
                "o": {
                    "name":"busserver",
                    "sw": "0.1",
                    "url": "https://github.com/xssfox"
                },
                "cmps": {
                    "lineinuse": {
                        "p": "binary_sensor",
                        "unique_id":"lineinuse",
                        "name": "Line In Use",
                        "payload_on": "inuse",
                        "payload_off": "free",
                        "value_template":"{{ value_json.line }}",
                    },
                    "last_message": {
                        "p": "sensor",
                        "unique_id":"lastmessage",
                        "name": "Last Message",
                        "state_topic": f"aiphone-{self.resident_address}/message",
                    },
                    "call": {
                        "p": "event",
                        "device_class": "doorbell",
                        "unique_id":"incoming_call",
                        "name": "Incoming Call",
                        "event_types": ["ring"],
                        "state_topic": f"aiphone-{self.resident_address}/event"
                    },
 
                    "unlock" : {
                        "p": "button",
                        "unique_id":f"unlock_call",
                        "name": f"Unlock call in progress",
                        "payload_press": json.dumps({
                            "type": "unlock"
                        }),
                        "command_topic": f"aiphone-{self.resident_address}/command",
                        "availability_topic": f"aiphone-{self.resident_address}/availability",
                    
                    },
                },
                "state_topic":f"aiphone-{self.resident_address}/state",
                "qos": 2
            }
        for entrance in self.entrance:
            config["cmps"][f"entrance_{entrance}"] = {
                "p": "button",
                "unique_id":f"entrance_{entrance}",
                "name": f"Entrance {entrance} Unlock",
                "payload_press": json.dumps({
                    "type": "entrance",
                    "entrance": f"{entrance}"
                }),
                "command_topic": f"aiphone-{self.resident_address}/command",
                "availability_topic": f"aiphone-{self.resident_address}/availability",
            }
        for lifts in self.lifts:
            config["cmps"][f"lift_{lifts}"] = {
                "p": "button",
                "unique_id":f"lift_{lifts}",
                "name": f"Lift {lifts} Unlock",
                "payload_press": json.dumps({
                    "type": "lift",
                    "lift": f"{lifts}"
                }),
                "command_topic": f"aiphone-{self.resident_address}/command",
                "availability_topic": f"aiphone-{self.resident_address}/availability",
            }
        for remote_entrance in self.remote_entrance:
            config["cmps"][f"remote_{remote_entrance}"] = {
                "p": "button",
                "unique_id":f"remote_{remote_entrance}",
                "name": f"Entrance {remote_entrance} Unlock",
                "payload_press": json.dumps({
                    "type": "remote",
                    "section": f"{remote_entrance.split(':')[0]}",
                    "entrance": f"{remote_entrance.split(':')[1]}",
                }),
                "command_topic": f"aiphone-{self.resident_address}/command",
                "availability_topic": f"aiphone-{self.resident_address}/availability",
            }            
        self.client.publish(TOPIC,json.dumps(config),2,True)
        self.client.publish(f"aiphone-{self.resident_address}/availability","online",2,True)
    def on_disconnect(self,client, userdata, rc):
        logging.error("Disconnected from MQTT")
        while 1:
            try:
                self.client.reconnect()
            except Exception as err:
                logging.error("%s. Reconnect failed. Retrying...", err)
            time.sleep(1)

    

class AddressType(Enum):
    # POSSIBLY others for the second trunk line
    BROADCAST_CLIENT = 0x0
    UNKNOWN_1        = 0x1
    UNKNOWN_2        = 0x2
    UNKNOWN_3        = 0x3
    UNKNOWN_4        = 0x4
    UNKNOWN_5        = 0x5
    UNKNOWN_6        = 0x6
    UNKNOWN_7        = 0x7
    RESIDENT         = 0x8
    UNKNOWN_9        = 0x9
    UNKNOWN_A        = 0xa
    UNKNOWN_b        = 0xb
    ENTRANCE         = 0xc # Can also be the gateway when address = 0x1
    UNKNOWN_d        = 0xd
    SECURITY         = 0xe # Can also be the gateway when address = 0x1
    BROADCAST        = 0xf # possibly the bus expander


COMMANDS = {
    0x06: "ACK",
    0x0b: "KEEPALIVE/LINEUSE?",
    0x11: "SYSTEM_INFO",
    0x12: "SYSTEM_INFO_2",
    0x13: "PING?",
    0x20: "LINE_REQUEST", # happens first
    0x23: "LINE_REQUEST_3",
    0x26: "LINE_REQUEST_2",
    0x24: "HANG_UP",
    0x29: "CALL_ESTABLISHED_1",
    0x33: "CALL_MON", #?
    0x35: "START_CALL_1", # from MCX?
    0x36: "START_CALL_2", # from bus expander?
    0x40: "CALL_ESTABLISHED_2",

    0x70: 'INCOMING_CALL_RING_NO_CAM',
    0x71: 'INCOMING_CALL_RING_CAM', # from gateway
    0x72: 'INCOMING_CALL_RING_NO_CAM_2',
    0x73: 'INCOMING_CALL_RING_CAM_2',
    0x74: 'INCOMING_CALL_RING_EMERGENCY',
    0x7b: 'INCOMING_CALL_ESTABLISHED_2', # from resident
    0x7d: 'CALL_ESTABLISHED_3',
    0x79: 'RING_ACK',

    0x81: 'END_CALL_1',
    0x82: 'END_CALL_2',
    0x83: 'UNLOCK_PRESS',
    0x84: 'UNLOCK_RELEASE',
    0x86: 'MONITOR_REQUEST',
    0x87: 'MONITOR_OK',
    0x89: 'MONITOR_END',

    0x9e: "REQ_TALK",

    0xe1: "CAMERA_INFO",
    0xe3: "CAMERA_INFO_2",
    0xb0: 'LIFT_CONTROL',

    0xf0: 'REMOTE_CALL_ESTABLISHED',
    0xf1: 'REMOTE_SET_ADDRESS_1',
    0xf2: 'REMOTE_SET_ADDRESS_2',
    0xf3: 'REMOTE_SET_ADDRESS_3',
    0xfb: 'REMOTE_SET_SECTION',
}

CommandType = Enum("CommandType",{ COMMANDS[x] if x in COMMANDS else f"UNKNOWN_{hex(x)}" : x for x in range(0,0xff)})


    

@dataclass
class Packet:
    header: int = 0
    from_type: AddressType = 0
    to_type: AddressType = 0
    from_address: int = 0
    to_address: int = 0
    cmd: CommandType = 0
    parameters: bytes = b''
    def encode(self):
        out = bytes([
            (len(self.parameters)<<6) | self.to_type.value | self.header << 4,
            self.to_address,
            self.from_type.value,
            self.from_address,
            self.cmd.value
        ]) + self.parameters
        return out


    def from_bytes(data: bytes):
        out = Packet()
        parameter_length = data[0] >> 6

        out.header = data[0] >> 4

        out.to_type =  AddressType(data[0] & 0xf)
        out.to_address = data[1]
        
        out.from_type = AddressType(data[2] & 0xf)
        out.from_address = data[3]

        out.cmd = CommandType(data[4])

        out.parameters = data[5:5+parameter_length]

        return out
