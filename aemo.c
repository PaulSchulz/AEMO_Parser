/* AEMO Parser */
// Download AEMO NEM Data. Parse ands Store this data.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <time.h>
#include <stdbool.h>
#include <unistd.h>
#include <libgen.h>
#include <signal.h>
#include "http.h"
#include "MQTTClient.h"
#include "mqtt.h"
#include "sqlite3.h"
#include "parser.h"

#define IDLE 	0
#define FETCH 	1

#define DELAY_LOOP 1
#define DELAY_RETRY 5
#define OFFSET_CHECK 20

bool exitflag;

static void print_usage(char *prg)
{
	// fprintf(stderr, "Usage: %s region [options]\n\n",prg);
	// fprintf(stderr, "Regions: NSW1, QLD1, SA1, TAS1, VIC1\n\n");
    fprintf(stderr, "Usage: %s [options]\n\n", prg);
	fprintf(stderr, "Options:\n");
    fprintf(stderr, "   -q                  Quiet mode (suppress output)\n");
	fprintf(stderr, "	-l <filename> 		Log to file\n");
	fprintf(stderr, "	-m <broker URI> 	Log to MQTT Broker\n");
	fprintf(stderr, "	-t <topic> 		MQTT topic\n");
	fprintf(stderr, "	-u <username> 		Username for MQTT Broker\n");
	fprintf(stderr, "	-p <password> 		Password for MQTT Broker\n");
    fprintf(stderr, "	-s <filename> 		Log to SQLite3 DB\n");
	fprintf(stderr, "\n");
}

int print_aemo_data(struct AEMO *aemo) {
    time_t now;
	struct tm timeinfo;

	time(&now);
	localtime_r(&now, &timeinfo);

	printf("# %04d-%02d-%02d %02d:%02d:%02d ",
	       timeinfo.tm_year + 1900,
	       timeinfo.tm_mon + 1,
	       timeinfo.tm_mday,
	       timeinfo.tm_hour,
	       timeinfo.tm_min,
	       timeinfo.tm_sec);
    printf("#   Region: %s\n",aemo->region);
	printf("#   Settlement Period: %04d-%02d-%02d %02d:%02d:%02d\n",
		   aemo->settlement.tm_year + 1900,
		   aemo->settlement.tm_mon + 1,
           aemo->settlement.tm_mday,
           aemo->settlement.tm_hour,
		   aemo->settlement.tm_min,
		   aemo->settlement.tm_sec);
	printf("#   Price: $%.02f\n",aemo->price);
	printf("#   Total Demand: %.02f MW\n",aemo->totaldemand);
	printf("#   Scheduled Generation (Baseload): %.02f MW\n",aemo->scheduledgeneration);
    printf("#   Semi Scheduled Generation (Renewable): %.02f MW\n",aemo->semischeduledgeneration);
    printf("#   Export: %.02f MW\n",aemo->netinterchange);
    fflush(stdout);

    return 0;
}

void print_aemo_parser_header (void) {
    printf("#\n"
           "#     _    _____ __  __  ___    ____                          \n"
           "#    / \\  | ____|  \\/  |/ _ \\  |  _ \\ __ _ _ __ ___  ___ _ __ \n"
           "#   / _ \\ |  _| | |\\/| | | | | | |_) / _` | '__/ __|/ _ \\ '__|\n"
           "#  / ___ \\| |___| |  | | |_| | |  __/ (_| | |  \\__ \\  __/ |   \n"
           "# /_/   \\_\\_____|_|  |_|\\___/  |_|   \\__,_|_|  |___/\\___|_|   \n"
           "#                                                             \n"
           );
}

void print_aemo_data_header () {
    printf("#\n");
	printf("# Timestamp           | Reg  | Settlement          |    Price |   Demand |      Sch |  SemiSch |  Export |\n");
    printf("#                     |      |                     |  ($/MWh) |     (MW) |     (MW) |     (MW) |    (MW) |\n");
    printf("# --------------------+------+---------------------+----------+----------+----------+----------+---------+\n");
}

void print_aemo_data_record (struct AEMO *aemo) {
    printf("  %04d-%02d-%02d %02d:%02d:%02d | %-4s | %04d-%02d-%02d %02d:%02d:%02d | %8.02f | %8.02f | %8.02f | %8.02f | %8.02f |\n",
           aemo->timestamp.tm_year + 1900,
	       aemo->timestamp.tm_mon + 1,
	       aemo->timestamp.tm_mday,
	       aemo->timestamp.tm_hour,
	       aemo->timestamp.tm_min,
	       aemo->timestamp.tm_sec,
           aemo->region,
	       aemo->settlement.tm_year + 1900,
	       aemo->settlement.tm_mon + 1,
           aemo->settlement.tm_mday,
           aemo->settlement.tm_hour,
	       aemo->settlement.tm_min,
	       aemo->settlement.tm_sec,
	       aemo->price,
           aemo->totaldemand,
	       aemo->scheduledgeneration,
           aemo->semischeduledgeneration,
           aemo->netinterchange);
}

int print_aemo_all_data_record (struct AEMO_ALL *aemo_all) {
    for (int i=0; i<MAX_REGIONS; i++) {
        print_aemo_data_record(&aemo_all->region[i]);
    }

    fflush(stdout);
    return 0;
}

int log_prices_file(FILE *fhandle, struct AEMO *aemo, int number_tries)
{
	time_t now;
	struct tm timeinfo;

	time(&now);
	localtime_r(&now, &timeinfo);

	fprintf(fhandle,"%04d-%02d-%02d %02d:%02d:%02d,",
		    timeinfo.tm_year + 1900,
	        timeinfo.tm_mon + 1,
	        timeinfo.tm_mday,
	        timeinfo.tm_hour,
	        timeinfo.tm_min,
	        timeinfo.tm_sec);

	fprintf(fhandle,"%d,", number_tries);

	fprintf(fhandle,"%04d-%02d-%02d %02d:%02d:%02d,",
		    aemo->settlement.tm_year + 1900,
		    aemo->settlement.tm_mon + 1,
		    aemo->settlement.tm_mday,
		    aemo->settlement.tm_hour,
		    aemo->settlement.tm_min,
		    aemo->settlement.tm_sec);

	fprintf(fhandle,"%.02f,%.02f,%.02f,%.02f,%.02f\r\n",
		    aemo->price,
		    aemo->totaldemand,
		    aemo->netinterchange,
		    aemo->scheduledgeneration,
		    aemo->semischeduledgeneration);

	fflush(fhandle);

    return 0;
}

int log_prices_mqtt(MQTTClient client, char * topic, struct AEMO *aemo)
{
	unsigned char mqtt_str[800];

	sprintf((char * restrict)mqtt_str,"{\"price\":%.02f,\"totaldemand\":%.02f,\"netinterchange\":%.02f,\"scheduledgeneration\":%.02f,\"semischeduledgeneration\":%.02f}",
            aemo->price,
            aemo->totaldemand,
            aemo->netinterchange,
            aemo->scheduledgeneration,
            aemo->semischeduledgeneration);

    MQTT_pub(client, topic ,(char *)mqtt_str);

    return 0;
}


int log_prices_sqlite3(sqlite3* db, struct AEMO *aemo, int number_tries)
{
    aemo_sqlite3_write(db, aemo, number_tries);
    return 0;
}

int log_aemo_all_sqlite3(sqlite3* db, struct AEMO_ALL *aemo_all)
{
    aemo_all_sqlite3_write(db, aemo_all);
    return 0;
}


void ctrlc_handler(int s) {
    exitflag = 1;
}

int main(int argc, char **argv) {
    bool quiet = false;

    char * logfilename = NULL;
    bool logtofile = false;

    char * mqttbrokerURI = NULL;
    char topic[] = {"electricity/5min"};
    char * mqtttopic = topic;
    char * mqttusername = NULL;
    char * mqttpassword = NULL;
    // char * nemregion = NULL;
    bool logtomqtt = false;

    char * sqlitedb = NULL;
    bool logtosqlite = false;

    int verbose = 0;  // 0:No extra message, 1:Event Messages, 2:Debug Messages

    int opt;

    while ((opt = getopt(argc, argv, "q:l:m:t:u:p:s:?")) != -1) {
        switch (opt) {
        case 'q':
            logfilename = (char *)optarg;
            quiet = true;
            break;

        case 'l':
            logfilename = (char *)optarg;
            logtofile = true;
            break;

        case 'm':
            mqttbrokerURI = (char *)optarg;
            logtomqtt = true;
            break;

        case 't':
            mqtttopic = (char *)optarg;
            break;

        case 'u':
            mqttusername = (char *)optarg;
            break;

        case 'p':
            mqttpassword = (char *)optarg;
            break;

        case 's':
            sqlitedb = (char *)optarg;
            logtosqlite = true;
            break;

        default:
            print_usage(basename(argv[0]));
            exit(1);
            break;
        }
    }

    if (!quiet) print_aemo_parser_header();

    //    if (optind >= argc) {
    //printf("No region specified\r\n");
    //print_usage(basename(argv[0]));
    //exit(1);
    //}

    //if (argv[optind] != NULL){
    //nemregion = (char *)argv[optind];
    //}

    if(!quiet) {
        if (logtofile) printf("# AEMO --> File\n");
        if (logtomqtt) printf("# AEMO --> MQTT Connector\n");
        if (logtosqlite) printf("# AEMO --> SQLite3 Database File\n");

        // printf("# Region:       %4s\n", nemregion);
        printf("# Loop delay:   %4d s\n",   DELAY_LOOP);
        printf("# Retry delay:  %4d s\n",  DELAY_RETRY);
        printf("# Check offset: %4d s\n", OFFSET_CHECK);
    }

    CURLcode res;
    unsigned char number_tries;

    char *data = NULL;

    struct buffer out_buf = {
        .data = data,
        .pos = 0
    };

    struct AEMO aemo;
    struct AEMO_ALL aemo_all;

    time_t now;
    struct tm timeinfo;
    int previous_period;

    unsigned int state = IDLE;

    FILE *fhandle;

    if (logtofile) {
        printf("Logging to %s\r\n",logfilename);
        fhandle = fopen(logfilename,"a+");
        if (fhandle == NULL) {
            printf("Unable to open %s for writing\r\n",logfilename);
            exit(1);
        }
    }

    MQTTClient client;

    if (logtomqtt) {
        printf("Connecting to broker: %s\r\n",mqttbrokerURI);
        printf("Publishing to topic: %s\r\n",mqtttopic);
        if (mqttusername) printf("Username: %s\r\n",mqttusername);
        //printf("Password: %s\r\n",mqttpassword);
        client = MQTT_connect(mqttbrokerURI, mqttusername, mqttpassword);
    }

    sqlite3 * db;
    if (logtosqlite) {
        printf("# Connecting to database file: %s\r\n", sqlitedb);
        db = aemo_sqlite3_open(sqlitedb);
        aemo_sqlite3_initialise(db);
    }

    /* Init CTRL-C handler */
    exitflag = 0;
    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = ctrlc_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);


    // Fetch first data - get current settlement period.
    /* Populate data */
    out_buf.data = malloc(16384);
    out_buf.pos = 0;

    res = http_json_request(&out_buf);
    if(res == CURLE_OK) {
        parse_aemo_request_all(out_buf.data, &aemo_all);
        // parse_aemo_request(out_buf.data, &aemo, nemregion);

        // Pointer to first record for time data.
        aemo = aemo_all.region[0];

        if(!quiet) {
            printf("# Current settlement period ends: %04d-%02d-%02d %02d:%02d:%02d\r\n",
               aemo.settlement.tm_year + 1900,
               aemo.settlement.tm_mon + 1,
               aemo.settlement.tm_mday,
               aemo.settlement.tm_hour,
               aemo.settlement.tm_min,
               aemo.settlement.tm_sec);
        }

        previous_period = aemo.settlement.tm_min;

        if(!quiet) {
            print_aemo_data_header();
        }
    } else {
        printf("Error: Failed to download AEMO ELEC_NEM_SUMMARY\r\n");
    }

    while (!exitflag) {
		/* Get Time */
		time(&now);
		localtime_r(&now, &timeinfo);

		switch (state) {

		case IDLE:
            // 20 seconds after a 5 minute period, start fetching a new JSON file
            // Independant of time zone

			if ((!(timeinfo.tm_min % 5)) & (timeinfo.tm_sec == OFFSET_CHECK)) {
				state = FETCH;
				number_tries = 0;

                if (verbose > 0) {
				    printf("# Fetching data for next settlement period. ");
                    // Dots will be placed after this to count fetch requests.
                }
				// Continue to next case if state changed to FETCH
			    // break;
            } else {
                break;
            }

		case FETCH:
			/* Start fetching a new JSON file. We keep trying every 5 seconds until */
			/* the settlement time is different from the previous period */

			/* Allocate a modest buffer now, we can realloc later if needed */
			out_buf.data = malloc(16384);
			out_buf.pos = 0;

			/* Fetch JSON file */
			res = http_json_request(&out_buf);
			if(res == CURLE_OK) {
                if (verbose > 0) {
                    // Print a dot each time we make a HTTP request
                    printf(".");
                    fflush(stdout);
                }
                number_tries++;

                // If HTTP request was successful, parse request
                // parse_aemo_request(out_buf.data, &aemo, nemregion);
                parse_aemo_request_all(out_buf.data, &aemo_all);
                aemo = aemo_all.region[0];

                // Set timestamp
                time(&now);
                localtime_r(&now, &timeinfo);

                aemo.timestamp.tm_year = timeinfo.tm_year;
                aemo.timestamp.tm_mon  = timeinfo.tm_mon;
                aemo.timestamp.tm_mday = timeinfo.tm_mday;
                aemo.timestamp.tm_hour = timeinfo.tm_hour;
                aemo.timestamp.tm_min  = timeinfo.tm_min;
                aemo.timestamp.tm_sec  = timeinfo.tm_sec;
                for (int i=0; i<MAX_REGIONS; i++) {
                    aemo_all.region[i].timestamp.tm_year = timeinfo.tm_year;
                    aemo_all.region[i].timestamp.tm_mon  = timeinfo.tm_mon;
                    aemo_all.region[i].timestamp.tm_mday = timeinfo.tm_mday;
                    aemo_all.region[i].timestamp.tm_hour = timeinfo.tm_hour;
                    aemo_all.region[i].timestamp.tm_min  = timeinfo.tm_min;
                    aemo_all.region[i].timestamp.tm_sec  = timeinfo.tm_sec;
                }

                if (aemo.settlement.tm_min != previous_period) {
	                // Settlement time has changed - new record received
	                previous_period = aemo.settlement.tm_min;

                    if (verbose >= 1) {
                        printf("\n");
                    };

                    if (verbose >= 2) {
                        printf("# %04d-%02d-%02d %02d:%02d:%02d ",
                               aemo.timestamp.tm_year + 1900,
                               aemo.timestamp.tm_mon + 1,
                               aemo.timestamp.tm_mday,
                               aemo.timestamp.tm_hour,
                               aemo.timestamp.tm_min,
                               aemo.timestamp.tm_sec);
                        printf(" %s", aemo.region);
                        print_aemo_data(&aemo);
                    }

                    // Print_aemo_data_record(&aemo);
                    print_aemo_all_data_record(&aemo_all);

                    if (logtofile) log_prices_file(fhandle, &aemo, number_tries);
                    if (logtomqtt) log_prices_mqtt(client, mqtttopic, &aemo);
                    // if (logtosqlite) log_prices_sqlite3(db, &aemo, number_tries);
                    if (logtosqlite) {
                        aemo_all.number_tries = number_tries;
                        log_aemo_all_sqlite3(db, &aemo_all);
                    }

	                // Success, go back to IDLE
	                state = IDLE;
                }
			}
			// No luck, we will try again in five seconds
			sleep(DELAY_RETRY);
			break;
		}
		sleep(DELAY_LOOP);
	}

	printf("\r\nClosing...\r\n");
	if (logtofile) fclose(fhandle);
	if (logtomqtt) MQTT_disconnect(client);
    if (logtosqlite) aemo_sqlite3_close(db);

}
