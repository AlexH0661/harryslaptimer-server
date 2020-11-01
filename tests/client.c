#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

#include <strings.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>

#include <time.h>

#include <sqlite3.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <uuid/uuid.h>

#include <pthread.h>

/**********************************************************************
 *
 *	DriverClient implementation creating a driver, contacting a server
 *	and sending several laps of data. Message sequence is 
 *
 *	MessageUserCredentials (for driver)
 *	MessageUserGroupMembership (for LapTimer group and current track)
 *
 *	MAXLAPS *
 *		{
 *			n * MessageCurrentPositionV2 (depends on track length and rate)
 *			MessageTimeLappedCertifiedV3
 *		}
 *
 *	The implementation is based on two threads:
 *
 *	Messenger (producer): thread generates the message sequence (see above)
 *	Sender (consumer): maintains connection and sends messages passed in
 *
 *	See http://cis.poly.edu/cs3224a/Code/ProducerConsumerUsingPthreads.c
 *
 **********************************************************************/

/**********************************************************************
 *
 *	Static configuration
 *
 **********************************************************************/

//#define MILLIS		200 //  5 Hz
#define MILLIS			1000 //  1 Hz
#define MAXLAPS			1

/**********************************************************************
 *
 *	Data structures and defines copied from master source code base
 *
 **********************************************************************/

//	Add missing types and defines
typedef unsigned short							UInt16;
typedef unsigned int							UInt32;

#define DRIVERIDLENGTH							3
#define UDIDSIZE								(8+1+4+1+4+1+4+1+12+1)
#define NOTCERTIFIEDTRACKID						0
#define LAPTIMERGROUPID							4000

#define LAPTIMERPROFESSIONALSKU					60
#define LAPTIMERPETROLHEADGRANDPRIXSKU			(LAPTIMERPROFESSIONALSKU+200)

//	Copied from model/PositionSets.h
typedef
enum
{
	PositionSetStatusUnknown = 0,							//	Initial and unknown status
	PositionSetStatusActive = 1,							//	User / track is visible currently (semantics: visible able to see others)
	PositionSetStatusSleeping = 2,							//	User / track is invisible temporarily
	PositionSetStatusDeleted = 3,							//	User deleted and handled like not existant (used on server side only)
	PositionSetStatusTemporary = 4							//	Used to mark groups loaded just to be shown as available

}	PositionSetStatusType;

#define ISTRACKID(TRACKID)			((TRACKID)>=1000&&(TRACKID)<3000)	//	See SKUS AND OTHER IDS.txt for more information

//	Copied from model/GPSMessagePrimitives.h
typedef
enum
{
	MessageCurrentPosition = 0,					//	deprecated, Client > Server
	MessageCurrentPositionV2 = 44,				//	Client > Server, replaced MessageCurrentPosition

	MessageTimeLapped = 1,						//	deprecated, Client > Server
	MessageTimeLappedCertified = 9,				//	deprecated, Client > Server, replaced MessageTimeLapped
	MessageTimeLappedCertifiedV2 = 12,			//	Client > Server, replaced MessageTimeLappedCertified
	MessageTimeLappedCertifiedV3 = 35,			//	Client > Server, replaced MessageTimeLappedCertifiedV2
	MessageDeleteTimeLapped = 24,				//	deprecated, Client > Server

	MessageRegisterForTrack = 2,				//	deprecated, Client > Server
	MessageRegisterGroupsAndTracks = 45,		//	Client > Server, replaced MessageRegisterForTrack
	MessagePositions = 3,						//	Server > Client (continous replies to MessageRegisterForTrack)
	MessagePositionsV2 = 46,					//	Server > Client (continous replies to MessageRegisterGroupsAndTracks)
	MessageTimesLapped = 4,						//	Server > Client (continous replies to MessageRegisterForTrack)
	MessageServerStatus = 5,					//	Server > Client (continous replies to MessageRegisterForTrack)

	MessageAlertOnTrack = 6,					//	Client > Server (submitting alert) AND Server > Client (broadcast by server on receive)

	MessageRequestHallOfFame = 7,				//	deprecated, Client > Server
	MessageHallOfFame = 8,						//	deprecated, Server > Client (reply to MessageRequestHallOfFame)
	MessageRequestHallOfFameCertified = 10,		//	deprecated, Client > Server, replaced MessageRequestHallOfFame
	MessageHallOfFameCertified = 11,			//	Server > Client
	MessageRequestHallOfFameCertifiedV2 = 23,	//	Client > Server, replaced MessageRequestHallOfFameCertified

	MessageRequestTracks = 13,					//	Client > Server
	MessageTracks = 14,							//	Server > Client (reply to MessageRequestTracks)

	MessageRequestTrackShape = 15,				//	Client > Server
	MessageTrackShape = 16,						//	Server > Client (reply to MessageRequestTrackShape)

	MessageSubmitChallenge = 17,				//	deprecated, Client > Server
	MessageSubmitChallengeV2 = 36,				//	Client > Server, replaced MessageSubmitChallenge

	MessageRequestChallenges = 18,				//	Client > Server
	MessageChallenges = 19,						//	Server > Client (reply to MessageRequestChallenges)

	MessageRequestChallenge = 20,				//	Client > Server
	MessageChallenge = 21,						//	Server > Client (reply to MessageRequestCallenge)
	MessageDeleteChallenge = 22,				//	Client > Server

	MessageSubmitVehicleCertification = 30,		//	deprecared, Client > Server
	MessageSubmitVehicleCertificationV2 = 33,	//	Client > Server, replaced MessageSubmitVehicleCertification
	MessageVehicleCertification = 31,			//	Server > Client (reply to MessageSubmitVehicleCertification)

	MessageRegisterDevice = 25,					//	deprecated, Client > Server
	MessageRegisterDeviceV2 = 34,				//	Client > Server, replaced MessageRegisterDevice
	MessageNotificationRead = 26,				//	Client > Server
	MessageReadNotification = 27,				//	Client > Server
	MessageNotification = 28,					//	Server > Client (reply to MessageReadNotification)
	MessageAnyNotification = 29,				//	Server > Client (reply to MessageRegisterDevice)

	MessageRequestCertifiedUDID = 32,			//	Client > Server
	MessageCertifiedUDID = 37,					//	Server > Client (reply to MessageRequestCertifiedUDID)

	MessageUserCredentials = 38,				//	Client > Server
	MessageUserGroupMembership = 39,			//	Client > Server

	MessageRequestGroupDetails = 40,			//	Client > Server
	MessageGroupDetails = 41,					//	Server > Client (reply to MessageRequestGroupDetails)

	MessageRequestGroupList = 42,				//	Client > Server
	MessageGroupList = 43						//	Server > Client (reply to MessageRequestGroupList)

} GPSMessageType;

#pragma pack(push,2)

typedef unsigned long long              UInt64;

//	Copied from utility/UUID.h
typedef
struct
{
	UInt64	mostSigBits;
	UInt64	leastSigBits;

}	UUID128;

#define UUID128IsNull(U)	((U).mostSigBits==0&&(U).leastSigBits==0)
#define UUID128Null			((UUID128) { 0, 0 })

//	Copied from model/GPSMessagePrimitives.h
typedef
struct
{
	char			driverId [DRIVERIDLENGTH];
	UInt32			sUDID;
	UInt32			lapTime100;
	UUID128			UDID;									  //	Full UDID

}	DriverLapTimeV2Type;

//	Copied from model/GPSMessageStructures.h

typedef
struct
{
	UInt32							lapTimerCreatorID;			//	Constant to identify sender
	UInt32							sUDID;						//	Hashed value
	UInt32							messageSize;				//	Length of message following this header
	GPSMessageType					messageType;				//	Selector for union below

}	GPSClientServerMessage;

typedef
struct
{
	UInt32							lapTimerCreatorID;			//	Constant to identify sender
	UInt32							sUDID;						//	Hashed value
	UInt32							messageSize;				//	Length of message following this header
	GPSMessageType					messageType;				//	Selector for union below
	UUID128							UDID;						//	128 bit / 16 bytes UUID format

	double							latitude;					//	Position
	double							longitude;

	UInt16							groupAndTrackIDs [0];		//	NOTCERTIFIEDTRACKID terminated list

}	GPSClientServerCurrentPositionV2Message;					//	Variable size

typedef
struct
{
	UInt32							lapTimerCreatorID;			//	Constant to identify sender
	UInt32							sUDID;						//	Hashed value
	UInt32							messageSize;				//	Length of message following this header
	GPSMessageType					messageType;				//	Selector for union below
	UUID128							UDID;						//	128 bit / 16 bytes UUID format

	UInt16							trackID;					//	Unique track id
	UInt32							lapEndSecondsUTC;			//	UTC time, 0 means use server system time
	DriverLapTimeV2Type				timeLapped;
	UInt32							overallDistance10;			//	Distance recorded, used for certification

	char							marshaledVehicle [1];		//	Exists with variable length or '\0'

}	GPSClientServerTimeLappedCertifiedV3Message;				//	Variable size

typedef
struct
{
	UInt32							lapTimerCreatorID;				//	Constant to identify sender
	UInt32							sUDID;							//	Hashed value
	UInt32							messageSize;					//	Length of message following this header
	GPSMessageType					messageType;					//	Selector for union below
	UUID128							UDID;							//	128 bit / 16 bytes UUID format

	UInt16							sku;							//	App sku used currently

	UInt32							iconSize;						//	Size of icon data starting data
	char							data [0];						//	Data for icon followed by zero terminated UTF8 encoded realname,
																	//	followed by zero terminated UTF8 encoded email, status, vehiclename

}	GPSClientServerUserCredentialsMessage;							//	Variable size

typedef
struct
{
	UInt32							lapTimerCreatorID;				//	Constant to identify sender
	UInt32							sUDID;							//	Hashed value
	UInt32							messageSize;					//	Length of message following this header
	GPSMessageType					messageType;					//	Selector for union below
	UUID128							UDID;						//	128 bit / 16 bytes UUID format

	UInt16							groupOrTrackID;					//	Unique group id (special range in trackIDs)
	PositionSetStatusType			status;							//	Message used for both addition / maintenance and deletion

	char							data [0];						//	Zero terminated UTF8 encoded nickname for this group

}	GPSClientServerUserGroupMembershipMessage;						//	Variable size

#pragma pack(pop)

/**********************************************************************
 *
 *	Global configuration
 *
 **********************************************************************/

static int		silent = 0;

/**********************************************************************
 *
 *	Shared data area
 *
 **********************************************************************/

static pthread_mutex_t			messageMutex;
static pthread_cond_t			condSender, condMessenger;
static GPSClientServerMessage	*messageToSend = NULL;
static int						lastMessageSent = 0;  // Used to signal end of messages

/**********************************************************************
 *
 *	Sender implementation
 *
 **********************************************************************/

static char						*serverip;
static int						portno;

static int connectToServer ()
{
	int							sockfd = -1;

	if (!silent)
		fprintf (stdout, "Connecting to server...\n");

	struct hostent				*server;

	sockfd = socket (AF_INET, SOCK_STREAM, 0);

	if (sockfd<0)
	{
		if (!silent)
			fprintf (stderr, "Error creating socket...\n");
	}
	else
	{
		server = gethostbyname (serverip);
		if (server==NULL)
		{
			if (!silent)
				fprintf (stderr, "SERVERIP cannot be resolved...\n");
			sockfd = -1;
		}
	}

	if (sockfd>=0)
	{
		struct sockaddr_in		serv_addr;

		bzero ((char *) &serv_addr, sizeof (serv_addr));
		serv_addr.sin_family = AF_INET;
		bcopy((char *) server->h_addr,
			  (char *) &serv_addr.sin_addr.s_addr,
			  server->h_length);
		serv_addr.sin_port = htons (portno);

		/* Now connect to the server */
		if (connect (sockfd, (struct sockaddr *) &serv_addr, sizeof (serv_addr))<0)
		{
			if (!silent)
				fprintf (stderr, "SERVERIP cannot be connected...\n");
			sockfd = -1;
		}
	}

	if (sockfd>=0)
	{
		//  Make sure write does not raise NOSIGPIPE exception and instead returns an error
		int set = 1;
		setsockopt (sockfd, SOL_SOCKET, SO_NOSIGPIPE, (void *) &set, sizeof (int));
	}
	else
	{
		if (!silent)
			fprintf (stderr, "reconnect () not successful...\n");
	}

	return sockfd;
}

static void	*sender (void *ptr)
{
	int		sockfd = -1;

	while (1)
	{
		pthread_mutex_lock (&messageMutex);	//  protect shared area

		while (!messageToSend&&!lastMessageSent)
			//	Nothing to do here, wait for Messenger to pass in a message
			pthread_cond_wait(&condSender, &messageMutex);

		//	Take ownership of message
		GPSClientServerMessage	*message = messageToSend;

		//	Allow producer to pass in the next message
		messageToSend = NULL;
		pthread_cond_signal (&condMessenger);	//	wake up consumer
		pthread_mutex_unlock (&messageMutex);	//	release the shared area lock

		if (lastMessageSent)
			//	Messenger will not send more messages, exit this thread
			break;

		//	What follows is the (optional) connection making and message submission
		while (message)
		{
			if (sockfd<0)
				sockfd = connectToServer ();

			if (sockfd<0)
			{
				//	Wait shortly before we try the next connect...
				struct timespec rqtp;

				rqtp.tv_sec = 0;
				rqtp.tv_nsec = 500000000L;	// Half a second

				nanosleep (&rqtp, NULL);
			}
			else
			{
				size_t	sizeOfMessage = message->messageSize;
				int     n = write (sockfd, message, sizeOfMessage);

				if (n<0)
				{
					if (!silent)
						perror ("error writing to server socket");

					//	Prepare a reconnect
					close (sockfd);
					sockfd = -1;
				}
				else
				{
					//	Remove message sent from memory
					free (message);
					message = NULL;  // Terminates inner loop
				}
			}
		}
	}

	if (sockfd>=0)
		close (sockfd);

	pthread_exit (0);
}

/**********************************************************************
 *
 *	Messenger implementation
 *
 **********************************************************************/

static void send_message (GPSClientServerMessage *message)
{
	pthread_mutex_lock (&messageMutex);	//  protect shared area

	while (messageToSend)
		//	Message not yet consumed, wait for Sender to consume it
		pthread_cond_wait (&condMessenger, &messageMutex);

	//	Pass in the message for sending
	messageToSend = message;
	if (!message)
		//	Signal we are finsihed
		lastMessageSent = 1;
	pthread_cond_signal (&condSender);		//	Wake up Sender
	pthread_mutex_unlock (&messageMutex);	//	Unlock shared area
}

static void send_laptime (int trackid, char *driver, char *UDID, UInt32 SUDID, UInt32 lapTime100, UInt32 overallDistance10)
{
	UInt32	sizeOfMessage = offsetof (GPSClientServerTimeLappedCertifiedV3Message, marshaledVehicle);

#define MARSHALEDVEHICLE	"LT Test Car"

	sizeOfMessage += strlen (MARSHALEDVEHICLE)+1;

	GPSClientServerTimeLappedCertifiedV3Message *message = (GPSClientServerTimeLappedCertifiedV3Message *) malloc (sizeOfMessage);

    memset (message, 0, sizeOfMessage);
    
    message->lapTimerCreatorID = 'LPTR';
    message->sUDID = SUDID;
    message->messageSize = sizeOfMessage;
    message->messageType = MessageTimeLappedCertifiedV3;

	uuid_t	uuid;

	if (uuid_parse (UDID, uuid)==0)
		message->UDID = *(UUID128 *) &uuid;

    message->trackID = trackid;
    message->lapEndSecondsUTC = 0;

    strncpy (message->timeLapped.driverId, driver, DRIVERIDLENGTH);
    message->timeLapped.sUDID = SUDID;
    message->timeLapped.lapTime100 = lapTime100;
	message->timeLapped.UDID = *(UUID128 *) &uuid;

	message->overallDistance10 = overallDistance10;
    strcpy (message->marshaledVehicle, MARSHALEDVEHICLE);

	send_message ((GPSClientServerMessage *) message);
}

static void send_positionmessage (int trackid, char *UDID, UInt32 SUDID, double latitude, double longitude, int trackMode)
{
	UInt32	sizeOfMessage = offsetof (GPSClientServerCurrentPositionV2Message, groupAndTrackIDs);

	if (trackMode)
		sizeOfMessage += sizeof(UInt16)*3;	//	LAPTIMERGROUPID, trackid, NOTCERTIFIEDTRACKID
	else
		sizeOfMessage += sizeof(UInt16)*2;	//	trackid, NOTCERTIFIEDTRACKID

	GPSClientServerCurrentPositionV2Message   *message = (GPSClientServerCurrentPositionV2Message *) malloc (sizeOfMessage);

    memset (message, 0, sizeOfMessage);

    message->lapTimerCreatorID = 'LPTR';
    message->sUDID = SUDID;
    message->messageSize = sizeOfMessage;
    message->messageType = MessageCurrentPositionV2;

	uuid_t	uuid;

	if (uuid_parse (UDID, uuid)==0)
		message->UDID = *(UUID128 *) &uuid;

	message->latitude = latitude;
	message->longitude = longitude;

	if (trackMode)
	{
		message->groupAndTrackIDs [0] = LAPTIMERGROUPID;
		message->groupAndTrackIDs [1] = trackid;
		message->groupAndTrackIDs [2] = NOTCERTIFIEDTRACKID;
	}
	else
	{
		message->groupAndTrackIDs [0] = trackid;
		message->groupAndTrackIDs [1] = NOTCERTIFIEDTRACKID;
	}

	send_message ((GPSClientServerMessage *) message);
}

static void send_usercredentials (char *UDID, UInt32 SUDID)
{
	UInt32	sizeOfMessage = offsetof (GPSClientServerUserCredentialsMessage, data);

	sizeOfMessage += 4;	//	No image, four zero terminators

	GPSClientServerUserCredentialsMessage   *message = (GPSClientServerUserCredentialsMessage *) malloc (sizeOfMessage);

	memset (message, 0, sizeOfMessage);

	message->lapTimerCreatorID = 'LPTR';
	message->sUDID = SUDID;
	message->messageSize = sizeOfMessage;
	message->messageType = MessageUserCredentials;

	uuid_t	uuid;

	if (uuid_parse (UDID, uuid)==0)
		message->UDID = *(UUID128 *) &uuid;

	message->sku = LAPTIMERPETROLHEADGRANDPRIXSKU;

	send_message ((GPSClientServerMessage *) message);
}

static void send_addmembership (char *UDID, UInt32 SUDID, UInt16 groupOrTrackID, char *nickname)
{
	UInt32	sizeOfMessage = offsetof (GPSClientServerUserGroupMembershipMessage, data);

	sizeOfMessage += strlen(nickname)+1;	//	Nickname

	GPSClientServerUserGroupMembershipMessage   *message = (GPSClientServerUserGroupMembershipMessage *) malloc (sizeOfMessage);

	memset (message, 0, sizeOfMessage);

	message->lapTimerCreatorID = 'LPTR';
	message->sUDID = SUDID;
	message->messageSize = sizeOfMessage;
	message->messageType = MessageUserGroupMembership;

	uuid_t	uuid;

	if (uuid_parse (UDID, uuid)==0)
		message->UDID = *(UUID128 *) &uuid;

	message->groupOrTrackID = groupOrTrackID;
	message->status = PositionSetStatusActive;
	strcpy (message->data, nickname);

	send_message ((GPSClientServerMessage *) message);
}

static void send_lap (sqlite3 *laptimerSqliteDB, int trackid, char *driver, char *UDID, UInt32 SUDID)
{
    sqlite3_stmt    *stmt = NULL;
    int             rc = SQLITE_OK;
    int             idx = -1;
    time_t          startTime = time (NULL);
    
    rc = sqlite3_prepare (laptimerSqliteDB,
                          "SELECT * FROM trackshapes WHERE trackid = :trackid ORDER BY [index] ASC", -1,
                          &stmt, NULL);
    if (rc==SQLITE_OK)
    {
        idx = sqlite3_bind_parameter_index (stmt, ":trackid");
        sqlite3_bind_int (stmt, idx, trackid);

        rc = sqlite3_step (stmt);
        
        while (rc==SQLITE_ROW)
        {
            double  latitude = atof ((char *) sqlite3_column_text (stmt, 3));
            double  longitude = atof ((char *) sqlite3_column_text (stmt, 2));

			send_positionmessage (trackid, UDID, SUDID, latitude, longitude, 1);

            //  Show progress and wait..
            if (!silent)
                fprintf (stdout, ".");
            fflush (stdout);

            sqlite3_sleep (MILLIS);
            
            rc = sqlite3_step (stmt);
        }

        sqlite3_finalize (stmt);
    }
    
    //  Now that the lap is sent, finish with a lap time...
	send_laptime (trackid, driver, UDID, SUDID, (UInt32) ((time (NULL)-startTime)*100l), 0);

    if (!silent)
        fprintf (stdout, "Done.\n");
}

static char         *driver;
static char			*UDID;
static UInt32		SUDID;
static int			trackid;
static char			*sqlitedatabase;

static void	*trackMessenger (void *ptr)
{
	if (!silent)
		fprintf (stdout, "Opening database...\n");

	int             rc = SQLITE_OK;
	static sqlite3	*laptimerSqliteDB;

	sqlite3_initialize ();
	rc = sqlite3_open_v2 (sqlitedatabase, &laptimerSqliteDB, SQLITE_OPEN_READWRITE, NULL);
	if (rc!=SQLITE_OK)
	{
		sqlite3_close (laptimerSqliteDB);
		fprintf (stderr, "SQLITEDATABASE could not be opened...\n");
		exit (1);
	}

	if (!silent)
		fprintf (stdout, "Retrieving plot data...\n");

	sqlite3_stmt    *stmt = NULL;
	int             numpositions = 0;

	rc = sqlite3_prepare (laptimerSqliteDB,
						  "SELECT COUNT (*) FROM trackshapes WHERE trackid = :trackid", -1,
						  &stmt, NULL);
	if (rc==SQLITE_OK)
	{
		int         idx = sqlite3_bind_parameter_index (stmt, ":trackid");

		sqlite3_bind_int (stmt, idx, trackid);

		if ((rc = sqlite3_step (stmt))==SQLITE_ROW)
			numpositions = sqlite3_column_int (stmt, 0);

		sqlite3_finalize (stmt);
	}

	if (!silent)
		fprintf (stdout, "Found %d positions for track id %d\n", numpositions, trackid);

	if (!numpositions)
	{
		sqlite3_close (laptimerSqliteDB);
		fprintf (stderr, "SQLITEDATABASE has no data for TRACKID...\n");
		exit (1);
	}

	if (!silent)
		fprintf (stdout, "Starting message sending...\n");

	send_usercredentials (UDID, SUDID);
	send_addmembership (UDID, SUDID, LAPTIMERGROUPID, driver);
	send_addmembership (UDID, SUDID, trackid, driver);

	int     lap = 1;

	while (lap<=MAXLAPS)
	{
		if (!silent)
			fprintf (stdout, "Lap #%d:\n", lap);
		send_lap (laptimerSqliteDB, trackid, driver, UDID, SUDID);

		lap++;
	}

	sqlite3_close (laptimerSqliteDB);

	//	Signal sender there will be no more messenges...
	send_message (NULL);
	
	pthread_exit (0);
}

#ifndef ULONG_MAX
#	define ULONG_MAX	((uint32_t)-1)
#endif

static void	*groupMessenger (void *ptr)
{
	if (!silent)
		fprintf (stdout, "Starting message sending...\n");

	send_usercredentials (UDID, SUDID);
	send_addmembership (UDID, SUDID, trackid, driver);

	//	Generate a random coordinate

	int		randomLine = 23503.0*arc4random ()/ULONG_MAX;
	FILE	*cityCoordinates = fopen("cityCoordinates.csv", "r");

	char	*line = NULL;
	size_t	len = 0;
	ssize_t read;

	if (cityCoordinates)
	{
		do
		{
			read = getline (&line, &len, cityCoordinates);
			if (read==-1)
				break;
		}
		while (randomLine-->0);

		fclose (cityCoordinates);
	}
	else
	{
		fprintf (stderr, "can't open file cityCoordinates.csv\n");
		exit (1);
	}

	if (line)
	{
		float	longitude;
		float	latitude;

		if (sscanf (line, "%f,%f", &longitude, &latitude)==2)
		{
			int		i = 0;

			while (i<10*60) // 10 minutes
			{
				send_positionmessage (trackid, UDID, SUDID, latitude, longitude, 0);
				sleep (1);
				i++;
			}
		}
		else
		{
			fprintf (stderr, "unexpected coordinate line: %s", line);
			exit (1);
		}

		free(line);
	}
	else
	{
		fprintf (stderr, "no coordinate line found!\n");
		exit (1);
	}

	pthread_exit (0);
}

/**********************************************************************
 *
 *	Messenger implementation
 *
 **********************************************************************/

static void testMessageAlignment ()
{
#	define offsetof_nonpod(NONPODTYPE,MEMBER) ({NONPODTYPE o; (size_t)&(o.MEMBER)-(size_t)&o;})

	fprintf (stderr, "sizeof (GPSClientServerCurrentPositionV2Message) = %lu\n", sizeof (GPSClientServerCurrentPositionV2Message));
	fprintf (stderr, "offsetof (GPSClientServerCurrentPositionV2Message, lapTimerCreatorID) = %lu\n", offsetof_nonpod (GPSClientServerCurrentPositionV2Message, lapTimerCreatorID));
	fprintf (stderr, "offsetof (GPSClientServerCurrentPositionV2Message, UDID) = %lu\n", offsetof_nonpod (GPSClientServerCurrentPositionV2Message, UDID));
	fprintf (stderr, "offsetof (GPSClientServerCurrentPositionV2Message, messageSize) = %lu\n", offsetof_nonpod (GPSClientServerCurrentPositionV2Message, messageSize));
	fprintf (stderr, "offsetof (GPSClientServerCurrentPositionV2Message, messageType) = %lu\n", offsetof_nonpod (GPSClientServerCurrentPositionV2Message, messageType));
	fprintf (stderr, "offsetof (GPSClientServerCurrentPositionV2Message, groupAndTrackIDs) = %lu\n", offsetof_nonpod (GPSClientServerCurrentPositionV2Message, groupAndTrackIDs));

	GPSClientServerCurrentPositionV2Message   testMessage;

	memset (&testMessage, 0, sizeof (GPSClientServerCurrentPositionV2Message));
	testMessage.sUDID = 0x00112233ul;

	char    *textMessageBuffer = (char *) &testMessage;

	for (int i = 0; i<sizeof (GPSClientServerCurrentPositionV2Message); i++)
		fprintf (stderr, "testMessage [%02d] = %#x\n", i, (int) textMessageBuffer [i]);

	if (textMessageBuffer [offsetof_nonpod (GPSClientServerCurrentPositionV2Message, UDID)]==0x00)
		fprintf (stderr, "storage scheme is little endian\n");
	else if (textMessageBuffer [offsetof_nonpod (GPSClientServerCurrentPositionV2Message, UDID)]==0x33)
		fprintf (stderr, "storage scheme is big endian\n");
	else
		fprintf (stderr, "endianness unclear\n");
}

int main (int argc, char *argv[])
{
	/**********************************************************************
	 *
	 *	Prepare run
	 *
	 **********************************************************************/

	//testMessageAlignment ();

	/**********************************************************************
	 *
	 *	Read and check arguments
	 *
	 **********************************************************************/

    if (argc<6)
    {
        fprintf (stderr, "Usage %s SERVERIP DRIVER UDID SUDID TRACKID SQLITEDATABASE\n", argv[0]);
        exit (1);
    }
    
    int             firstArgIndex = 1;
    
    silent = 0;
    if (strcmp (argv[firstArgIndex], "-silent")==0)
    {
        silent = 1;
        firstArgIndex++;
    }

	//	Set connect parameters for connect calls
    serverip = argv[firstArgIndex];
    portno = 1428;

	driver = argv[firstArgIndex+1];
	UDID = argv[firstArgIndex+2];
    SUDID = atol (argv [firstArgIndex+3]);
    trackid = atoi (argv[firstArgIndex+4]);
	sqlitedatabase = argv[firstArgIndex+5];
    
    //  Check valid arguments
    if (trackid<1000)
    {
        fprintf (stderr, "TRACKID needs to be a valid track id (starting at 1000)...\n");
        exit (1);
    }

#define UDIDSIZE								(8+1+4+1+4+1+4+1+12+1)

	if (strlen(UDID)!=UDIDSIZE-1)
	{
		fprintf (stderr, "UDID needs to be formated like 'XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX' (%hu characters expected, %hu found in '%s')...\n",
				 (UInt16) (UDIDSIZE-1), (UInt16) strlen(UDID), UDID);
		exit (1);
	}

	/**********************************************************************
	 *
	 *	Intialize and start threads
	 *
	 **********************************************************************/

	pthread_t		threadMessenger, threadSender;

	// Initialize the mutex and condition variables
	/* What's the NULL for ??? */
	pthread_mutex_init (&messageMutex, NULL);
	pthread_cond_init (&condSender, NULL);		//	Initialize consumer condition variable
	pthread_cond_init (&condMessenger, NULL);	//	Initialize producer condition variable

	// Create the threads
	pthread_create (&threadSender, NULL, sender, NULL);

	if (ISTRACKID (trackid))
		pthread_create (&threadMessenger, NULL, trackMessenger, NULL);
	else
		pthread_create (&threadMessenger, NULL, groupMessenger, NULL);

	// Wait for the threads to finish
	// Otherwise main might run to the end
	// and kill the entire process when it exits.
	pthread_join (threadSender, NULL);
	pthread_join (threadMessenger, NULL);

	// Cleanup -- would happen automatically at end of program
	pthread_mutex_destroy (&messageMutex);			//	Free up the_mutex
	pthread_cond_destroy (&condSender);			//	Free up consumer condition variable
	pthread_cond_destroy (&condMessenger);		//	Free up producer condition variable

	return 0;
}

