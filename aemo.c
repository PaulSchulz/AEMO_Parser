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

bool exitflag;

static void print_usage(char *prg)
{
	fprintf(stderr, "Usage: %s region [options]\n\n",prg);
	fprintf(stderr, "Regions: NSW1, QLD1, SA1, TAS1, VIC1\n\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "	-l <filename> 		Log to file\n");
	fprintf(stderr, "	-m <broker URI> 	Log to MQTT Broker\n");
	fprintf(stderr, "	-t <topic> 		MQTT topic\n");
	fprintf(stderr, "	-u <username> 		Username for MQTT Broker\n");
	fprintf(stderr, "	-p <password> 		Password for MQTT Broker\n");
    fprintf(stderr, "	-s <filename> 		Log to SQLite3 DB\n");
	fprintf(stderr, "\n");
}

int print_aemo_data(struct AEMO *aemo)
{
	printf("\r\n");
	printf(" Settlement Period: %04d-%02d-%02d %02d:%02d:%02d\r\n",
		aemo->settlement.tm_year + 1900,
		aemo->settlement.tm_mon + 1,
           aemo->settlement.tm_mday,
           aemo->settlement.tm_hour,
		aemo->settlement.tm_min,
		aemo->settlement.tm_sec);
	printf(" Price: $%.02f\r\n",aemo->price);
	printf(" Total Demand: %.02f MW\r\n",aemo->totaldemand);
	printf(" Scheduled Generation (Baseload): %.02f MW\r\n",aemo->scheduledgeneration);
	printf(" Semi Scheduled Generation (Renewable): %.02f MW\r\n",aemo->semischeduledgeneration);
	printf(" Export: %.02f MW\r\n",aemo->netinterchange);
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


void ctrlc_handler(int s) {
        exitflag = 1;
    }

int main(int argc, char **argv)
{
    char * logfilename = NULL;
    bool logtofile = false;

    char * mqttbrokerURI = NULL;
    char topic[] = {"electricity/5min"};
    char * mqtttopic = topic;
    char * mqttusername = NULL;
    char * mqttpassword = NULL;
    char * nemregion = NULL;
    bool logtomqtt = false;

    char * sqlitedb = NULL;
    bool logtosqlite = false;

    int opt;

    while ((opt = getopt(argc, argv, "l:m:t:u:p:s:?")) != -1) {
        switch (opt) {
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

        if (optind >= argc) {
            printf("No region specified\r\n");
            print_usage(basename(argv[0]));
            exit(1);
        }

        if (argv[optind] != NULL){
            nemregion = (char *)argv[optind];
        }

        printf("Region = %s\r\n", nemregion);

        if (logtofile) printf("AEMO --> File\r\n");
        if (logtomqtt) printf("AEMO --> MQTT Connector\r\n");
        if (logtosqlite) printf("AEMO --> SQLite3 Database File\r\n");

        CURLcode res;
        unsigned char number_tries;

        char *data = NULL;

        struct buffer out_buf = {
            .data = data,
            .pos = 0
        };

        struct AEMO aemo;

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
            printf("Connecting to database file: %s\r\n", sqlitedb);
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

        /* Populate data */
        out_buf.data = malloc(16384);
        out_buf.pos = 0;

        res = http_json_request(&out_buf);
        if(res == CURLE_OK) {
            parse_aemo_request(out_buf.data, &aemo, nemregion);

            printf("Current settlement period %04d-%02d-%02d %02d:%02d:%02d\r\n",
                   aemo.settlement.tm_year + 1900,
                   aemo.settlement.tm_mon + 1,
                   aemo.settlement.tm_mday,
                   aemo.settlement.tm_hour,
                   aemo.settlement.tm_min,
                   aemo.settlement.tm_sec);

            previous_period = aemo.settlement.tm_min;

        } else {
            printf("Failed to download AEMO ELEC_NEM_SUMMARY\r\n");
        }

	while (!exitflag) {

		/* Get Time */
		time(&now);
		localtime_r(&now, &timeinfo);

		switch (state) {

			case IDLE:
				/* 20 seconds after a 5 minute period, start fetching a new JSON file */
				if ((!(timeinfo.tm_min % 5)) & (timeinfo.tm_sec == 20)) {
					state = FETCH;
					number_tries = 0;
					printf("Fetching data for next settlement period");
				}
				break;

			case FETCH:
				/* Start fetching a new JSON file. We keep trying every 5 seconds until */
				/* the settlement time is different from the previous period */

				/* Allocate a modest buffer now, we can realloc later if needed */
				out_buf.data = malloc(16384);
				out_buf.pos = 0;

				/* Fetch JSON file */
				res = http_json_request(&out_buf);
				if(res == CURLE_OK) {
					/* If HTTP request was successful, parse request */
					parse_aemo_request(out_buf.data, &aemo, nemregion);

					/* Print a dot each time we make a HTTP request */
					printf(".");
					fflush(stdout);
					number_tries++;

					if (aemo.settlement.tm_min != previous_period) {
						/* Change in settlement time, log new period */
						previous_period = aemo.settlement.tm_min;

						print_aemo_data(&aemo);
						if (logtofile) log_prices_file(fhandle, &aemo, number_tries);
						if (logtomqtt) log_prices_mqtt(client, mqtttopic, &aemo);
                        if (logtosqlite) log_prices_sqlite3(db, &aemo, number_tries);

						/* Success, go back to IDLE */
						state = IDLE;
					}
				}
				/* No luck, we will try again in five */
				sleep(5);
				break;
		}
		sleep(1);
	}
	printf("\r\nClosing...\r\n");
	if (logtofile) fclose(fhandle);
	if (logtomqtt) MQTT_disconnect(client);
}
