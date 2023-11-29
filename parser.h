#ifndef PARSER_H
#define PARSER_H

struct AEMO {
    struct tm timestamp;;
    char   region[8];
	struct tm settlement;
	double price;
	double totaldemand;
	double netinterchange;
	double scheduledgeneration;
	double semischeduledgeneration;
};

#define MAX_REGIONS 5

struct AEMO_ALL {
    int number_tries;
    struct AEMO region[MAX_REGIONS];
};

void parse_aemo_request(char *ptr, struct AEMO *aemo, char *region);
void parse_aemo_request_all(char *ptr, struct AEMO_ALL *aemo_all);

#endif
