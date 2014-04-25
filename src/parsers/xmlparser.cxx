#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <ctype.h>

#include "../hunspell/csutil.hxx"
#include "xmlparser.hxx"


#ifndef W32
using namespace std;
#endif

enum { ST_NON_WORD, ST_WORD, ST_TAG, ST_CHAR_ENTITY, ST_OTHER_TAG, ST_ATTRIB };

static const char * __PATTERN__[][2] = {
	{ "<!--", "-->" },
	{ "<[cdata[", "]]>" }, // XML comment
	{ "<", ">" }
};

#define __PATTERN_LEN__ (sizeof(__PATTERN__) / (sizeof(char *) * 2))

static const char * __PATTERN2__[][2] = {
};

#define __PATTERN_LEN2__ (sizeof(__PATTERN2__) / (sizeof(char *) * 2))

XMLParser::XMLParser()
{
}

XMLParser::XMLParser(const char * wordchars)
{
	init(wordchars);
}

XMLParser::XMLParser(unsigned short * wordchars, int len)
{
	init(wordchars, len);
}

XMLParser::~XMLParser()
{
}

int XMLParser::look_pattern(const char * p[][2], unsigned int len, int column)
{
	for (unsigned int i = 0; i < len; i++) {
		char * j = line[actual] + head;
		const char * k = p[i][column];
		while ((*k != '\0') && (tolower(*j) == *k)) {
			j++;
			k++;
		}
		if (*k == '\0') return i;
	}
	return -1;
}

/*
 * XML parser
 *
 */

char * XMLParser::next_token(const char * PATTERN[][2], unsigned int PATTERN_LEN, const char * PATTERN2[][2], unsigned int PATTERN_LEN2)
{
	const char * latin1;

	for (;;) {
		switch (state)
		{
		case ST_NON_WORD: // non word chars
			prevstate = ST_NON_WORD;
			if ((pattern_num = look_pattern(PATTERN, PATTERN_LEN, 0)) != -1) {
				checkattr = 0;
				if ((pattern2_num = look_pattern(PATTERN2, PATTERN_LEN2, 0)) != -1) {
					checkattr = 1;
				}
				state = ST_TAG;
			} else if (is_wordchar(line[actual] + head)) {
				state = ST_WORD;
				token = head;
			} else if ((latin1 = get_latin1(line[actual] + head))) {
				state = ST_WORD;
				token = head;
				head += strlen(latin1);
			} else if (line[actual][head] == '&') {
				state = ST_CHAR_ENTITY;
			} 			
			break;
		case ST_WORD: // wordchar
			if ((latin1 = get_latin1(line[actual] + head))) {
				head += strlen(latin1);
			} else if (! is_wordchar(line[actual] + head)) {
				state = prevstate;
				char * t = alloc_token(token, &head);
				if (t) return t;
			}
			break;
		case ST_TAG: // comment, labels, etc
			int i;
			if ((checkattr == 1) && ((i = look_pattern(PATTERN2, PATTERN_LEN2, 1)) != -1)
				&& (strcmp(PATTERN2[i][0],PATTERN2[pattern2_num][0]) == 0)) {
					checkattr = 2;
			} else if ((checkattr > 0) && (line[actual][head] == '>')) {
					state = ST_NON_WORD;
			} else if (((i = look_pattern(PATTERN, PATTERN_LEN, 1)) != -1) && 
				(strcmp(PATTERN[i][1],PATTERN[pattern_num][1]) == 0)) {
					state = ST_NON_WORD;
					head += strlen(PATTERN[pattern_num][1]) - 1;
			} else if ( (strcmp(PATTERN[pattern_num][0], "<") == 0) &&
				((line[actual][head] == '"') || (line[actual][head] == '\''))) {
				quotmark = line[actual][head];
				state = ST_ATTRIB;
			}
			break;
		case ST_ATTRIB: // non word chars
			prevstate = ST_ATTRIB;
			if (line[actual][head] == quotmark) {
				state = ST_TAG;
				if (checkattr == 2) checkattr = 1;
			 // for IMG ALT
			} else if (is_wordchar(line[actual] + head) && (checkattr == 2)) {
				state = ST_WORD;
				token = head;
			} else if (line[actual][head] == '&') {
				state = ST_CHAR_ENTITY;
			} 			
			break;
		case ST_CHAR_ENTITY: // SGML element
			if ((tolower(line[actual][head]) == ';')) {
				state = prevstate;
				head--;
			}
		}
                if (next_char(line[actual], &head)) return NULL;
	}
}

char * XMLParser::next_token()
{
	return next_token(__PATTERN__, __PATTERN_LEN__, __PATTERN2__, __PATTERN_LEN2__);
}