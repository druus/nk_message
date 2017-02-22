/****************************************************************************
# File         nk_message.c
# Version      1.4
# Description  Send messages to Messaging Queue
# Written by   Daniel Ruus
# Copyright    Daniel Ruus
# Created      2013-02-26
# Modified     2017-02-20
#
# Changelog
# ===============================================
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
#include <dirent.h>		// For readdir()
#ifdef WIN32
  #include <Windows.h>
  #include <tchar.h>
#else
  #include <pwd.h>
#endif

#define PROGRAM_NAME "nk_message"
#define MY_VERSION "1.4"

/* Define a structure for the XML contents */
struct XMLDATA {
    char host[1024];            /* Host name                                            */
    char user[64];              /* User name of the sender                              */
    char timestamp[20];         /* Date and time message is created                     */
    char subject[100];          /* Subject line                                         */
    char text[8192];            /* Message body text                                    */
    char status[32];            /* Status level                                         */
    char app[100];              /* Name of the application/script creating the message  */
};

int compile_message( struct XMLDATA xmldata );
int purgeMessageFiles( int age, char* path );
void usage();
void about();

int main( int argc, char *argv[] )
{

    struct XMLDATA  xmldata;
    struct hostent *he;

    int count, flags, opt, fileAge = 1;
    int isPurge = 0;
	
	char filePath[255] = ".";	// Path for message files, defaults to current dir

    /* Have any arguments been passed? */
    if ( argc < 2 ) {
        usage();
        exit( EXIT_FAILURE );
    }

    strcpy( xmldata.app, PROGRAM_NAME );

    while (( opt = getopt(argc, argv, "hl:s:m:a:c:u:vPA:p:D") ) != -1 ) {
        switch (opt) {
            case 'h':
                usage();
                exit(EXIT_SUCCESS);
                break;
            case 'l':
                strncpy( xmldata.status, optarg, 31 );
                break;
            case 's':
                strncpy( xmldata.subject, optarg, 99 );
                break;
            case 'm':
                strncpy( xmldata.text, optarg, 8191 );
                break;
            case 'a':
                strncpy( xmldata.app, optarg, 99 );
                break;
            case 'c':
                strncpy( xmldata.host, optarg, 1023 );
                break;
            case 'u':
                strncpy( xmldata.user, optarg, 63 );
                break;
            case 'A':
	            fileAge = atoi( optarg );
		        break;
            case 'P':
	            isPurge = 1;
		        break;
			case 'p':
			    strncpy( filePath, optarg, 254);
				break;
            case 'D':
                print_timestamp();
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


    compile_message( xmldata );

    /**
     * Get last modified time of the created file
     */
    struct stat attr;
    stat( "nk_message.c", &attr );
    printf("Modified: %s\n", ctime(&attr.st_mtime));

    return 0;
} // EOF main()


/**
 * Print out a timestamp in the format YYYYMMDDHHMMSS
 */
int print_timestamp( void )
{
    time_t t = time(0);
    struct tm* lt = localtime(&t);
    char time_str[21];

    sprintf(time_str, "%04d%02d%02d%02d%02d%02d",
      lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday,
      lt->tm_hour, lt->tm_min, lt->tm_sec
    );

    printf("%s\n", time_str);

} // EOF print_timestamp()


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
		while ( ep = readdir(dir) ) {
			if ( ep->d_type == DT_REG && strncmp( ep->d_name, "message-", 8) == 0 ) {
				printf("Filename: %s    --  ", ep->d_name);
				
				// Get the stats for the current file
				if ((stat(ep->d_name, &file_stats)) == -1 ) {
					perror("fstat");
					return 1;
				}
				
				fileModTime = file_stats.st_mtime;
				if ( currentTime - fileModTime > (fileAge * 86400) ) {
					printf("Old file\n");
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


int compile_message( struct XMLDATA xmldata )
{
    char xmltext[4096];

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
        strncpy( userName, getenv("USERNAME"), 1023 );
        if ( strlen( userName ) > 1 ) {
                strcpy( xmldata.user, userName );
        } else {
                strcpy( xmldata.user, "No Username" );
        }
#else
        // For Posix
        register struct passwd *pw;
        register uid_t  uid;

        uid = geteuid();
        pw  = getpwuid( uid );
        if ( pw ) {
            strcpy( xmldata.user, pw->pw_name );
        }
#endif
    }

    /* If no hostname provided, use the running systems name */
    if ( strlen( xmldata.host ) < 2 ) {
#ifdef WIN32
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
        strncpy( xmldata.host, Name, 100 );
#else
        // For Posix
        gethostname( xmldata.host, 1024 );
#endif
    }


    /* Set the timestamp */
    strcpy( xmldata.timestamp, time_str );

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

    printf("%s\n", xmltext);

} // EOF compile_message()


void about()
{
    printf( "%s, version %s\n", PROGRAM_NAME, MY_VERSION );
    printf( "(c) Copyright 2013-2015 Daniel Ruus, IT-enheten\n" );
} // EOF about()

void usage()
{
    about();

    printf( "Usage: nk_message options\n\n" );
    printf( "OPTIONS:\n" );
    printf( "   -h      Show this message\n" );
    printf( "   -l      Status level (info|information|warn|warning|crit|critical|test|success|successful)\n" );
    printf( "   -s      Subject of message\n" );
    printf( "   -m      Message text\n" );
    printf( "   -a      Application name\n" );
    printf( "   -c      Client host name (used to override the default host name)\n" );
    printf( "   -u      Name of user creating the message\n" );
    printf( "   -P      Purge old message files (default: 1 day, override with -A <days>\n" );
    printf( "   -p      Path for checking for old message files (default: current directory\n" );
    printf( "   -A      Files older than age (in days) will be deleted with -P (purge)\n" );
    printf( "   -D      Print out a timestamp in the format YYYYMMDDHHMMSS\n" );
    printf( "   -v      Verbose\n" );

    printf( "\nSTATUS LEVEL:\n" );
    printf( "The status level can be as follows:\n" );
    printf( "Code  Name                Meaning (colour in web interface)\n" );
    printf( "  0   info|information    'Normal', succesful execution of an action (white)\n" );
    printf( "  1   warn|warning        A non-critical problem has occurred (yellow)\n" );
    printf( "  2   crit|critical       A critical problem (red)\n" );
    printf( "  3   success|successful  An 'extra good' successful execution (green)\n" );
    printf( "  9   test                Used when testing messages (blue)\n" );

} // EOF usage()

