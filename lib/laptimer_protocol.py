import json
import threading
import socket
import logging

class laptimer():
    _timeout = 5
    NUMMESSAGETYPES = 25
    NUMLASTLAPTIMES = 10 # Number of laps used in MessageTimesLapped
    DEFAULTHALLOFFAMELIMIT = 200 # Number of laps used in MessageHallOfFame*
    MAXLIMITRESULTSETS = 2000 # Hard number applied to all hall of fame queries

    def __init__(self, _ip, _port):
        self.ip = _ip
        self.port = _port
    '''
    @staticmethod
    def GPSMessageType():
        MessageCurrentPosition = 0 # Client > Server
        MessageTimeLapped = 1 # Deprecated, Client > Server
        MessageTimeLappedCertified = 9 # Deprecated, Client > Server, replaced MessageTimeLapped
        MessageTimeLappedCertifiedV2 = 12 # Client > Server, replaced MessageTimeLappedCertified
        MessageDeleteTimeLapped = 24 # Client > Server

        MessageRegisterForTrack = 2 # Client > Server
        MessagePositions = 3 # Server > Client (continous replies to MessageRegisterForTrack)
        MessageTimesLapped = 4 # Server > Client (continous replies to MessageRegisterForTrack)
        MessageServerStatus = 5 # Server > Client (continous replies to MessageRegisterForTrack)
        MessageAlertOnTrack = 6 # Client > Server (submitting alert) AND
        # Server > Client (broadcast by server on receive)

        MessageRequestHallOfFame = 7 # Deprecated, Client > Server
        MessageHallOfFame = 8 # Deprecated, Server > Client (reply to MessageRequestHallOfFame)
        MessageRequestHallOfFameCertified = 10 # deprecated, Client > Server, replaced MessageRequestHallOfFame
        MessageHallOfFameCertified = 11 # Server > Client
        MessageRequestHallOfFameCertifiedV2 = 23 # Client > Server, replaced MessageRequestHallOfFameCertified

        MessageRequestTracks = 13 # Client > Server
        MessageTracks = 14 # Server > Client (reply to MessageRequestTracks)

        MessageRequestTrackShape = 15 # Client > Server
        MessageTrackShape = 16 # Server > Client (reply to MessageRequestTrackShape)

        MessageSubmitChallenge = 17 # Client > Server

        MessageRequestChallenges = 18 # Client > Server
        MessageChallenges = 19 # Server > Client (reply to MessageRequestChallenges)

        MessageRequestChallenge = 20 # Client > Server
        MessageChallenge = 21 # Server > Client (reply to MessageRequestCallenge)
        MessageDeleteChallenge = 22 # Client > Server

    @staticmethod
    def Coordinate2D():
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
    def TrackType():
        numDrivers: int = 0
        trackID: int = 0
        position = Coordinate2D()
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
    '''