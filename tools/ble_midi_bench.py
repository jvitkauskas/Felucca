#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Owner-operated Linux BlueZ BLE-MIDI bench. Never used by the build/tests.
Requires Python 3 + dbus-next, a desktop LE adapter, and this S2 image on FM-1.
Example: python3 tools/ble_midi_bench.py --address 62:40:8B:67:65:58 --duration 600
Central update, using the *desktop's* handle printed by btmon:
  sudo python3 tools/ble_midi_bench.py --conn-update HANDLE 6 6 0 500
The update mode sends standard HCI LE Connection Update to the desktop adapter.
"""
import argparse
import asyncio
import socket
import struct
import time
import tempfile
from pathlib import Path

MIDI='7772e5db-3868-4112-a1a9-f2669d106bf3'
SERVICE='03b80e5a-ede8-4b33-a751-6ce34ec4c700'

def packet(status,note,velocity):
    timestamp=(time.monotonic_ns()//1_000_000)&8191
    return bytes((0x80|(timestamp>>7),0x80|(timestamp&127),status,note,velocity))

def update(args):
    handle,minimum,maximum,latency,timeout=[int(v,0) for v in args.conn_update]
    if not (0<=handle<=0x0eff and 6<=minimum<=maximum<=3200 and 0<=latency<=499 and 10<=timeout<=3200 and timeout*4>(latency+1)*maximum):
        raise SystemExit('invalid Core connection parameters')
    body=struct.pack('<7H',handle,minimum,maximum,latency,timeout,0,0)
    with socket.socket(socket.AF_BLUETOOTH,socket.SOCK_RAW,socket.BTPROTO_HCI) as radio:
        radio.bind((args.index,))
        radio.send(bytes((1,0x13,0x20,len(body)))+body)
    print('Central HCI LE Connection Update sent; verify Command Status and LE Connection Update Complete in btmon.')

async def connect_retry(call,device):
    for attempt in range(3):
        try:
            await call(device,'org.bluez.Device1','Connect')
            return
        except RuntimeError as error:
            if 'AlreadyConnected' in str(error): return
            if attempt==2: raise
            print(f'Connect attempt {attempt+1} failed: {error}; retrying',flush=True)
            await asyncio.sleep(1)

async def alsa_note(port,note):
    # Standard MIDI file: quarter-note Note On/Off, 120 bpm. BlueZ's MIDI
    # plugin owns WriteValue; the ALSA sequencer port is its supported input.
    track=bytes((0,0x90,note,100,0x83,0x60,0x80,note,0,0,0xff,0x2f,0))
    midi=b'MThd'+struct.pack('>IHHH',6,0,1,480)+b'MTrk'+struct.pack('>I',len(track))+track
    with tempfile.TemporaryDirectory(prefix='felucca-midi-') as directory:
        file=Path(directory)/'note.mid'; file.write_bytes(midi)
        process=await asyncio.create_subprocess_exec('aplaymidi','-p',port,str(file))
        if await process.wait(): raise RuntimeError('aplaymidi failed; check aplaymidi -l')

async def run(args):
    from dbus_next.aio import MessageBus
    from dbus_next import BusType,Message,MessageType,Variant
    bus=await MessageBus(bus_type=BusType.SYSTEM).connect()
    async def call(path,interface,member,signature='',body=None):
        reply=await bus.call(Message(destination='org.bluez',path=path,interface=interface,member=member,signature=signature,body=body or []))
        if reply.message_type==MessageType.ERROR:
            raise RuntimeError(f'{member}: {reply.error_name}: {reply.body}')
        return reply.body
    async def objects(): return (await call('/','org.freedesktop.DBus.ObjectManager','GetManagedObjects'))[0]
    adapter=f'/org/bluez/hci{args.index}'
    await call(adapter,'org.bluez.Adapter1','SetDiscoveryFilter','a{sv}',[{'Transport':Variant('s','le')}])
    await call(adapter,'org.bluez.Adapter1','StartDiscovery')
    device=None
    for _ in range(100):
        for path,interfaces in (await objects()).items():
            props=interfaces.get('org.bluez.Device1',{})
            address=props.get('Address')
            if address and address.value.upper()==args.address.upper(): device=path
        if device: break
        await asyncio.sleep(0.1)
    await call(adapter,'org.bluez.Adapter1','StopDiscovery')
    if not device: raise RuntimeError('FM-1 was not discovered in 10 seconds')
    notifications=0
    def changed(message):
        nonlocal notifications
        if message.interface!='org.freedesktop.DBus.Properties' or message.member!='PropertiesChanged': return
        interface,props,_=message.body
        if interface=='org.bluez.GattCharacteristic1' and 'Value' in props:
            value=bytes(props['Value'].value); notifications+=1
            print(f'FM-1 -> desktop #{notifications}: {value.hex(" ")}',flush=True)
        if interface=='org.bluez.Device1' and 'Connected' in props:
            print(f'Connected={props["Connected"].value}',flush=True)
    bus.add_message_handler(changed)
    # Raw D-Bus handlers need an explicit match for PropertiesChanged signals.
    await bus.call(Message(destination='org.freedesktop.DBus',path='/org/freedesktop/DBus',interface='org.freedesktop.DBus',member='AddMatch',signature='s',body=["type='signal',interface='org.freedesktop.DBus.Properties',member='PropertiesChanged'"]))
    for cycle in range(args.cycles):
        print(f'Connect cycle {cycle+1}/{args.cycles}',flush=True)
        await connect_retry(call,device)
        characteristic=None
        for _ in range(100):
            tree=await objects()
            for path,interfaces in tree.items():
                props=interfaces.get('org.bluez.GattCharacteristic1',{})
                if path.startswith(device+'/') and props.get('UUID') and props['UUID'].value.lower()==MIDI:
                    service=interfaces['org.bluez.GattCharacteristic1']['Service'].value
                    if tree[service]['org.bluez.GattService1']['UUID'].value.lower()==SERVICE: characteristic=path
            if characteristic: break
            await asyncio.sleep(0.1)
        if not characteristic: raise RuntimeError('BLE-MIDI service/characteristic not resolved in 10 seconds')
        value=(await call(characteristic,'org.bluez.GattCharacteristic1','ReadValue','a{sv}',[{}]))[0]
        if value: raise RuntimeError('MIDI read should be empty')
        await call(characteristic,'org.bluez.GattCharacteristic1','StartNotify')
        print('MIDI UUID discovered; empty read passed; notifications enabled. Play FM-1 keys.',flush=True)
        deadline=time.monotonic()+args.duration
        while time.monotonic()<deadline:
            if args.send_notes:
                if not args.alsa_port: raise RuntimeError('--send-notes requires --alsa-port CLIENT:PORT; find it with aplaymidi -l')
                await alsa_note(args.alsa_port,args.note)
            await asyncio.sleep(0.85 if args.send_notes else 1)
        await call(device,'org.bluez.Device1','Disconnect')
        await asyncio.sleep(0.5)
    print(f'Completed {args.cycles} cycle(s); notifications received: {notifications}')
    bus.disconnect()

def main():
    parser=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--address',default='62:40:8B:67:65:58')
    parser.add_argument('--index',type=int,default=0)
    parser.add_argument('--duration',type=float,default=30)
    parser.add_argument('--cycles',type=int,default=1)
    parser.add_argument('--send-notes',action='store_true')
    parser.add_argument('--note',type=int,default=60)
    parser.add_argument('--alsa-port',help='BlueZ MIDI ALSA sequencer CLIENT:PORT (aplaymidi -l)')
    parser.add_argument('--conn-update',nargs=5,metavar=('HANDLE','MIN','MAX','LATENCY','TIMEOUT'))
    args=parser.parse_args()
    if args.conn_update: update(args)
    else: asyncio.run(run(args))
if __name__=='__main__': main()
