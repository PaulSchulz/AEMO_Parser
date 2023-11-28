#ifndef PARSER_H
#define PARSER_H

struct AEMO {
    char   region[8];
	struct tm settlement;
	double price;
	double totaldemand;
	double netinterchange;
	double scheduledgeneration;
	double semischeduledgeneration;
};

void parse_aemo_request(char *ptr, struct AEMO *aemo, char *region);

#endif
