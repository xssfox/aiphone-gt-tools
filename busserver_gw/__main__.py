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
from threading import Thread, Lock

mutex = Lock()

from . import Packet, MqttClient, AddressType, CommandType
logging.basicConfig()
logging.getLogger().setLevel(logging.INFO)

parser = argparse.ArgumentParser(
                    prog='busserver-gw',
                    description='Interfaces aiphone busserver with mqtt. Note currently everything is sent as Guard 2')

parser.add_argument('-b','--busserver-host', type=str, help="busserver (intercom) address", required=True)
parser.add_argument('-m','--mqtt-host', type=str, help="MQTT Host", required=True)
parser.add_argument('-o','--mqtt-port', type=int, help="MQTT Port", default=1883)
parser.add_argument('-u','--mqtt-username', type=str, help="MQTT Username", required=True)
parser.add_argument('-p','--mqtt-password', type=str, help="MQTT Password", required=True)
parser.add_argument('-a','--resident-address', type=int, help="Resident Address", required=True)
parser.add_argument('-e','--entrance', nargs="+", type=int, help="Simple same bus entrance IDs 1-15", default=[])
parser.add_argument('-l','--lifts', nargs="+", type=int, help="Lift Control IDs", default=[])
parser.add_argument('-r','--remote-entrance', nargs="+", type=str, default=[], help="Remote entrances [section:entranceid] eg: 25:1. Generally only shared zones will work (25-31)")
parser.add_argument('-v','--verbose', help="MQTT Host", action="store_true")


args = parser.parse_args()

if args.verbose == True:
    logging.getLogger().setLevel(logging.DEBUG)

tcp_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
tcp_client = tcp_socket.connect((args.busserver_host, 4444))
tcp_socket.settimeout(0.3)

MAX_SEND_RETRIES=5
MAX_RX_RETRIES=4

def send_aiphone_packet(packet: Packet):
    for x in range(MAX_SEND_RETRIES):
        time.sleep(0.1)
        logging.debug(f"Sending: {str(packet)}")
        tcp_socket.send(packet.encode())
        rx_packet = None
        for y in range(MAX_RX_RETRIES):
            rx_packet = recv_aiphone_packet()
            if rx_packet:
                rx_packet.header=0
                packet.header=0
            if rx_packet and packet == rx_packet:
                return
        logging.error(f"Failed to detect send. Attempt: {x+1}/{MAX_SEND_RETRIES}")
    raise TimeoutError("Failed to send onto bus")


def recv_aiphone_packet():
    try:
        message = tcp_socket.recv(24)
        logging.debug(message.hex())
        
        if len(message) == 24: # TODO WE CAN CHECK FOR ACKS HERE
            message = message[13:]
            try:
                packet = Packet.from_bytes(message)
                if packet.to_address == args.resident_address and packet.to_type == AddressType.RESIDENT:
                    logging.info(packet)
                else:
                    logging.debug(packet)
                return packet
            except:
                logging.error(f"Error parsing packet: {message.hex()}")
    except TimeoutError:
        pass
        

def unlock_entrance(entrance_id: int):
    with mutex:
        logging.info(f"unlock entrance: {entrance_id}")
        packets = [
            Packet(cmd=CommandType.LINE_REQUEST_2, to_type=AddressType.RESIDENT, to_address=0xf, from_type=AddressType.BROADCAST, from_address=1, header=3),
            Packet(cmd=CommandType.REQ_TALK, to_type=AddressType.ENTRANCE, to_address=entrance_id, from_type=AddressType.BROADCAST, from_address=1),
        ]
        for packet in packets:
            send_aiphone_packet(packet)
        time.sleep(1)

        packets = [
            Packet(cmd=CommandType.UNLOCK_PRESS, to_type=AddressType.ENTRANCE, to_address=entrance_id, from_type=AddressType.BROADCAST, from_address=1),
            Packet(cmd=CommandType.UNLOCK_RELEASE, to_type=AddressType.ENTRANCE, to_address=entrance_id, from_type=AddressType.BROADCAST, from_address=1),
        ]
        for packet in packets:
            send_aiphone_packet(packet)
            time.sleep(0.2)
        time.sleep(0.5)
        
        packets = [
            Packet(cmd=CommandType.END_CALL_1, to_type=AddressType.ENTRANCE, to_address=entrance_id, from_type=AddressType.BROADCAST, from_address=1),
            Packet(cmd=CommandType.HANG_UP, to_type=AddressType.RESIDENT, to_address=0xf, from_type=AddressType.BROADCAST, from_address=1, header=3),
        ]
        for packet in packets:
            send_aiphone_packet(packet)

def trigger_lift_control(lift_id: int):
    with mutex:
        packet = Packet(
            to_type=AddressType.BROADCAST, 
            to_address=1, 
            from_type=AddressType.ENTRANCE, 
            from_address=1, # we pretend to be the gateway here - I don't think it matters
            cmd=CommandType.LIFT_CONTROL,
            parameters=lift_id.to_bytes(length=2,byteorder='big')
        )
        send_aiphone_packet(packet)

def unlock_remote_entrance(section: int,entrance_id: int):
    with mutex:
        logging.debug(f"unlock remote entrance: {section} {entrance_id}")
        for y in range(4):
            packets = [
                Packet(cmd=CommandType.REMOTE_SET_SECTION, parameters=bytes([section]), to_type=AddressType.ENTRANCE, to_address=1, from_type=AddressType.SECURITY, from_address=2),
                Packet(cmd=CommandType.REMOTE_SET_ADDRESS_1, parameters=";;".encode(), to_type=AddressType.ENTRANCE, to_address=1, from_type=AddressType.SECURITY, from_address=2),
                Packet(cmd=CommandType.REMOTE_SET_ADDRESS_2, parameters=";;".encode(), to_type=AddressType.ENTRANCE, to_address=1, from_type=AddressType.SECURITY, from_address=2),
                Packet(cmd=CommandType.REMOTE_SET_ADDRESS_3, parameters=f";{entrance_id}".encode(), to_type=AddressType.ENTRANCE, to_address=1, from_type=AddressType.SECURITY, from_address=2),
            ]
            for packet in packets:
                send_aiphone_packet(packet)
            
            rx_expected= False
            for x in range(6): # clear out rx buffer
                rx_packet = recv_aiphone_packet()
                expecting = Packet(header=3, from_type=AddressType.ENTRANCE, to_type=AddressType.RESIDENT, from_address=1, to_address=15, cmd=CommandType.LINE_REQUEST, parameters=b'')
                if rx_packet == expecting:
                    rx_expected = True
            if rx_expected:
                break
            else:
                logging.error(f"Didn't receive remote line request - trying again attempt : {y+1}")
        else:
            logging.error("line request failure giving up")
            return
        time.sleep(1)

        packets = [
            Packet(cmd=CommandType.UNLOCK_PRESS, to_type=AddressType.ENTRANCE, to_address=1, from_type=AddressType.SECURITY, from_address=2),
            Packet(cmd=CommandType.UNLOCK_RELEASE, to_type=AddressType.ENTRANCE, to_address=1, from_type=AddressType.SECURITY, from_address=2),
        ]
        for packet in packets:
            send_aiphone_packet(packet)
            time.sleep(0.2)
        time.sleep(0.5)
        
        packets = [
            Packet(cmd=CommandType.END_CALL_1, to_type=AddressType.ENTRANCE, to_address=1, from_type=AddressType.SECURITY, from_address=2),
        ]
        for packet in packets:
            send_aiphone_packet(packet)

last_seen_entrance = 0

def unlock():
    with mutex:
        logging.info(f"unlock in progress: {last_seen_entrance}")
        packets = [
            Packet(cmd=CommandType.UNLOCK_PRESS, to_type=AddressType.ENTRANCE, to_address=last_seen_entrance, from_type=AddressType.RESIDENT, from_address=args.resident_address),
            Packet(cmd=CommandType.UNLOCK_RELEASE, to_type=AddressType.ENTRANCE, to_address=last_seen_entrance, from_type=AddressType.RESIDENT, from_address=args.resident_address),
        ]
        for packet in packets:
            send_aiphone_packet(packet)
        
    


mqtt = MqttClient(
    args.mqtt_host,
    args.mqtt_port,
    args.mqtt_username,
    args.mqtt_password,
    unlock_entrance,
    trigger_lift_control, 
    unlock_remote_entrance, 
    unlock,
    args.resident_address,
    args.entrance,
    args.lifts,
    args.remote_entrance
    )

CALL_TYPES = [
    CommandType.INCOMING_CALL_RING_NO_CAM,
    CommandType.INCOMING_CALL_RING_CAM,
    CommandType.INCOMING_CALL_RING_NO_CAM_2,
    CommandType.INCOMING_CALL_RING_CAM_2,
    CommandType.INCOMING_CALL_RING_EMERGENCY

]

while True:
    try:
        with mutex:
            packet = recv_aiphone_packet()


        if packet:
                
            if packet.cmd in [
                CommandType.LINE_REQUEST,
                CommandType.START_CALL_1, 
                CommandType.START_CALL_2, 
                CommandType.LINE_REQUEST_3, 
                CommandType.LINE_REQUEST_2
            ]:
                mqtt.send_update(True)
            if packet.cmd == CommandType.HANG_UP:
                mqtt.send_update(False)
            
            if packet.cmd in CALL_TYPES:
                if packet.to_address == args.resident_address and packet.to_type == AddressType.RESIDENT:
                    logging.info("Detected incoming call")
                    mqtt.send_ring(
                        packet.from_type.name,
                        packet.from_address, 
                        packet.to_type.name, 
                        packet.to_address
                    )
                    last_seen_entrance = packet.from_address 


    except TimeoutError:
        pass
    