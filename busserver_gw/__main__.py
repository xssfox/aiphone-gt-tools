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

server = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
server.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
server.bind(('', 23569)) # This should be 4444 but there's a bug in busserver....
server.settimeout(0.01)


def unlock_entrance(entrance_id: int):
    logging.info(f"unlock entrance: {entrance_id}")
    packets = [
        Packet(cmd=CommandType.LINE_REQUEST_2, to_type=AddressType.RESIDENT, to_address=0xf, from_type=AddressType.BROADCAST, from_address=1, header=3),
        Packet(cmd=CommandType.REQ_TALK, to_type=AddressType.ENTRANCE, to_address=entrance_id, from_type=AddressType.BROADCAST, from_address=1),
    ]
    for packet in packets:
        server.sendto(packet.encode(),(args.busserver_host, 4444))
        time.sleep(0.2)
    time.sleep(1)

    packets = [
        Packet(cmd=CommandType.UNLOCK_PRESS, to_type=AddressType.ENTRANCE, to_address=entrance_id, from_type=AddressType.BROADCAST, from_address=1),
        Packet(cmd=CommandType.UNLOCK_RELEASE, to_type=AddressType.ENTRANCE, to_address=entrance_id, from_type=AddressType.BROADCAST, from_address=1),
    ]
    for packet in packets:
        server.sendto(packet.encode(),(args.busserver_host, 4444))
        time.sleep(0.3)
    time.sleep(0.5)
    
    packets = [
        Packet(cmd=CommandType.END_CALL_1, to_type=AddressType.ENTRANCE, to_address=entrance_id, from_type=AddressType.BROADCAST, from_address=1),
        Packet(cmd=CommandType.HANG_UP, to_type=AddressType.RESIDENT, to_address=0xf, from_type=AddressType.BROADCAST, from_address=1, header=3),
    ]
    for packet in packets:
        server.sendto(packet.encode(),(args.busserver_host, 4444))
        time.sleep(0.2)
def trigger_lift_control(lift_id: int):
    packet = Packet(
        to_type=AddressType.BROADCAST, 
        to_address=1, 
        from_type=AddressType.ENTRANCE, 
        from_address=1, # we pretend to be the gateway here - I don't think it matters
        cmd=CommandType.LIFT_CONTROL,
        parameters=lift_id.to_bytes(length=2,byteorder='big')
    ).encode()
    server.sendto(packet,(args.busserver_host, 4444))

def unlock_remote_entrance(section: int,entrance_id: int):
    logging.debug(f"unlock remote entrance: {section} {entrance_id}")
    packets = [
        Packet(cmd=CommandType.REMOTE_SET_SECTION, parameters=bytes([section]), to_type=AddressType.ENTRANCE, to_address=1, from_type=AddressType.SECURITY, from_address=2),
        Packet(cmd=CommandType.REMOTE_SET_ADDRESS_1, parameters=";;".encode(), to_type=AddressType.ENTRANCE, to_address=1, from_type=AddressType.SECURITY, from_address=2),
        Packet(cmd=CommandType.REMOTE_SET_ADDRESS_2, parameters=";;".encode(), to_type=AddressType.ENTRANCE, to_address=1, from_type=AddressType.SECURITY, from_address=2),
        Packet(cmd=CommandType.REMOTE_SET_ADDRESS_3, parameters=f";{entrance_id}".encode(), to_type=AddressType.ENTRANCE, to_address=1, from_type=AddressType.SECURITY, from_address=2),
    ]
    for packet in packets:
        server.sendto(packet.encode(),(args.busserver_host, 4444))
        time.sleep(0.2)
    time.sleep(0.7)

    packets = [
        Packet(cmd=CommandType.UNLOCK_PRESS, to_type=AddressType.ENTRANCE, to_address=1, from_type=AddressType.SECURITY, from_address=2),
        Packet(cmd=CommandType.UNLOCK_RELEASE, to_type=AddressType.ENTRANCE, to_address=1, from_type=AddressType.SECURITY, from_address=2),
    ]
    for packet in packets:
        server.sendto(packet.encode(),(args.busserver_host, 4444))
        time.sleep(0.2)
    time.sleep(0.5)
    
    packets = [
        Packet(cmd=CommandType.END_CALL_1, to_type=AddressType.ENTRANCE, to_address=1, from_type=AddressType.SECURITY, from_address=2),
    ]
    for packet in packets:
        server.sendto(packet.encode(),(args.busserver_host, 4444))
        time.sleep(0.2)

last_seen_entrance = 0

def unlock():
    logging.info(f"unlock in progress: {last_seen_entrance}")
    packets = [
        Packet(cmd=CommandType.UNLOCK_PRESS, to_type=AddressType.ENTRANCE, to_address=last_seen_entrance, from_type=AddressType.RESIDENT, from_address=args.resident_address),
        Packet(cmd=CommandType.UNLOCK_RELEASE, to_type=AddressType.ENTRANCE, to_address=last_seen_entrance, from_type=AddressType.RESIDENT, from_address=args.resident_address),
    ]
    for packet in packets:
        server.sendto(packet.encode(),(args.busserver_host, 4444))
        time.sleep(0.2)
    
    


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
        message, address = server.recvfrom(24)
        if len(message) == 24: # TODO WE CAN CHECK FOR ACKS HERE
            message = message[13:]
        logging.debug(message.hex())
        try:
            packet = Packet.from_bytes(message)
        except:
            logging.error(f"Error parsing packet: {message.hex()}")
            continue

                
        if packet.to_address == args.resident_address and packet.to_type == AddressType.RESIDENT:
            logging.info(packet)
        else:
            logging.debug(packet)
            
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
    