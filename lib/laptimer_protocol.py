# Native Libraries
import json
import threading
import socket
import logging
import struct
import codecs
import binascii
import array

# Third Party Libraries
import numpy as np

class message_handler():
    _timeout = 5
    NUMMESSAGETYPES = 25
    NUMLASTLAPTIMES = 10 # Number of laps used in MessageTimesLapped
    DEFAULTHALLOFFAMELIMIT = 200 # Number of laps used in MessageHallOfFame*
    MAXLIMITRESULTSETS = 2000 # Hard number applied to all hall of fame queries

    def __init__(self, msg):
        self.data = binascii.hexlify(bytearray(msg))
        try:
            self.headers = struct.unpack("> 8s  8s  8s  8s 32s 16s 16s 8s", self.data[0:128])
            self.msg_length = len(self.data) - 128
            logging.debug(f'Message Length: {self.msg_length}')
            self.msg = self.data[self.msg_length:]
            print(f'Message: {self.msg}')
            self.header_type = 'v2'
        except:
            self.headers = struct.unpack(f"> 8s  8s  8s  8s", self.data[0:32])
            self.msg_length = len(self.data) - 32
            logging.debug(f'Message Length: {self.msg_length}')
            self.msg = self.data[self.msg_length:]
            print(f'Message: {self.msg}')
            self.header_type = 'v1'

        logging.debug(f'Headers: {self.headers}')
        self.handleMessage()

    def handleMessage (self):
        self.creatorID = codecs.decode(self.swapEndianness(self.headers[0]), "hex").decode('utf-8')
        self.sUID = self.swapEndianness(self.headers[1]).decode()
        self.msgsize =  np.int32(self.swapEndianness(self.headers[2]))
        self.msgType =  np.int32(self.swapEndianness(self.headers[3]))

        if self.header_type == 'v2':
            self.uDID = np.int32(self.swapEndianness(self.headers[4]))
            print(f'Creator ID: {self.creatorID} sUID: {self.sUID} size: {self.msgsize} msgType: {self.msgType} uDID: {self.uDID}')
            logging.debug(f'Creator ID: {self.creatorID} sUID: {self.sUID} size: {self.msgsize} msgType: {self.msgType} uDID: {self.uDID}')

        elif self.header_type == 'v1':
            print(f'Creator ID: {self.creatorID} sUID: {self.sUID} size: {self.msgsize} msgType: {self.msgType}')
            logging.debug(f'Creator ID: {self.creatorID} sUID: {self.sUID} size: {self.msgsize} msgType: {self.msgType}')

        self.GPSMessageType()

        #self.msg = self.msgBody
        #if self.msg != '':
            #logging.debug(self.msg)

    def swapEndianness(self, swap_data):
        if isinstance(swap_data, (bytes, bytearray)):
            byteswapped = bytearray(len(swap_data))
            byteswapped[0::8] = swap_data[6::8]
            byteswapped[1::8] = swap_data[7::8]
            byteswapped[2::8] = swap_data[4::8]
            byteswapped[3::8] = swap_data[5::8]
            byteswapped[4::8] = swap_data[2::8]
            byteswapped[5::8] = swap_data[3::8]
            byteswapped[6::8] = swap_data[0::8]
            byteswapped[7::8] = swap_data[1::8]

            return bytes(byteswapped)
        else:
            print('Data is not in bytes or bytearray format!')
            return None

    def closeClient(self):
        print('Closing connection')
        exit(0)

    def GPSMessageType(self):
        switcher = {
            0: "MessageCurrentPosition",
            1: "MessageTimeLapped",
            2: "MessageRegisterForTrack",
            3: "MessagePositions",
            4: "MessageTimesLapped",
            5: "MessageServerStatus",
            6: "MessageAlertOnTrack",
            7: "MessageRequestHallOfFame",
            8: "MessageHallOfFame",
            9: "MessageTimeLappedCertified",
            10: "MessageRequestHallOfFameCertified",
            11: "MessageHallOfFameCertified",
            12: "MessageTimeLappedCertifiedV2",
            13: "MessageRequestTracks",
            14: "MessageTracks",
            15: "MessageRequestTrackShape",
            16: "MessageTrackShape",
            17: "MessageSubmitChallenge",
            18: "MessageRequestChallenges",
            19: "MessageChallenges",
            20: "MessageRequestChallenge",
            21: "MessageChallenge",
            22: "MessageDeleteChallenge",
            23: "MessageRequestHallOfFameCertifiedV2",
            24: "MessageDeleteTimeLapped"
        }
        print(f'Parsing message using: {switcher[self.msgType]}')
        return switcher.get(self.msgType, lambda: "Invalid Message Type")

    def MessageRegisterForTrack(self):
        self.trackID: int = self.swapEndianness(self.msg)
        print(f'User: {self.sUID} TrackID: {self.trackID}')

    @staticmethod
    def Coordinate2D(self):
        latitude: float = 0.0
        longitude: float = 0.0

    @staticmethod
    def DriverPositionType():
        driverId: chr = "" #[DRIVERIDLENGTH]
        UDID: int = 0
        latitude: float = 0.0
        longitude: float = 0.0
    
    @staticmethod
    def DriverLapTimeType():
        driverId: chr = "" #[DRIVERIDLENGTH]
        UDID: int = 0
        lapTime100: int = 0

    @staticmethod
    def DriverLapTimeDatedType():
        driverId: chr = "" #[DRIVERIDLENGTH]
        UDID: int = 0
        lapTime100: int = 0
        seconds: int = 0
        marshaledVehicle: chr = "" # Variable size, vehicle has at least 1 byte!
    
    @staticmethod
    def DriverLapTimeDatedCertifiedType():
        driverId: chr = "" #[DRIVERIDLENGTH]
        UDID: int = 0
        lapTime100: int = 0
        seconds: int = 0
        overallDistance10: int = 0
        marshaledVehicle: chr = "" # Variable size, vehicle has at least 1 byte!
    
    @staticmethod
    def TrackType(self):
        numDrivers: int = 0
        trackID: int = 0
        position = self.Coordinate2D()
        hasShape: bool = False
        trackname: chr = "" # Variable size, trackname hat at least 1 byte!
    
    @staticmethod
    def ChallengeDescriptionType():
        trackID: int = 0
        numDownloads: int = 0
        lapTime100: int = 0
        submitterUDID: int = 0
        challengeCode: int = 0
        listed: bool = False
        fullnameAndVehicle: chr = "" # Variable size, fullnameAndVehicle are two zero
        # terminated c strings (both UTF8)
