import socket
import multiprocessing
import json
import logging
import getpass
import argparse
import datetime
import os
import threading
import socket
import sys

#+--------------------------------+
#| Required external libraries    |
#+--------------------------------+
from lib.laptimer_protocol import laptimer
from lib.sql_handler import postgresql_connection


#+--------------------------------------------------------------+
#| Setup logging                                                |
#+--------------------------------------------------------------+
os.makedirs(".\\logs", exist_ok=True)
cur_date = (datetime.datetime.now()).strftime('%Y%m%d-%H%M%S')
logging.basicConfig(level=logging.DEBUG,
                    filename='.\\logs\\{0}-hlts_debug.log'.format(cur_date),
                    filemode='w',
                    format='%(asctime)s [%(levelname)s / %(name)s / %(threadName)s]: %(message)s',
                    datefmt='%Y-%m-%d %H:%M:%S')
console = logging.StreamHandler()
console.setLevel(logging.INFO)
formatter = logging.Formatter('%(asctime)s [%(levelname)s]: %(message)s', datefmt='%Y-%m-%d %H:%M:%S')
console.setFormatter(formatter)
logging.getLogger('').addHandler(console)

parser = argparse.ArgumentParser()
parser.add_argument('-v', '--verbose', help='Increase application verbosity', action='store_true')
args = parser.parse_args()
if args.verbose:
    console.setLevel(logging.DEBUG)

#+--------------------------------------------------------------+
#| Client Connection Threads                                    |
#+--------------------------------------------------------------+
class ClientThread(threading.Thread):
    def __init__(self,clientAddress,clientsocket):
        self.ip = clientAddress
        threading.Thread.__init__(self)
        self.csocket = clientsocket
        print ("New connection added: {0}:{1}".format(self.ip[0], self.ip[1]))
    def run(self):
        logging.info("Connection from : {0}:{1}".format(self.ip[0], self.ip[1]))
        #self.csocket.send(bytes("Harry's Laptimer Server - V0.1\r\n",'utf-8'))
        self.client = laptimer(self.ip[0], self.ip[1])
        msg = ''
        while True:
            data = self.csocket.recv(2048)
            if data != b'':
                creatorID = data[0:3]
                sUID = data[4:7]
                msgsize = data[8:11]
                msgType = data[12:15]
                try:
                    uDID = data[16:31]
                    header_type = 'v2'
                    msgBody = data[32:]
                    logging.debug(f'Creator ID: {creatorID} sUID: {sUID} size: {msgsize} msgType: {msgType} uDID: {uDID}')
                except:
                    header_type = 'v1'
                    msgBody = data[16:]
                    logging.debug(f'Creator ID: {creatorID} sUID: {sUID} size: {msgsize} msgType: {msgType}')
                logging.debug(f'Header Type: {header_type}')
            msg = msgBody.decode('utf-8')
            if msg != '':
                logging.debug(msg)
            #if str(msg) == 'exit':
            #    break
            #else:
            #    print("From client: {0}".format(msg))
                #encoded_msg = bytes("From Server: " + msg, 'UTF-8')
            #self.csocket.send(encoded_msg)
        logging.info("Client at {0} disconnected".format(self.ip[0]))

#+--------------------------------------------------------------+
#| ASCII Art                                                    |
#+--------------------------------------------------------------+

def ascii_art():
    _version = '0.1'
    _author = 'fuzzychicken'
    _logging_dir = os.getcwd() + '\\logs\\{0}-hlts_debug.log'.format(cur_date)
    art = r'''
  _   _                       _       _                _   _                       ____                           
 | | | | __ _ _ __ _ __ _   _( )___  | |    __ _ _ __ | |_(_)_ __ ___   ___ _ __  / ___|  ___ _ ____   _____ _ __ 
 | |_| |/ _` | '__| '__| | | |// __| | |   / _` | '_ \| __| | '_ ` _ \ / _ \ '__| \___ \ / _ \ '__\ \ / / _ \ '__|
 |  _  | (_| | |  | |  | |_| | \__ \ | |__| (_| | |_) | |_| | | | | | |  __/ |     ___) |  __/ |   \ V /  __/ |   
 |_| |_|\__,_|_|  |_|   \__, | |___/ |_____\__,_| .__/ \__|_|_| |_| |_|\___|_|    |____/ \___|_|    \_/ \___|_|   
                        |___/                   |_|                                                               

Version: {0}
Author: {1}
Logging to: {2}

'''.format(_version, _author, _logging_dir)
    print(art)

#+--------------------------------------------------------------+
#| Main Script                                                  |
#+--------------------------------------------------------------+
def main():
    ascii_art()
    logging.info('Loading')
    try:
        with open('server_config.json', 'r') as fp:
            content = json.load(fp)
        _host = content['server']['host']
        _port = content['server']['port']
        _postgresqlhost = content['postgresql']['postgresqlhost']
        _postgresqlport = content['postgresql']['postgresqlport']
        _username = content['postgresql']['username']
        _password = content['postgresql']['password']
        while True:
            user_input = input('To start the server [ENTER] or press [Q] and then press [ENTER] to quit: ')
            if user_input == "":
                newthread = postgresql_connection(_postgresqlhost, _postgresqlport, _username, _password)
                newthread.daemon = True
                newthread.start()
                server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                server.bind((_host, int(_port)))
                logging.info('Server started')
                logging.info('Waiting for client request')
                while True:
                    server.listen(1)
                    clientsocket, clientAddress = server.accept()
                    newthread = ClientThread(clientAddress, clientsocket)
                    newthread.daemon = True
                    newthread.start()
            elif user_input == 'q':
                logging.info('You have selected to quit. Exiting')
                sys.exit(0)
            else:
                logging.warning('Invalid input!')
    except BaseException as e:
        logging.error(e)

if __name__ == "__main__":
    main()