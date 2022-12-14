/****************************************************************************
# File         nk_message.c
# Version      1.8.2
# Description  Send messages to Messaging Queue
# Written by   Daniel Ruus
# Copyright    Daniel Ruus
# Created      2013-02-26
# Modified     2022-09-16
#
# Changelog
# ===============================================
#  1.8.2 2022-09-16 Daniel Ruus
#    - Fixed some compilation warnings [caused by -Wformat-security] due to
#      Ubuntu having changed the default settings for gcc.
#  1.8.1 2018-01-11 Daniel Ruus
#    - Amended the help text as it didn't explain to the user about the new
#      argument -O.
#  1.8.0 2017-04-12 Daniel Ruus
#    - Automatically create a filename for output file with new argument -O.
#      The format of the filename is message-<hostname->-<timestamp>.xml.
#  1.7.2 2017-04-11 Daniel Ruus
#    - Further code cleanup, also replacing most calls to strncpy() with snprintf()
#  1.7.1 2017-03-27 Daniel Ruus
#    - Tidy up the code base
#  1.7  2017-03-27 Daniel Ruus
#    - Modified compile_message() to return pointer to buffer instead of abort
#      string. Also fixed a memory leak found when running valgrind.
#  1.6  2017-02-23 Daniel Ruus
#    - TBD
#  1.5.1 2017-02-22 Daniel Ruus
#    - Removed some testcode used to print out the last modification time
#      for nk_message.c, ie this file. This will mess up the creation of xmlfiles
#      so removed the code,
#  1.5  2017-02-22 Daniel Ruus
#    - Added removal of old files in function purgeMessageFiles()
#  1.4  2017-02-20 Daniel Ruus
#    - Implemented a purge function to delete message files older than
#      a specified number of days
#  1.3  2015-10-16 Daniel Ruus
#    - Include a command line argument which spits out a timestamp
#  1.2  2015-08-26 Daniel Ruus
#    - If no application name given on command line a random string of
#      characters was printed out. Now defaults to nk_message unless overridden.
#  1.1  2015-07-03 Daniel Ruus
#    - Modified the program to work under Windows.
#  1.0  2013-02-26 Daniel Ruus
#    - Initial version
****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>	// For stat()
#include <dirent.h>	// For readdir()
#ifdef WIN32
  #include <Windows.h>
  #include <tchar.h>
#else
  #include <pwd.h>
#endif

#define PROGRAM_NAME "nk_message"
#define MY_VERSION "1.8.2"

/* Define a structure for the XML contents */
struct XMLDATA {
    char host[255];             /* Host name                                            */
    char user[64];              /* User name of the sender                              */
    char timestamp[24];         /* Date and time message is created                     */
    char subject[100];          /* Subject line                                         */
    char text[8192];            /* Message body text                                    */
    char status[32];            /* Status level                                         */
    char app[100];              /* Name of the application/script creating the message  */
};

int print_timestamp(char *timestamp, int bufSize);
int get_hostname(char *hostname, int bufSize);
int compile_message( struct XMLDATA xmldata, char *buffer, int bufSize );
int purgeMessageFiles( int age, char* path );
void usage();
void about();

int main( int argc, char *argv[] )
{

	struct XMLDATA  xmldata;

	int opt, fileAge = 1;
	int isPurge = 0;		// Flag to indicate whether old files should be removed
	int isOutput = 0;		// Flag to indicate that the resulting output is written to a file
	int isBuildFilename = 0;	// Flag to indicate if we are to build our own filename

	char filePath[255] = ".";	// Path for message files, defaults to current dir
	char outputFile[1024];		// File to write to if requested through the argument -o
	char xmlBuf[4096] = {'\0'};
	char timestamp[32] = {'\0'};	// Holding the timestamp if it's been requested
	
	/* Have any arguments been passed? */
	if ( argc < 2 ) {
		usage();
		exit( EXIT_FAILURE );
	}

	// Initialize the structure
	strcpy( xmldata.host, 		"" );
	strcpy( xmldata.user, 		"" );
	strcpy( xmldata.timestamp, 	"" );
	strcpy( xmldata.subject, 	"" );
	strcpy( xmldata.text, 		"" );
	strcpy( xmldata.status, 	"" );
	snprintf( xmldata.app, sizeof(xmldata.app), "%s", PROGRAM_NAME );

	while (( opt = getopt(argc, argv, "hl:s:m:a:c:u:vPA:p:o:OD") ) != -1 ) {
		switch (opt) {
		    case 'h':
			usage();
			exit(EXIT_SUCCESS);
			break;
		    case 'l':
			snprintf( xmldata.status, sizeof(xmldata.status), "%s", optarg );
			break;
		    case 's':
			snprintf( xmldata.subject, sizeof(xmldata.subject), "%s", optarg );
			break;
		    case 'm':
			snprintf( xmldata.text, sizeof(xmldata.text), "%s", optarg );
			break;
		    case 'a':
			snprintf( xmldata.app, sizeof(xmldata.app), "%s", optarg );
			break;
		    case 'c':
			snprintf( xmldata.host, sizeof(xmldata.host), "%s", optarg );
			break;
		    case 'u':
			snprintf( xmldata.user, sizeof(xmldata.user), "%s", optarg );
			break;
		    case 'A':
			fileAge = atoi( optarg );
			break;
		    case 'P':
			isPurge = 1;
			break;
		    case 'p':
			snprintf( filePath, sizeof(filePath), "%s", optarg );
			break;
		    case 'o':
			snprintf( outputFile, sizeof(outputFile), "%s", optarg );
			isOutput = 1;
			break;
		    case 'O':
			//snprint( outputFile, sizeof(outputFile), "%s"
			//build_filename(outputFile, sizeof(outputFile));
			isOutput = 1;
			isBuildFilename = 1;
			break;
		    case 'D':
			//print_timestamp();
			print_timestamp(timestamp, sizeof(timestamp));
			printf("%s\n", timestamp);
			exit(EXIT_SUCCESS);
			break;
		    default: /* '?' */
			usage();
			exit(EXIT_FAILURE);
		}
	}

	// Did the user ask us to purge old files?
	if ( isPurge == 1 ) {
		purgeMessageFiles( fileAge, filePath );
		exit(EXIT_SUCCESS);
	}

	// If the user has elected to let us build the filename, now is the time
	if ( isOutput == 1 && isBuildFilename == 1 ) {
		
		if ( strlen( xmldata.host ) < 2 ) {
			char hostname[255];
			get_hostname(hostname, sizeof(hostname));
			snprintf(xmldata.host, sizeof(xmldata.host), "%s", hostname);
		}
	
		print_timestamp(timestamp, sizeof(timestamp));
		//printf("message-%s-%s.xml\n", xmldata.host, timestamp);
		snprintf(outputFile, sizeof(outputFile), "message-%s-%s.xml", xmldata.host, timestamp);
	}

	// Either print out the xml or write to a file
	if ( isOutput == 0 ) {
		compile_message( xmldata, xmlBuf, sizeof(xmldata) );
		printf("%s\n", xmlBuf);
	} else {
		FILE *fp;
		
		fp = fopen( outputFile, "w+");
		if ( fp == NULL ) {
			perror("fopen");
			exit(1);
		}
		
		compile_message( xmldata, xmlBuf, sizeof(xmldata) );
		fprintf( fp, "%s\n", xmlBuf ) ;
		
		fclose(fp);
	}

	return 0;
	
} // EOF main()


/**
 * Print out a timestamp in the format YYYYMMDDHHMMSS
 */
int print_timestamp(char *timestamp, int bufSize)
{
	time_t t = time(0);
	struct tm* lt = localtime(&t);
	char time_str[21];

	sprintf(time_str, "%04d%02d%02d%02d%02d%02d",
		lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday,
		lt->tm_hour, lt->tm_min, lt->tm_sec
		);

	//printf("%s\n", time_str);
	snprintf(timestamp, bufSize, "%s", time_str);

	return 0;
	
} // EOF print_timestamp()


/**
 * Get the hostname for the current machine
 */
int get_hostname(char *hostname, int bufSize)
{
	char Name[255];
#ifdef WIN32
		// For Windows
		int i=0;
		TCHAR infoBuf[150];
		DWORD bufCharCount = 150;
		memset(Name, 0, 150);
		if( GetComputerName( infoBuf, &bufCharCount ) )
		{
			for(i=0; i<150; i++)
			{
				Name[i] = infoBuf[i];
			}
		}
		else
		{
			strcpy(Name, "Unknown_Host_Name");
		}
		snprintf( hostname, bufSize, "%s", Name );
#else
		// For Posix
		gethostname( Name, 255);
		snprintf(hostname, bufSize, "%s", Name);
#endif
	return 0;
	
} // EOF get_hostname()


/**
 * Purge message files older than fileAge days
 */
int purgeMessageFiles( int fileAge, char* path )
{
	DIR *dir;
	struct dirent *ep;
	struct stat   file_stats;
	time_t sec;
	unsigned int fileModTime;
	unsigned int currentTime;
	char filePath[512];

	printf("Purging files older than %d days\n", fileAge);
	printf("Directory to search for old files in: '%s'\n", path);
	
	// Open directory for reading
	dir = opendir( path );
	if ( dir != NULL ) {
		printf("Directory %s opened\n", path);
		
		// Get the current time
		sec = time(NULL);
		currentTime = sec;
		
		// Loop through the directory and look at 'regular' entries, i.e. files
		while ( (ep = readdir(dir)) != NULL ) {
			if ( ep->d_type == DT_REG && strncmp( ep->d_name, "message-", 8) == 0 ) {				
				// Get the stats for the current file
#ifdef WIN32
				sprintf(filePath, "%s\\%s", path, ep->d_name);
#else
				sprintf(filePath, "%s/%s", path, ep->d_name);
#endif
				printf("Filename: %s    --  ", filePath);
				
				if ((stat(filePath, &file_stats)) == -1 ) {
					perror("fstat");
					return 1;
				}
				
				fileModTime = file_stats.st_mtime;
				if ( currentTime - fileModTime > (fileAge * 86400) ) {
					printf("Old file   ");
					// We have determined that the file is too old, so get rid of itoa
					if ( remove ( filePath ) == 0 ) {
						printf("[File removed]\n");
					} else {
						perror("remove");
					}
					
				} else {
					printf("NEWish file\n");
				}
			}
		}
		(void) closedir(dir);
	} else {
		perror("Couldn't open the directory");
	}
	
	return 0;

} // EOF purgeMessageFiles()


int compile_message( struct XMLDATA xmldata, char *buffer, int bufSize )
{
	char xmltext[16384];
	//char *returnString = malloc( sizeof(char) * 4096 );

	time_t t = time(0);
	struct tm* lt = localtime(&t);
	char time_str[21];

	sprintf(time_str, "%04d-%02d-%02d %02d:%02d:%02d",
		lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday,
		lt->tm_hour, lt->tm_min, lt->tm_sec
		);

	/* If no username provided, use the current user */
	if ( strlen( xmldata.user ) < 2 ) {
#ifdef WIN32
		// For Windows
		char userName[1024];
		snprintf( userName, sizeof(userName), "%s", getenv("USERNAME") );
		if ( strlen( userName ) > 1 ) {
			//strcpy( xmldata.user, userName );
			snprintf(xmldata.user, sizeof(xmldata.user), "%s", userName);
		} else {
			//strcpy( xmldata.user, "No Username" );
			snprintf(xmldata.user, sizeof(xmldata.user), "%s", "No Username");
		}
#else
		// For Posix
		register struct passwd *pw;
		register uid_t  uid;

		uid = geteuid();
		pw  = getpwuid( uid );
		if ( pw ) {
			snprintf( xmldata.user, sizeof(xmldata.user), "%s", pw->pw_name );
		}
#endif
	}

	/* If no hostname provided, use the running systems name */
	if ( strlen( xmldata.host ) < 2 ) {
#ifdef WIN32
/*
		// For Windows
		char Name[150];
		int i=0;
		TCHAR infoBuf[150];
		DWORD bufCharCount = 150;
		memset(Name, 0, 150);
		if( GetComputerName( infoBuf, &bufCharCount ) )
		{
			for(i=0; i<150; i++)
			{
				Name[i] = infoBuf[i];
			}
		}
		else
		{
			strcpy(Name, "Unknown_Host_Name");
		}
		snprintf( xmldata.host, sizeof(xmldata.host), "%s", Name );
		*/
#else
		// For Posix
		//gethostname( xmldata.host, 1024 );
		
#endif
		char hostname[255];
		get_hostname(hostname, sizeof(hostname));
		snprintf(xmldata.host, sizeof(xmldata.host), "%s", hostname);
	}


	/* Set the timestamp */
	snprintf(xmldata.timestamp, sizeof(xmldata.timestamp), "%s", time_str);

	/* We need to convert the status level to a numeric value, as the server expects this. */
	/* Use status level 'info' (value 0) as the default                                    */
	if ( strcmp( xmldata.status, "test" ) == 0 ) {
		strcpy( xmldata.status, "9" );
	}
	else if ( strcmp( xmldata.status, "warn" ) == 0 || strcmp( xmldata.status, "warning" ) == 0 ) {
		strcpy( xmldata.status, "1" );
	}
	else if ( strcmp( xmldata.status, "crit" ) == 0 || strcmp( xmldata.status, "critical" ) == 0 ) {
		strcpy( xmldata.status, "2" );
	}
	else if ( strcmp( xmldata.status, "success" ) == 0 || strcmp( xmldata.status, "successful" ) == 0 ) {
		strcpy ( xmldata.status, "3" );
	}
	else {
		strcpy( xmldata.status, "0" );
	}

	sprintf( xmltext,
		"<?xml version='1.0' standalone='yes'?>\n"
		"<messages>\n "
		"  <message>\n"
		"    <host>%s</host>\n"
		"    <sender>%s</sender>\n"
		"    <timestamp>%s</timestamp>\n"
		"    <subject>%s</subject>\n"
		"    <status>%s</status>\n"
		"    <text>%s</text>\n"
		"    <application>%s</application>\n"
		"  </message>\n"
		"</messages>",
		xmldata.host, xmldata.user, xmldata.timestamp, xmldata.subject, xmldata.status, xmldata.text, xmldata.app );

	snprintf(buffer, bufSize, "%s", xmltext);

	return 0;

} // EOF compile_message()


void about()
{
	printf( "%s, version %s\n", PROGRAM_NAME, MY_VERSION );
	printf( "(c) Copyright 2013-2017 Daniel Ruus, IT-enheten\n" );
    
} // EOF about()

void usage()
{
	about();

	printf( "Usage: nk_message options\n\n" );
	printf( "OPTIONS:\n" );
	printf( "   -h          Show this message\n" );
	printf( "   -l <level>  Status level (info|information|warn|warning|crit|critical|test|success|successful)\n" );
	printf( "   -s <subj>   Subject of message\n" );
	printf( "   -m <msg>    Message text\n" );
	printf( "   -a <app>    Application name\n" );
	printf( "   -c <client> Client host name (used to override the default host name)\n" );
	printf( "   -u <user>   Name of user creating the message\n" );
	printf( "   -P          Purge old message files (default: 1 day, override with -A <days>)\n" );
	printf( "   -p <path>   Path for checking for old message files (default: current directory)\n" );
	printf( "   -o <file>   Output to file <file> instead of to stdout\n" );
	printf( "   -O          Output to file with filename message-<hostname>-<timestamp>.xml\n" );
	printf( "   -A <age>    Files older than age (in days) will be deleted with -P (purge)\n" );
	printf( "   -D          Print out a timestamp in the format YYYYMMDDHHMMSS\n" );
	printf( "   -v          Verbose\n" );

	printf( "\nSTATUS LEVEL:\n" );
	printf( "The status level can be as follows:\n" );
	printf( "Code  Name                Meaning (colour in web interface)\n" );
	printf( "  0   info|information    'Normal', succesful execution of an action (white)\n" );
	printf( "  1   warn|warning        A non-critical problem has occurred (yellow)\n" );
	printf( "  2   crit|critical       A critical problem (red)\n" );
	printf( "  3   success|successful  An 'extra good' successful execution (green)\n" );
	printf( "  9   test                Used when testing messages (blue)\n" );

} // EOF usage()


